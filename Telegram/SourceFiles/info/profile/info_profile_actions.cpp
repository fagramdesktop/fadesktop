/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "info/profile/info_profile_actions.h"

#include "fa/settings/fa_settings.h"
#include "fa/ui/md3/fa_cards.h"
#include "fa/ui/md3/fa_profile_cards.h"
#include "fa/utils/fa_profile_values.h"
#include "fa/utils/telegram_helpers.h"
#include "fa_lang_auto.h"

#include "api/api_blocked_peers.h"
#include "api/api_chat_participants.h"
#include "api/api_credits.h"
#include "api/api_report.h"
#include "api/api_statistics.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/event_filter.h"
#include "base/options.h"
#include "base/qt/qt_key_modifiers.h"
#include "base/timer_rpl.h"
#include "base/unixtime.h"
#include "boxes/choose_filter_box.h"
#include "boxes/peer_list_box.h"
#include "boxes/peers/add_bot_to_chat_box.h"
#include "boxes/peers/edit_contact_box.h"
#include "boxes/peers/edit_participants_box.h"
#include "boxes/peers/edit_peer_info_box.h"
#include "boxes/peers/verify_peers_box.h"
#include "boxes/report_messages_box.h"
#include "boxes/share_box.h"
#include "boxes/star_gift_box.h"
#include "boxes/translate_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/ui_integration.h"
#include "data/business/data_business_common.h"
#include "data/business/data_business_info.h"
#include "data/components/credits.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_forum.h"
#include "data/data_forum_topic.h"
#include "data/data_peer_values.h"
#include "data/data_saved_sublist.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/notify/data_notify_settings.h"
#include "ui/text/text_entity.h"
#include "data/stickers/data_custom_emoji.h"
#include "dialogs/ui/dialogs_layout.h"
#include "dialogs/ui/dialogs_message_view.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "history/history_item_helpers.h"
#include "history/view/history_view_item_preview.h"
#include "history/view/reactions/history_view_reactions_list.h"
#include "info/bot/earn/info_bot_earn_widget.h"
#include "info/bot/starref/info_bot_starref_common.h"
#include "info/channel_statistics/earn/earn_format.h"
#include "info/channel_statistics/earn/earn_icons.h"
#include "info/channel_statistics/earn/info_channel_earn_list.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_phone_menu.h"
#include "info/profile/info_profile_text.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_widget.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "inline_bots/bot_attach_web_view.h"
#include "iv/iv_instance.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "menu/menu_mute.h"
#include "settings/settings_common.h"
#include "support/support_helper.h"
#include "ui/boxes/peer_qr_box.h"
#include "ui/boxes/report_box_graphics.h"
#include "ui/controls/userpic_button.h"
#include "ui/effects/credits_graphics.h"
#include "ui/effects/toggle_arrow.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/ui_utility.h"
#include "ui/userpic_view.h"
#include "ui/text/format_values.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/text/text_utilities.h"
#include "ui/text/text_variant.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h" // Window::Controller::show.
#include "window/window_peer_menu.h"
#include "window/window_separate_id.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_channel_earn.h" // st::channelEarnCurrencyCommonMargins
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "styles/style_info_profile_actions.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h" // settingsButtonRightSkip.
#include "styles/style_window.h" // mainMenuToggleFourStrokes.

#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>

namespace Info {
namespace Profile {
namespace FAUi = ::FA::Ui;
namespace {

[[nodiscard]] object_ptr<Ui::RpWidget> CreateSkipWidget(
		not_null<Ui::RpWidget*> parent) {
	return Ui::CreateSkipWidget(parent, st::infoProfileSkip);
}

template <typename Text, typename ToggleOn, typename Callback>
auto AddActionButton(
		not_null<Ui::VerticalLayout*> parent,
		Text &&text,
		ToggleOn &&toggleOn,
		Callback &&callback,
		const style::icon *icon,
		const style::SettingsButton &st = st::infoSharedMediaButton,
		Ui::MultiSlideTracker *tracker = nullptr) {
	auto result = parent->add(object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
		parent,
		object_ptr<Ui::SettingsButton>(
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
		object_ptr<Profile::FloatingIcon>(
			result,
			*icon,
			st::infoSharedMediaButtonIconPosition);
	}
	if (tracker) {
		tracker->track(result);
	}
	return result;
};

template <typename Text, typename ToggleOn, typename Callback>
auto AddMainButton(
		not_null<Ui::VerticalLayout*> parent,
		Text &&text,
		ToggleOn &&toggleOn,
		Callback &&callback,
		Ui::MultiSlideTracker *tracker = nullptr,
		Ui::MultiSlideTracker *buttonTracker = nullptr,
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

rpl::producer<CreditsAmount> AddCurrencyAction(
		not_null<UserData*> user,
		not_null<Ui::VerticalLayout*> wrap,
		not_null<Controller*> controller) {
	struct State final {
		rpl::variable<CreditsAmount> balance;
		Ui::Text::CustomEmojiHelper helper;
	};
	const auto state = wrap->lifetime().make_state<State>();
	const auto parentController = controller->parentController();
	const auto wrapButton = AddActionButton(
		wrap,
		tr::lng_manage_peer_bot_balance_currency(),
		state->balance.value(
		) | rpl::map(rpl::mappers::_1 > CreditsAmount(0)),
		[=] { parentController->showSection(Info::ChannelEarn::Make(user)); },
		nullptr);
	{
		const auto button = wrapButton->entity();
		const auto icon = Ui::CreateChild<Ui::RpWidget>(button);
		icon->resize(st::infoIconReport.size());
		const auto image = Ui::Earn::MenuIconCurrency(icon->size());
		icon->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(icon);
			p.drawImage(0, 0, image);
		}, icon->lifetime());

		button->sizeValue(
		) | rpl::on_next([=](const QSize &size) {
			icon->move(st::infoEarnCurrencyIconPosition);
		}, icon->lifetime());
	}
	const auto balance = user->session().credits().balanceCurrency(user->id);
	if (balance) {
		state->balance = balance;
	}
	{
		const auto weak = base::make_weak(wrap);
		const auto currencyLoadLifetime
			= std::make_shared<rpl::lifetime>();
		const auto currencyLoad
			= currencyLoadLifetime->make_state<Api::EarnStatistics>(user);
		const auto done = [=](CreditsAmount balance) {
			if ([[maybe_unused]] const auto strong = weak.get()) {
				state->balance = balance;
				currencyLoadLifetime->destroy();
			}
		};
		currencyLoad->request() | rpl::on_error_done(
			[=](const QString &error) {
				done(CreditsAmount(0, CreditsType::Ton));
			},
			[=] { done(currencyLoad->data().currentBalance); },
			*currencyLoadLifetime);
	}
	const auto &st = st::infoSharedMediaButton;
	const auto button = wrapButton->entity();
	const auto name = Ui::CreateChild<Ui::FlatLabel>(button, st.rightLabel);
	const auto icon = state->helper.paletteDependent({ .factory = [=] {
		return Ui::Earn::IconCurrencyColored(
			st.rightLabel.style.font,
			st.rightLabel.textFg->c);
	}, .margin = st::channelEarnCurrencyCommonMargins });
	name->show();
	rpl::combine(
		button->widthValue(),
		tr::lng_manage_peer_bot_balance_currency(),
		state->balance.value()
	) | rpl::on_next([=, &st](
			int width,
			const QString &button,
			CreditsAmount balance) {
		const auto available = width
			- rect::m::sum::h(st.padding)
			- st.style.font->width(button)
			- st::settingsButtonRightSkip;
		name->setMarkedText(
			base::duplicate(icon)
				.append(QChar(' '))
				.append(Info::ChannelEarn::MajorPart(balance))
				.append(Info::ChannelEarn::MinorPart(balance)),
			state->helper.context());
		name->resizeToNaturalWidth(available);
		name->moveToRight(st::settingsButtonRightSkip, st.padding.top());
	}, name->lifetime());
	name->setAttribute(Qt::WA_TransparentForMouseEvents);
	wrapButton->finishAnimating();
	return state->balance.value();
}

rpl::producer<CreditsAmount> AddCreditsAction(
		not_null<UserData*> user,
		not_null<Ui::VerticalLayout*> wrap,
		not_null<Controller*> controller) {
	struct State final {
		rpl::variable<CreditsAmount> balance;
	};
	const auto state = wrap->lifetime().make_state<State>();
	const auto parentController = controller->parentController();
	const auto wrapButton = AddActionButton(
		wrap,
		tr::lng_manage_peer_bot_balance_credits(),
		state->balance.value(
		) | rpl::map(rpl::mappers::_1 > CreditsAmount(0)),
		[=] { parentController->showSection(Info::BotEarn::Make(user)); },
		nullptr);
	{
		const auto button = wrapButton->entity();
		const auto icon = Ui::CreateChild<Ui::RpWidget>(button);
		const auto image = Ui::Earn::MenuIconCredits();
		icon->resize(image.size() / style::DevicePixelRatio());
		icon->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(icon);
			p.drawImage(0, 0, image);
		}, icon->lifetime());

