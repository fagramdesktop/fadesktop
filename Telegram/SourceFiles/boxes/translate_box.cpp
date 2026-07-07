/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/translate_box.h"
#include "boxes/translate_box_content.h"
#include "lang/translate_provider.h"

#include "base/weak_ptr.h"
#include "chat_helpers/spellchecker_common.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/ui_integration.h"
#include "data/data_msg_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_article_scroll_forwarder.h"
#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/iv_cached_media.h"
#include "iv/iv_rich_page.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "spellcheck/platform/platform_language.h"
#include "ui/boxes/choose_language_box.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/effects/loading_element.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/multi_select.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/painter.h"
#include "ui/power_saving.h"
#include "ui/vertical_list.h"
#include "window/themes/window_theme.h"

#include "styles/style_boxes.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"

namespace Ui {
namespace {

constexpr auto kSkipAtLeastOneDuration = 3 * crl::time(1000);

class TranslateMediaBlockHost final : public Iv::Markdown::MediaBlockHost {
public:
	TranslateMediaBlockHost(
		Fn<void(QRect)> repaint,
		Fn<void(QRect)> relayout);

	void requestRepaint(QRect articleRect) override;
	void requestRelayout(QRect articleRect) override;

private:
	const Fn<void(QRect)> _repaint;
	const Fn<void(QRect)> _relayout;

};

class RichTranslateArticle final : public RpWidget {
public:
	RichTranslateArticle(
		QWidget *parent,
		not_null<Main::Session*> session,
		FullMsgId itemId);
	~RichTranslateArticle();

	[[nodiscard]] bool showPage(std::shared_ptr<const Iv::RichPage> page);

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;
	void wheelEvent(QWheelEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	bool eventHook(QEvent *e) override;

private:
	void paintArticle(Painter &p, QRect clip);
	void requestArticleRepaint(QRect articleRect);
	void requestArticleRelayout();
	void detachArticleBindings();
	[[nodiscard]] QRect articleRect() const;
	[[nodiscard]] Iv::Markdown::MarkdownArticle *scrollTarget();

	const not_null<Main::Session*> _session;
	const FullMsgId _itemId;
	std::shared_ptr<Iv::Markdown::MediaRuntime> _mediaRuntime;
	std::unique_ptr<Iv::Markdown::MediaBlockHost> _host;
	Iv::Markdown::MarkdownArticle _article;
	std::unique_ptr<ChatTheme> _theme;
	std::unique_ptr<ChatStyle> _style;
	Iv::Markdown::MarkdownArticleScrollForwarder _scrollForwarder;
	int _articleHeight = 0;
	int _paletteVersion = -1;
	bool _hasArticle = false;

};

TranslateMediaBlockHost::TranslateMediaBlockHost(
	Fn<void(QRect)> repaint,
	Fn<void(QRect)> relayout)
: _repaint(std::move(repaint))
, _relayout(std::move(relayout)) {
}

void TranslateMediaBlockHost::requestRepaint(QRect articleRect) {
	crl::on_main([repaint = _repaint, articleRect] {
		if (repaint) {
			repaint(articleRect);
		}
	});
}

void TranslateMediaBlockHost::requestRelayout(QRect articleRect) {
	crl::on_main([relayout = _relayout, articleRect] {
		if (relayout) {
			relayout(articleRect);
		}
	});
}

RichTranslateArticle::RichTranslateArticle(
	QWidget *parent,
	not_null<Main::Session*> session,
	FullMsgId itemId)
: RpWidget(parent)
, _session(session)
, _itemId(itemId)
, _article(st::messageMarkdown)
, _theme(Window::Theme::DefaultChatThemeOn(lifetime()))
, _style(std::make_unique<ChatStyle>(session->colorIndicesValue())) {
	_style->apply(_theme.get());
	_paletteVersion = _style->paletteVersion();
	setAttribute(Qt::WA_AcceptTouchEvents);

	const auto weak = base::make_weak(this);
	_host = std::make_unique<TranslateMediaBlockHost>(
		[weak](QRect articleRect) {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(articleRect);
			}
		},
		[weak](QRect) {
			if (const auto owner = weak.get()) {
				owner->requestArticleRelayout();
			}
		});
	_article.setMediaBlockHost(_host.get());
	_article.setTextRepaintCallbacks(
		[weak] {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(QRect());
			}
		},
		[weak](QRect articleRect) {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(articleRect);
			}
		});

