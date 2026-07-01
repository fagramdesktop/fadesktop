/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/create_ai_box.h"

#include "apiwrap.h"
#include "base/object_ptr.h"
#include "base/weak_ptr.h"
#include "boxes/create_ai_tone_box.h"
#include "chat_helpers/compose/compose_show.h"
#include "data/data_ai_compose_tones.h"
#include "data/data_file_origin.h"
#include "data/data_msg_id.h"
#include "data/data_session.h"
#include "iv/iv_cached_media.h"
#include "iv/iv_rich_message_serializer.h"
#include "iv/iv_rich_page.h"
#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_common.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "lang/lang_keys.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "mtproto/mtproto_response.h"
#include "spellcheck/spellcheck_types.h"
#include "ui/boxes/choose_language_box.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/layers/generic_box.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "window/themes/window_theme.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"

namespace Iv::Editor {
namespace {

[[nodiscard]] LanguageId DefaultAiTranslateTo(LanguageId offeredFrom) {
	const auto current = LanguageId{
		QLocale(Lang::LanguageIdOrDefault(Lang::Id())).language()
	};
	if (current && (current != offeredFrom)) {
		return current;
	}
	const auto english = LanguageId{ QLocale::English };
	if (english != offeredFrom) {
		return english;
	}
	return LanguageId{ QLocale::Spanish };
}

[[nodiscard]] TextWithEntities SelectorTitle(LanguageId id) {
	return tr::lng_ai_compose_in_language(
		tr::now,
		lt_language,
		tr::link(Ui::LanguageName(id)),
		tr::marked);
}

enum class AiEditorMode {
	Translate,
	Style,
	Fix,
};

[[nodiscard]] QColor AiEditorColorWithAlpha(
		const style::color &color,
		float64 alpha) {
	auto result = color->c;
	result.setAlphaF(result.alphaF() * alpha);
	return result;
}

[[nodiscard]] const style::icon &AiEditorModeIcon(
		AiEditorMode mode,
		bool active) {
	switch (mode) {
	case AiEditorMode::Translate:
		return active
			? st::aiComposeTabTranslateIconActive
			: st::aiComposeTabTranslateIcon;
	case AiEditorMode::Style:
		return active
			? st::aiComposeTabStyleIconActive
			: st::aiComposeTabStyleIcon;
	case AiEditorMode::Fix:
		return active
			? st::aiComposeTabFixIconActive
			: st::aiComposeTabFixIcon;
	}
	return active
		? st::aiComposeTabTranslateIconActive
		: st::aiComposeTabTranslateIcon;
}

class AiEditorModeButton final : public Ui::RippleButton {
public:
	AiEditorModeButton(QWidget *parent, AiEditorMode mode, QString label);

	void setSelected(bool selected);
	[[nodiscard]] AiEditorMode mode() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	[[nodiscard]] QImage prepareRippleMask() const override;

private:
	const AiEditorMode _mode;
	const QString _label;
	bool _selected = false;

};

class AiEditorModeTabs final : public Ui::RpWidget {
public:
	explicit AiEditorModeTabs(QWidget *parent);