		button->sizeValue(
		) | rpl::on_next([=](const QSize &size) {
			icon->move(st::infoEarnCreditsIconPosition);
		}, icon->lifetime());
	}
	if (const auto balance = user->session().credits().balance(user->id)) {
		state->balance = balance;
	}
	{
		const auto api = wrap->lifetime().make_state<Api::CreditsStatus>(
			user);
		api->request({}, [=](Data::CreditsStatusSlice data) {
			state->balance = data.balance;
		});
	}
	const auto &st = st::infoSharedMediaButton;
	const auto button = wrapButton->entity();
	const auto name = Ui::CreateChild<Ui::FlatLabel>(button, st.rightLabel);

	auto helper = Ui::Text::CustomEmojiHelper();
	const auto icon = helper.paletteDependent(Ui::Earn::IconCreditsEmoji());
	const auto context = helper.context([=] { name->update(); });
	name->show();
	rpl::combine(
		button->widthValue(),
		tr::lng_manage_peer_bot_balance_credits(),
		state->balance.value()
	) | rpl::on_next([=, &st](
			int width,
			const QString &button,
			CreditsAmount balance) {
		const auto available = width
			- rect::m::sum::h(st.padding)
			- st.style.font->width(button)
			- st::settingsButtonRightSkip;
		name->setMarkedText(
			base::duplicate(icon)
				.append(QChar(' '))
				.append(Lang::FormatCreditsAmountDecimal(balance)),
			context);
		name->resizeToNaturalWidth(available);
		name->moveToRight(st::settingsButtonRightSkip, st.padding.top());
	}, name->lifetime());
	name->setAttribute(Qt::WA_TransparentForMouseEvents);
	wrapButton->finishAnimating();
	return state->balance.value();
}

class DetailsFiller {
public:
	DetailsFiller(
		not_null<Controller*> controller,
		not_null<SectionStack*> stack,
		not_null<PeerData*> peer,
		Origin origin);
	DetailsFiller(
		not_null<Controller*> controller,
		not_null<SectionStack*> stack,
		not_null<Data::SavedSublist*> sublist);
	DetailsFiller(
		not_null<Controller*> controller,
		not_null<SectionStack*> stack,
		not_null<Data::ForumTopic*> topic);

	void buildSections();

private:
	[[nodiscard]] Section makePersonalChannel(not_null<UserData*> user);
	void addBotVerify();
	void addMainApp(not_null<UserData*> user);
	[[nodiscard]] Section makeBotPermissions(not_null<UserData*> user);
	void addManagedBotFooter(not_null<UserData*> managerUser);
	[[nodiscard]] Section makeReportOrDeleteReaction();
	[[nodiscard]] Section makeCommunityLink(not_null<PeerData*> peer);
	void addCommunityHiddenNote();
	[[nodiscard]] Section makeTopicsList(not_null<Data::Forum*> forum);

	[[nodiscard]] Section makeDeleteReactionSection(GroupReactionOrigin data);
	[[nodiscard]] Section makeReportReactionSection(
		GroupReactionOrigin data,
		bool ban);

	not_null<Controller*> _controller;
	not_null<SectionStack*> _stack;
	not_null<PeerData*> _peer;
	Data::ForumTopic *_topic = nullptr;
	Data::SavedSublist *_sublist = nullptr;
	Origin _origin;

};

class ActionsFiller {
public:
	ActionsFiller(
		not_null<Controller*> controller,
		not_null<Ui::RpWidget*> parent,
		not_null<PeerData*> peer,
		Ui::MultiSlideTracker *tracker = nullptr);

	object_ptr<Ui::RpWidget> fill();
	void fillInto(not_null<Ui::VerticalLayout*> card);

private:
	void addAffiliateProgram(not_null<UserData*> user);
	void addBalanceActions(not_null<UserData*> user);
	void addInviteToGroupAction(not_null<UserData*> user);
	void addShareContactAction(not_null<UserData*> user);
	void addEditContactAction(not_null<UserData*> user);
	void addDeleteContactAction(not_null<UserData*> user);
	void addBotCommandActions(not_null<UserData*> user);
	void addFastButtonsMode(not_null<UserData*> user);
	void addReportAction();
	void addBlockAction(not_null<UserData*> user);
	void fillUserActions(not_null<UserData*> user);

	not_null<Controller*> _controller;
	not_null<Ui::RpWidget*> _parent;
	not_null<PeerData*> _peer;
	Ui::MultiSlideTracker *_tracker = nullptr;
	object_ptr<Ui::VerticalLayout> _wrap = { nullptr };
	Ui::VerticalLayout *_card = nullptr;

};

void ReportReactionBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> participant,
		GroupReactionOrigin data,
		bool ban,
		Fn<void()> sent) {
	box->setTitle(tr::lng_report_reaction_title());
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_report_reaction_about(),
		st::boxLabel));
	const auto check = ban
		? box->addRow(
			object_ptr<Ui::Checkbox>(
				box,
				tr::lng_report_and_ban_button(tr::now),
				true),
			st::boxRowPadding + QMargins{ 0, st::boxLittleSkip, 0, 0 })
		: nullptr;
	box->addButton(tr::lng_report_button(), [=] {
		const auto chat = data.group->asChat();
		const auto channel = data.group->asMegagroup();
		if (check && check->checked()) {
			if (chat) {
				chat->session().api().chatParticipants().kick(
					chat,
					participant);
			} else if (channel) {
				channel->session().api().chatParticipants().kick(
					channel,
					participant,
					ChatRestrictionsInfo());
			}
		}
		Api::ReportReaction(
			controller->uiShow(),
			data.group,
			data.messageId,
			participant);
		sent();
		box->closeBox();
	}, st::attentionBoxButton);
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

DetailsFiller::DetailsFiller(
	not_null<Controller*> controller,
	not_null<SectionStack*> stack,
	not_null<PeerData*> peer,
	Origin origin)
: _controller(controller)
, _stack(stack)
, _peer(peer)
, _origin(origin) {
}

DetailsFiller::DetailsFiller(
	not_null<Controller*> controller,
	not_null<SectionStack*> stack,
	not_null<Data::SavedSublist*> sublist)
: _controller(controller)
, _stack(stack)
, _peer(sublist->sublistPeer())
, _sublist(sublist) {
}

DetailsFiller::DetailsFiller(
	not_null<Controller*> controller,
	not_null<SectionStack*> stack,
	not_null<Data::ForumTopic*> topic)
: _controller(controller)
, _stack(stack)
, _peer(topic->peer())
, _topic(topic) {
}