	Spellchecker::HighlightReady(
	) | rpl::on_next([=](Spellchecker::HighlightProcessId processId) {
		if (_article.highlightProcessDone(processId)) {
			requestArticleRepaint(QRect());
		}
	}, lifetime());

	_style->paletteChanged(
	) | rpl::on_next([=] {
		update();
	}, lifetime());

	Window::Theme::Background()->updates(
	) | rpl::on_next([=](const Window::Theme::BackgroundUpdate &) {
		update();
	}, lifetime());
}

RichTranslateArticle::~RichTranslateArticle() {
	detachArticleBindings();
}

void RichTranslateArticle::detachArticleBindings() {
	_article.setTextRepaintCallbacks(nullptr, nullptr);
	_article.setMediaBlockHost(nullptr);
}

bool RichTranslateArticle::showPage(
		std::shared_ptr<const Iv::RichPage> page) {
	const auto limits = Iv::ResolveRichMessageLimits(_session);
	auto mediaRuntime = Iv::CreateMessageMediaRuntime(
		_session,
		_itemId,
		[](QString) {},
		[](QString) {},
		::Data::FileOrigin());
	auto prepared = Iv::Markdown::TryPrepareNativeInstantView({
		.richPage = page,
		.mediaRuntime = mediaRuntime,
		.dimensionsOverride = Iv::Markdown::CaptureMarkdownPrepareDimensions(
			st::messageMarkdown),
		.tableRenderLimits
			= Iv::Markdown::PrepareTableRenderLimitsForRichMessage(limits),
	});
	if (!prepared.supported()) {
		return false;
	}
	_mediaRuntime = std::move(mediaRuntime);
	_article.setContent(std::move(prepared.content));
	_hasArticle = true;
	if (width() > 0) {
		resizeToWidth(width());
	}
	update();
	return true;
}

void RichTranslateArticle::requestArticleRepaint(QRect rect) {
	crl::on_main(this, [=] {
		if (rect.isEmpty()) {
			update();
		} else {
			update(rect.translated(articleRect().topLeft()));
		}
	});
}

void RichTranslateArticle::requestArticleRelayout() {
	_article.invalidateLayout();
	if (width() > 0) {
		resizeToWidth(width());
	}
	update();
}

QRect RichTranslateArticle::articleRect() const {
	return QRect(0, 0, width(), _articleHeight);
}

Iv::Markdown::MarkdownArticle *RichTranslateArticle::scrollTarget() {
	return _hasArticle ? &_article : nullptr;
}

void RichTranslateArticle::wheelEvent(QWheelEvent *e) {
	_scrollForwarder.handleWheel(scrollTarget(), e, articleRect().topLeft());
}

void RichTranslateArticle::mousePressEvent(QMouseEvent *e) {
	if (!_scrollForwarder.handleMousePress(
			scrollTarget(),
			e,
			articleRect().topLeft())) {
		RpWidget::mousePressEvent(e);
	}
}

void RichTranslateArticle::mouseMoveEvent(QMouseEvent *e) {
	if (!_scrollForwarder.handleMouseMove(
			scrollTarget(),
			e,
			articleRect().topLeft())) {
		RpWidget::mouseMoveEvent(e);
	}
}

void RichTranslateArticle::mouseReleaseEvent(QMouseEvent *e) {
	if (!_scrollForwarder.handleMouseRelease(
			scrollTarget(),
			e,
			articleRect().topLeft())) {
		RpWidget::mouseReleaseEvent(e);
	}
}

bool RichTranslateArticle::eventHook(QEvent *e) {
	if (Iv::Markdown::MarkdownArticleScrollForwarder::IsTouchEvent(e)
		&& _scrollForwarder.handleTouchHook(
			scrollTarget(),
			this,
			e,
			articleRect().topLeft())) {
		return true;
	}
	return RpWidget::eventHook(e);
}