	void setActive(AiEditorMode mode);
	void setChangedCallback(Fn<void(AiEditorMode)> callback);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;

private:
	const not_null<AiEditorModeButton*> _translate;
	const not_null<AiEditorModeButton*> _style;
	const not_null<AiEditorModeButton*> _fix;
	Fn<void(AiEditorMode)> _changed;

};

AiEditorModeButton::AiEditorModeButton(
	QWidget *parent,
	AiEditorMode mode,
	QString label)
: RippleButton(parent, st::aiComposeButtonRippleInactive)
, _mode(mode)
, _label(std::move(label)) {
	setCursor(style::cur_pointer);
	setAccessibleName(_label);
}

void AiEditorModeButton::setSelected(bool selected) {
	if (_selected == selected) {
		return;
	}
	_selected = selected;
	update();
}

AiEditorMode AiEditorModeButton::mode() const {
	return _mode;
}

void AiEditorModeButton::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	const auto radius = height() / 2.;
	if (_selected) {
		p.setPen(Qt::NoPen);
		p.setBrush(AiEditorColorWithAlpha(
			st::aiComposeTabButtonBgActive,
			st::aiComposeButtonBgActiveOpacity));
		p.drawRoundedRect(rect(), radius, radius);
	}
	const auto ripple = AiEditorColorWithAlpha(
		(_selected
			? st::aiComposeButtonRippleActive
			: st::aiComposeButtonRippleInactive).color,
		_selected
			? st::aiComposeButtonRippleActiveOpacity
			: st::aiComposeButtonRippleInactiveOpacity);
	paintRipple(p, 0, 0, &ripple);

	const auto &icon = AiEditorModeIcon(_mode, _selected);
	const auto iconLeft = (width() - icon.width()) / 2;
	icon.paint(p, iconLeft, st::aiComposeTabIconTop, width());

	p.setPen(_selected
		? st::aiComposeTabLabelFgActive
		: st::aiComposeTabLabelFg);
	p.setFont(st::aiComposeTabLabelFont);
	p.drawText(
		QRect(
			0,
			st::aiComposeTabLabelTop,
			width(),
			height() - st::aiComposeTabLabelTop),
		Qt::AlignHCenter | Qt::AlignTop,
		_label);
}

QImage AiEditorModeButton::prepareRippleMask() const {
	return Ui::RippleAnimation::MaskByDrawer(size(), false, [&](QPainter &p) {
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		const auto radius = height() / 2.;
		p.drawRoundedRect(rect(), radius, radius);
	});
}

AiEditorModeTabs::AiEditorModeTabs(QWidget *parent)
: RpWidget(parent)
, _translate(Ui::CreateChild<AiEditorModeButton>(
	this,
	AiEditorMode::Translate,
	tr::lng_ai_compose_tab_translate(tr::now)))
, _style(Ui::CreateChild<AiEditorModeButton>(
	this,
	AiEditorMode::Style,
	tr::lng_ai_compose_tab_style(tr::now)))
, _fix(Ui::CreateChild<AiEditorModeButton>(
	this,
	AiEditorMode::Fix,
	tr::lng_ai_compose_tab_fix(tr::now))) {
	const auto bind = [=](not_null<AiEditorModeButton*> button) {
		button->setClickedCallback([=] {
			setActive(button->mode());
			if (_changed) {
				_changed(button->mode());
			}
		});
	};
	bind(_translate);
	bind(_style);
	bind(_fix);
	setActive(AiEditorMode::Translate);
}

void AiEditorModeTabs::setActive(AiEditorMode mode) {
	_translate->setSelected(mode == AiEditorMode::Translate);
	_style->setSelected(mode == AiEditorMode::Style);
	_fix->setSelected(mode == AiEditorMode::Fix);
}

void AiEditorModeTabs::setChangedCallback(Fn<void(AiEditorMode)> callback) {
	_changed = std::move(callback);
}

int AiEditorModeTabs::resizeGetHeight(int newWidth) {
	const auto padding = st::aiComposeTabsPadding;
	const auto skip = st::aiComposeTabsSkip;
	const auto innerWidth = newWidth - padding.left() - padding.right();
	const auto buttonWidth = (innerWidth - (2 * skip)) / 3;
	const auto buttonHeight = st::aiComposeTabsHeight
		- padding.top()
		- padding.bottom();
	const auto top = padding.top();
	auto left = padding.left();
	for (const auto &button : { _translate, _style, _fix }) {
		button->setGeometry(left, top, buttonWidth, buttonHeight);
		left += buttonWidth + skip;
	}
	return st::aiComposeTabsHeight;
}

void AiEditorModeTabs::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);
	p.setPen(Qt::NoPen);
	p.setBrush(st::aiComposeTabsBg);
	const auto radius = st::aiComposeTabsRadius;
	p.drawRoundedRect(rect(), radius, radius);
}

