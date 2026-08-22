/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_profile_cards.h"

#include "fa/settings/fa_settings.h"
#include "fa/ui/md3/fa_cards.h"
#include "fa/utils/fa_profile_values.h"
#include "fa/utils/telegram_helpers.h"
#include "fa/lastfm/fa_lastfm_card.h"
#include "fa/lastfm/fa_lastfm_client.h"
#include "fa/lastfm/fa_lastfm_bio_helper.h"
#include "fa/lastfm/fa_lastfm_config.h"
#include "fa_lang_auto.h"

#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/options.h"
#include "base/qt/qt_key_modifiers.h"
#include "base/timer_rpl.h"
#include "base/unixtime.h"
#include "boxes/peers/edit_contact_box.h"
#include "boxes/report_messages_box.h"
#include "boxes/share_box.h"
#include "boxes/star_gift_box.h"
#include "boxes/translate_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/ui_integration.h"
#include "data/business/data_business_common.h"
#include "data/business/data_business_info.h"
#include "data/data_birthday.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_forum_topic.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "info/info_controller.h"
#include "info/info_wrap_widget.h"
#include "info/profile/info_profile_actions.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_phone_menu.h"
#include "info/profile/info_profile_text.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "support/support_helper.h"
#include "ui/boxes/peer_qr_box.h"
#include "ui/effects/toggle_arrow.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/text/format_values.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/text/text_entity.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_separate_id.h"
#include "window/window_session_controller.h"

#include "styles/style_boxes.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "styles/style_info_profile_actions.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