void RichTranslateArticle::paintArticle(Painter &p, QRect clip) {
	if (!_hasArticle) {
		return;
	}
	if (_paletteVersion != _style->paletteVersion()) {
		_paletteVersion = _style->paletteVersion();
		_article.invalidatePaletteCache();
	}
	const auto content = articleRect();
	if (content.isEmpty()) {
		return;
	}
	const auto articleClip = content.intersected(clip).translated(
		-content.topLeft());
	if (articleClip.isEmpty()) {
		return;
	}
	auto context = Iv::Markdown::MarkdownArticlePaintContext(
		_theme->preparePaintContext(
			_style.get(),
			QRect(QPoint(), content.size()),
			QRect(QPoint(), content.size()),
			articleClip,
			false));
	const auto messageStyle = context.messageStyle();
	context.caches = {
		.pre = messageStyle->preCache.get(),
		.blockquote = context.quoteCache({}, 0),
		.colors = _style->highlightColors(),
		.st = &messageStyle->richPageStyle,
		.repaint = [weak = base::make_weak(this)] {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(QRect());
			}
		},
		.repaintRect = [weak = base::make_weak(this)](QRect rect) {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(rect);
			}
		},
	};
	_article.setVisibleTopBottom(0, content.height());
	p.save();
	p.setClipRect(content.intersected(clip));
	p.translate(content.topLeft());
	_article.paint(p, context);
	p.restore();
}

void RichTranslateArticle::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	paintArticle(p, e->rect());
}

int RichTranslateArticle::resizeGetHeight(int newWidth) {
	_articleHeight = _hasArticle ? _article.resizeGetHeight(newWidth) : 0;
	return _articleHeight;
}

[[nodiscard]] bool TranslateRichBox(
		not_null<GenericBox*> box,
		not_null<PeerData*> peer,
		MsgId msgId,
		std::shared_ptr<const Iv::RichPage> page,
		TextWithEntities summaryText,
		bool hasCopyRestriction) {
	const auto session = &peer->session();
	const auto itemId = FullMsgId(peer->id, msgId);
	auto originalArticle = object_ptr<RichTranslateArticle>(
		box,
		session,
		itemId);
	if (!originalArticle->showPage(page)) {
		return false;
	}

	struct State {
		State(not_null<Main::Session*> session)
		: api(&session->mtp()) {
		}

		MTP::Sender api;
		rpl::variable<LanguageId> to;
		mtpRequestId requestId = 0;
	};
	const auto state = box->lifetime().make_state<State>(session);
	state->to = ChooseTranslateTo(peer->owner().history(peer));

	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
	const auto container = box->verticalLayout();

	const auto textContext = Core::TextContext({ .session = session });

	auto to = state->to.value() | rpl::start_spawning(box->lifetime());
	const auto toTitle = rpl::duplicate(to) | rpl::map(LanguageName);
	const auto toDirection = rpl::duplicate(to) | rpl::map([=](
			LanguageId id) {
		return id.locale().textDirection() == Qt::RightToLeft;
	});

	const auto &stLabel = st::aboutLabel;
	const auto lineHeight = stLabel.style.lineHeight;

	Ui::AddSkip(container);

	const auto animationsPaused = [] {
		using Which = FlatLabel::WhichAnimationsPaused;
		const auto emoji = On(PowerSaving::kEmojiChat);
		const auto spoiler = On(PowerSaving::kChatSpoiler);
		return emoji
			? (spoiler ? Which::All : Which::CustomEmoji)
			: (spoiler ? Which::Spoiler : Which::None);
	};
	const auto summary = box->addRow(object_ptr<SlideWrap<FlatLabel>>(
		box,
		object_ptr<FlatLabel>(box, stLabel)));
	if (hasCopyRestriction) {
		summary->entity()->setContextMenuHook([](auto&&) {
		});
	}
	summary->entity()->setAnimationsPausedCallback(animationsPaused);
	summary->entity()->setMarkedText(summaryText, textContext);
	summary->setMinimalHeight(lineHeight);
	summary->hide(anim::type::instant);

	const auto show = Ui::CreateChild<FadeWrap<TranslateShowButton>>(
		container.get(),
		object_ptr<TranslateShowButton>(container));
	rpl::combine(
		container->widthValue(),
		summary->geometryValue()
	) | rpl::on_next([=](int width, const QRect &rect) {
		show->moveToLeft(
			width - show->width() - st::boxRowPadding.right(),
			rect.y() + std::abs(lineHeight - show->height()) / 2);
	}, show->lifetime());

	const auto original = box->addRow(
		object_ptr<SlideWrap<RichTranslateArticle>>(
			box,
			std::move(originalArticle)));
	original->hide(anim::type::instant);

	show->entity()->clicks() | rpl::on_next([=] {
		show->hide(anim::type::instant);
		summary->setMinimalHeight(0);
		summary->hide(anim::type::normal);
		original->show(anim::type::normal);
	}, show->lifetime());

	Ui::AddSkip(container);
	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);

	{
		const auto padding = st::defaultSubsectionTitlePadding;
		const auto subtitle = Ui::AddSubsectionTitle(container, std::move(toTitle));

		rpl::duplicate(to) | rpl::on_next([=] {
			subtitle->resizeToWidth(container->width()
				- padding.left()
				- padding.right());
		}, subtitle->lifetime());
	}

	const auto translated = box->addRow(
		object_ptr<SlideWrap<RichTranslateArticle>>(
			box,
			object_ptr<RichTranslateArticle>(box, session, itemId)));
	translated->hide(anim::type::instant);

	const auto error = box->addRow(object_ptr<SlideWrap<FlatLabel>>(
		box,
		object_ptr<FlatLabel>(box, stLabel)));
	error->hide(anim::type::instant);

	constexpr auto kMaxLines = 3;
	const auto loading = box->addRow(object_ptr<SlideWrap<RpWidget>>(
		box,
		CreateLoadingTextWidget(
			box,
			st::aboutLabel.style,
			kMaxLines,
			std::move(toDirection))));

	const auto showError = [=] {
		error->entity()->setMarkedText(
			tr::italic(tr::lng_translate_box_error(tr::now)),
			textContext);
		error->show(anim::type::instant);
		loading->hide(anim::type::instant);
	};
	const auto showResult = [=](std::shared_ptr<const Iv::RichPage> result) {
		if (result && translated->entity()->showPage(result)) {
			translated->show(anim::type::instant);
			loading->hide(anim::type::instant);
		} else {
			showError();
		}
	};
	const auto send = [=](LanguageId id) {
		state->api.request(base::take(state->requestId)).cancel();
		loading->show(anim::type::instant);
		translated->hide(anim::type::instant);
		error->hide(anim::type::instant);
		using Flag = MTPmessages_TranslateRichMessage::Flag;
		state->requestId = state->api.request(
			MTPmessages_TranslateRichMessage(
				MTP_flags(Flag::f_peer | Flag::f_id),
				peer->input(),
				MTP_vector<MTPint>(1, MTP_int(msgId)),
				MTPVector<MTPInputRichMessage>(),
				MTP_string(id.twoLetterCode()),
				MTPstring())
		).done([=](const MTPmessages_TranslatedRichMessage &result) {
			state->requestId = 0;
			const auto &list = result.data().vresult().v;
			showResult(list.isEmpty()
				? nullptr
				: Iv::ParseRichPage(session, list.front()));
		}).fail([=](const MTP::Error &) {
			state->requestId = 0;
			showResult(nullptr);
		}).send();
	};
	std::move(to) | rpl::on_next(send, box->lifetime());

	box->addLeftButton(tr::lng_settings_language(), [=] {
		if (loading->toggled()) {
			return;
		}
		box->uiShow()->showBox(ChooseTranslateToBox(
			state->to.current(),
			crl::guard(box, [=](LanguageId id) { state->to = id; })));
	});
	return true;
}

} // namespace