class ResponseIsland final : public Ui::RpWidget {
public:
	ResponseIsland(
		QWidget *parent,
		not_null<Main::Session*> session,
		std::shared_ptr<const RichPage> page,
		LanguageId language,
		bool emojify,
		Fn<void()> chooseLanguage,
		Fn<void(bool)> emojifyChanged);
	~ResponseIsland();

private:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;

	void paintArticle(Painter &p, QRect clip);
	void requestArticleRepaint(QRect articleRect);
	void detachArticleBindings();

	[[nodiscard]] int controlRowHeight() const;
	[[nodiscard]] QRect articleRect() const;

	const not_null<Main::Session*> _session;
	const object_ptr<Ui::FlatLabel> _selector;
	const object_ptr<Ui::RpWidget> _arrows;
	const object_ptr<Ui::Checkbox> _emojify;
	std::shared_ptr<Iv::Markdown::MediaRuntime> _mediaRuntime;
	Iv::Markdown::MarkdownArticle _article;
	std::unique_ptr<Ui::ChatTheme> _theme;
	std::unique_ptr<Ui::ChatStyle> _style;
	int _articleHeight = 0;
	int _paletteVersion = -1;
	bool _hasArticle = false;

};

ResponseIsland::ResponseIsland(
	QWidget *parent,
	not_null<Main::Session*> session,
	std::shared_ptr<const RichPage> page,
	LanguageId language,
	bool emojify,
	Fn<void()> chooseLanguage,
	Fn<void(bool)> emojifyChanged)
: RpWidget(parent)
, _session(session)
, _selector(this, st::aiComposeCardTitle)
, _arrows(this)
, _emojify(
	this,
	tr::lng_ai_compose_emojify(tr::now),
	st::aiComposeEmojifyCheckbox,
	std::make_unique<Ui::RoundCheckView>(st::defaultCheck, emojify))
, _article(st::messageMarkdown)
, _theme(Window::Theme::DefaultChatThemeOn(lifetime()))
, _style(std::make_unique<Ui::ChatStyle>(session->colorIndicesValue())) {
	_style->apply(_theme.get());
	_paletteVersion = _style->paletteVersion();

	_selector->setMarkedText(SelectorTitle(language));
	_selector->setClickHandlerFilter([=](const auto &...) {
		if (chooseLanguage) {
			chooseLanguage();
		}
		return false;
	});

	const auto &icon = st::createAiSelectorArrowsIcon;
	_arrows->setAttribute(Qt::WA_TransparentForMouseEvents);
	_arrows->resize(icon.size());
	_arrows->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(_arrows.data());
		st::createAiSelectorArrowsIcon.paint(p, 0, 0, _arrows->width());
	}, _arrows->lifetime());

	_emojify->checkedChanges() | rpl::on_next([=](bool checked) {
		if (emojifyChanged) {
			emojifyChanged(checked);
		}
	}, _emojify->lifetime());

	const auto weak = base::make_weak(this);
	_article.setTextRepaintCallbacks(
		[weak] {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(QRect());
			}
		},
		[weak](QRect rect) {
			if (const auto owner = weak.get()) {
				owner->requestArticleRepaint(rect);
			}
		});

	const auto richLimits = Iv::ResolveRichMessageLimits(session);
	_mediaRuntime = Iv::CreateMessageMediaRuntime(
		session,
		FullMsgId(),
		[](QString) {},
		[](QString) {},
		::Data::FileOrigin());
	auto prepared = Iv::Markdown::TryPrepareNativeInstantView({
		.richPage = page,
		.mediaRuntime = _mediaRuntime,
		.dimensionsOverride = Iv::Markdown::CaptureMarkdownPrepareDimensions(
			st::messageMarkdown),
		.tableRenderLimits
			= Iv::Markdown::PrepareTableRenderLimitsForRichMessage(richLimits),
	});
	if (prepared.supported()) {
		_article.setContent(std::move(prepared.content));
		_hasArticle = true;
	}
}