Section DetailsFiller::makePersonalChannel(not_null<UserData*> user) {
	const auto parent = _stack->layout();
	auto result = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto card = FAUi::CreateCardContainer(result->entity(), 6, 6);
	const auto container = card;
	const auto window = _controller->parentController();
	const auto duration = st::slideWrapDuration;

	result->toggleOn(PersonalChannelValue(
		user
	) | rpl::map(rpl::mappers::_1 != nullptr));
	result->finishAnimating();

	auto channel = PersonalChannelValue(
		user
	) | rpl::start_spawning(result->lifetime());

	const auto channelLabelFactory = [=](rpl::producer<ChannelData*> c) {
		return rpl::combine(
			tr::lng_info_personal_channel_label(tr::marked),
			std::move(c)
		) | rpl::map([](TextWithEntities &&text, ChannelData *channel) {
			const auto count = channel ? channel->membersCount() : 0;
			if (count > 1) {
				text.append(' ')
				.append(Ui::kQBullet)
				.append(' ')
				.append(
					tr::lng_chat_status_subscribers(
						tr::now,
						lt_count_decimal,
						count));
			}
			return text;
		});
	};
	if (user->isSelf()) {
		struct State {
			base::unique_qptr<Ui::PopupMenu> menu;
		};
		const auto state = container->lifetime().make_state<State>();
		base::install_event_filter(container, [=](
				not_null<QEvent*> e) {
			if (e->type() == QEvent::ContextMenu) {
				const auto ce = static_cast<QContextMenuEvent*>(e.get());
				state->menu = base::make_unique_q<Ui::PopupMenu>(
					container,
					st::defaultPopupMenu);
				state->menu->addAction(
					tr::lng_settings_channel_menu_remove(tr::now),
					[] {
						UrlClickHandler::Open(
							u"internal:edit_personal_channel:remove"_q);
					});
				state->menu->popup(ce->globalPos());
				return base::EventFilterResult::Cancel;
			}
			return base::EventFilterResult::Continue;
		}, container->lifetime());
	}

	{
		const auto onlyChannelWrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		onlyChannelWrap->toggleOn(rpl::duplicate(channel) | rpl::map([=] {
			return user->personalChannelId()
				&& !user->personalChannelMessageId();
		}));
		onlyChannelWrap->finishAnimating();

		auto text = rpl::duplicate(
			channel
		) | rpl::map([=](ChannelData *channel) {
			return channel ? NameValue(channel) : rpl::single(QString());
		}) | rpl::flatten_latest() | rpl::map([](const QString &name) {
			return name.isEmpty() ? TextWithEntities() : tr::link(name);
		});
		auto line = CreateTextWithLabel(
			container,
			channelLabelFactory(rpl::duplicate(channel)),
			std::move(text),
			st::infoLabel,
			st::infoLabeled,
			st::infoProfilePersonalChannelPadding);
		onlyChannelWrap->entity()->add(std::move(line.wrap));

		line.text->setClickHandlerFilter([=](
				const ClickHandlerPtr &handler,
				Qt::MouseButton button) {
			if (const auto channelId = user->personalChannelId()) {
				window->showPeerInfo(peerFromChannel(channelId));
			}
			return false;
		});

		object_ptr<FloatingIcon>(
			onlyChannelWrap,
			st::infoIconMediaChannel,
			st::infoPersonalChannelIconPosition);
	}

	{
		const auto messageChannelWrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		messageChannelWrap->setDuration(duration);
		messageChannelWrap->toggleOn(rpl::duplicate(channel) | rpl::map([=] {
			return user->personalChannelId()
				&& user->personalChannelMessageId();
		}));
		messageChannelWrap->finishAnimating();
		messageChannelWrap->toggledValue(
		) | rpl::filter(rpl::mappers::_1) | rpl::on_next([=] {
			messageChannelWrap->resizeToWidth(messageChannelWrap->width());
		}, messageChannelWrap->lifetime());

		const auto clear = [=] {
			while (messageChannelWrap->entity()->count()) {
				delete messageChannelWrap->entity()->widgetAt(0);
			}
		};

		const auto rebuild = [=](
				not_null<HistoryItem*> item,
				anim::type animated) {
			const auto &stUserpic = st::infoPersonalChannelUserpic;
			const auto &stLabeled = st::infoProfilePersonalChannelPadding;

			messageChannelWrap->toggle(false, anim::type::instant);
			clear();

			const auto inner = messageChannelWrap->entity()->add(
				object_ptr<Ui::VerticalLayout>(messageChannelWrap->entity()));

			const auto line = inner->add(
				object_ptr<Ui::FixedHeightWidget>(
					inner,
					stUserpic.photoSize + rect::m::sum::v(stLabeled)));
			const auto userpic = Ui::CreateChild<Ui::UserpicButton>(
				line,
				item->history()->peer,
				st::infoPersonalChannelUserpic);

			userpic->moveToLeft(
				16,
				stLabeled.top());
			userpic->setAttribute(Qt::WA_TransparentForMouseEvents);

			const auto date = Ui::CreateChild<Ui::FlatLabel>(
				line,
				Ui::FormatDialogsDate(ItemDateTime(item)),
				st::infoPersonalChannelDateLabel);

			const auto name = Ui::CreateChild<Ui::FlatLabel>(
				line,
				NameValue(item->history()->peer),
				st::infoPersonalChannelNameLabel);

			const auto preview = Ui::CreateChild<Ui::RpWidget>(line);
			auto &lifetime = preview->lifetime();
			using namespace Dialogs::Ui;
			struct State {
				MessageView view;
				HistoryItem *item = nullptr;
				rpl::lifetime lifetime;
			};
			const auto state = lifetime.make_state<State>();
			state->item = item;
			item->history()->session().changes().realtimeMessageUpdates(
				Data::MessageUpdate::Flag::Destroyed
			) | rpl::on_next([=](const Data::MessageUpdate &update) {
				if (update.item == state->item) {
					state->lifetime.destroy();
					state->item = nullptr;
					preview->update();
				}
			}, state->lifetime);

			preview->resize(0, st::infoLabeled.style.font->height);
			preview->paintRequest(
			) | rpl::on_next([=] {
				auto p = Painter(preview);
				const auto item = state->item;
				if (!item) {
					p.setPen(st::infoPersonalChannelDateLabel.textFg);
					p.setBrush(Qt::NoBrush);
					p.setFont(st::infoPersonalChannelDateLabel.style.font);
					p.drawText(
						preview->rect(),
						tr::lng_deleted_message(tr::now),
						style::al_left);
					return;
				}
				if (!state->view.prepared(item, nullptr, nullptr)) {
					const auto repaint = [=] { preview->update(); };
					state->view.prepare(
						item,
						nullptr,
						nullptr,
						repaint,
						{});
				}
				state->view.paint(p, preview->rect(), {
					.st = &st::defaultDialogRow,
					.currentBg = st::settingsThemeNotSupportedBg->c,
				});
			}, preview->lifetime());

			line->sizeValue() | rpl::filter_size(
			) | rpl::on_next([=](const QSize &size) {
				const auto left = 16 + stUserpic.photoSize + 12;
				const auto right = st::infoPersonalChannelDateSkip;
				const auto top = stLabeled.top();
				date->moveToRight(right, top, size.width());

				name->resizeToWidth(size.width()
					- left
					- date->width()
					- st::defaultVerticalListSkip
					- right);
				name->moveToLeft(left, top);

				preview->resize(
					size.width() - left - right,
					st::infoLabeled.style.font->height);
				preview->moveToLeft(
					left,
					size.height() - stLabeled.bottom() - preview->height());
			}, preview->lifetime());

			{
				inner->add(
					object_ptr<Ui::FlatLabel>(
						inner,
						channelLabelFactory(
							rpl::single(item->history()->peer->asChannel())),
						st::infoLabel),
					QMargins(
						st::infoProfilePersonalChannelPadding.left(),
						0,
						st::infoProfilePersonalChannelPadding.right(),
						st::infoProfilePersonalChannelPadding.bottom()));
			}
			{
				inner->setAttribute(Qt::WA_TransparentForMouseEvents);
				const auto button = FA::Ui::CreateCardRippleButton(
					messageChannelWrap->entity(),
					14);
				button->lower();
				messageChannelWrap->entity()->sizeValue(
				) | rpl::on_next([=](const QSize &size) {
					button->setGeometry(QRect(0, 0, size.width(), size.height()));
				}, button->lifetime());
				const auto channelPeer = item->history()->peer;
				const auto msg = item->fullId().msg;
				const auto openInWindow = [=] {
					window->showInNewWindow(
						Window::SeparateId(channelPeer),
						msg);
				};
				const auto openInCurrent = [=] {
					window->showPeerHistory(
						channelPeer,
						Window::SectionShow::Way::Forward,
						msg);
				};
				button->setAcceptBoth();
				struct State {
					base::unique_qptr<Ui::PopupMenu> menu;
				};
				const auto state
					= button->lifetime().make_state<State>();
				button->addClickHandler([=](Qt::MouseButton mouse) {
					if (mouse == Qt::RightButton) {
						state->menu = base::make_unique_q<Ui::PopupMenu>(
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
						return;
					}
					if (base::IsCtrlPressed()
						|| mouse == Qt::MiddleButton) {
						openInWindow();
					} else {
						openInCurrent();
					}
				});
				button->lower();
				inner->lifetime().make_state<base::unique_qptr<Ui::RpWidget>>(
					button);
				button->setAccessibleName(tr::lng_profile_view_channel(tr::now));
			}
			inner->setAttribute(Qt::WA_TransparentForMouseEvents);

			Ui::ToggleChildrenVisibility(messageChannelWrap->entity(), true);
			Ui::ToggleChildrenVisibility(line, true);
			messageChannelWrap->toggle(true, animated);
		};

		rpl::duplicate(
			channel
		) | rpl::on_next([=](ChannelData *channel) {
			if (!channel && messageChannelWrap->animating()) {
				base::call_delayed(duration, messageChannelWrap, clear);
			} else {
				clear();
			}
			if (!channel) {
				return;
			}
			const auto id = FullMsgId(
				channel->id,
				user->personalChannelMessageId());
			if (const auto item = user->session().data().message(id)) {
				return rebuild(item, anim::type::instant);
			}
			user->session().api().requestMessageData(
				channel,
				user->personalChannelMessageId(),
				crl::guard(container, [=] {
					if (const auto i = user->session().data().message(id)) {
						rebuild(i, anim::type::normal);
					}
				}));
		}, messageChannelWrap->lifetime());
	}

	const auto raw = result.data();
	return Section{
		.widget = std::move(result),
		.shown = raw->toggledValue(),
	};
}

void DetailsFiller::addMainApp(not_null<UserData*> user) {
	const auto parent = _stack->layout();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	const auto inner = raw->entity();
	const auto button = inner->add(
		object_ptr<Ui::RoundButton>(
			inner,
			tr::lng_profile_open_app(),
			st::infoOpenApp),
		st::infoOpenAppMargin,
		style::al_justify);
	button->setFullRadius(true);

	const auto controller = _controller->parentController();
	button->setClickedCallback([=] {
		user->session().attachWebView().open({
			.bot = user,
			.context = {
				.controller = controller,
				.maySkipConfirmation = true,
			},
			.source = InlineBots::WebViewSourceBotProfile(),
		});
	});

	const auto url = tr::lng_mini_apps_tos_url(tr::now);
	auto textProducer = rpl::combine(
		tr::lng_profile_open_app_about(
			lt_terms,
			tr::lng_profile_open_app_terms(tr::url(url)),
			tr::marked),
		user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::VerifyInfo)
	) | rpl::map([=](TextWithEntities text, auto) {
		if (const auto verify = user->botVerifyDetails()) {
			text = text.append(u"\n\n"_q).append(verify->description);
		}
		return text;
	});
	auto setup = [url](not_null<Ui::FlatLabel*> label) {
		label->setClickHandlerFilter([=](const auto &...) {
			UrlClickHandler::Open(url);
			return false;
		});
	};

	_stack->add(Section{
		.widget = std::move(wrap),
		.shown = rpl::single(true),
	});
	_stack->addTextSeparator(
		std::move(textProducer),
		rpl::single(true),
		std::move(setup));
}