void TranslateBox(
		not_null<Ui::GenericBox*> box,
		not_null<PeerData*> peer,
		MsgId msgId,
		TextWithEntities text,
		bool hasCopyRestriction) {
	struct State {
		State(not_null<Main::Session*> session)
		: provider(CreateTranslateProvider(session)) {
		}

		std::unique_ptr<TranslateProvider> provider;
		rpl::variable<LanguageId> to;
	};
	const auto state = box->lifetime().make_state<State>(&peer->session());
	if (IsServerMsgId(msgId) && state->provider->supportsMessageId()) {
		if (const auto item = peer->owner().message(peer->id, msgId)) {
			if (const auto page = item->richPage()) {
				if (TranslateRichBox(
						box,
						peer,
						msgId,
						page,
						text,
						hasCopyRestriction)) {
					return;
				}
			}
		}
	}
	state->to = ChooseTranslateTo(peer->owner().history(peer));
	const auto request = std::make_shared<TranslateProviderRequest>(
		PrepareTranslateProviderRequest(
			state->provider.get(),
			peer,
			msgId,
			std::move(text)));

	TranslateBoxContent(box, {
		.text = request->text,
		.hasCopyRestriction = hasCopyRestriction,
		.textContext = Core::TextContext({ .session = &peer->session() }),
		.to = state->to.value(),
		.chooseTo = [=] {
			box->uiShow()->showBox(ChooseTranslateToBox(
				state->to.current(),
				crl::guard(box, [=](LanguageId id) { state->to = id; })));
		},
		.request = [=](
				LanguageId to,
				Fn<void(TranslateBoxContentResult)> done) {
			state->provider->request(
				*request,
				to,
				[done = std::move(done)](TranslateProviderResult result) {
					using ProviderError = TranslateProviderError;
					using UiError = TranslateBoxContentError;
					done(TranslateBoxContentResult{
						.text = std::move(result.text),
						.error = (result.error
								== ProviderError::LocalLanguagePackMissing)
							? UiError::LocalLanguagePackMissing
							: (result.error == ProviderError::None)
							? UiError::None
							: UiError::Unknown,
					});
				});
		},
	});
}