ResponseIsland::~ResponseIsland() {
	detachArticleBindings();
}

void ResponseIsland::detachArticleBindings() {
	_article.setTextRepaintCallbacks(nullptr, nullptr);
}

void ResponseIsland::requestArticleRepaint(QRect rect) {
	crl::on_main(this, [=] {
		if (rect.isEmpty()) {
			update();
		} else {
			update(rect.translated(articleRect().topLeft()));
		}
	});
}

QRect ResponseIsland::articleRect() const {
	const auto &padding = st::aiComposeCardPadding;
	const auto inner = std::max(width() - padding.left() - padding.right(), 0);
	const auto top = padding.top()
		+ controlRowHeight()
		+ st::aiComposeCardSectionSkip;
	return QRect(padding.left(), top, inner, _articleHeight);
}

void ResponseIsland::paintArticle(Painter &p, QRect clip) {
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

int ResponseIsland::controlRowHeight() const {
	return std::max(_selector->height(), _emojify->height());
}

void ResponseIsland::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	{
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::aiComposeCardBg);
		p.drawRoundedRect(
			rect(),
			st::aiComposeCardRadius,
			st::aiComposeCardRadius);
	}
	paintArticle(p, e->rect());
}

void ResponseIsland::resizeEvent(QResizeEvent *e) {
	const auto &padding = st::aiComposeCardPadding;
	const auto inner = width() - padding.left() - padding.right();
	const auto rowHeight = controlRowHeight();

	_emojify->resizeToNaturalWidth(inner);
	_emojify->moveToRight(padding.right(), padding.top(), width());

	const auto arrowsSkip = st::aiComposeCardControlSkip;
	const auto selectorWidth = std::max(
		inner
			- _emojify->width()
			- st::aiComposeCardControlSkip
			- _arrows->width()
			- arrowsSkip,
		0);
	_selector->resizeToWidth(selectorWidth);
	_selector->moveToLeft(padding.left(), padding.top());
	_arrows->moveToLeft(
		padding.left() + _selector->textMaxWidth() + arrowsSkip,
		padding.top() + (_selector->height() - _arrows->height()) / 2);

	auto y = padding.top() + rowHeight;
	if (_hasArticle && inner > 0) {
		y += st::aiComposeCardSectionSkip;
		_articleHeight = _article.resizeGetHeight(inner);
		y += _articleHeight;
	}
	y += padding.bottom();
	if (height() != y) {
		resize(width(), y);
	}
}

struct State {
	not_null<Main::Session*> session;
	Fn<void(std::shared_ptr<const RichPage>)> applyToPage;
	mtpRequestId requestId = 0;
	Ui::InputField *prompt = nullptr;
	LanguageId language;
	bool emojify = false;
	enum class Phase {
		Initial,
		Loading,
		HasResult,
	};
	Phase phase = Phase::Initial;
	std::shared_ptr<const RichPage> page;
	Ui::SlideWrap<ResponseIsland> *responseWrap = nullptr;
	Ui::RoundButton *reloadButton = nullptr;
	Ui::RoundButton *primaryButton = nullptr;
	Fn<void()> generate;
	Fn<void()> rebuildButtons;
	Fn<void()> rebuildResponseIsland;
	Fn<void()> enterLoading;
};

} // namespace