Section DetailsFiller::makeBotPermissions(not_null<UserData*> user) {
	const auto parent = _stack->layout();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	const auto inner = raw->entity();
	AddSkip(inner);
	AddSubsectionTitle(inner, tr::lng_profile_bot_permissions_title());
	const auto emoji = inner->add(
		object_ptr<Ui::SettingsButton>(
			inner,
			tr::lng_profile_bot_emoji_status_access(),
			st::infoSharedMediaButton));
	object_ptr<Profile::FloatingIcon>(
		emoji,
		st::infoIconEmojiStatusAccess,
		st::infoSharedMediaButtonIconPosition);

	emoji->toggleOn(
		rpl::single(bool(user->botInfo->canManageEmojiStatus))
	)->toggledValue() | rpl::filter([=](bool allowed) {
		return allowed != user->botInfo->canManageEmojiStatus;
	}) | rpl::on_next([=](bool allowed) {
		user->botInfo->canManageEmojiStatus = allowed;
		const auto session = &user->session();
		session->api().request(MTPbots_ToggleUserEmojiStatusPermission(
			user->inputUser(),
			MTP_bool(allowed)
		)).send();
	}, emoji->lifetime());
	AddSkip(inner);
	return Section{
		.widget = std::move(wrap),
		.shown = rpl::single(true),
	};
}

void DetailsFiller::addBotVerify() {
	const auto peer = _peer.get();
	auto shown = peer->session().changes().peerFlagsValue(
		peer,
		Data::PeerUpdate::Flag::VerifyInfo
			| Data::PeerUpdate::Flag::FullInfo
	) | rpl::map([=] {
		const auto info = peer->botVerifyDetails();
		if (!info || info->description.empty()) {
			return false;
		}
		if (const auto user = peer->asUser()) {
			if (user->botInfo && user->botInfo->hasMainApp) {
				return false;
			}
		}
		return true;
	}) | rpl::distinct_until_changed();

	auto description = peer->session().changes().peerFlagsValue(
		peer,
		Data::PeerUpdate::Flag::VerifyInfo
	) | rpl::map([=] {
		const auto info = peer->botVerifyDetails();
		return info ? info->description : TextWithEntities();
	});

	_stack->addTextSeparator(std::move(description), std::move(shown));
}

void DetailsFiller::addManagedBotFooter(not_null<UserData*> managerUser) {
	const auto botUsername = managerUser->username();
	const auto linkText = botUsername.isEmpty()
		? managerUser->name()
		: (u"@"_q + botUsername);
	auto text = tr::lng_managed_bot_label(
		lt_icon,
		rpl::single(Ui::Text::IconEmoji(&st::managedBotIconEmoji)),
		lt_bot,
		rpl::single(tr::link(linkText)),
		tr::marked);
	const auto weak = base::make_weak(_controller);
	auto setup = [=](not_null<Ui::FlatLabel*> label) {
		label->setClickHandlerFilter([=](const auto &...) {
			if (const auto strong = weak.get()) {
				strong->showPeerInfo(managerUser);
			}
			return false;
		});
	};
	_stack->addTextSeparator(
		std::move(text),
		rpl::single(true),
		std::move(setup));
}

Section DetailsFiller::makeReportOrDeleteReaction() {
	if (_peer->isSelf()) {
		return Section{ .widget = nullptr };
	}
	auto result = Section{ .widget = nullptr };
	v::match(_origin.data, [&](GroupReactionOrigin data) {
		if (HistoryView::Reactions::CanModerateReactionByDeleteMessages(
				data.group)) {
			result = makeDeleteReactionSection(data);
			return;
		}
		const auto capabilities = Api::GetReactionReportCapabilities(
			data.group,
			_peer);
		if (capabilities.canReport) {
			result = makeReportReactionSection(data, capabilities.canBan);
		}
	}, [](const auto &) {});
	return result;
}