namespace {

using namespace ::Ui;
using namespace ::Info::Profile;
using ::Info::Wrap;

constexpr auto kDay = Data::WorkingInterval::kDay;
constexpr auto kPeerIdLinkIndex = uint16(1);

class DraggableUrlClickHandler final : public UrlClickHandler {
public:
	DraggableUrlClickHandler(const QString &url, QString drag)
	: UrlClickHandler(url, false)
	, _drag(std::move(drag)) {
	}
	QString dragText() const override {
		return _drag;
	}

private:
	const QString _drag;

};

base::options::toggle ShowPeerIdBelowAbout({
	.id = Info::Profile::kOptionShowPeerIdBelowAbout,
	.name = "Show Peer IDs in Profile",
	.description = "Show peer IDs from API below their Bio / Description."
		" Add contact IDs to exported data.",
});

base::options::toggle ShowChannelJoinedBelowAbout({
	.id = Info::Profile::kOptionShowChannelJoinedBelowAbout,
	.name = "Show Channel Joined Date in Profile",
	.description = "Show when you join Channel under its Description.",
});

[[nodiscard]] rpl::producer<TextWithEntities> UsernamesSubtext(
		not_null<PeerData*> peer,
		rpl::producer<QString> fallback) {
	return rpl::combine(
		Info::Profile::UsernamesValue(peer),
		std::move(fallback)
	) | rpl::map([](std::vector<TextWithEntities> usernames, QString text) {
		if (usernames.size() < 2) {
			return TextWithEntities{ .text = text };
		} else {
			auto result = TextWithEntities();
			result.append(tr::lng_info_usernames_label(tr::now));
			result.append(' ');
			auto &&subrange = ranges::make_subrange(
				begin(usernames) + 1,
				end(usernames));
			for (auto &username : std::move(subrange)) {
				const auto isLast = (usernames.back() == username);
				result.append(tr::link(
					'@' + base::take(username.text),
					username.entities.front().data()));
				if (!isLast) {
					result.append(u", "_q);
				}
			}
			return result;
		}
	});
}

[[nodiscard]] rpl::producer<TextWithEntities> TopicSubtext(
		not_null<PeerData*> peer) {
	return rpl::conditional(
		Info::Profile::UsernamesValue(peer) | rpl::map([](std::vector<TextWithEntities> v) {
			return !v.empty();
		}),
		tr::lng_filters_link_subtitle(tr::marked),
		tr::lng_info_link_topic_label(tr::marked));
}

[[nodiscard]] Fn<void(QString)> UsernamesLinkCallback(
		not_null<PeerData*> peer,
		not_null<Window::SessionController*> controller,
		const QString &addToLink) {
	const auto weak = base::make_weak(controller);
	return [=](QString link) {
		if (link.startsWith(u"internal:"_q)) {
			Core::App().openInternalUrl(link,
				QVariant::fromValue(ClickHandlerContext{
					.sessionWindow = weak,
				}));
			return;
		} else if (!link.startsWith(u"https://"_q)) {
			link = peer->session().createInternalLinkFull(peer->username())
				+ addToLink;
		}
		if (!link.isEmpty()) {
			TextUtilities::SetClipboardText(TextForMimeData::Simple(link));
			if (const auto strong = weak.get()) {
				strong->showToast({
					.text = {
						tr::lng_channel_public_link_copied(tr::now),
					},
					.iconLottie = u"toast/voip_invite"_q,
					.iconLottieSize = st::toastLottieIconSize,
				});
			}
		}
	};
}

[[nodiscard]] rpl::producer<TextWithEntities> AboutWithAdvancedValue(
		not_null<PeerData*> peer) {
	return Info::Profile::AboutValue(
		peer
	) | rpl::map([=](TextWithEntities &&value) {
		const auto cleanText = Fa::LastFm::StripBioTag(value.text);
		if (cleanText != value.text) {
			value = Info::Profile::AboutWithEntities(peer, cleanText);
		}
		if (ShowPeerIdBelowAbout.value()) {
			using namespace ::Ui::Text;
			if (!value.empty()) {
				value.append("\n\n");
			}
			value.append(Italic(u"id: "_q));
			const auto raw = peer->id.value & PeerId::kChatTypeMask;
			value.append(Link(
				Italic(Lang::FormatCountDecimal(raw)),
				u"internal:~peer_id~:copy:"_q + QString::number(raw)));
		}
		if (ShowChannelJoinedBelowAbout.value()) {
			if (const auto channel = peer->asChannel()) {
				if (!channel->amCreator() && channel->inviteDate) {
					if (!value.empty()) {
						if (ShowPeerIdBelowAbout.value()) {
							value.append("\n");
						} else {
							value.append("\n\n");
						}
					}
					using namespace ::Ui::Text;
					value.append((channel->isMegagroup()
						? tr::lng_you_joined_group
						: tr::lng_action_you_joined)(
							tr::now,
							tr::italic));
					value.append(Italic(": "));
					const auto raw = channel->inviteDate;
					value.append(Link(
						Italic(langDateTimeFull(base::unixtime::parse(raw))),
						"internal:~join_date~:show:" + QString::number(raw)));
				}
			}
		}
		return std::move(value);
	});
}

void SetupAboutPeerIdDrag(
		not_null<FlatLabel*> label,
		not_null<PeerData*> peer) {
	if (!ShowPeerIdBelowAbout.value()) {
		return;
	}
	const auto id = QString::number(peer->id.value & PeerId::kChatTypeMask);
	Info::Profile::AboutValue(
		peer
	) | rpl::on_next([=] {
		label->setLink(
			kPeerIdLinkIndex,
			std::make_shared<DraggableUrlClickHandler>(
				u"internal:~peer_id~:copy:"_q + id,
				id));
	}, label->lifetime());
}

template <typename T>
bool SetClickContext(
		const ClickHandlerPtr &handler,
		const ClickContext &context) {
	if (const auto casted = std::dynamic_pointer_cast<T>(handler)) {
		casted->T::onClick(context);
		return true;
	}
	return false;
}

template <typename Text, typename ToggleOn, typename Callback>
auto AddActionButton(
		not_null<VerticalLayout*> parent,
		Text &&text,
		ToggleOn &&toggleOn,
		Callback &&callback,
		const style::icon *icon,
		const style::SettingsButton &st = st::infoSharedMediaButton,
		MultiSlideTracker *tracker = nullptr) {
	auto result = parent->add(object_ptr<SlideWrap<SettingsButton>>(
		parent,
		object_ptr<SettingsButton>(
			parent,
			std::move(text),
			st))
	);
	result->setDuration(
		st::infoSlideDuration
	)->toggleOn(
		std::move(toggleOn)
	)->entity()->addClickHandler(std::move(callback));
	result->finishAnimating();
	if (icon) {
		object_ptr<FloatingIcon>(
			result,
			*icon,
			st::infoSharedMediaButtonIconPosition);
	}
	if (tracker) {
		tracker->track(result);
	}
	return result;
}

template <typename Text, typename ToggleOn, typename Callback>
auto AddMainButton(
		not_null<VerticalLayout*> parent,
		Text &&text,
		ToggleOn &&toggleOn,
		Callback &&callback,
		MultiSlideTracker *tracker = nullptr,
		MultiSlideTracker *buttonTracker = nullptr,
		const style::SettingsButton &st = st::infoMainButton) {
	const auto button = AddActionButton(
		parent,
		std::move(text) | rpl::map(tr::upper),
		std::move(toggleOn),
		std::move(callback),
		nullptr,
		st);
	if (tracker) {
		tracker->track(button);
	}
	if (buttonTracker) {
		buttonTracker->track(button);
	}
	return button->entity();
}

[[nodiscard]] bool AreNonTrivialHours(const Data::WorkingHours &hours) {
	if (!hours) {
		return false;
	}
	const auto &intervals = hours.intervals.list;
	for (auto i = 0; i != 7; ++i) {
		const auto day = Data::WorkingInterval{ i * kDay, (i + 1) * kDay };
		for (const auto &interval : intervals) {
			const auto intersection = interval.intersected(day);
			if (intersection && intersection != day) {
				return true;
			}
		}
	}
	return false;
}

[[nodiscard]] TimeId OpensIn(
		const Data::WorkingIntervals &intervals,
		TimeId now) {
	using namespace Data;

	while (now < 0) {
		now += WorkingInterval::kWeek;
	}
	while (now > WorkingInterval::kWeek) {
		now -= WorkingInterval::kWeek;
	}
	auto closest = WorkingInterval::kWeek;
	for (const auto &interval : intervals.list) {
		if (interval.start <= now && interval.end > now) {
			return TimeId(0);
		} else if (interval.start > now && interval.start - now < closest) {
			closest = interval.start - now;
		} else if (interval.start < now) {
			const auto next = interval.start + WorkingInterval::kWeek - now;
			if (next < closest) {
				closest = next;
			}
		}
	}
	return closest;
}

[[nodiscard]] rpl::producer<QString> OpensInText(
		rpl::producer<TimeId> in,
		rpl::producer<bool> hoursExpanded,
		rpl::producer<QString> fallback) {
	return rpl::combine(
		std::move(in),
		std::move(hoursExpanded),
		std::move(fallback)
	) | rpl::map([](TimeId in, bool hoursExpanded, QString fallback) {
		return (!in || hoursExpanded)
			? std::move(fallback)
			: (in >= 86400)
			? tr::lng_info_hours_opens_in_days(tr::now, lt_count, in / 86400)
			: (in >= 3600)
			? tr::lng_info_hours_opens_in_hours(tr::now, lt_count, in / 3600)
			: tr::lng_info_hours_opens_in_minutes(
				tr::now,
				lt_count,
				std::max(in / 60, 1));
	});
}

[[nodiscard]] QString FormatDayTime(TimeId time) {
	const auto wrap = [](TimeId value) {
		const auto hours = value / 3600;
		const auto minutes = (value % 3600) / 60;
		return QString::number(hours).rightJustified(2, u'0')
			+ ':'
			+ QString::number(minutes).rightJustified(2, u'0');
	};
	return (time > kDay)
		? tr::lng_info_hours_next_day(tr::now, lt_time, wrap(time - kDay))
		: wrap(time == kDay ? 0 : time);
}

[[nodiscard]] QString JoinIntervals(const Data::WorkingIntervals &data) {
	auto result = QStringList();
	result.reserve(data.list.size());
	for (const auto &interval : data.list) {
		const auto start = FormatDayTime(interval.start);
		const auto end = FormatDayTime(interval.end);
		result.push_back(start + u" - "_q + end);
	}
	return result.join('\n');
}

[[nodiscard]] QString FormatDayHours(
		const Data::WorkingHours &hours,
		const Data::WorkingIntervals &mine,
		bool my,
		int day) {
	using namespace Data;

	const auto local = ExtractDayIntervals(hours.intervals, day);
	if (IsFullOpen(local)) {
		return tr::lng_info_hours_open_full(tr::now);
	}
	const auto use = my ? ExtractDayIntervals(mine, day) : local;
	if (!use) {
		return tr::lng_info_hours_closed(tr::now);
	}
	return JoinIntervals(use);
}

[[nodiscard]] Data::WorkingIntervals ShiftedIntervals(
		Data::WorkingIntervals intervals,
		int delta) {
	auto &list = intervals.list;
	if (!delta || list.empty()) {
		return { std::move(list) };
	}
	for (auto &interval : list) {
		interval.start += delta;
		interval.end += delta;
	}
	while (list.front().start < 0) {
		constexpr auto kWeek = Data::WorkingInterval::kWeek;
		const auto first = list.front();
		if (first.end > 0) {
			list.push_back({ first.start + kWeek, kWeek });
			list.front().start = 0;
		} else {
			list.push_back(first.shifted(kWeek));
			list.erase(list.begin());
		}
	}
	return intervals.normalized();
}

[[nodiscard]] object_ptr<SlideWrap<>> CreateWorkingHours(
		not_null<QWidget*> parent,
		not_null<UserData*> user) {
	using namespace Data;

	auto result = object_ptr<SlideWrap<RoundButton>>(
		parent,
		object_ptr<RoundButton>(
			parent,
			rpl::single(QString()),
			st::infoHoursOuter),
		st::infoProfileLabeledPadding - st::infoHoursOuterMargin);
	const auto button = result->entity();
	button->setTextTransform(RoundButtonTextTransform::ToUpper);
	const auto inner = CreateChild<VerticalLayout>(button);
	button->widthValue() | rpl::on_next([=](int width) {
		const auto margin = st::infoHoursOuterMargin;
		inner->resizeToWidth(width - margin.left() - margin.right());
		inner->move(margin.left(), margin.top());
	}, inner->lifetime());
	inner->heightValue() | rpl::on_next([=](int height) {
		const auto margin = st::infoHoursOuterMargin;
		height += margin.top() + margin.bottom();
		button->resize(button->width(), height);
	}, inner->lifetime());

	const auto info = &user->owner().businessInfo();

	struct State {
		rpl::variable<WorkingHours> hours;
		rpl::variable<TimeId> time;
		rpl::variable<int> day;
		rpl::variable<int> timezoneDelta;

		rpl::variable<WorkingIntervals> mine;
		rpl::variable<WorkingIntervals> mineByDays;
		rpl::variable<TimeId> opensIn;
		rpl::variable<bool> opened;
		rpl::variable<bool> expanded;
		rpl::variable<bool> nonTrivial;
		rpl::variable<bool> myTimezone;

		rpl::event_stream<> recounts;
	};
	const auto state = inner->lifetime().make_state<State>();

	auto recounts = state->recounts.events_starting_with_copy(rpl::empty);
	const auto recount = [=] {
		state->recounts.fire({});
	};

	state->hours = user->session().changes().peerFlagsValue(
		user,
		PeerUpdate::Flag::BusinessDetails
	) | rpl::map([=] {
		return user->businessDetails().hours;
	});
	state->nonTrivial = state->hours.value() | rpl::map(AreNonTrivialHours);

	const auto seconds = QTime::currentTime().msecsSinceStartOfDay() / 1000;
	const auto inMinute = seconds % 60;
	const auto firstTick = inMinute ? (61 - inMinute) : 1;
	state->time = rpl::single(rpl::empty) | rpl::then(
		base::timer_once(firstTick * crl::time(1000))
	) | rpl::then(
		base::timer_each(60 * crl::time(1000))
	) | rpl::map([] {
		const auto local = QDateTime::currentDateTime();
		const auto day = local.date().dayOfWeek() - 1;
		const auto seconds = local.time().msecsSinceStartOfDay() / 1000;
		return day * kDay + seconds;
	});

	state->day = state->time.value() | rpl::map([](TimeId time) {
		return time / kDay;
	});
	state->timezoneDelta = rpl::combine(
		state->hours.value(),
		info->timezonesValue()
	) | rpl::filter([](
			const WorkingHours &hours,
			const Timezones &timezones) {
		return ranges::contains(
			timezones.list,
			hours.timezoneId,
			&Timezone::id);
	}) | rpl::map([](WorkingHours &&hours, const Timezones &timezones) {
		const auto &list = timezones.list;
		const auto closest = FindClosestTimezoneId(list);
		const auto i = ranges::find(list, closest, &Timezone::id);
		const auto j = ranges::find(list, hours.timezoneId, &Timezone::id);
		Assert(i != end(list));
		Assert(j != end(list));
		return i->utcOffset - j->utcOffset;
	});

	state->mine = rpl::combine(
		state->hours.value(),
		state->timezoneDelta.value()
	) | rpl::map([](WorkingHours &&hours, int delta) {
		return ShiftedIntervals(hours.intervals, delta);
	});

	state->opensIn = rpl::combine(
		state->mine.value(),
		state->time.value()
	) | rpl::map([](const WorkingIntervals &mine, TimeId time) {
		return OpensIn(mine, time);
	});
	state->opened = state->opensIn.value() | rpl::map(rpl::mappers::_1 == 0);

	state->mineByDays = rpl::combine(
		state->hours.value(),
		state->timezoneDelta.value()
	) | rpl::map([](WorkingHours &&hours, int delta) {
		auto full = std::array<bool, 7>();
		auto withoutFullDays = hours.intervals;
		for (auto i = 0; i != 7; ++i) {
			if (IsFullOpen(ExtractDayIntervals(hours.intervals, i))) {
				full[i] = true;
				withoutFullDays = ReplaceDayIntervals(
					withoutFullDays,
					i,
					Data::WorkingIntervals());
			}
		}
		auto result = ShiftedIntervals(withoutFullDays, delta);
		for (auto i = 0; i != 7; ++i) {
			if (full[i]) {
				result = ReplaceDayIntervals(
					result,
					i,
					Data::WorkingIntervals{ { { 0, kDay } } });
			}
		}
		return result;
	});

	const auto dayHoursText = [=](int day) {
		return rpl::combine(
			state->hours.value(),
			state->mineByDays.value(),
			state->myTimezone.value()
		) | rpl::map([=](
				const WorkingHours &hours,
				const WorkingIntervals &mine,
				bool my) {
			return FormatDayHours(hours, mine, my, day);
		});
	};
	const auto dayHoursTextValue = [=](rpl::producer<int> day) {
		return std::move(day)
			| rpl::map(dayHoursText)
			| rpl::flatten_latest();
	};

	const auto openedWrap = inner->add(object_ptr<RpWidget>(inner));
	const auto opened = CreateChild<FlatLabel>(
		openedWrap,
		rpl::conditional(
			state->opened.value(),
			tr::lng_info_work_open(),
			tr::lng_info_work_closed()
		) | rpl::after_next(recount),
		st::infoHoursState);
	opened->setAttribute(Qt::WA_TransparentForMouseEvents);
	const auto timing = CreateChild<FlatLabel>(
		openedWrap,
		OpensInText(
			state->opensIn.value(),
			state->expanded.value(),
			dayHoursTextValue(state->day.value())
		) | rpl::after_next(recount),
		st::infoHoursValue);
	const auto timingArrow = CreateChild<RpWidget>(openedWrap);
	timingArrow->resize(Size(timing->st().style.font->height));
	timing->setAttribute(Qt::WA_TransparentForMouseEvents);
	state->opened.value() | rpl::on_next([=](bool value) {
		opened->setTextColorOverride(value
			? st::boxTextFgGood->c
			: st::boxTextFgError->c);
	}, opened->lifetime());

	rpl::combine(
		openedWrap->widthValue(),
		opened->heightValue(),
		timing->sizeValue()
	) | rpl::on_next([=](int width, int h1, QSize size) {
		opened->moveToLeft(0, 0, width);
		timingArrow->moveToRight(0, 0, width);
		timing->moveToRight(timingArrow->width(), 0, width);

		const auto margins = opened->getMargins();
		const auto added = margins.top() + margins.bottom();
		openedWrap->resize(width, std::max(h1, size.height()) - added);
	}, openedWrap->lifetime());

	rpl::combine(
		state->opened.value(),
		state->opensIn.value(),
		state->expanded.value(),
		dayHoursTextValue(state->day.value())
	) | rpl::on_next([=](
			bool opened,
			TimeId opensIn,
			bool expanded,
			const QString &timing) {
		const auto status = (opened
			? tr::lng_info_work_open
			: tr::lng_info_work_closed)(tr::now);
		const auto when = (!opensIn || expanded)
			? timing
			: (opensIn >= 86400)
			? tr::lng_info_hours_opens_in_days(tr::now, lt_count, opensIn / 86400)
			: (opensIn >= 3600)
			? tr::lng_info_hours_opens_in_hours(tr::now, lt_count, opensIn / 3600)
			: tr::lng_info_hours_opens_in_minutes(
				tr::now,
				lt_count,
				std::max(opensIn / 60, 1));
		button->setAccessibleName(
			tr::lng_info_hours_label(tr::now) + ": " + status + ", " + when);
	}, inner->lifetime());

	const auto labelWrap = inner->add(object_ptr<RpWidget>(inner));
	const auto label = CreateChild<FlatLabel>(
		labelWrap,
		tr::lng_info_hours_label(),
		st::infoLabel);
	label->setAttribute(Qt::WA_TransparentForMouseEvents);
	auto linkText = rpl::combine(
		state->nonTrivial.value(),
		state->hours.value(),
		state->mine.value(),
		state->myTimezone.value()
	) | rpl::map([=](
			bool complex,
			const WorkingHours &hours,
			const WorkingIntervals &mine,
			bool my) {
		return (!complex || hours.intervals == mine)
			? rpl::single(QString())
			: my
			? tr::lng_info_hours_my_time()
			: tr::lng_info_hours_local_time();
	}) | rpl::flatten_latest();
	const auto link = CreateChild<RoundButton>(
		labelWrap,
		std::move(linkText),
		st::defaultTableSmallButton);
	link->setClickedCallback([=] {
		state->myTimezone = !state->myTimezone.current();
		state->expanded = true;
	});

	rpl::combine(
		labelWrap->widthValue(),
		label->heightValue(),
		link->sizeValue()
	) | rpl::on_next([=](int width, int h1, QSize size) {
		label->moveToLeft(0, 0, width);
		link->moveToRight(0, 0, width);

		const auto margins = label->getMargins();
		const auto added = margins.top() + margins.bottom();
		labelWrap->resize(width, std::max(h1, size.height()) - added);
	}, labelWrap->lifetime());

	const auto other = inner->add(
		object_ptr<SlideWrap<VerticalLayout>>(
			inner,
			object_ptr<VerticalLayout>(inner)));
	other->toggleOn(state->expanded.value(), anim::type::normal);
	constexpr auto kSlideDuration = float64(st::slideWrapDuration);
	other->setDuration(kSlideDuration);
	{
		const auto arrowAnimation
			= other->lifetime().make_state<Animations::Basic>();
		arrowAnimation->init([=] {
			timingArrow->update();
			if (!other->animating()) {
				arrowAnimation->stop();
			}
		});
		timingArrow->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(timingArrow);
			const auto progress = other->animating()
				? (crl::now() - arrowAnimation->started()) / kSlideDuration
				: 1.;

			const auto path = ToggleUpDownArrowPath(
				timingArrow->width() / 2,
				timingArrow->height() / 2,
				st::infoHoursArrowSize,
				st::mainMenuToggleFourStrokes,
				other->toggled() ? progress : 1 - progress);

			auto hq = PainterHighQualityEnabler(p);
			p.fillPath(path, timing->st().textFg);
		}, timingArrow->lifetime());
		state->expanded.value() | rpl::on_next([=] {
			arrowAnimation->start();
		}, other->lifetime());
	}

	other->finishAnimating();
	const auto days = other->entity();

	for (auto i = 1; i != 7; ++i) {
		const auto dayWrap = days->add(
			object_ptr<RpWidget>(other),
			QMargins(0, st::infoHoursDaySkip, 0, 0));
		auto label = state->day.value() | rpl::map([=](int day) {
			switch ((day + i) % 7) {
			case 0: return tr::lng_hours_monday();
			case 1: return tr::lng_hours_tuesday();
			case 2: return tr::lng_hours_wednesday();
			case 3: return tr::lng_hours_thursday();
			case 4: return tr::lng_hours_friday();
			case 5: return tr::lng_hours_saturday();
			case 6: return tr::lng_hours_sunday();
			}
			Unexpected("Index in working hours.");
		}) | rpl::flatten_latest();
		const auto dayLabel = CreateChild<FlatLabel>(
			dayWrap,
			std::move(label),
			st::infoHoursDayLabel);
		dayLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
		const auto dayHours = CreateChild<FlatLabel>(
			dayWrap,
			dayHoursTextValue(state->day.value()
				| rpl::map((rpl::mappers::_1 + i) % 7)),
			st::infoHoursValue);
		dayHours->setAttribute(Qt::WA_TransparentForMouseEvents);
		rpl::combine(
			dayWrap->widthValue(),
			dayLabel->heightValue(),
			dayHours->sizeValue()
		) | rpl::on_next([=](int width, int h1, QSize size) {
			dayLabel->moveToLeft(0, 0, width);
			dayHours->moveToRight(0, 0, width);

			const auto margins = dayLabel->getMargins();
			const auto added = margins.top() + margins.bottom();
			dayWrap->resize(width, std::max(h1, size.height()) - added);
		}, dayWrap->lifetime());
	}

	button->setClickedCallback([=] {
		state->expanded = !state->expanded.current();
	});

	result->toggleOn(state->hours.value(
	) | rpl::map([](const WorkingHours &data) {
		return bool(data);
	}));

	return result;
}