void CreateAiBox(not_null<Ui::GenericBox*> box, CreateAiBoxArgs &&args) {
	const auto state = box->lifetime().make_state<State>(State{
		.session = args.session,
		.applyToPage = std::move(args.applyToPage),
		.language = DefaultAiTranslateTo(LanguageId()),
	});

	box->setWidth(st::boxWideWidth);
	box->setTitle(tr::lng_ai_compose_create_title());
	box->addTopButton(st::aiComposeBoxClose, [=] {
		box->closeBox();
	});
	box->setStyle(st::aiComposeBox);

	const auto prompt = box->addRow(
		object_ptr<Ui::InputField>(
			box,
			st::aiTonePromptField,
			Ui::InputField::Mode::MultiLine,
			rpl::producer<QString>()),
		st::aiToneFieldsMargin);
	prompt->setSubmitSettings(Ui::InputField::SubmitSettings::None);
	prompt->setMaxLength(state->session->appConfig().get<int>(
		u"aicompose_tone_prompt_length_max"_q,
		1024));
	state->prompt = prompt;

	const auto promptPad = st::aiToneFieldPadding;
	const auto promptLineHeight = st::aiTonePromptField.style.font->height;
	const auto promptMaxLines = 7;
	prompt->setMaxHeight((promptLineHeight * promptMaxLines) / 2
		+ promptPad.top()
		+ promptPad.bottom());

	const auto promptPlaceholder = AddAiComposeFieldDecor(
		prompt,
		tr::lng_ai_compose_create_placeholder());

	promptPlaceholder->heightValue(
	) | rpl::on_next([=](int phHeight) {
		const auto pad = st::aiToneFieldPadding;
		prompt->setMinHeight(phHeight + pad.top() + pad.bottom());
	}, prompt->lifetime());

	state->prompt->changes(
	) | rpl::on_next([=] {
		if (!state->page) {
			return;
		}
		if (const auto wrap = state->responseWrap) {
			wrap->setDirectionUp(true);
			wrap->toggle(false, anim::type::normal);
			wrap->setFinishedCallback([=] {
				if (state->responseWrap == wrap && !wrap->toggled()) {
					delete state->responseWrap;
					state->responseWrap = nullptr;
				}
			});
		}
		state->page = nullptr;
		state->phase = State::Phase::Initial;
		state->rebuildButtons();
	}, prompt->lifetime());

	const auto chooseLanguage = [=] {
		box->uiShow()->showBox(Box([=](not_null<Ui::GenericBox*> chooser) {
			Ui::ChooseLanguageBox(
				chooser,
				tr::lng_languages(),
				[=](std::vector<LanguageId> ids) {
					if (ids.empty()) {
						return;
					}
					state->language = ids.front();
					state->generate();
				},
				{ state->language },
				false,
				nullptr);
		}));
	};

	state->rebuildResponseIsland = [=] {
		if (state->responseWrap) {
			delete state->responseWrap;
			state->responseWrap = nullptr;
		}
		if (!state->page) {
			return;
		}
		const auto content = box->verticalLayout();
		auto wrap = object_ptr<Ui::SlideWrap<ResponseIsland>>(
			content,
			object_ptr<ResponseIsland>(
				content,
				state->session,
				state->page,
				state->language,
				state->emojify,
				chooseLanguage,
				[=](bool checked) {
					state->emojify = checked;
					state->generate();
				}),
			style::margins(
				st::aiComposeContentMargin.left(),
				st::aiComposeCardSectionSkip,
				st::aiComposeContentMargin.right(),
				0));
		const auto ptr = wrap.data();
		content->add(std::move(wrap));
		state->responseWrap = ptr;
		ptr->toggle(false, anim::type::instant);
		ptr->toggle(true, anim::type::normal);
	};

	state->rebuildButtons = [=] {
		if (state->reloadButton) {
			// May be invoked from inside the reload button's own click
			// handler (generate() -> rebuildButtons()), so destroy it
			// without freeing the object that is still on the stack.
			state->reloadButton->hide();
			state->reloadButton->setParent(nullptr);
			state->reloadButton->deleteLater();
			state->reloadButton = nullptr;
		}
		box->clearButtons();
		state->primaryButton = nullptr;
		if (state->phase == State::Phase::HasResult) {
			box->setStyle(st::aiComposeBoxWithSend);
			const auto pill = box->addButton(
				tr::lng_ai_compose_add_to_page(),
				[=] {
					if (state->applyToPage) {
						state->applyToPage(state->page);
					}
					box->closeBox();
				});
			pill->setFullRadius(true);
			state->primaryButton = pill;

			const auto reload = Ui::CreateChild<Ui::RoundButton>(
				pill->parentWidget(),
				rpl::single(QString()),
				st::createAiReloadButton);
			reload->setFullRadius(true);
			reload->show();
			const auto icon = Ui::CreateChild<Ui::RpWidget>(reload);
			icon->setAttribute(Qt::WA_TransparentForMouseEvents);
			icon->resize(st::createAiReloadIcon.size());
			icon->show();
			icon->paintRequest() | rpl::on_next([=] {
				auto p = QPainter(icon);
				st::createAiReloadIcon.paint(p, 0, 0, icon->width());
			}, icon->lifetime());
			icon->move(
				(reload->width() - icon->width()) / 2,
				(reload->height() - icon->height()) / 2);
			pill->geometryValue() | rpl::on_next([=](QRect geometry) {
				const auto size = st::createAiReloadButton.height;
				reload->moveToLeft(
					geometry.x()
						+ geometry.width()
						+ st::aiComposeSendButtonSkip,
					geometry.y() + (geometry.height() - size) / 2);
			}, reload->lifetime());
			reload->setClickedCallback([=] {
				state->generate();
			});
			state->reloadButton = reload;
		} else {
			box->setStyle(st::aiComposeBox);
			const auto pill = box->addButton(
				tr::lng_ai_compose_generate(),
				[=] { state->generate(); });
			pill->setFullRadius(true);
			state->primaryButton = pill;
		}
	};

	state->enterLoading = [=] {
		if (state->reloadButton) {
			state->reloadButton->hide();
			state->reloadButton->setParent(nullptr);
			state->reloadButton->deleteLater();
			state->reloadButton = nullptr;
		}
		const auto pill = state->primaryButton;
		if (!pill) {
			state->rebuildButtons();
			return;
		}
		pill->setText(rpl::single(tr::lng_ai_compose_add_to_page(tr::now)));
		pill->setDisabled(true);
		pill->setAttribute(Qt::WA_TransparentForMouseEvents);
		pill->setTextFgOverride(
			anim::color(st::activeButtonBg, st::activeButtonFg, 0.5));
		pill->setClickedCallback([] {});
	};

	state->generate = [=] {
		const auto prompt = state->prompt->getLastText();
		if (prompt.trimmed().isEmpty()) {
			return;
		}
		if (state->requestId) {
			state->session->api().request(state->requestId).cancel();
			state->requestId = 0;
		}
		state->phase = State::Phase::Loading;
		state->enterLoading();

		using Flag = MTPmessages_composeRichMessageWithAI::Flag;
		auto flags = MTPmessages_composeRichMessageWithAI::Flags(0)
			| Flag::f_tone;
		if (state->emojify) {
			flags |= Flag::f_emojify;
		}
		const auto lang = state->language
			? state->language.twoLetterCode()
			: QString();
		if (!lang.isEmpty()) {
			flags |= Flag::f_translate_to_lang;
		}
		state->requestId = state->session->api().request(
			MTPmessages_ComposeRichMessageWithAI(
				MTP_flags(flags),
				MTPInputRichMessage(),
				lang.isEmpty() ? MTPstring() : MTP_string(lang),
				MTP_inputAiComposeToneSingleUse(MTP_string(prompt)))
		).done([=](const MTPmessages_ComposedRichMessageWithAI &result) {
			state->requestId = 0;
			state->page = Iv::ParseRichPage(
				state->session,
				result.data().vresult());
			state->phase = State::Phase::HasResult;
			state->rebuildResponseIsland();
			state->rebuildButtons();
			state->prompt->clearFocus();
		}).fail([=](const MTP::Error &error) {
			state->requestId = 0;
			state->phase = state->page
				? State::Phase::HasResult
				: State::Phase::Initial;
			state->rebuildButtons();
			if (MTP::IgnoreError(error)) {
				return;
			}
			box->showToast(error.type());
		}).handleFloodErrors().send();
	};

	state->rebuildButtons();
}