Section DetailsFiller::makeDeleteReactionSection(GroupReactionOrigin data) {
	const auto parent = _stack->layout();
	const auto peer = _peer;
	const auto controller = _controller->parentController();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	auto shown = rpl::single(true);
	raw->toggleOn(rpl::duplicate(shown));
	AddMainButton(
		raw->entity(),
		tr::lng_context_delete_this_reaction(),
		std::move(shown),
		[=] {
			HistoryView::Reactions::ShowModerateReactionBox(
				controller,
				data.group,
				data.messageId,
				peer);
		},
		nullptr,
		nullptr,
		st::infoMainButtonAttention);
	return Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

Section DetailsFiller::makeReportReactionSection(
		GroupReactionOrigin data,
		bool ban) {
	const auto parent = _stack->layout();
	const auto peer = _peer;
	const auto controller = _controller->parentController();
	const auto forceHidden = std::make_shared<rpl::variable<bool>>(false);
	const auto user = peer->asUser();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	auto shown = user
		? rpl::combine(
			Info::Profile::IsContactValue(user),
			forceHidden->value(),
			!rpl::mappers::_1 && !rpl::mappers::_2
		) | rpl::type_erased
		: (forceHidden->value() | rpl::map(!rpl::mappers::_1));
	const auto sent = [=] {
		*forceHidden = true;
	};
	raw->toggleOn(rpl::duplicate(shown));
	AddMainButton(
		raw->entity(),
		(ban
			? tr::lng_report_and_ban()
			: tr::lng_report_reaction()),
		std::move(shown),
		[=] { controller->show(
			Box(ReportReactionBox, controller, peer, data, ban, sent)); },
		nullptr,
		nullptr,
		st::infoMainButtonAttention);
	return Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

Section DetailsFiller::makeCommunityLink(not_null<PeerData*> peer) {
	const auto parent = _stack->layout();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	const auto card = FAUi::CreateCardContainer(raw->entity(), 6, 6);
	const auto container = card;
	const auto window = _controller->parentController();
	const auto community = peer->owner().channel(
		Data::PeerLinkedCommunityId(peer));

	const auto line = container->add(
		object_ptr<Ui::FixedHeightWidget>(
			container,
			56));

	const auto userpic = Ui::CreateChild<Ui::RpWidget>(line);
	userpic->setGeometry(16, 8, 40, 40);
	userpic->setAttribute(Qt::WA_TransparentForMouseEvents);
	struct UserpicState {
		Ui::CommunityUserpicEffect effect;
		Ui::PeerUserpicView view;
	};
	const auto userpicState = userpic->lifetime().make_state<UserpicState>();
	userpic->paintRequest(
	) | rpl::on_next([=] {
		Painter p(userpic);
		PainterHighQualityEnabler hq(p);
		Ui::PaintCommunityUserpicEffect(
			p,
			userpicState->effect,
			0,
			0,
			40,
			st::windowBgOver->c);
		community->paintUserpicLeft(
			p,
			userpicState->view,
			0,
			0,
			40,
			40);
	}, userpic->lifetime());

	const auto name = Ui::CreateChild<Ui::FlatLabel>(
		line,
		NameValue(community),
		st::infoPersonalChannelNameLabel);
	name->setAttribute(Qt::WA_TransparentForMouseEvents);

	const auto statusText = [=] {
		const auto info = community->communityInfo();
		const auto count = info
			? int(info->linkedPeers().size())
			: 0;
		return count
			? tr::lng_community_profile_status(
				tr::now,
				lt_count,
				count)
			: tr::lng_community_title(tr::now);
	};
	const auto status = Ui::CreateChild<Ui::FlatLabel>(
		line,
		statusText(),
		st::defaultFlatLabel);
	status->setTextColorOverride(st::windowSubTextFg->c);
	status->setAttribute(Qt::WA_TransparentForMouseEvents);

	community->session().changes().peerUpdates(
		community,
		Data::PeerUpdate::Flag::FullInfo | Data::PeerUpdate::Flag::Name
	) | rpl::on_next([=] {
		status->setText(statusText());
		userpic->update();
	}, line->lifetime());

	line->sizeValue(
	) | rpl::filter_size(
	) | rpl::on_next([=](const QSize &size) {
		const auto left = 68;
		const auto right = 16;
		const auto availableWidth = size.width() - left - right;
		name->resizeToWidth(availableWidth);
		name->moveToLeft(left, 9);
		status->resizeToWidth(availableWidth);
		status->moveToLeft(left, 29);
	}, line->lifetime());

	const auto button = FA::Ui::CreateCardRippleButton(
		line,
		14);
	button->lower();
	line->sizeValue(
	) | rpl::on_next([=](const QSize &size) {
		button->setGeometry(QRect(0, 0, size.width(), size.height()));
	}, button->lifetime());

	button->setAcceptBoth();
	struct State {
		base::unique_qptr<Ui::PopupMenu> menu;
	};
	const auto state = button->lifetime().make_state<State>();
	const auto open = [=] { window->showPeerInfo(community); };
	button->addClickHandler([=](Qt::MouseButton mouse) {
		if (mouse == Qt::RightButton) {
			const auto history = community->owner().history(community);
			if (!history->owner().chatsFilters().has()
				|| !history->inChatList()
				|| (community->isCommunity()
					&& !community->collapsedInDialogs())) {
				return;
			}
			state->menu = base::make_unique_q<Ui::PopupMenu>(
				button,
				st::popupMenuWithIcons);
			Ui::Menu::CreateAddActionCallback(state->menu.get())({
				.text = tr::lng_filters_menu_add(tr::now),
				.handler = nullptr,
				.icon = &st::menuIconAddToFolder,
				.fillSubmenu = [&](not_null<Ui::PopupMenu*> submenu) {
					FillChooseFilterMenu(window, submenu, history);
				},
				.submenuSt = &st::foldersMenu,
			});
			state->menu->popup(QCursor::pos());
			return;
		}
		open();
	});

	if (!community->wasFullUpdated()) {
		community->session().api().requestFullPeer(community);
	}

	raw->toggle(true, anim::type::instant);
	return Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

void DetailsFiller::addCommunityHiddenNote() {
	const auto peer = _peer.get();
	const auto community = peer->owner().channel(
		Data::PeerLinkedCommunityId(peer));
	auto shown = peer->session().changes().peerFlagsValue(
		community,
		Data::PeerUpdate::Flag::FullInfo
	) | rpl::map([=] {
		return community->communityInfo();
	}) | rpl::map([=](Data::CommunityInfo *info) -> rpl::producer<bool> {
		if (!info) {
			return rpl::single(false);
		}
		return info->linkedPeersValue() | rpl::map([=] {
			return info->isHidden(peer);
		});
	}) | rpl::flatten_latest() | rpl::distinct_until_changed();

	_stack->addTextSeparator(
		tr::lng_community_hidden_chat_about(tr::marked),
		std::move(shown));
}

Section DetailsFiller::makeTopicsList(not_null<Data::Forum*> forum) {
	using namespace rpl::mappers;

	const auto parent = _stack->layout();
	auto wrap = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto raw = wrap.data();
	const auto card = FAUi::CreateCardContainer(raw->entity(), 6, 6);
	const auto window = _controller->parentController();
	const auto peer = forum->peer();
	auto showTopicsVisible = rpl::combine(
		window->adaptive().oneColumnValue(),
		window->shownForum().value(),
		_1 || (_2 != forum));
	const auto callback = [=] {
		if (const auto forum = peer->forum()) {
			if (peer->useSubsectionTabs()) {
				window->searchInChat(forum->history());
			} else {
				window->showForum(forum);
			}
		}
	};
	raw->toggleOn(rpl::duplicate(showTopicsVisible));
	AddMainButton(
		card,
		(forum->peer()->isBot()
			? tr::lng_bot_show_threads_list()
			: tr::lng_forum_show_topics_list()),
		std::move(showTopicsVisible),
		callback,
		nullptr,
		nullptr);
	return Section{
		.widget = std::move(wrap),
		.shown = raw->toggledValue(),
	};
}

void DetailsFiller::buildSections() {
	Expects(!_topic || !_topic->creating());

	if (const auto user = _sublist ? nullptr : _peer->asUser()) {
		_stack->add(makePersonalChannel(user));
		_stack->addPlainSeparator();
	}
	if (Data::PeerLinkedCommunityId(_peer)) {
		_stack->add(makeCommunityLink(_peer));
		addCommunityHiddenNote();
		_stack->addPlainSeparator();
	}
	_stack->add(FAUi::MakeProfileInfo(_controller, _peer, _topic, _stack->layout()));
	if (const auto user = _peer->asUser()) {
		addBotVerify();
		if (const auto info = user->botInfo.get()) {
			if (info->hasMainApp) {
				addMainApp(user);
			}
			if (info->canManageEmojiStatus) {
				_stack->add(makeBotPermissions(user));
			}
			if (const auto id = user->botManagerId()) {
				if (const auto mgr = user->owner().userLoaded(id)) {
					addManagedBotFooter(mgr);
				}
			}
		}
		if (!_sublist) {
			auto reactionSection = makeReportOrDeleteReaction();
			if (reactionSection.widget) {
				_stack->add(std::move(reactionSection));
			}
		}
	} else if (const auto channel = _peer->asChannel()) {
		addBotVerify();
		if (const auto forum = channel->forum()) {
			_stack->add(makeTopicsList(forum));
		}
	}
}

ActionsFiller::ActionsFiller(
	not_null<Controller*> controller,
	not_null<Ui::RpWidget*> parent,
	not_null<PeerData*> peer,
	Ui::MultiSlideTracker *tracker)
: _controller(controller)
, _parent(parent)
, _peer(peer)
, _tracker(tracker) {
}

void ActionsFiller::fillInto(not_null<Ui::VerticalLayout*> card) {
	_card = card;
	if (auto user = _peer->asUser()) {
		fillUserActions(user);
	}
}

void ActionsFiller::addAffiliateProgram(not_null<UserData*> user) {
	if (!user->isBot()) {
		return;
	}

	const auto wrap = _card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_card,
			object_ptr<Ui::VerticalLayout>(_card)));
	const auto inner = wrap->entity();
	auto program = user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::StarRefProgram
	) | rpl::map([=] {
		return user->botInfo->starRefProgram;
	}) | rpl::start_spawning(inner->lifetime());
	auto commission = rpl::duplicate(
		program
	) | rpl::filter([=](StarRefProgram program) {
		return program.commission > 0;
	}) | rpl::map([=](StarRefProgram program) {
		return Info::BotStarRef::FormatCommission(program.commission);
	});
	const auto show = _controller->uiShow();

	struct StarRefRecipients {
		std::vector<not_null<PeerData*>> list;
		bool requested = false;
		Fn<void()> open;
	};
	const auto recipients = std::make_shared<StarRefRecipients>();
	recipients->open = [=] {
		if (!recipients->list.empty()) {
			const auto program = user->botInfo->starRefProgram;
			show->show(Info::BotStarRef::JoinStarRefBox(
				{ user, { program } },
				user->session().user(),
				recipients->list));
		} else if (!recipients->requested) {
			recipients->requested = true;
			const auto done = [=](std::vector<not_null<PeerData*>> list) {
				recipients->list = std::move(list);
				recipients->open();
			};
			Info::BotStarRef::ResolveRecipients(&user->session(), done);
		}
	};

	inner->add(EditPeerInfoBox::CreateButton(
		inner,
		tr::lng_manage_peer_bot_star_ref(),
		rpl::duplicate(commission),
		recipients->open,
		st::infoSharedMediaCountButton,
		{ .icon = &st::menuIconSharing }));
	Ui::AddSkip(inner);
	Ui::AddDividerText(
		inner,
		tr::lng_manage_peer_bot_star_ref_about(
			lt_bot,
			rpl::single(TextWithEntities{ user->name() }),
			lt_amount,
			rpl::duplicate(commission) | rpl::map(tr::marked),
			tr::rich));
	Ui::AddSkip(inner);

	wrap->toggleOn(std::move(
		program
	) | rpl::map([](StarRefProgram program) {
		return program.commission > 0;
	}));
	wrap->finishAnimating();
	if (_tracker) {
		_tracker->track(wrap);
	}
}