void DeleteContactNote(
		not_null<UserData*> user,
		Fn<void(const QString &)> showError = nullptr) {
	user->session().api().request(MTPcontacts_UpdateContactNote(
		user->inputUser(),
		MTP_textWithEntities(MTP_string(), MTP_vector<MTPMessageEntity>())
	)).done([=] {
		user->setNote(TextWithEntities());
	}).fail([=](const MTP::Error &error) {
		if (showError) {
			showError(error.description());
		}
	}).send();
}

[[nodiscard]] object_ptr<SlideWrap<>> CreateNotes(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	auto allNotesText = user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::FullInfo
	) | rpl::map([=] {
		return user->note();
	});

	auto notesText = rpl::duplicate(
		allNotesText
	) | rpl::filter([](const TextWithEntities &note) {
		return !note.text.isEmpty();
	});

	auto result = object_ptr<SlideWrap<VerticalLayout>>(
		parent,
		object_ptr<VerticalLayout>(parent));
	result->toggleOn(rpl::duplicate(
		allNotesText
	) | rpl::map([](const TextWithEntities &note) {
		return !note.text.isEmpty();
	}));
	result->finishAnimating();

	const auto notesContainer = result->entity();

	const auto context = ::Ui::Text::MarkedContext{
		.customEmojiFactory = user->owner().customEmojiManager().factory(
			Data::CustomEmojiManager::SizeTag::Normal)
	};

	auto notesLine = CreateTextWithLabel(
		notesContainer,
		tr::lng_info_notes_label(TextWithEntities::Simple),
		rpl::duplicate(notesText),
		st::infoLabel,
		st::infoLabeled,
		st::infoProfileLabeledPadding);

	std::move(
		notesText
	) | rpl::on_next([=, raw = notesLine.text](
			TextWithEntities note) {
		TextUtilities::ParseEntities(note, TextParseLinks);
		raw->setMarkedText(note, context);
	}, notesLine.text->lifetime());

	notesLine.text->setContextMenuHook([=, raw = notesLine.text](
			FlatLabel::ContextMenuRequest request) {
		raw->fillContextMenu(request);
		const auto addAction = ::Ui::Menu::CreateAddActionCallback(
			request.menu);
		addAction({
			.text = tr::lng_edit_note(tr::now),
			.handler = [=] {
				controller->window().show(
					Box(EditContactNoteBox, controller, user));
			},
		});
		addAction({
			.text = tr::lng_delete_note(tr::now),
			.handler = [=] {
				DeleteContactNote(user, [=](const QString &error) {
					controller->showToast(error);
				});
			},
			.isAttention = true,
		});
	});

	rpl::merge(
		notesLine.wrap->events(),
		notesLine.subtext->events()
	) | rpl::on_next([=, raw = notesLine.text](not_null<QEvent*> e) {
		if (e->type() == QEvent::ContextMenu) {
			const auto ce = static_cast<QContextMenuEvent*>(e.get());
			QCoreApplication::postEvent(
				raw,
				new QContextMenuEvent(
					ce->reason(),
					ce->pos(),
					ce->globalPos()));
		}
	}, notesLine.wrap->lifetime());

	const auto subtextLabel = CreateChild<FlatLabel>(
		notesLine.wrap->entity(),
		tr::lng_info_notes_private(tr::now),
		st::infoLabel);
	subtextLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

	rpl::combine(
		notesLine.wrap->entity()->widthValue(),
		notesLine.subtext->geometryValue()
	) | rpl::on_next([=, skip = st::lineWidth * 5](
			int width,
			const QRect &subtextGeometry) {
		subtextLabel->moveToRight(
			0,
			subtextGeometry.y() + skip,
			width);
	}, subtextLabel->lifetime());

	notesContainer->add(std::move(notesLine.wrap));

	return result;
}