void ShowCreateAiBox(
		std::shared_ptr<ChatHelpers::Show> show,
		CreateAiBoxArgs &&args) {
	show->show(Box(CreateAiBox, std::move(args)));
}

namespace {

struct AiEditorState {
	not_null<Main::Session*> session;
	std::shared_ptr<const RichPage> source;
	Fn<void(std::shared_ptr<const RichPage>)> apply;
	AiEditorMode mode = AiEditorMode::Translate;
	LanguageId language;
	bool emojify = false;
	mtpRequestId requestId = 0;
	std::shared_ptr<const RichPage> result;
	enum class Phase {
		Initial,
		Loading,
		HasResult,
	};
	Phase phase = Phase::Initial;
	Ui::SlideWrap<ResponseIsland> *responseWrap = nullptr;
	Ui::RoundButton *primaryButton = nullptr;
	Fn<void()> generate;
	Fn<void()> rebuildButtons;
	Fn<void()> rebuildResponseIsland;
};

[[nodiscard]] const ::Data::AiComposeTone *DefaultStyleTone(
		not_null<Main::Session*> session) {
	auto &tones = session->data().aiComposeTones();
	for (const auto &tone : tones.list()) {
		if (tone.isDefault) {
			return &tone;
		}
	}
	tones.refresh();
	return nullptr;
}

} // namespace