bool SkipTranslate(TextWithEntities textWithEntities) {
	const auto &text = textWithEntities.text;
	if (text.isEmpty()) {
		return true;
	}
	if (!Core::App().settings().translateButtonEnabled()) {
		return true;
	}
	constexpr auto kFirstChunk = size_t(100);
	auto hasLetters = (text.size() >= kFirstChunk);
	for (auto i = 0; i < kFirstChunk; i++) {
		if (i >= text.size()) {
			break;
		}
		if (text.at(i).isLetter()) {
			hasLetters = true;
			break;
		}
	}
	if (!hasLetters) {
		return true;
	}
#ifndef TDESKTOP_DISABLE_SPELLCHECK
	const auto result = Platform::Language::Recognize(text);
	const auto skip = Core::App().settings().skipTranslationLanguages();
	return result.known() && ranges::contains(skip, result);
#else
	return false;
#endif
}

object_ptr<BoxContent> EditSkipTranslationLanguages() {
	auto title = tr::lng_translate_settings_choose();
	const auto selected = std::make_shared<std::vector<LanguageId>>(
		Core::App().settings().skipTranslationLanguages());
	const auto weak = std::make_shared<base::weak_qptr<BoxContent>>();
	const auto check = [=](LanguageId id) {
		const auto already = ranges::contains(*selected, id);
		if (already) {
			selected->erase(ranges::remove(*selected, id), selected->end());
		} else {
			selected->push_back(id);
		}
		if (already && selected->empty()) {
			if (const auto strong = weak->get()) {
				strong->showToast(
					tr::lng_translate_settings_one(tr::now),
					kSkipAtLeastOneDuration);
			}
			return false;
		}
		return true;
	};
	auto result = Box(ChooseLanguageBox, std::move(title), [=](
			std::vector<LanguageId> &&list) {
		Core::App().settings().setSkipTranslationLanguages(
			std::move(list));
		Core::App().saveSettingsDelayed();
	}, *selected, true, check);
	*weak = result.data();
	return result;
}

object_ptr<BoxContent> ChooseTranslateToBox(
		LanguageId bringUp,
		Fn<void(LanguageId)> callback) {
	auto &settings = Core::App().settings();
	auto selected = std::vector<LanguageId>{
		settings.translateTo(),
	};
	for (const auto &id : settings.skipTranslationLanguages()) {
		if (id != selected.front()) {
			selected.push_back(id);
		}
	}
	if (bringUp && ranges::contains(selected, bringUp)) {
		selected.push_back(bringUp);
	}
	return Box(ChooseLanguageBox, tr::lng_languages(), [=](
			const std::vector<LanguageId> &ids) {
		Expects(!ids.empty());

		const auto id = ids.front();
		Core::App().settings().setTranslateTo(id);
		Core::App().saveSettingsDelayed();
		callback(id);
	}, selected, false, nullptr);
}

LanguageId ChooseTranslateTo(not_null<History*> history) {
	return ChooseTranslateTo(history->translateOfferedFrom());
}

LanguageId ChooseTranslateTo(LanguageId offeredFrom) {
	auto &settings = Core::App().settings();
	return ChooseTranslateTo(
		offeredFrom,
		settings.translateTo(),
		settings.skipTranslationLanguages());
}

LanguageId ChooseTranslateTo(
		not_null<History*> history,
		LanguageId savedTo,
		const std::vector<LanguageId> &skip) {
	return ChooseTranslateTo(history->translateOfferedFrom(), savedTo, skip);
}

LanguageId ChooseTranslateTo(
		LanguageId offeredFrom,
		LanguageId savedTo,
		const std::vector<LanguageId> &skip) {
	return (offeredFrom != savedTo) ? savedTo : skip.front();
}

} // namespace Ui