[[nodiscard]] object_ptr<SlideWrap<>> CreateBirthday(
		not_null<QWidget*> parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	using namespace Data;

	auto result = object_ptr<SlideWrap<RoundButton>>(
		parent,
		object_ptr<RoundButton>(
			parent,
			rpl::single(QString()),
			st::infoHoursOuter),
		st::infoProfileLabeledPadding - st::infoHoursOuterMargin);
	result->setDuration(st::infoSlideDuration);
	const auto button = result->entity();
	button->setTextTransform(RoundButtonTextTransform::ToUpper);

	auto outer = CreateChild<SlideWrap<VerticalLayout>>(
		button,
		object_ptr<VerticalLayout>(button),
		st::infoHoursOuterMargin);
	const auto layout = outer->entity();
	layout->setAttribute(Qt::WA_TransparentForMouseEvents);

	auto birthday = BirthdayValue(
		user
	) | rpl::start_spawning(result->lifetime());

	auto label = BirthdayLabelText(rpl::duplicate(birthday));
	auto text = BirthdayValueText(
		rpl::duplicate(birthday)
	) | rpl::map(tr::marked);

	const auto giftIcon = CreateChild<RpWidget>(layout);
	giftIcon->resize(st::birthdayTodayIcon.size());
	layout->sizeValue() | rpl::on_next([=](QSize size) {
		giftIcon->moveToRight(
			0,
			(size.height() - giftIcon->height()) / 2,
			size.width());
	}, giftIcon->lifetime());
	giftIcon->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(giftIcon);
		st::birthdayTodayIcon.paint(p, 0, 0, giftIcon->width());
	}, giftIcon->lifetime());

	rpl::duplicate(
		birthday
	) | rpl::map([](Data::Birthday value) {
		return Data::IsBirthdayTodayValue(value);
	}) | rpl::flatten_latest(
	) | rpl::distinct_until_changed(
	) | rpl::on_next([=](bool today) {
		const auto disable = !today && user->session().premiumCanBuy();
		button->setDisabled(disable);
		button->setAttribute(Qt::WA_TransparentForMouseEvents, disable);
		button->clearState();
		giftIcon->setVisible(!disable);
	}, result->lifetime());

	BirthdayValueText(
		rpl::duplicate(birthday),
		true
	) | rpl::on_next([=](const QString &accessibleText) {
		button->setAccessibleName(
			tr::lng_info_birthday_label(tr::now) + ": " + accessibleText);
	}, button->lifetime());

	auto nonEmptyText = std::move(
		text
	) | rpl::before_next([slide = result.data()](
			const TextWithEntities &value) {
		if (value.text.isEmpty()) {
			slide->hide(anim::type::normal);
		}
	}) | rpl::filter([](const TextWithEntities &value) {
		return !value.text.isEmpty();
	}) | rpl::after_next([slide = result.data()](
			const TextWithEntities &value) {
		slide->show(anim::type::normal);
	});
	layout->add(object_ptr<FlatLabel>(
		layout,
		std::move(nonEmptyText),
		st::birthdayLabeled));
	layout->add(CreateSkipWidget(layout, st::infoLabelSkip));
	layout->add(object_ptr<FlatLabel>(
		layout,
		std::move(
			label
		) | rpl::after_next([=] {
			layout->resizeToWidth(layout->widthNoMargins());
		}),
		st::birthdayLabel));
	result->finishAnimating();

	ResizeFitChild(button, outer);

	button->setClickedCallback([=] {
		if (!button->isDisabled()) {
			ShowStarGiftBox(controller, user);
		}
	});

	return result;
}

} // namespace