void AiEditorBox(not_null<Ui::GenericBox*> box, AiEditorBoxArgs &&args) {
	const auto state = box->lifetime().make_state<AiEditorState>(AiEditorState{
		.session = args.session,
		.source = std::move(args.source),
		.apply = std::move(args.apply),
		.language = DefaultAiTranslateTo(LanguageId()),
	});

	box->setWidth(st::boxWideWidth);
	box->setTitle(tr::lng_ai_editor_title());
	box->addTopButton(st::aiComposeBoxClose, [=] {
		box->closeBox();
	});
	box->setStyle(st::aiComposeBox);

	const auto tabs = box->addRow(
		object_ptr<AiEditorModeTabs>(box),
		st::aiComposeContentMargin);

	const auto chooseLanguage = [=] {
		box->uiShow()->showBox(Box([=](not_null<Ui::GenericBox*> chooser) {
			Ui::ChooseLanguageBox(
				chooser,
				tr::lng_languages(),
				[=](std::vector<LanguageId> ids) {
					if (ids.empty()) {
						return;
					}
					state->language = ids.front();
					state->generate();
				},
				{ state->language },
				false,
				nullptr);
		}));
	};

	state->rebuildResponseIsland = [=] {
		if (state->responseWrap) {
			delete state->responseWrap;
			state->responseWrap = nullptr;
		}
		const auto page = (state->phase == AiEditorState::Phase::HasResult)
			? state->result
			: state->source;
		if (!page) {
			return;
		}
		const auto content = box->verticalLayout();
		auto wrap = object_ptr<Ui::SlideWrap<ResponseIsland>>(
			content,
			object_ptr<ResponseIsland>(
				content,
				state->session,
				page,
				state->language,
				state->emojify,
				chooseLanguage,
				[=](bool checked) {
					state->emojify = checked;
					state->generate();
				}),
			style::margins(
				st::aiComposeContentMargin.left(),
				st::aiComposeCardSectionSkip,
				st::aiComposeContentMargin.right(),
				0));
		const auto ptr = wrap.data();
		content->add(std::move(wrap));
		state->responseWrap = ptr;
		ptr->toggle(false, anim::type::instant);
		ptr->toggle(true, anim::type::normal);
	};

	state->rebuildButtons = [=] {
		box->clearButtons();
		state->primaryButton = nullptr;
		if (state->phase == AiEditorState::Phase::HasResult) {
			box->setStyle(st::aiComposeBoxWithSend);
			const auto pill = box->addButton(
				(state->mode == AiEditorMode::Style)
					? tr::lng_ai_compose_apply_style()
					: tr::lng_ai_compose_apply(),
				[=] {
					if (state->apply && state->result) {
						state->apply(state->result);
					}
					box->closeBox();
				});
			pill->setFullRadius(true);
			state->primaryButton = pill;
		} else {
			box->setStyle(st::aiComposeBox);
			const auto pill = box->addButton(
				tr::lng_ai_compose_generate(),
				[=] { state->generate(); });
			pill->setFullRadius(true);
			state->primaryButton = pill;
		}
	};

	state->generate = [=] {
		if (!state->source) {
			return;
		}
		if (state->requestId) {
			state->session->api().request(state->requestId).cancel();
			state->requestId = 0;
		}
		const auto serialized = Iv::SerializeInputRichMessage(
			state->session,
			*state->source,
			Iv::SerializeInputRichMessageMode::Draft);
		if (serialized.status != Iv::SerializeInputRichMessageStatus::Success
			|| !serialized.value) {
			return;
		}
		state->phase = AiEditorState::Phase::Loading;

		using Flag = MTPmessages_composeRichMessageWithAI::Flag;
		auto flags = MTPmessages_composeRichMessageWithAI::Flags(0)
			| Flag::f_text;
		if (state->emojify && state->mode != AiEditorMode::Fix) {
			flags |= Flag::f_emojify;
		}
		auto lang = QString();
		auto tone = MTPInputAiComposeTone();
		switch (state->mode) {
		case AiEditorMode::Translate:
			lang = state->language ? state->language.twoLetterCode() : QString();
			if (!lang.isEmpty()) {
				flags |= Flag::f_translate_to_lang;
			}
			break;
		case AiEditorMode::Style:
			if (const auto styleTone = DefaultStyleTone(state->session)) {
				tone = state->session->data().aiComposeTones().toneToMTP(
					*styleTone);
				flags |= Flag::f_tone;
			}
			break;
		case AiEditorMode::Fix:
			flags |= Flag::f_proofread;
			break;
		}

		state->requestId = state->session->api().request(
			MTPmessages_ComposeRichMessageWithAI(
				MTP_flags(flags),
				*serialized.value,
				lang.isEmpty() ? MTPstring() : MTP_string(lang),
				tone)
		).done([=](const MTPmessages_ComposedRichMessageWithAI &result) {
			state->requestId = 0;
			state->result = Iv::ParseRichPage(
				state->session,
				result.data().vresult());
			state->phase = AiEditorState::Phase::HasResult;
			state->rebuildResponseIsland();
			state->rebuildButtons();
		}).fail([=](const MTP::Error &error) {
			state->requestId = 0;
			state->phase = state->result
				? AiEditorState::Phase::HasResult
				: AiEditorState::Phase::Initial;
			state->rebuildButtons();
			if (MTP::IgnoreError(error)) {
				return;
			}
			box->showToast(error.type());
		}).handleFloodErrors().send();
	};

	tabs->setChangedCallback([=](AiEditorMode mode) {
		state->mode = mode;
		state->generate();
	});
	tabs->setActive(state->mode);

	state->rebuildResponseIsland();
	state->rebuildButtons();
}

void ShowAiEditorBox(
		std::shared_ptr<ChatHelpers::Show> show,
		AiEditorBoxArgs &&args) {
	show->show(Box(AiEditorBox, std::move(args)));
}

} // namespace Iv::Editor