void ActionsFiller::addBalanceActions(not_null<UserData*> user) {
	const auto wrap = _card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_card,
			object_ptr<Ui::VerticalLayout>(_card)));
	const auto inner = wrap->entity();
	Ui::AddSubsectionTitle(inner, tr::lng_manage_peer_bot_balance());
	auto currencyBalance = AddCurrencyAction(user, inner, _controller);
	auto creditsBalance = AddCreditsAction(user, inner, _controller);
	Ui::AddSkip(inner);
	Ui::AddDivider(inner);
	Ui::AddSkip(inner);
	wrap->toggleOn(
		rpl::combine(
			std::move(currencyBalance),
			std::move(creditsBalance)
		) | rpl::map((rpl::mappers::_1 > CreditsAmount(0))
			|| (rpl::mappers::_2 > CreditsAmount(0))));
	if (_tracker) {
		_tracker->track(wrap);
	}
}

void ActionsFiller::addInviteToGroupAction(not_null<UserData*> user) {
	const auto notEmpty = [](const QString &value) {
		return !value.isEmpty();
	};
	const auto controller = _controller->parentController();
	AddActionButton(
		_card,
		InviteToChatButton(user) | rpl::filter(notEmpty),
		InviteToChatButton(user) | rpl::map(notEmpty),
		[=] { AddBotToGroupBoxController::Start(controller, user); },
		&st::infoIconAddMember,
		st::infoSharedMediaButton,
		_tracker);
	const auto about = _card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_card,
			object_ptr<Ui::VerticalLayout>(_card)));
	about->toggleOn(InviteToChatAbout(user) | rpl::map(notEmpty));
	Ui::AddSkip(about->entity());
	Ui::AddDividerText(
		about->entity(),
		InviteToChatAbout(user) | rpl::filter(notEmpty));
	Ui::AddSkip(about->entity());
	about->finishAnimating();
	if (_tracker) {
		_tracker->track(about);
	}
}

void ActionsFiller::addShareContactAction(not_null<UserData*> user) {
	const auto controller = _controller->parentController();
	AddActionButton(
		_card,
		tr::lng_info_share_contact(),
		CanShareContactValue(user),
		[=] { Window::PeerMenuShareContactBox(controller, user); },
		&st::infoIconShare,
		st::infoSharedMediaButton,
		_tracker);
}

void ActionsFiller::addEditContactAction(not_null<UserData*> user) {
	const auto controller = _controller->parentController();
	const auto edit = [=] {
		if (controller->showFrozenError()) {
			return;
		}
		controller->window().show(Box(EditContactBox, controller, user));
	};
	AddActionButton(
		_card,
		tr::lng_info_edit_contact(),
		IsContactValue(user),
		edit,
		&st::infoIconEdit,
		st::infoSharedMediaButton,
		_tracker);
}

void ActionsFiller::addDeleteContactAction(not_null<UserData*> user) {
	const auto controller = _controller->parentController();
	AddActionButton(
		_card,
		tr::lng_info_delete_contact(),
		IsContactValue(user),
		[=] { Window::PeerMenuDeleteContact(controller, user); },
		&st::infoIconDelete,
		st::infoSharedMediaButton,
		_tracker);
}