namespace FA::Ui {

::Info::Profile::Section MakeLastFmCard(
		not_null<::Info::Controller*> controller,
		not_null<UserData*> user,
		not_null<::Ui::VerticalLayout*> parent) {
	using namespace ::Ui;
	using namespace ::Info::Profile;

	const auto parentCtrl = controller->parentController();

	auto wrap = object_ptr<SlideWrap<VerticalLayout>>(
		parent,
		object_ptr<VerticalLayout>(parent));
	const auto raw = wrap.data();
	raw->hide(anim::type::instant);

	const auto lastFmCard = raw->entity()->add(
		object_ptr<Fa::LastFm::NowPlayingCard>(raw->entity(), parentCtrl),
		style::margins(16, 6, 16, 6));

	struct LastFmState {
		QString currentUsername;
		base::Timer pollTimer;
	};
	const auto state = lastFmCard->lifetime().make_state<LastFmState>();

	const auto poll = [=] {
		if (state->currentUsername.isEmpty()) {
			return;
		}
		const auto &settings = user->session().settings();
		const auto customKey = (user->isSelf() && settings.lastFmUseCustomApiKey())
			? settings.lastFmCustomApiKey()
			: QString();
		Fa::LastFm::Client::Instance().fetchNowPlaying(
			state->currentUsername,
			[=](std::optional<Fa::LastFm::LastFmTrack> track) {
				lastFmCard->setTrack(track);
				raw->toggle(track.has_value(), anim::type::normal);
			},
			customKey);
	};
	state->pollTimer.setCallback(poll);

	const auto resolveUsername = [=] {
		auto username = Fa::LastFm::ExtractLastFmUsername(user->about()).value_or(QString());
		if (username.isEmpty() && user->isSelf()) {
			const auto &settings = user->session().settings();
			if (settings.lastFmShowOnProfile() && !settings.lastFmUsername().isEmpty()) {
				username = settings.lastFmUsername().trimmed();
			}
		}
		return username;
	};

	const auto updateUsername = [=] {
		const auto username = resolveUsername();
		if (state->currentUsername != username) {
			state->currentUsername = username;
			lastFmCard->setUsername(username);
			if (!username.isEmpty()) {
				poll();
				state->pollTimer.callEach(Fa::LastFm::kPollInterval);
			} else {
				state->pollTimer.cancel();
				lastFmCard->setTrack(std::nullopt);
				raw->toggle(false, anim::type::instant);
			}
		}
	};

	user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::About
	) | rpl::on_next(updateUsername, lastFmCard->lifetime());

	if (user->isSelf()) {
		rpl::combine(
			user->session().settings().lastFmUsernameChanges(),
			user->session().settings().lastFmShowOnProfileChanges()
		) | rpl::on_next(updateUsername, lastFmCard->lifetime());
	}

	return {
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

::Info::Profile::Section MakeProfileInfo(
		not_null<::Info::Controller*> controller,
		not_null<PeerData*> peer,
		const Data::ForumTopic *topic,
		not_null<::Ui::VerticalLayout*> parent) {
	using namespace ::Ui;
	using namespace ::Info::Profile;
	using ::Info::Wrap;

	auto wrap = object_ptr<SlideWrap<VerticalLayout>>(
		parent,
		object_ptr<VerticalLayout>(parent));
	const auto raw = wrap.data();
	auto tracker = MultiSlideTracker();

	const auto infoClickFilter = [=,
		p = peer.get(),
		window = controller->parentController()](
			const ClickHandlerPtr &handler,
			Qt::MouseButton button) {
		const auto context = ClickContext{
			button,
			QVariant::fromValue(ClickHandlerContext{
				.sessionWindow = base::make_weak(window),
				.peer = p,
			})
		};
		if (SetClickContext<BotCommandClickHandler>(handler, context)) {
			return false;
		} else if (SetClickContext<MentionClickHandler>(handler, context)) {
			return false;
		} else if (SetClickContext<HashtagClickHandler>(handler, context)) {
			return false;
		} else if (SetClickContext<CashtagClickHandler>(handler, context)) {
			return false;
		} else if (handler->url().startsWith(u"internal:~join_date~:"_q)) {
			const auto joinDate = handler->url().split(
				u"show:"_q,
				Qt::SkipEmptyParts).last();
			if (!joinDate.isEmpty()) {
				const auto weak = base::make_weak(window);
				window->session().api().resolveJumpToDate(
					Dialogs::Key(p->owner().history(p)),
					base::unixtime::parse(joinDate.toULongLong()).date(),
					[=](not_null<PeerData*> p, MsgId m) {
						const auto f = Window::SectionShow::Way::Forward;
						if (const auto strong = weak.get()) {
							strong->showPeerHistory(p, f, m);
						}
					});
				return false;
			}
		} else if (SetClickContext<UrlClickHandler>(handler, context)) {
			return false;
		}
		return true;
	};

	const auto addTranslateToMenu = [&,
			p = peer.get(),
			parentCtrl = controller->parentController()](
			not_null<FlatLabel*> label,
			rpl::producer<TextWithEntities> &&text) {
		struct State {
			rpl::variable<TextWithEntities> labelText;
		};
		const auto state = label->lifetime().make_state<State>();
		state->labelText = std::move(text);
		label->setContextMenuHook([=](
				FlatLabel::ContextMenuRequest request) {
			if (request.link) {
				const auto &url = request.link->url();
				if (url.startsWith(u"internal:~peer_id~:"_q)) {
					const auto weak = base::make_weak(parentCtrl);
					request.menu->addAction(u"Copy ID"_q, [=] {
						Core::App().openInternalUrl(
							url,
							QVariant::fromValue(ClickHandlerContext{
								.sessionWindow = weak,
							}));
					});
					return;
				}
			}
			label->fillContextMenu(request);
			if (::Ui::SkipTranslate(state->labelText.current())) {
				return;
			}
			auto item = (request.selection.empty()
				? tr::lng_context_translate
				: tr::lng_context_translate_selected)(tr::now);
			request.menu->addAction(std::move(item), [=] {
				parentCtrl->window().show(Box(
					::Ui::TranslateBox,
					p,
					MsgId(),
					request.selection.empty()
						? state->labelText.current()
						: ::Ui::Text::Mid(
							state->labelText.current(),
							request.selection.from,
							request.selection.to - request.selection.from),
					false));
			});
		});
	};

	const auto card = CreateCardContainer(raw->entity(), 6, 6);

	const auto addInfoLineGeneric = [&](
			v::text::data &&label,
			rpl::producer<TextWithEntities> &&text,
			const style::FlatLabel &textSt = st::infoLabeled,
			const style::margins &padding = st::infoProfileLabeledPadding,
			const style::PopupMenu &stMenu = st::defaultPopupMenu) {
		auto line = CreateTextWithLabel(
			card,
			v::text::take_marked(std::move(label)),
			std::move(text),
			st::infoLabel,
			textSt,
			padding,
			stMenu);
		tracker.track(card->add(std::move(line.wrap)));

		line.text->setClickHandlerFilter(infoClickFilter);
		return line;
	};

	const auto addInfoLine = [&](
			v::text::data &&label,
			rpl::producer<TextWithEntities> &&text,
			const style::FlatLabel &textSt = st::infoLabeled,
			const style::margins &padding = st::infoProfileLabeledPadding,
			const style::PopupMenu &stMenu = st::defaultPopupMenu) {
		return addInfoLineGeneric(
			std::move(label),
			std::move(text),
			textSt,
			padding,
			stMenu);
	};

	const auto addInfoOneLine = [&](
			v::text::data &&label,
			rpl::producer<TextWithEntities> &&text,
			const QString &contextCopyText,
			const style::margins &padding = st::infoProfileLabeledPadding,
			const style::PopupMenu &stMenu = st::defaultPopupMenu) {
		auto result = addInfoLine(
			std::move(label),
			std::move(text),
			st::infoLabeledOneLine,
			padding,
			stMenu);
		result.text->setDoubleClickSelectsParagraph(true);
		result.text->setContextCopyText(contextCopyText);
		return result;
	};

	const auto fitLabelToButton = [&](
			not_null<RpWidget*> button,
			not_null<FlatLabel*> label,
			int rightSkip) {
		const auto parentWidget = static_cast<RpWidget*>(label->parentWidget());
		const auto container = parentWidget;
		rpl::combine(
			container->widthValue(),
			label->geometryValue(),
			button->sizeValue(),
			button->shownValue()
		) | rpl::on_next([=](
				int width,
				QRect,
				QSize buttonSize,
				bool buttonShown) {
			button->moveToRight(
				rightSkip,
				(parentWidget->height() - buttonSize.height()) / 2);
			const auto x = MapFrom(container, label, QPoint(0, 0)).x();
			const auto s = buttonShown
				? MapFrom(container, button, QPoint(0, 0)).x()
				: width;
			label->resizeToWidth(s - x);
		}, button->lifetime());
	};

	const auto parentCtrl = controller->parentController();
	const auto weak = base::make_weak(parentCtrl);
	const auto peerIdRaw = QString::number(peer->id.value);
	const auto lnkHook = [=](FlatLabel::ContextMenuRequest request) {
		const auto strong = weak.get();
		if (!strong || !request.link) {
			return;
		}
		const auto url = request.link->url();
		if (url.startsWith(u"https://")) {
			request.menu->addAction(
				tr::lng_context_copy_link(tr::now),
				[=] {
					TextUtilities::SetClipboardText(TextForMimeData::Simple(url));
					if (const auto strong = weak.get()) {
						strong->showToast({
							.text = {
								tr::lng_channel_public_link_copied(tr::now),
							},
							.iconLottie = u"toast/voip_invite"_q,
							.iconLottieSize = st::toastLottieIconSize,
						});
					}
				});
			request.menu->addAction(
				tr::lng_group_invite_share(tr::now),
				[=] {
					if (const auto strong = weak.get()) {
						FastShareLink(strong, url);
					}
				});
			return;
		}
		static const auto kPrefix = QRegularExpression(u"^internal:"
			"(collectible_username|username_link|username_regular)/"
			"([a-zA-Z0-9\\-\\_\\.]+)@"_q);
		const auto match = kPrefix.match(url);
		if (!match.hasMatch()) {
			return;
		}
		const auto username = match.captured(2);
		const auto fullname = username + '@' + peerIdRaw;
		const auto mentionLink = "internal:username_regular/" + fullname;
		const auto linkLink = "internal:username_link/" + fullname;
		const auto context = QVariant::fromValue(ClickHandlerContext{
			.sessionWindow = weak,
		});
		const auto session = &strong->session();
		const auto link = session->createInternalLinkFull(username);
		request.menu->addAction(
			tr::lng_context_copy_mention(tr::now),
			[=] { Core::App().openInternalUrl(mentionLink, context); });
		request.menu->addAction(
			tr::lng_context_copy_link(tr::now),
			[=] { Core::App().openInternalUrl(linkLink, context); });
		request.menu->addAction(
			tr::lng_group_invite_share(tr::now),
			[=] {
				if (const auto strong = weak.get()) {
					FastShareLink(strong, link);
				}
			});
	};

	if (const auto user = peer->asUser()) {
		if (user->session().supportMode()) {
			addInfoLineGeneric(
				user->session().supportHelper().infoLabelValue(user),
				user->session().supportHelper().infoTextValue(user));
		}

		{
			const auto phoneLabel = addInfoOneLine(
				tr::lng_info_mobile_label(),
				PhoneWithSpoilerValue(user, PhoneOrHiddenValue(user)),
				tr::lng_profile_copy_phone(tr::now),
				st::infoProfileLabeledPadding,
				st::popupMenuWithIcons).text;
			const auto hook = [=](FlatLabel::ContextMenuRequest request) {
				if (request.selection.empty()) {
					const auto callback = [=] {
						CopyPhoneToClipboard(PhoneOrHiddenValue(user));
					};
					request.menu->addAction(
						tr::lng_profile_copy_phone(tr::now),
						callback,
						&st::menuIconCopy);
				} else {
					phoneLabel->fillContextMenu(request);
				}
				AddPhoneMenu(request.menu, user);
				AddPhoneSpoilerMenu(request.menu, user);
			};
			phoneLabel->setContextMenuHook(hook);
		}
		auto label = user->isBot()
			? tr::lng_info_about_label()
			: tr::lng_info_bio_label();
		const auto about = addInfoLine(
			std::move(label),
			AboutWithAdvancedValue(user));
		addTranslateToMenu(about.text, AboutWithAdvancedValue(user));
		SetupAboutPeerIdDrag(about.text, user);

		const auto usernameLine = addInfoOneLine(
			UsernamesSubtext(peer, tr::lng_info_username_label()),
			UsernameValue(user, true) | rpl::map([=](TextWithEntities u) {
				return u.text.isEmpty()
					? TextWithEntities()
					: tr::link(u, UsernameUrl(user, u.text.mid(1)));
			}),
			QString(),
			st::infoProfileLabeledUsernamePadding);
		const auto callback = UsernamesLinkCallback(
			peer,
			parentCtrl,
			QString());
		usernameLine.text->overrideLinkClickHandler(callback);
		usernameLine.subtext->overrideLinkClickHandler(callback);
		usernameLine.text->setContextMenuHook(lnkHook);
		usernameLine.subtext->setContextMenuHook(lnkHook);
		UsernameValue(
			user,
			true
		) | rpl::on_next([=, label = usernameLine.text](
				const TextWithEntities &u) {
			if (u.text.isEmpty()) {
				return;
			}
			const auto username = u.text.mid(1);
			label->setLink(1, std::make_shared<DraggableUrlClickHandler>(
				UsernameUrl(user, username),
				user->session().createInternalLinkFull(username)));
		}, usernameLine.text->lifetime());

		const auto qrButton = CreateChild<IconButton>(
			usernameLine.text->parentWidget(),
			st::infoProfileLabeledButtonQr);
		qrButton->setAccessibleName(tr::lng_group_invite_context_qr(tr::now));
		UsernamesValue(peer) | rpl::on_next([=](const auto &u) {
			qrButton->setVisible(!u.empty());
		}, qrButton->lifetime());
		const auto rightSkip = st::infoProfileLabeledButtonQrRightSkip;
		fitLabelToButton(qrButton, usernameLine.text, rightSkip);
		fitLabelToButton(qrButton, usernameLine.subtext, rightSkip);
		qrButton->setClickedCallback([=, show = controller->uiShow()] {
			DefaultShowFillPeerQrBoxCallback(show, user);
			return false;
		});

		if (!user->isBot()) {
			tracker.track(card->add(
				CreateBirthday(card, parentCtrl, user),
				{},
				style::al_justify));

			tracker.track(card->add(
				CreateWorkingHours(card, user), {}, style::al_justify));

			tracker.track(card->add(
				CreateNotes(card, parentCtrl, user), {}, style::al_justify));

			auto locationText = user->session().changes().peerFlagsValue(
				user,
				Data::PeerUpdate::Flag::BusinessDetails
			) | rpl::map([=] {
				const auto &details = user->businessDetails();
				if (!details.location) {
					return TextWithEntities();
				} else if (!details.location.point) {
					return TextWithEntities{ details.location.address };
				}
				return tr::link(
					TextUtilities::SingleLine(details.location.address),
					LocationClickHandler::Url(*details.location.point));
			});
			addInfoOneLine(
				tr::lng_info_location_label(),
				std::move(locationText),
				QString()
			).text->setLinksTrusted();

		}

		bool show_peer_id = FASettings::FASettings::getInstance().showPeerId();
		bool show_dc_id = FASettings::FASettings::getInstance().showDcId();
		if (show_peer_id) {
			const auto dataCenter = getPeerDC(peer);
			const auto idLabel = !show_dc_id ? QString("ID") : dataCenter;

			auto idDrawableText = IDValue(
					user
			) | rpl::map([](TextWithEntities &&text) {
				return tr::bold(text.text);
			});
			auto idInfo = addInfoOneLine(
					rpl::single(idLabel),
					std::move(idDrawableText),
					fatr::fa_copy_id(fatr::now)
			);
			idInfo.text->setClickHandlerFilter([=](auto &&...) {
				const auto idText = IDString(user);
				if (!idText.isEmpty()) {
					QGuiApplication::clipboard()->setText(idText);
					parentCtrl->showToast(fatr::fa_id_copied(fatr::now));
				}
				return false;
			});
		}

		bool show_registration_date = FASettings::FASettings::getInstance().showRegistrationDate();
		if (show_registration_date) {
			addInfoOneLine(
				fatr::fa_registration_date(),
				std::move(RegistrationValue(user)),
				fatr::fa_copy_registration_date(fatr::now)
			);
		}

		if (!user->isSelf() && !user->isBot()) {
			const auto contactCard = CreateCardContainer(raw->entity(), 6, 6);
			AddMainButton(
				contactCard,
				tr::lng_info_add_as_contact(),
				CanAddContactValue(user),
				[=] {
					controller->uiShow()->show(
						Box(EditContactBox, parentCtrl, user));
				},
				&tracker,
				nullptr);
		}
	} else {
		const auto topicRootId = topic ? topic->rootId() : 0;
		if (topicRootId || !peer->username().isEmpty()) {
			const auto addToLink = topicRootId
				? ('/' + QString::number(topicRootId.bare))
				: QString();
			auto linkText = LinkValue(
				peer,
				true,
				topicRootId
			) | rpl::map([=](const LinkWithUrl &link) {
				const auto text = link.text;
				return text.isEmpty()
					? TextWithEntities()
					: tr::link(
						(text.startsWith(u"https://"_q)
							? text.mid(u"https://"_q.size())
							: text) + addToLink,
						(addToLink.isEmpty() ? link.url : (text + addToLink)));
			});
			const auto linkLine = addInfoOneLine(
				(topicRootId
					? TopicSubtext(peer)
					: UsernamesSubtext(peer, tr::lng_info_link_label())),
				std::move(linkText),
				QString());
			const auto linkCallback = UsernamesLinkCallback(
				peer,
				parentCtrl,
				addToLink);
			linkLine.text->overrideLinkClickHandler(linkCallback);
			linkLine.subtext->overrideLinkClickHandler(linkCallback);
			linkLine.text->setContextMenuHook(lnkHook);
			linkLine.subtext->setContextMenuHook(lnkHook);
			LinkValue(
				peer,
				true,
				topicRootId
			) | rpl::on_next([=, label = linkLine.text](const LinkWithUrl &link) {
				if (link.text.isEmpty()) {
					return;
				}
				label->setLink(1, std::make_shared<DraggableUrlClickHandler>(
					addToLink.isEmpty() ? link.url : (link.text + addToLink),
					link.text + addToLink));
			}, linkLine.text->lifetime());
			const auto qr = CreateChild<IconButton>(
				linkLine.text->parentWidget(),
				st::infoProfileLabeledButtonQr);
			qr->setAccessibleName(tr::lng_group_invite_context_qr(tr::now));
			UsernamesValue(peer) | rpl::on_next([=](const auto &u) {
				qr->setVisible(!u.empty());
			}, qr->lifetime());
			const auto rightSkip = st::infoProfileLabeledButtonQrRightSkip;
			fitLabelToButton(qr, linkLine.text, rightSkip);
			fitLabelToButton(qr, linkLine.subtext, rightSkip);
			qr->setClickedCallback([=, show = controller->uiShow()] {
				DefaultShowFillPeerQrBoxCallback(show, peer);
				return false;
			});
		}

		if (const auto channel = topic ? nullptr : peer->asChannel()) {
			if (channel->hasLocation()) {
				auto locationText = LocationValue(
					channel
				) | rpl::map([](const ChannelLocation *location) {
					return location
						? tr::link(
							TextUtilities::SingleLine(location->address),
							LocationClickHandler::Url(location->point))
						: TextWithEntities();
				});
				addInfoOneLine(
					tr::lng_info_location_label(),
					std::move(locationText),
					QString()
				).text->setLinksTrusted();
			}
		}

		const auto about = addInfoLine(tr::lng_info_about_label(), topic
			? rpl::single(TextWithEntities())
			: AboutWithAdvancedValue(peer));
		if (!topic) {
			addTranslateToMenu(about.text, AboutWithAdvancedValue(peer));
			SetupAboutPeerIdDrag(about.text, peer);
		}

		bool show_peer_id = FASettings::FASettings::getInstance().showPeerId();
		bool show_dc_id = FASettings::FASettings::getInstance().showDcId();
		if (show_peer_id) {
			const auto dataCenter = getPeerDC(peer);
			const auto idLabel = !show_dc_id ? QString("ID") : dataCenter;
			auto idDrawableText = IDValue(
					peer
			) | rpl::map([](TextWithEntities &&text) {
				return tr::bold(text.text);
			});
			auto idInfo = addInfoOneLine(
					idLabel,
					std::move(idDrawableText),
					fatr::fa_copy_id(fatr::now)
			);
			idInfo.text->setClickHandlerFilter([=](auto &&...) {
				const auto idText = IDString(peer);
				if (!idText.isEmpty()) {
					QGuiApplication::clipboard()->setText(idText);
					parentCtrl->showToast(fatr::fa_id_copied(fatr::now));
				}
				return false;
			});
		}

		if (const auto channel = peer->asChannel()) {
			if (!channel->isMegagroup()) {
				using namespace rpl::mappers;
				auto activePeerValue = parentCtrl->activeChatValue(
				) | rpl::map([](Dialogs::Key key) {
					return key.peer();
				});
				auto viewChannelVisible = rpl::combine(
					controller->wrapValue(),
					std::move(activePeerValue),
					(_1 != Wrap::Side) || (_2 != channel));
				const auto openInWindow = [=] {
					parentCtrl->showInNewWindow(Window::SeparateId(channel));
				};
				const auto openInCurrent = [=] {
					parentCtrl->showPeerHistory(
						channel,
						Window::SectionShow::Way::Forward);
				};
				struct State {
					base::unique_qptr<PopupMenu> menu;
				};
				const auto state = raw->lifetime().make_state<State>();
				auto viewChannel = [=](Qt::MouseButton mouse) {
					if (mouse == Qt::RightButton) {
						return;
					}
					if (base::IsCtrlPressed() || mouse == Qt::MiddleButton) {
						openInWindow();
					} else {
						openInCurrent();
					}
				};
				const auto viewCard = CreateCardContainer(raw->entity(), 6, 6);
				const auto button = AddMainButton(
					viewCard,
					tr::lng_profile_view_channel(),
					std::move(viewChannelVisible),
					std::move(viewChannel),
					&tracker,
					nullptr);
				button->setAcceptBoth();
				button->addClickHandler([=](Qt::MouseButton mouse) {
					if (mouse != Qt::RightButton) {
						return;
					}
					state->menu = base::make_unique_q<PopupMenu>(
						button,
						st::popupMenuWithIcons);
					state->menu->addAction(
						tr::lng_context_new_window(tr::now),
						[=] {
							base::call_delayed(
								st::popupMenuWithIcons.showDuration,
								crl::guard(button, openInWindow));
						},
						&st::menuIconNewWindow);
					state->menu->popup(QCursor::pos());
				});
			}
		}
	}
	raw->toggleOn(tracker.atLeastOneShownValue());
	raw->finishAnimating();

	return ::Info::Profile::Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

} // namespace FA::Ui