void ActionsFiller::addFastButtonsMode(not_null<UserData*> user) {
	Expects(user->isBot());

	const auto bots = &user->session().fastButtonsBots();
	const auto button = _card->add(object_ptr<Ui::SettingsButton>(
		_card,
		rpl::single(u"Fast buttons mode"_q),
		st::infoSharedMediaButton));
	object_ptr<Info::Profile::FloatingIcon>(
		button,
		st::infoIconMediaBot,
		st::infoSharedMediaButtonIconPosition);

	AddSkip(_card);
	AddDivider(_card);
	AddSkip(_card);

	button->toggleOn(bots->enabledValue(user));
	button->toggledValue(
	) | rpl::filter([=](bool value) {
		return value != bots->enabled(user);
	}) | rpl::on_next([=](bool value) {
		bots->setEnabled(user, value);
	}, button->lifetime());
}

void ActionsFiller::addBotCommandActions(not_null<UserData*> user) {
	if (FastButtonsMode()) {
		addFastButtonsMode(user);
	}
	const auto window = _controller->parentController();
	const auto findBotCommand = [user](const QString &command) {
		if (!user->isBot()) {
			return QString();
		}
		for (const auto &data : user->botInfo->commands) {
			const auto isSame = !data.command.compare(
				command,
				Qt::CaseInsensitive);
			if (isSame) {
				return data.command;
			}
		}
		return QString();
	};
	const auto hasBotCommandValue = [=](const QString &command) {
		return user->session().changes().peerFlagsValue(
			user,
			Data::PeerUpdate::Flag::BotCommands
		) | rpl::map([=] {
			return !findBotCommand(command).isEmpty();
		});
	};
	const auto makeOtherContext = [=] {
		return QVariant::fromValue(ClickHandlerContext{
			.sessionWindow = base::make_weak(window),
			.peer = user,
		});
	};
	const auto sendBotCommand = [=](const QString &command) {
		const auto original = findBotCommand(command);
		if (original.isEmpty()) {
			return false;
		}
		BotCommandClickHandler('/' + original).onClick(ClickContext{
			Qt::LeftButton,
			makeOtherContext()
		});
		return true;
	};
	const auto addBotCommand = [=](
			rpl::producer<QString> text,
			const QString &command,
			const style::icon *icon = nullptr) {
		AddActionButton(
			_card,
			std::move(text),
			hasBotCommandValue(command),
			[=] { sendBotCommand(command); },
			icon);
	};
	addBotCommand(
		tr::lng_profile_bot_help(),
		u"help"_q,
		&st::infoIconInformation);
	addBotCommand(
		tr::lng_profile_bot_settings(),
		u"settings"_q,
		&st::infoIconSettings);
	//addBotCommand(tr::lng_profile_bot_privacy(), u"privacy"_q);
	const auto openUrl = [=](const QString &url) {
		Core::App().iv().openWithIvPreferred(
			&user->session(),
			url,
			makeOtherContext());
	};
	const auto openPrivacyPolicy = [=] {
		if (const auto info = user->botInfo.get()) {
			if (!info->privacyPolicyUrl.isEmpty()) {
				openUrl(info->privacyPolicyUrl);
				return;
			}
		}
		if (!sendBotCommand(u"privacy"_q)) {
			openUrl(tr::lng_profile_bot_privacy_url(tr::now));
		}
	};
	AddActionButton(
		_card,
		tr::lng_profile_bot_privacy(),
		rpl::single(true),
		openPrivacyPolicy,
		&st::infoIconPrivacyPolicy);
}

void ActionsFiller::addReportAction() {
	const auto peer = _peer;
	const auto controller = _controller->parentController();
	const auto report = [=] {
		ShowReportMessageBox(controller->uiShow(), peer, {}, {});
	};
	AddActionButton(
		_card,
		tr::lng_profile_report(),
		rpl::single(true),
		report,
		&st::infoIconReport,
		st::infoBlockButton,
		_tracker);
}

void ActionsFiller::addBlockAction(not_null<UserData*> user) {
	const auto controller = _controller->parentController();
	const auto window = &controller->window();

	auto text = user->session().changes().peerFlagsValue(
		user,
		Data::PeerUpdate::Flag::IsBlocked
	) | rpl::map([=] {
		switch (user->blockStatus()) {
		case UserData::BlockStatus::Blocked:
			return ((user->isBot() && !user->isSupport())
				? tr::lng_profile_restart_bot
				: tr::lng_profile_unblock_user)();
		case UserData::BlockStatus::NotBlocked:
		default:
			return ((user->isBot() && !user->isSupport())
				? tr::lng_profile_block_bot
				: tr::lng_profile_block_user)();
		}
	}) | rpl::flatten_latest(
		) | rpl::start_spawning(_card->lifetime());

	auto toggleOn = rpl::duplicate(
		text
	) | rpl::map([](const QString &text) {
		return !text.isEmpty();
	});
	auto callback = [=] {
		if (user->isBlocked()) {
			const auto show = controller->uiShow();
			Window::PeerMenuUnblockUserWithBotRestart(show, user);
			if (user->isBot()) {
				controller->showPeerHistory(user);
			}
		} else if (user->isBot()) {
			user->session().api().blockedPeers().block(user);
		} else {
			window->show(Box(
				Window::PeerMenuBlockUserBox,
				window,
				user,
				v::null,
				v::null));
		}
	};
	AddActionButton(
		_card,
		rpl::duplicate(text),
		std::move(toggleOn),
		std::move(callback),
		&st::infoIconBlock,
		st::infoBlockButton,
		_tracker);
}

void ActionsFiller::fillUserActions(not_null<UserData*> user) {
	if (user->isBot()) {
		addAffiliateProgram(user);
		addBalanceActions(user);
		addInviteToGroupAction(user);
	}
	addShareContactAction(user);
	if (!user->isSelf()) {
		addEditContactAction(user);
		addDeleteContactAction(user);
	}
	if (!user->isSelf() && !user->isSupport() && !user->isVerifyCodes()) {
		if (user->isBot()) {
			addBotCommandActions(user);
			addReportAction();
		}
		addBlockAction(user);
	}
}

object_ptr<Ui::RpWidget> ActionsFiller::fill() {
	return { nullptr };
}

} // namespace

void SetupUserActions(
		not_null<Ui::VerticalLayout*> container,
		not_null<Controller*> controller,
		not_null<UserData*> user,
		Ui::MultiSlideTracker &tracker) {
	ActionsFiller filler(controller, container, user, &tracker);
	filler.fillInto(container);
}

const char kOptionShowPeerIdBelowAbout[] = "show-peer-id-below-about";
const char kOptionShowChannelJoinedBelowAbout[] = "show-channel-joined-below-about";

object_ptr<Ui::RpWidget> SetupActions(
		not_null<Controller*> controller,
		not_null<Ui::RpWidget*> parent,
		not_null<PeerData*> peer) {
	ActionsFiller filler(controller, parent, peer);
	return filler.fill();
}

void SetupAddChannelMember(
		not_null<Window::SessionNavigation*> navigation,
		not_null<Ui::RpWidget*> parent,
		not_null<ChannelData*> channel) {
	auto add = Ui::CreateChild<Ui::IconButton>(
		parent.get(),
		st::infoMembersAddMember);
	add->setAccessibleName(tr::lng_channel_add_members(tr::now));
	add->showOn(CanAddMemberValue(channel));
	add->addClickHandler([=] {
		Window::PeerMenuAddChannelMembers(navigation, channel);
	});
	parent->widthValue(
	) | rpl::on_next([add](int newWidth) {
		auto availableWidth = newWidth
			- st::infoMembersButtonPosition.x();
		add->moveToLeft(
			availableWidth - add->width(),
			st::infoMembersButtonPosition.y(),
			newWidth);
	}, add->lifetime());
}

object_ptr<Ui::RpWidget> SetupChannelMembersAndManage(
		not_null<Controller*> controller,
		not_null<Ui::RpWidget*> parent,
		not_null<PeerData*> peer) {
	using namespace rpl::mappers;

	auto channel = peer->asChannel();
	if (!channel || channel->isMegagroup()) {
		return { nullptr };
	}

	auto result = object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		parent,
		object_ptr<Ui::VerticalLayout>(parent));
	const auto content = FAUi::CreateCardContainer(result->entity(), 6, 6);

	auto membersShown = rpl::combine(
		MembersCountValue(channel),
		Data::PeerFlagValue(
			channel,
			ChannelDataFlag::CanViewParticipants),
			(_1 > 0) && _2);
	auto membersText = tr::lng_chat_status_subscribers(
		lt_count_decimal,
		MembersCountValue(channel) | tr::to_count());
	auto membersCallback = [=] {
		controller->showSection(std::make_shared<Info::Memento>(
			channel,
			Info::Section::Type::Members));
	};

	const auto membersWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)));
	membersWrap->setDuration(
		st::infoSlideDuration
	)->toggleOn(rpl::duplicate(membersShown));

	const auto members = membersWrap->entity();
	{
		auto button = AddActionButton(
			members,
			std::move(membersText),
			rpl::single(true),
			std::move(membersCallback),
			nullptr)->entity();

		SetupAddChannelMember(controller, button, channel);
	}

	object_ptr<FloatingIcon>(
		members,
		st::infoIconMembers,
		st::infoChannelMembersIconPosition);

	auto adminsShown = peer->session().changes().peerFlagsValue(
		channel,
		Data::PeerUpdate::Flag::Rights
	) | rpl::map([=] { return channel->canViewAdmins(); });
	auto adminsText = tr::lng_profile_administrators(
		lt_count_decimal,
		Info::Profile::MigratedOrMeValue(
			channel
		) | rpl::map(
			Info::Profile::AdminsCountValue
		) | rpl::flatten_latest() | tr::to_count());
	auto adminsCallback = [=] {
		ParticipantsBoxController::Start(
			controller,
			channel,
			ParticipantsBoxController::Role::Admins);
	};

	const auto adminsWrap = content->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			content,
			object_ptr<Ui::VerticalLayout>(content)));
	adminsWrap->setDuration(
		st::infoSlideDuration
	)->toggleOn(rpl::duplicate(adminsShown));

	const auto admins = adminsWrap->entity();
	AddActionButton(
		admins,
		std::move(adminsText),
		rpl::single(true),
		std::move(adminsCallback),
		nullptr);

	object_ptr<FloatingIcon>(
		admins,
		st::menuIconAdmin,
		st::infoChannelAdminsIconPosition);

	const auto canViewBalance = false
		|| (channel->flags() & ChannelDataFlag::CanViewRevenue)
		|| (channel->flags() & ChannelDataFlag::CanViewCreditsRevenue)
		|| (channel->loadedStatus() != ChannelData::LoadedStatus::Full);
	if (canViewBalance) {
		const auto balanceWrap = content->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				content,
				object_ptr<Ui::VerticalLayout>(content)));
		auto refreshed = channel->session().credits().refreshedByPeerId(
			channel->id);
		auto creditsValue = rpl::single(
			rpl::empty_value()
		) | rpl::then(rpl::duplicate(refreshed)) | rpl::map([=] {
			return channel->session().credits().balance(channel->id);
		});
		auto currencyValue = rpl::single(
			rpl::empty_value()
		) | rpl::then(rpl::duplicate(refreshed)) | rpl::map([=] {
			return channel->session().credits().balanceCurrency(channel->id);
		});
		const auto emptyAmount = CreditsAmount(0);
		balanceWrap->toggleOn(
			rpl::combine(
				rpl::duplicate(creditsValue),
				rpl::duplicate(currencyValue)
			) | rpl::map(rpl::mappers::_1 > emptyAmount
				|| rpl::mappers::_2 > emptyAmount),
			anim::type::normal);
		balanceWrap->finishAnimating();

		const auto &st = st::infoSharedMediaButton;

		auto customEmojiFactory = [height = st.style.font->height,
				font = st.rightLabel.style.font,
				color = st.rightLabel.textFg->c](
			QStringView data,
			const Ui::Text::MarkedContext &context
		) -> std::unique_ptr<Ui::Text::CustomEmoji> {
			return (data == Ui::kCreditsCurrency)
				? Ui::MakeCreditsIconEmoji(height, 1)
				: MakeWrappedEmoji<Ui::Text::ShiftedEmoji>(
					Ui::Earn::MakeCurrencyIconEmoji(font, color),
					QPoint(0, st::channelEarnCurrencyCommonMargins.top()));
		};
		const auto context = Ui::Text::MarkedContext{
			.customEmojiFactory = std::move(customEmojiFactory),
		};

		const auto balance = balanceWrap->entity();
		const auto button = AddActionButton(
			balance,
			tr::lng_manage_peer_bot_balance(),
			rpl::single(true),
			[=] { controller->showSection(Info::ChannelEarn::Make(peer)); },
			nullptr);

		::Settings::CreateRightLabel(
			button->entity(),
			rpl::combine(
				std::move(creditsValue),
				std::move(currencyValue)
			) | rpl::map([](CreditsAmount credits, CreditsAmount currency) {
				auto creditsText = (credits > CreditsAmount(0))
					? Ui::MakeCreditsIconEntity()
						.append(QChar(' '))
						.append(Info::ChannelEarn::MajorPart(credits))
						.append(credits.nano()
							? Info::ChannelEarn::MinorPart(credits)
							: QString())
					: TextWithEntities();
				auto currencyText = (currency > CreditsAmount(0))
					? Ui::Text::SingleCustomEmoji("_")
						.append(QChar(' '))
						.append(Info::ChannelEarn::MajorPart(currency))
						.append(Info::ChannelEarn::MinorPart(currency))
					: TextWithEntities();
				return currencyText
					.append(QChar(' '))
					.append(std::move(creditsText));
			}),
			st,
			tr::lng_manage_peer_bot_balance(),
			context);

		object_ptr<FloatingIcon>(
			balance,
			st::menuIconEarn,
			st::infoChannelAdminsIconPosition);
	}

	auto joinVisible = AmInChannelValue(channel)
		| rpl::map(!_1)
		| rpl::start_spawning(content->lifetime());
	AddActionButton(
		content,
		tr::lng_profile_join_channel(),
		rpl::duplicate(joinVisible),
		[=] { channel->session().api().joinChannel(channel); },
		&st::infoIconAddMember);

	AddActionButton(
		content,
		tr::lng_profile_leave_channel(),
		AmInChannelValue(channel),
		Window::DeleteAndLeaveHandler(
			controller->parentController(),
			channel),
		&st::infoIconLeave);

	if (!channel->amCreator()) {
		const auto report = [=] {
			ShowReportMessageBox(
				controller->parentController()->uiShow(),
				channel,
				{},
				{});
		};
		AddActionButton(
			content,
			tr::lng_profile_report(),
			rpl::single(true),
			report,
			&st::infoIconReport,
			st::infoBlockButton);
	}

	result->setDuration(st::infoSlideDuration)->toggleOn(rpl::single(true));

	result->entity()->add(CreateSkipWidget(result));

	return result;
}

void BuildProfileDetailsSections(
		SectionStack &stack,
		not_null<Controller*> controller,
		not_null<PeerData*> peer,
		Data::ForumTopic *topic,
		Data::SavedSublist *sublist,
		Origin origin) {
	if (topic) {
		DetailsFiller filler(controller, &stack, topic);
		filler.buildSections();
	} else if (sublist) {
		DetailsFiller filler(controller, &stack, sublist);
		filler.buildSections();
	} else {
		DetailsFiller filler(controller, &stack, peer, origin);
		filler.buildSections();
	}
}

void AddDetails(
		not_null<Ui::VerticalLayout*> container,
		not_null<Controller*> controller,
		not_null<PeerData*> peer,
		Data::ForumTopic *topic,
		Data::SavedSublist *sublist,
		Origin origin) {
	auto layout = object_ptr<Ui::VerticalLayout>(container);
	auto stack = SectionStack(layout.data());
	BuildProfileDetailsSections(
		stack,
		controller,
		peer,
		topic,
		sublist,
		origin);
	stack.finalize();
	container->add(std::move(layout));
}

} // namespace Profile
} // namespace Info
