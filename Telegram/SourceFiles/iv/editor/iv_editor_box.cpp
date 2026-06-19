/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/editor/iv_editor_box.h"

#include <QtCore/QDir>

#include "base/algorithm.h"
#include "base/event_filter.h"
#include "base/unique_qptr.h"
#include "base/weak_qptr.h"
#include "boxes/premium_preview_box.h"
#include "chat_helpers/compose/compose_show.h"
#include "data/data_file_origin.h"
#include "data/data_msg_id.h"
#include "data/data_types.h"
#include "data/data_emoji_statuses.h"
#include "history/history_item.h"
#include "main/main_session.h"
#include "ui/emoji_config.h"
#include "ui/painter.h"
#include "data/data_document.h"
#include "data/stickers/data_custom_emoji.h"
#include "data/stickers/data_stickers.h"
#include "chat_helpers/tabbed_selector.h"
#include "iv/editor/iv_editor_state.h"
#include "iv/editor/iv_editor_widget.h"
#include "iv/editor/iv_editor_window.h"
#include "lang/lang_keys.h"
#include "menu/menu_send_details.h"
#include "ui/boxes/confirm_box.h"
#include "ui/delayed_activation.h"
#include "ui/rect_part.h"
#include "ui/rp_widget.h"
#include "ui/wrap/fade_wrap.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/scroll_area.h"
#include "ui/widgets/tooltip.h"

#include <crl/crl_on_main.h>
#include <rpl/never.h>

#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QScreen>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include "styles/style_chat_helpers.h"
#include "styles/style_basic.h"
#include "styles/style_editor.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_widgets.h"

namespace Iv::Editor {
namespace {

enum class ToolbarGroupId : uchar {
	UndoRedo,
	TextModifiers,
	HeadingLinkMath,
	BlockInsertion,
	Lists,
	Insertions,
};

enum class ToolbarActionId : uchar {
	Undo,
	Redo,
	Bold,
	Italic,
	Underline,
	StrikeOut,
	Spoiler,
	Subscript,
	Superscript,
	Marked,
	PlainText,
	Heading,
	Link,
	Math,
	Blockquote,
	Pullquote,
	CodeBlock,
	Details,
	OrderedList,
	BulletList,
	TaskList,
	Attach,
	Table,
	Location,
	Divider,
};

[[nodiscard]] auto SimpleTooltipFactory(QString text) {
	return [text = std::move(text)] {
		return rpl::single(TextWithEntities::Simple(text));
	};
}

[[nodiscard]] QString ToolbarActionLabel(
		ToolbarActionId action,
		Widget::ToolbarLinkMode linkMode
			= Widget::ToolbarLinkMode::Create) {
	switch (action) {
	case ToolbarActionId::Undo:
		return tr::lng_wnd_menu_undo(tr::now);
	case ToolbarActionId::Redo:
		return tr::lng_wnd_menu_redo(tr::now);
	case ToolbarActionId::Bold:
		return tr::lng_menu_formatting_bold(tr::now);
	case ToolbarActionId::Italic:
		return tr::lng_menu_formatting_italic(tr::now);
	case ToolbarActionId::Underline:
		return tr::lng_menu_formatting_underline(tr::now);
	case ToolbarActionId::StrikeOut:
		return tr::lng_menu_formatting_strike_out(tr::now);
	case ToolbarActionId::Spoiler:
		return tr::lng_menu_formatting_spoiler(tr::now);
	case ToolbarActionId::Subscript:
		return tr::lng_menu_formatting_subscript(tr::now);
	case ToolbarActionId::Superscript:
		return tr::lng_menu_formatting_superscript(tr::now);
	case ToolbarActionId::Marked:
		return tr::lng_menu_formatting_marked(tr::now);
	case ToolbarActionId::PlainText:
		return tr::lng_menu_formatting_plain_text(tr::now);
	case ToolbarActionId::Heading:
		return tr::lng_article_insert_heading(tr::now);
	case ToolbarActionId::Link:
		return (linkMode == Widget::ToolbarLinkMode::Edit)
			? tr::lng_menu_formatting_link_edit(tr::now)
			: tr::lng_menu_formatting_link_create(tr::now);
	case ToolbarActionId::Math:
		return tr::lng_article_insert_math(tr::now);
	case ToolbarActionId::Blockquote:
		return tr::lng_article_insert_blockquote(tr::now);
	case ToolbarActionId::Pullquote:
		return tr::lng_article_insert_pullquote(tr::now);
	case ToolbarActionId::CodeBlock:
		return tr::lng_article_insert_code(tr::now);
	case ToolbarActionId::Details:
		return tr::lng_article_insert_details(tr::now);
	case ToolbarActionId::OrderedList:
		return tr::lng_article_insert_ordered_list(tr::now);
	case ToolbarActionId::BulletList:
		return tr::lng_article_insert_bullet_list(tr::now);
	case ToolbarActionId::TaskList:
		return tr::lng_article_insert_task_list(tr::now);
	case ToolbarActionId::Attach:
		return tr::lng_article_insert_media(tr::now);
	case ToolbarActionId::Table:
		return tr::lng_article_insert_table(tr::now);
	case ToolbarActionId::Location:
		return tr::lng_article_insert_map(tr::now);
	case ToolbarActionId::Divider:
		return tr::lng_article_insert_divider(tr::now);
	}
	return QString();
}

[[nodiscard]] QString OverflowLabel() {
	return tr::lng_profile_action_short_more(tr::now);
}

[[nodiscard]] const style::icon *HeadingIcon(int level) {
	switch (level) {
	case 1: return &st::ivEditorToolbarHeading1Icon;
	case 2: return &st::ivEditorToolbarHeading2Icon;
	case 3: return &st::ivEditorToolbarHeading3Icon;
	case 4: return &st::ivEditorToolbarHeading4Icon;
	case 5: return &st::ivEditorToolbarHeading5Icon;
	case 6: return &st::ivEditorToolbarHeading6Icon;
	}
	return &st::ivEditorToolbarHeadingIcon;
}

[[nodiscard]] std::optional<Widget::ToolbarFormatAction> ToolbarFormatAction(
		ToolbarActionId action) {
	switch (action) {
	case ToolbarActionId::Undo:
		return Widget::ToolbarFormatAction::Undo;
	case ToolbarActionId::Redo:
		return Widget::ToolbarFormatAction::Redo;
	case ToolbarActionId::Bold:
		return Widget::ToolbarFormatAction::Bold;
	case ToolbarActionId::Italic:
		return Widget::ToolbarFormatAction::Italic;
	case ToolbarActionId::Underline:
		return Widget::ToolbarFormatAction::Underline;
	case ToolbarActionId::StrikeOut:
		return Widget::ToolbarFormatAction::StrikeOut;
	case ToolbarActionId::Spoiler:
		return Widget::ToolbarFormatAction::Spoiler;
	case ToolbarActionId::Subscript:
		return Widget::ToolbarFormatAction::Subscript;
	case ToolbarActionId::Superscript:
		return Widget::ToolbarFormatAction::Superscript;
	case ToolbarActionId::Marked:
		return Widget::ToolbarFormatAction::Marked;
	case ToolbarActionId::PlainText:
		return Widget::ToolbarFormatAction::PlainText;
	case ToolbarActionId::Link:
		return Widget::ToolbarFormatAction::Link;
	case ToolbarActionId::Math:
		return Widget::ToolbarFormatAction::Math;
	case ToolbarActionId::Heading:
	case ToolbarActionId::Blockquote:
	case ToolbarActionId::Pullquote:
	case ToolbarActionId::CodeBlock:
	case ToolbarActionId::Details:
	case ToolbarActionId::OrderedList:
	case ToolbarActionId::BulletList:
	case ToolbarActionId::TaskList:
	case ToolbarActionId::Attach:
	case ToolbarActionId::Table:
	case ToolbarActionId::Location:
	case ToolbarActionId::Divider:
		return std::nullopt;
	}
	return std::nullopt;
}

struct ToolbarButton {
	ToolbarActionId action = ToolbarActionId::Undo;
	ToolbarGroupId group = ToolbarGroupId::UndoRedo;
	QString label;
	Fn<rpl::producer<TextWithEntities>()> tooltipFactory;
	const style::icon *icon = nullptr;
	const style::icon *iconOver = nullptr;
	Fn<void()> callback;
	object_ptr<Ui::RippleButton> widget = { nullptr };
	object_ptr<Ui::FadeWrapScaled<Ui::IconButton>> wrap = { nullptr };
	Ui::RippleButton *button = nullptr;
	Ui::IconButton *iconButton = nullptr;
	bool stateInitialized = false;
	bool shown = false;
	bool enabled = false;

	[[nodiscard]] Ui::RpWidget *host() const {
		return wrap
			? static_cast<Ui::RpWidget*>(wrap.data())
			: static_cast<Ui::RpWidget*>(widget.data());
	}
};

class Toolbar final : public Ui::RpWidget {
public:
	Toolbar(
		QWidget *parent,
		not_null<Widget*> editor,
		QPointer<QWidget> tooltipParent,
		bool hasRequestMedia,
		Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> requestMap,
		Fn<void()> toggleEmoji);

	int resizeGetHeight(int width) override;
	void hideShownTooltip();

protected:
	bool eventFilter(QObject *object, QEvent *event) override;
	void paintEvent(QPaintEvent *e) override;

private:
	not_null<Ui::RippleButton*> addAction(
		ToolbarActionId action,
		ToolbarGroupId group,
		const style::icon *icon,
		const style::icon *iconOver,
		Fn<void()> callback,
		bool wrapped = false);
	void addButtons();
	void addEmojiButton();
	void fillHeadingMenu(not_null<Ui::PopupMenu*> menu);
	void showHeadingMenu(not_null<Ui::IconButton*> button);
	void showOverflowMenu(not_null<Ui::IconButton*> button);
	void showTooltip(not_null<Ui::RippleButton*> button);
	void hideTooltip();
	void updateTooltipGeometry();
	void refreshButtonText(ToolbarButton &button);
	void updateFromEditorState();
	void updateOverflowButtonState();
	[[nodiscard]] Widget::ToolbarActionState actionState(
		const ToolbarButton &button) const;
	[[nodiscard]] ToolbarButton *buttonData(not_null<Ui::RippleButton*> button);

	const QPointer<Widget> _editor;
	const QPointer<QWidget> _tooltipParent;
	const bool _hasRequestMedia = false;
	const Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> _requestMap;
	const Fn<void()> _toggleEmoji;
	Widget::ToolbarState _toolbarState = {};
	std::vector<ToolbarButton> _buttons;
	ToolbarButton _emoji;
	object_ptr<Ui::IconButton> _overflow = { nullptr };
	std::vector<int> _overflowActions;
	std::vector<int> _dividerXs;
	base::unique_qptr<Ui::ImportantTooltip> _tooltip;
	base::unique_qptr<Ui::PopupMenu> _menu;
	Ui::RippleButton *_hovered = nullptr;
	bool _overflowStateInitialized = false;
	bool _overflowEnabled = false;

};

[[nodiscard]] QRect DefaultWindowGeometry() {
	const auto padding = st::ivEditorBodyPadding;
	const auto size = QSize(
		std::max(
			st::ivEditorWindowMinSize.width(),
			(st::defaultMarkdown.pageMaxWidth * 3) / 4
				+ padding.left()
				+ padding.right()),
		st::ivEditorWindowDefaultSize.height());
	auto result = QRect(QPoint(), size);
	if (const auto screen = QGuiApplication::primaryScreen()) {
		result.moveCenter(screen->availableGeometry().center());
	}
	return result;
}

[[nodiscard]] int MaximalExtendBy(not_null<Window*> window) {
	const auto screen = window->screen()
		? window->screen()
		: QGuiApplication::primaryScreen();
	if (!screen) {
		return 0;
	}
	const auto desktop = screen->availableGeometry();
	return std::max(desktop.width() - window->body()->width(), 0);
}

[[nodiscard]] bool CanExtendNoMove(
		not_null<Window*> window,
		int extendBy) {
	const auto screen = window->screen()
		? window->screen()
		: QGuiApplication::primaryScreen();
	if (!screen) {
		return false;
	}
	const auto desktop = screen->availableGeometry();
	const auto inner = window->body()->mapToGlobal(window->body()->rect());
	const auto innerRight = inner.x() + inner.width() + extendBy;
	const auto desktopRight = desktop.x() + desktop.width();
	return innerRight <= desktopRight;
}

int TryToExtendWidthBy(not_null<Window*> window, int addToWidth) {
	const auto screen = window->screen()
		? window->screen()
		: QGuiApplication::primaryScreen();
	if (!screen) {
		return 0;
	}
	const auto desktop = screen->availableGeometry();
	const auto inner = window->body()->mapToGlobal(window->body()->rect());
	accumulate_min(addToWidth, MaximalExtendBy(window));
	const auto newWidth = inner.width() + addToWidth;
	const auto newLeft = std::min(
		inner.x(),
		desktop.x() + desktop.width() - newWidth);
	if (inner.x() != newLeft || inner.width() != newWidth) {
		window->setGeometry(QRect(newLeft, inner.y(), newWidth, inner.height()));
	}
	return addToWidth;
}

[[nodiscard]] QString HeadingLabel(int level) {
	switch (level) {
	case 1: return tr::lng_article_insert_heading1(tr::now);
	case 2: return tr::lng_article_insert_heading2(tr::now);
	case 3: return tr::lng_article_insert_heading3(tr::now);
	case 4: return tr::lng_article_insert_heading4(tr::now);
	case 5: return tr::lng_article_insert_heading5(tr::now);
	case 6: return tr::lng_article_insert_heading6(tr::now);
	}
	return tr::lng_article_insert_heading1(tr::now);
}

[[nodiscard]] QString SubmitText(const ShowWindowDescriptor &descriptor) {
	if (!descriptor.submitLabel.isEmpty()) {
		return descriptor.submitLabel;
	}
	switch (descriptor.submitType) {
	case ShowWindowDescriptor::SubmitType::Send:
		return tr::lng_send_button(tr::now);
	case ShowWindowDescriptor::SubmitType::Save:
		return tr::lng_settings_save(tr::now);
	}
	return tr::lng_send_button(tr::now);
}

[[nodiscard]] bool IsEmojiDocument(not_null<DocumentData*> document) {
	const auto info = document->sticker();
	return info && (info->setType == Data::StickersType::Emoji);
}

class WindowContext final : public ChatHelpers::Show {
public:
	WindowContext(
		not_null<Window*> window,
		not_null<Main::Session*> session)
	: _window(window.get())
	, _session(session) {
	}

	void activate() override {
		if (const auto window = _window.data()) {
			Ui::ActivateWindow(window);
		}
	}

	void showOrHideBoxOrLayer(
			std::variant<
				v::null_t,
				object_ptr<Ui::BoxContent>,
				std::unique_ptr<Ui::LayerWidget>> &&layer,
			Ui::LayerOptions options,
			anim::type animated) const override {
		using ObjectBox = object_ptr<Ui::BoxContent>;
		using UniqueLayer = std::unique_ptr<Ui::LayerWidget>;
		const auto window = _window.data();
		if (!window) {
			return;
		} else if (auto layerWidget = std::get_if<UniqueLayer>(&layer)) {
			window->showLayer(std::move(*layerWidget), options, animated);
		} else if (auto box = std::get_if<ObjectBox>(&layer)) {
			window->showBox(std::move(*box), options, animated);
		} else {
			window->hideLayer(animated);
		}
	}

	not_null<QWidget*> toastParent() const override {
		const auto window = _window.data();
		Assert(window != nullptr);
		return window->body();
	}

	bool valid() const override {
		return !_window.isNull();
	}

	operator bool() const override {
		return valid();
	}

	Main::Session &session() const override {
		return *_session;
	}

	bool paused(ChatHelpers::PauseReason reason) const override {
		const auto window = _window.data();
		if (!window
			|| window->isHidden()
			|| !window->isActiveWindow()) {
			return true;
		} else if (reason < ChatHelpers::PauseReason::RoundPlaying
			&& window->isLayerShown()) {
			return true;
		}
		return false;
	}

	rpl::producer<> pauseChanged() const override {
		return rpl::never<>();
	}

	rpl::producer<bool> adjustShadowLeft() const override {
		return rpl::single(false);
	}

	SendMenu::Details sendMenuDetails() const override {
		return { SendMenu::Type::Disabled };
	}

	bool showMediaPreview(
			Data::FileOrigin,
			not_null<DocumentData*>) const override {
		return false;
	}

	bool showMediaPreview(
			Data::FileOrigin,
			not_null<PhotoData*>) const override {
		return false;
	}

	void processChosenSticker(
			ChatHelpers::FileChosen &&) const override {
	}

	::Window::SessionController *resolveWindow() const override {
		return nullptr;
	}

private:
	const QPointer<Window> _window;
	const not_null<Main::Session*> _session;

};

Toolbar::Toolbar(
	QWidget *parent,
	not_null<Widget*> editor,
	QPointer<QWidget> tooltipParent,
	bool hasRequestMedia,
	Fn<void(not_null<Widget*>, QPointer<QWidget>, rpl::producer<>)> requestMap,
	Fn<void()> toggleEmoji)
: Ui::RpWidget(parent)
, _editor(editor.get())
, _tooltipParent(std::move(tooltipParent))
, _hasRequestMedia(hasRequestMedia)
, _requestMap(std::move(requestMap))
, _toggleEmoji(std::move(toggleEmoji)) {
	setMouseTracking(true);
	_buttons.reserve(25);
	addButtons();
	addEmojiButton();
	_overflow = object_ptr<Ui::IconButton>(this, st::ivEditorToolbarOverflowButton);
	_overflow->setAccessibleName(OverflowLabel());
	_overflow->setIsMenuButton(true);
	_overflow->setClickedCallback([=] {
		hideTooltip();
		showOverflowMenu(not_null<Ui::IconButton*>(_overflow.data()));
	});
	_overflow->hide();
	_toolbarState = _editor ? _editor->toolbarStateValue() : Widget::ToolbarState();
	if (_editor) {
		_editor->toolbarStateChanges() | rpl::on_next([=](const Widget::ToolbarState &state) {
			_toolbarState = state;
			updateFromEditorState();
		}, lifetime());
	}
	updateFromEditorState();
}

not_null<Ui::RippleButton*> Toolbar::addAction(
		ToolbarActionId action,
		ToolbarGroupId group,
		const style::icon *icon,
		const style::icon *iconOver,
		Fn<void()> callback,
		bool wrapped) {
	auto data = ToolbarButton{
		.action = action,
		.group = group,
		.label = ToolbarActionLabel(action),
		.tooltipFactory = SimpleTooltipFactory(ToolbarActionLabel(action)),
		.icon = icon,
		.iconOver = iconOver,
		.callback = std::move(callback),
	};
	Assert(icon != nullptr);
	auto button = object_ptr<Ui::IconButton>(this, st::ivEditorToolbarButton);
	data.iconButton = button.data();
	data.button = button.data();
	data.iconButton->setIconOverride(icon, iconOver ? iconOver : icon);
	if (wrapped) {
		data.wrap = object_ptr<Ui::FadeWrapScaled<Ui::IconButton>>(
			this,
			std::move(button));
		data.wrap->toggle(false, anim::type::instant);
	} else {
		data.widget = std::move(button);
	}
	const auto raw = not_null<Ui::RippleButton*>(data.button);
	raw->setAccessibleName(data.label);
	raw->setClickedCallback([=] {
		if (const auto stored = buttonData(raw)) {
			if (stored->callback) {
				stored->callback();
			}
		}
	});
	raw->installEventFilter(this);
	_buttons.push_back(std::move(data));
	refreshButtonText(_buttons.back());
	return raw;
}

void Toolbar::addButtons() {
	const auto insert = [=](State::InsertAction action) {
		if (_editor) {
			_editor->insertBlock(action);
		}
	};
	const auto insertType = [=](State::InsertBlockType type) {
		insert({ .type = type });
	};
	const auto addEditorAction = [=](
			ToolbarActionId action,
			ToolbarGroupId group,
			const style::icon *icon = nullptr,
			const style::icon *iconOver = nullptr,
			bool wrapped = false) {
		return addAction(
			action,
			group,
			icon,
			iconOver,
			[=] {
				if (_editor) {
					if (const auto mapped = ToolbarFormatAction(action)) {
						_editor->applyToolbarFormatAction(*mapped);
					}
				}
			},
			wrapped);
	};
	const auto undo = addAction(
		ToolbarActionId::Undo,
		ToolbarGroupId::UndoRedo,
		&st::ivEditorToolbarUndoIcon,
		&st::ivEditorToolbarUndoIconOver,
		[=] {
			if (_editor) {
				_editor->performToolbarUndoRedo(false);
			}
		});
	undo->setAccessibleName(ToolbarActionLabel(ToolbarActionId::Undo));
	addAction(
		ToolbarActionId::Redo,
		ToolbarGroupId::UndoRedo,
		&st::ivEditorToolbarRedoIcon,
		&st::ivEditorToolbarRedoIconOver,
		[=] {
			if (_editor) {
				_editor->performToolbarUndoRedo(true);
			}
		},
		true);
	addEditorAction(
		ToolbarActionId::Bold,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarBoldIcon,
		&st::ivEditorToolbarBoldIcon);
	addEditorAction(
		ToolbarActionId::Italic,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarItalicIcon,
		&st::ivEditorToolbarItalicIcon);
	addEditorAction(
		ToolbarActionId::Underline,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarUnderlineIcon,
		&st::ivEditorToolbarUnderlineIcon);
	addEditorAction(
		ToolbarActionId::StrikeOut,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarStrikeOutIcon,
		&st::ivEditorToolbarStrikeOutIcon);
	addEditorAction(
		ToolbarActionId::Spoiler,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarSpoilerIcon,
		&st::ivEditorToolbarSpoilerIcon);
	addEditorAction(
		ToolbarActionId::Subscript,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarSubscriptIcon,
		&st::ivEditorToolbarSubscriptIcon);
	addEditorAction(
		ToolbarActionId::Superscript,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarSuperscriptIcon,
		&st::ivEditorToolbarSuperscriptIcon);
	addEditorAction(
		ToolbarActionId::Marked,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarMarkedIcon,
		&st::ivEditorToolbarMarkedIcon);
	addEditorAction(
		ToolbarActionId::PlainText,
		ToolbarGroupId::TextModifiers,
		&st::ivEditorToolbarPlainTextIcon,
		&st::ivEditorToolbarPlainTextIcon);
	const auto heading = addAction(
		ToolbarActionId::Heading,
		ToolbarGroupId::HeadingLinkMath,
		&st::ivEditorToolbarHeadingIcon,
		&st::ivEditorToolbarHeadingIcon,
		[] {});
	if (const auto data = buttonData(heading)) {
		data->callback = [=] {
			showHeadingMenu(not_null<Ui::IconButton*>(
				static_cast<Ui::IconButton*>(heading.get())));
		};
	}
	static_cast<Ui::IconButton*>(heading.get())->setIsMenuButton(true);
	heading->setClickedCallback([=] {
		hideTooltip();
		if (const auto data = buttonData(heading)) {
			if (data->callback) {
				data->callback();
			}
		}
	});
	addAction(
		ToolbarActionId::Link,
		ToolbarGroupId::HeadingLinkMath,
		&st::ivEditorToolbarLinkIcon,
		&st::ivEditorToolbarLinkIcon,
		[=] {
			if (_editor) {
				_editor->editLinkFromToolbar();
			}
		});
	addAction(
		ToolbarActionId::Math,
		ToolbarGroupId::HeadingLinkMath,
		&st::ivEditorToolbarMathIcon,
		&st::ivEditorToolbarMathIcon,
		[=] {
			if (_editor) {
				_editor->editMathFromToolbar();
			}
		});
	addAction(
		ToolbarActionId::Blockquote,
		ToolbarGroupId::BlockInsertion,
		&st::ivEditorToolbarBlockquoteIcon,
		&st::ivEditorToolbarBlockquoteIcon,
		[=] { insertType(State::InsertBlockType::Blockquote); });
	addAction(
		ToolbarActionId::Pullquote,
		ToolbarGroupId::BlockInsertion,
		&st::ivEditorToolbarPullquoteIcon,
		&st::ivEditorToolbarPullquoteIcon,
		[=] { insertType(State::InsertBlockType::Pullquote); });
	addAction(
		ToolbarActionId::CodeBlock,
		ToolbarGroupId::BlockInsertion,
		&st::ivEditorToolbarCodeIcon,
		&st::ivEditorToolbarCodeIcon,
		[=] { insertType(State::InsertBlockType::Code); });
	addAction(
		ToolbarActionId::Details,
		ToolbarGroupId::BlockInsertion,
		&st::ivEditorToolbarDetailsIcon,
		&st::ivEditorToolbarDetailsIcon,
		[=] { insertType(State::InsertBlockType::Details); });
	addAction(
		ToolbarActionId::OrderedList,
		ToolbarGroupId::Lists,
		&st::ivEditorToolbarOrderedListIcon,
		&st::ivEditorToolbarOrderedListIcon,
		[=] { insertType(State::InsertBlockType::OrderedList); });
	addAction(
		ToolbarActionId::BulletList,
		ToolbarGroupId::Lists,
		&st::ivEditorToolbarBulletListIcon,
		&st::ivEditorToolbarBulletListIcon,
		[=] { insertType(State::InsertBlockType::BulletList); });
	addAction(
		ToolbarActionId::TaskList,
		ToolbarGroupId::Lists,
		&st::ivEditorToolbarTaskListIcon,
		&st::ivEditorToolbarTaskListIcon,
		[=] { insertType(State::InsertBlockType::TaskList); });
	if (_hasRequestMedia) {
		addAction(
			ToolbarActionId::Attach,
			ToolbarGroupId::Insertions,
			&st::ivEditorToolbarAttachIcon,
			&st::ivEditorToolbarAttachIcon,
			[=] {
				if (_editor) {
					_editor->requestMedia(std::nullopt);
				}
			});
	}
	addAction(
		ToolbarActionId::Table,
		ToolbarGroupId::Insertions,
		&st::ivEditorToolbarTableIcon,
		&st::ivEditorToolbarTableIcon,
		[=] { insertType(State::InsertBlockType::Table); });
	if (_requestMap) {
		addAction(
			ToolbarActionId::Location,
			ToolbarGroupId::Insertions,
			&st::ivEditorToolbarLocationIcon,
			&st::ivEditorToolbarLocationIcon,
			[=] {
				if (_editor) {
					const auto parent = _tooltipParent;
					auto closeRequests = parent
						? static_cast<Ui::RpWidget*>(parent.data())->death()
						: rpl::never<>();
					_requestMap(
						not_null<Widget*>(_editor.data()),
						parent,
						std::move(closeRequests));
				}
			});
	}
	addAction(
		ToolbarActionId::Divider,
		ToolbarGroupId::Insertions,
		&st::ivEditorToolbarDividerIcon,
		&st::ivEditorToolbarDividerIcon,
		[=] { insertType(State::InsertBlockType::Divider); });
}

void Toolbar::addEmojiButton() {
	auto data = ToolbarButton{
		.action = ToolbarActionId::Attach,
		.group = ToolbarGroupId::Insertions,
		.label = tr::lng_article_insert_emoji(tr::now),
		.tooltipFactory = SimpleTooltipFactory(tr::lng_article_insert_emoji(tr::now)),
		.icon = &st::ivEditorToolbarEmojiIcon,
		.iconOver = &st::ivEditorToolbarEmojiIcon,
		.callback = [=] {
			if (_toggleEmoji) {
				_toggleEmoji();
			}
		},
	};
	auto button = object_ptr<Ui::IconButton>(this, st::ivEditorToolbarButton);
	data.iconButton = button.data();
	data.button = button.data();
	data.widget = std::move(button);
	data.iconButton->setIconOverride(data.icon, data.iconOver);
	data.button->setAccessibleName(data.label);
	data.button->setClickedCallback([=] {
		if (_emoji.callback) {
			_emoji.callback();
		}
	});
	data.button->installEventFilter(this);
	_emoji = std::move(data);
}

void Toolbar::fillHeadingMenu(not_null<Ui::PopupMenu*> menu) {
	for (const auto level : std::array{ 1, 2, 3, 4, 5, 6 }) {
		const auto icon = HeadingIcon(level);
		menu->addAction(
			HeadingLabel(level),
			[=] {
				if (_editor) {
					_editor->insertBlock({
						.type = State::InsertBlockType::Heading,
						.headingLevel = level,
					});
				}
			},
			icon,
			icon);
	}
}

void Toolbar::showHeadingMenu(not_null<Ui::IconButton*> button) {
	if (_menu) {
		return;
	}
	_menu = base::make_unique_q<Ui::PopupMenu>(this, st::popupMenuWithIcons);
	fillHeadingMenu(not_null<Ui::PopupMenu*>(_menu.get()));
	_menu->popup(button->mapToGlobal(QPoint(0, button->height())));
}

void Toolbar::showOverflowMenu(not_null<Ui::IconButton*> button) {
	if (_menu || _overflowActions.empty()) {
		return;
	}
	_menu = base::make_unique_q<Ui::PopupMenu>(this, st::popupMenuWithIcons);
	updateOverflowButtonState();
	const auto self = QPointer<Toolbar>(this);
	_menu->setDestroyedCallback([=] {
		if (!self) {
			return;
		}
		self->updateOverflowButtonState();
	});
	auto previousGroup = std::optional<ToolbarGroupId>();
	for (const auto index : _overflowActions) {
		const auto &data = _buttons[index];
		if (previousGroup && (*previousGroup != data.group)) {
			_menu->addSeparator();
		}
		if (data.action == ToolbarActionId::Heading) {
			auto submenu = std::make_unique<Ui::PopupMenu>(
				this,
				st::popupMenuWithIcons);
			fillHeadingMenu(not_null<Ui::PopupMenu*>(submenu.get()));
			_menu->addAction(
				data.label,
				std::move(submenu),
				data.icon,
				data.iconOver);
		} else {
			const auto callback = data.callback;
			const auto action = _menu->addAction(
				data.label,
				[callback] {
					if (callback) {
						callback();
					}
				},
				data.icon,
				data.iconOver);
			const auto state = actionState(data);
			action->setEnabled(state.enabled);
			action->setCheckable(state.active);
			action->setChecked(state.active);
		}
		previousGroup = data.group;
	}
	_menu->setForcedOrigin(Ui::PanelAnimation::Origin::TopRight);
	_menu->popup(button->mapToGlobal(QPoint(0, button->height())));
}

Widget::ToolbarActionState Toolbar::actionState(
		const ToolbarButton &button) const {
	if (const auto mapped = ToolbarFormatAction(button.action)) {
		return _toolbarState[*mapped];
	}
	return {
		.shown = true,
		.enabled = true,
		.active = false,
	};
}

void Toolbar::refreshButtonText(ToolbarButton &button) {
	const auto label = ToolbarActionLabel(button.action, _toolbarState.linkMode);
	if (button.label == label) {
		return;
	}
	if (_hovered == button.button) {
		hideTooltip();
	}
	button.label = label;
	button.tooltipFactory = SimpleTooltipFactory(label);
	if (button.button) {
		button.button->setAccessibleName(label);
	}
}

void Toolbar::updateFromEditorState() {
	auto shownChanged = false;
	for (auto &button : _buttons) {
		refreshButtonText(button);
		const auto state = actionState(button);
		const auto initialized = button.stateInitialized;
		if (!initialized || button.shown != state.shown) {
			if (button.wrap) {
				button.wrap->toggle(
					state.shown,
					initialized ? anim::type::normal : anim::type::instant);
			}
			button.shown = state.shown;
			shownChanged = true;
		}
		if (!initialized || button.enabled != state.enabled) {
			button.button->setPointerCursor(state.enabled);
			button.button->setAttribute(
				Qt::WA_TransparentForMouseEvents,
				!state.enabled);
			if (state.enabled) {
				button.button->setDisabled(false);
			} else {
				button.button->clearState();
				button.button->setDisabled(true);
			}
			if (button.iconButton) {
				button.iconButton->setIconColorOverride(
					state.enabled
						? std::nullopt
						: std::make_optional(st::windowSubTextFg->c));
			}
			button.enabled = state.enabled;
		}
		button.stateInitialized = true;
	}
	if (shownChanged && width() > 0) {
		resizeGetHeight(width());
	} else {
		updateOverflowButtonState();
	}
}

int Toolbar::resizeGetHeight(int width) {
	const auto padding = st::ivEditorToolbarPadding;
	const auto buttonSkip = st::ivEditorToolbarButtonSkip;
	const auto dividerWidth = st::lineWidth;
	const auto dividerSkip = st::ivEditorToolbarDividerSkip;
	const auto emojiSkip = st::ivEditorToolbarEmojiSkip;
	const auto separatorWidth = dividerSkip + dividerWidth + dividerSkip;
	const auto top = padding.top();
	const auto buttonHeight = st::ivEditorToolbarButton.height;
	for (auto &button : _buttons) {
		if (const auto host = button.host()) {
			host->hide();
		}
	}
	if (_overflow) {
		_overflow->hide();
	}
	_dividerXs.clear();
	_overflowActions.clear();
	const auto emojiHost = _emoji.host();
	const auto emojiWidth = emojiHost ? emojiHost->width() : 0;
	const auto emojiLeft = emojiHost
		? std::max(width - padding.right() - emojiWidth, padding.left())
		: width - padding.right();
	if (emojiHost) {
		emojiHost->moveToLeft(emojiLeft, top, width);
		emojiHost->show();
	}
	const auto available = std::max(
		emojiLeft - padding.left() - (emojiHost ? emojiSkip : 0),
		0);
	auto groups = std::vector<std::vector<int>>();
	for (auto i = 0, count = int(_buttons.size()); i != count; ++i) {
		const auto state = actionState(_buttons[i]);
		if (!state.shown) {
			continue;
		}
		if (groups.empty()
			|| (_buttons[groups.back().back()].group != _buttons[i].group)) {
			groups.push_back({ i });
		} else {
			groups.back().push_back(i);
		}
	}
	const auto widthForAction = [&](int index) {
		const auto host = _buttons[index].host();
		return host ? host->width() : st::ivEditorToolbarButton.width;
	};
	const auto widthWithOverflow = [&](
			const std::vector<int> &visible,
			int overflowStart) {
		auto result = 0;
		auto previous = std::optional<ToolbarGroupId>();
		const auto addWidth = [&](ToolbarGroupId group, int itemWidth) {
			if (previous) {
				result += (*previous == group) ? buttonSkip : separatorWidth;
			}
			result += itemWidth;
			previous = group;
		};
		for (const auto index : visible) {
			addWidth(_buttons[index].group, widthForAction(index));
		}
		if (overflowStart >= 0 && _overflow) {
			addWidth(_buttons[overflowStart].group, _overflow->width());
		}
		return result;
	};
	auto visible = std::vector<int>();
	auto overflowStart = -1;
	auto used = 0;
	auto previous = std::optional<ToolbarGroupId>();
	auto done = false;
	for (const auto &group : groups) {
		for (const auto index : group) {
			const auto extra = previous
				? (*previous == _buttons[index].group)
					? buttonSkip
					: separatorWidth
				: 0;
			const auto needed = extra + widthForAction(index);
			if (used + needed > available) {
				done = true;
				break;
			}
			used += needed;
			visible.push_back(index);
			previous = _buttons[index].group;
		}
		if (done) {
			break;
		}
	}
	if (done) {
		if (!visible.empty()) {
			overflowStart = visible.back();
			visible.pop_back();
		} else if (!groups.empty() && !groups.front().empty()) {
			overflowStart = groups.front().front();
		}
		while ((overflowStart >= 0)
			&& (widthWithOverflow(visible, overflowStart) > available)
			&& !visible.empty()) {
			overflowStart = visible.back();
			visible.pop_back();
		}
		if ((overflowStart >= 0) && (widthWithOverflow({}, overflowStart) > available)) {
			overflowStart = -1;
		}
	}
	if (overflowStart >= 0) {
		auto append = false;
		for (const auto &group : groups) {
			for (const auto index : group) {
				if (index == overflowStart) {
					append = true;
				}
				if (append) {
					_overflowActions.push_back(index);
				}
			}
		}
	}
	auto left = padding.left();
	previous = std::nullopt;
	for (const auto index : visible) {
		const auto group = _buttons[index].group;
		if (previous) {
			if (*previous == group) {
				left += buttonSkip;
			} else {
				left += dividerSkip;
				_dividerXs.push_back(left);
				left += dividerWidth + dividerSkip;
			}
		}
		if (const auto host = _buttons[index].host()) {
			host->moveToLeft(left, top, width);
			host->show();
			left += host->width();
		}
		previous = group;
	}
	if (!_overflowActions.empty() && _overflow) {
		const auto overflowGroup = _buttons[_overflowActions.front()].group;
		if (previous) {
			if (*previous == overflowGroup) {
				left += buttonSkip;
			} else {
				left += dividerSkip;
				_dividerXs.push_back(left);
				left += dividerWidth + dividerSkip;
			}
		}
		_overflow->moveToLeft(left, top, width);
		_overflow->show();
	}
	updateOverflowButtonState();
	if (_hovered && !_hovered->isVisible()) {
		hideTooltip();
	}
	updateTooltipGeometry();
	update();
	return padding.top() + buttonHeight + padding.bottom();
}

void Toolbar::hideShownTooltip() {
	hideTooltip();
}

bool Toolbar::eventFilter(QObject *object, QEvent *event) {
	for (auto &data : _buttons) {
		if (data.button != object) {
			continue;
		}
		if (event->type() == QEvent::Enter) {
			showTooltip(not_null<Ui::RippleButton*>(data.button));
		} else if (event->type() == QEvent::Leave && _hovered == data.button) {
			hideTooltip();
		}
		return Ui::RpWidget::eventFilter(object, event);
	}
	if ((_emoji.button == object) && _emoji.button) {
		if (event->type() == QEvent::Enter) {
			showTooltip(not_null<Ui::RippleButton*>(_emoji.button));
		} else if (event->type() == QEvent::Leave && _hovered == _emoji.button) {
			hideTooltip();
		}
	}
	return Ui::RpWidget::eventFilter(object, event);
}

void Toolbar::paintEvent(QPaintEvent *e) {
	Painter p(this);
	const auto lineTop = st::ivEditorToolbarPadding.top()
		+ ((st::ivEditorToolbarButton.height
			- st::ivEditorToolbarDividerHeight) / 2);
	const auto lineHeight = st::ivEditorToolbarDividerHeight;
	for (const auto x : _dividerXs) {
		p.fillRect(
			QRect(x, lineTop, st::lineWidth, lineHeight),
			st::defaultMarkdown.rule.fg);
	}
}

void Toolbar::showTooltip(not_null<Ui::RippleButton*> button) {
	hideTooltip();
	const auto data = buttonData(button);
	if (!data) {
		return;
	}
	_hovered = button;
	const auto tooltipParent = _tooltipParent
		? _tooltipParent.data()
		: (parentWidget() ? parentWidget() : this);
	_tooltip.reset(Ui::CreateChild<Ui::ImportantTooltip>(
		tooltipParent,
		Ui::MakeNiceTooltipLabel(
			tooltipParent,
			data->tooltipFactory(),
			st::boxWideWidth,
			st::defaultImportantTooltipLabel),
		st::defaultImportantTooltip));
	_tooltip->setAttribute(Qt::WA_TransparentForMouseEvents);
	_tooltip->toggleFast(false);
	updateTooltipGeometry();
	_tooltip->raise();
	_tooltip->toggleAnimated(true);
}

void Toolbar::hideTooltip() {
	_hovered = nullptr;
	if (_tooltip) {
		_tooltip->toggleFast(false);
		_tooltip = nullptr;
	}
}

void Toolbar::updateTooltipGeometry() {
	if (!_tooltip || !_hovered) {
		return;
	}
	const auto tooltipParent = _tooltip->parentWidget();
	const auto geometry = Ui::MapFrom(
		tooltipParent,
		_hovered,
		_hovered->rect());
	_tooltip->pointAt(geometry, RectPart::Top | RectPart::Center);
}

void Toolbar::updateOverflowButtonState() {
	if (!_overflow) {
		return;
	}
	const auto enabled = !_overflowActions.empty();
	if (!_overflowStateInitialized || _overflowEnabled != enabled) {
		_overflow->setPointerCursor(enabled);
		_overflow->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
		if (enabled) {
			_overflow->setDisabled(false);
		} else {
			_overflow->clearState();
			_overflow->setDisabled(true);
		}
		_overflowEnabled = enabled;
	}
	_overflowStateInitialized = true;
}

ToolbarButton *Toolbar::buttonData(not_null<Ui::RippleButton*> button) {
	for (auto &data : _buttons) {
		if (data.button == button.get()) {
			return &data;
		}
	}
	return (_emoji.button == button.get()) ? &_emoji : nullptr;
}

} // namespace

struct WindowHost::Impl final {
public:
	explicit Impl(ShowWindowDescriptor descriptor);
	~Impl();
	void close();

private:
	void setupWindow(ShowWindowDescriptor &&descriptor);
	void setupEmojiColumn(const ShowWindowDescriptor &descriptor);
	void layout();
	void toggleEmojiColumn();
	void showEmojiColumn();
	void hideEmojiColumn(bool skipResize = false);
	void updateEditorVisibleTopBottom();
	void setEmojiColumnInteractionActive(bool active);
	[[nodiscard]] int emojiColumnWidth() const;
	[[nodiscard]] int minimalWindowWidthWithEmojiColumn() const;
	[[nodiscard]] bool handleCloseRequest();
	void finishCloseFromAcceptedEvent();
	void finishClose();
	[[nodiscard]] bool articleChanged();
	[[nodiscard]] bool showCloseConfirmation();
	[[nodiscard]] bool showDiscardConfirmation();
	[[nodiscard]] bool confirmCancel();
	void discard();
	void submit();

	std::unique_ptr<Window> _window;
	std::shared_ptr<ChatHelpers::Show> _show;
	std::shared_ptr<State> _state;
	RichPage _initialPage;
	object_ptr<Ui::RpWidget> _top = { nullptr };
	object_ptr<Ui::RpWidget> _bottom = { nullptr };
	object_ptr<Ui::ScrollArea> _scroll = { nullptr };
	QPointer<Widget> _editor;
	object_ptr<Toolbar> _toolbar = { nullptr };
	object_ptr<Ui::RoundButton> _discard = { nullptr };
	object_ptr<Ui::RoundButton> _cancel = { nullptr };
	object_ptr<Ui::RoundButton> _submit = { nullptr };
	object_ptr<ChatHelpers::TabbedSelector> _emojiColumn = { nullptr };
	Fn<bool()> _discarded;
	Fn<bool()> _cancelled;
	Fn<bool()> _changedCancelled;
	Fn<bool()> _confirmed;
	Fn<void()> _closed;
	base::weak_qptr<Ui::GenericBox> _closeConfirmation;
	base::weak_qptr<Ui::GenericBox> _discardConfirmation;
	rpl::lifetime _lifetime;
	int _emojiColumnExtendedBy = 0;
	bool _emojiColumnShown = false;
	bool _emojiColumnInteractionActive = false;
	bool _closingApproved = false;
	bool _closedNotified = false;

};

WindowHost::Impl::Impl(ShowWindowDescriptor descriptor) {
	setupWindow(std::move(descriptor));
}

WindowHost::Impl::~Impl() {
	hideEmojiColumn(true);
}

void WindowHost::Impl::close() {
	finishClose();
}

void WindowHost::Impl::setupWindow(ShowWindowDescriptor &&descriptor) {
	const auto title = tr::lng_article_editor_title(tr::now);

	if (!descriptor.state) {
		descriptor.state = std::make_shared<State>();
	}
	_state = descriptor.state;
	_initialPage = _state->richPage();

	_window = std::make_unique<Window>();
	const auto window = _window.get();
	window->setCloseRequestHandler([=] {
		return handleCloseRequest();
	});
	_show = std::make_shared<WindowContext>(window, descriptor.session);
	if (descriptor.showCreated) {
		descriptor.showCreated(_show);
	}
	window->setTitle(title);
	window->setWindowTitle(title);
	window->setMinimumSize(st::ivEditorWindowMinSize);
	window->setGeometry(DefaultWindowGeometry());

	window->body()->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(window->body().get()).fillRect(clip, st::windowBg);
	}, window->body()->lifetime());

	_top = object_ptr<Ui::RpWidget>(window->body().get());
	_top->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(_top.data()).fillRect(clip, st::windowBg);
	}, _top->lifetime());
	_bottom = object_ptr<Ui::RpWidget>(window->body().get());
	_bottom->paintRequest() | rpl::on_next([=](QRect clip) {
		QPainter(_bottom.data()).fillRect(clip, st::windowBg);
	}, _bottom->lifetime());

	const auto hasRequestMedia = static_cast<bool>(descriptor.requestMedia);
	_scroll = object_ptr<Ui::ScrollArea>(window->body().get(), st::boxScroll);
	_editor = _scroll->setOwnedWidget(object_ptr<Widget>(
		_scroll.data(),
		WidgetServices{
			.session = descriptor.session,
			.show = _show,
			.outer = window->body(),
			.customEmojiPaused = [show = _show] {
				return show->paused(ChatHelpers::PauseReason::Layer);
			},
			.requestMedia = std::move(descriptor.requestMedia),
			.applyPreparedMedia = std::move(descriptor.applyPreparedMedia),
			.imeCompositionStarts = window->imeCompositionStarts(),
		},
		descriptor.peer,
		descriptor.state,
		std::move(descriptor.showLimitToast)));
	const auto editor = not_null<Widget*>(_editor.data());
	if (descriptor.editorCreated) {
		descriptor.editorCreated(editor);
	}
	const auto body = QPointer<QWidget>(window->body().get());
	setupEmojiColumn(descriptor);

	_toolbar = object_ptr<Toolbar>(
		_top.data(),
		editor,
		body,
		hasRequestMedia,
		std::move(descriptor.requestMap),
		[=] {
			_toolbar->hideShownTooltip();
			toggleEmojiColumn();
		});
	if (descriptor.discarded) {
		_discard = object_ptr<Ui::RoundButton>(
			_bottom.data(),
			tr::lng_record_lock_discard(),
			st::ivEditorDiscardButton);
		_discard->setClickedCallback([=] {
			discard();
		});
	}
	_cancel = object_ptr<Ui::RoundButton>(
		_bottom.data(),
		tr::lng_cancel(),
		st::ivEditorCancelButton);
	_cancel->setClickedCallback([=] {
		if (confirmCancel()) {
			finishClose();
		}
	});
	_submit = object_ptr<Ui::RoundButton>(
		_bottom.data(),
		rpl::single(SubmitText(descriptor)),
		st::ivEditorSubmitButton);
	_submit->setClickedCallback([=] { submit(); });
	if (descriptor.setupSubmitButton) {
		descriptor.setupSubmitButton(not_null<Ui::RpWidget*>(_submit.data()));
	}

	_discarded = std::move(descriptor.discarded);
	_cancelled = std::move(descriptor.cancelled);
	_changedCancelled = std::move(descriptor.changedCancelled);
	_confirmed = std::move(descriptor.confirmed);
	_closed = std::move(descriptor.closed);

	window->body()->sizeValue() | rpl::on_next([=](QSize) {
		layout();
	}, _lifetime);
	rpl::combine(
		_scroll->scrollTopValue(),
		_scroll->heightValue()
	) | rpl::on_next([=](int, int) {
		updateEditorVisibleTopBottom();
	}, _lifetime);
	window->events() | rpl::on_next([=](not_null<QEvent*> e) {
		if (e->type() == QEvent::KeyPress) {
			const auto event = static_cast<QKeyEvent*>(e.get());
			if (event->key() == Qt::Key_Escape) {
				event->accept();
				if (confirmCancel()) {
					finishClose();
				}
			}
		}
	}, _lifetime);

	layout();
	_top->show();
	_bottom->show();
	_scroll->show();
	window->show();
	editor->activateInitialNode();
}

void WindowHost::Impl::setupEmojiColumn(const ShowWindowDescriptor &descriptor) {
	using Selector = ChatHelpers::TabbedSelector;
	_emojiColumn = object_ptr<Selector>(
		_window->body().get(),
		ChatHelpers::TabbedSelectorDescriptor{
			.show = _show,
			.st = st::defaultEmojiPan,
			.level = ChatHelpers::PauseReason::Layer,
			.mode = Selector::Mode::EmojiOnly,
			.features = {
				.stickersSettings = false,
				.openStickerSets = false,
			},
		});
	_emojiColumn->hide();
	_emojiColumn->setCurrentPeer(descriptor.peer);
	_emojiColumn->emojiChosen(
	) | rpl::on_next([=](ChatHelpers::EmojiChosen data) {
		if (_editor) {
			_editor->insertEmoji(data.emoji);
		}
	}, _lifetime);
	_emojiColumn->customEmojiChosen(
	) | rpl::on_next([=](ChatHelpers::FileChosen data) {
		const auto document = data.document;
		if (!IsEmojiDocument(document)) {
			return;
		}
		if (document->isPremiumEmoji()
			&& !descriptor.session->premium()
			&& !Data::AllowEmojiWithoutPremium(descriptor.peer, document)) {
			ShowPremiumPreviewBox(
				_show,
				PremiumFeature::AnimatedEmoji);
		} else if (_editor) {
			_editor->insertCustomEmoji(document);
		}
	}, _lifetime);
}

void WindowHost::Impl::layout() {
	if (!_window || !_top || !_bottom || !_toolbar || !_editor || !_emojiColumn) {
		return;
	}
	const auto width = _window->body()->width();
	const auto height = _window->body()->height();
	const auto padding = st::ivEditorBottomControlsPadding;
	const auto emojiWidth = _emojiColumnShown ? emojiColumnWidth() : 0;
	const auto editorWidth = std::max(width - emojiWidth, 0);
	const auto toolbarHeight = _toolbar->resizeGetHeight(editorWidth);
	auto buttonsHeight = std::max(_cancel->height(), _submit->height());
	if (_discard) {
		buttonsHeight = std::max(buttonsHeight, _discard->height());
	}
	const auto bottomHeight = padding.top() + buttonsHeight + padding.bottom();
	const auto contentHeight = std::max(height - toolbarHeight - bottomHeight, 0);
	const auto buttonsTop = padding.top();
	_top->setGeometry(0, 0, editorWidth, toolbarHeight);
	_toolbar->setGeometry(0, 0, editorWidth, toolbarHeight);
	_bottom->setGeometry(
		0,
		std::max(height - bottomHeight, toolbarHeight),
		editorWidth,
		bottomHeight);
	if (_discard) {
		_discard->moveToLeft(padding.left(), buttonsTop);
	}
	_submit->moveToRight(padding.right(), buttonsTop, editorWidth);
	_cancel->moveToRight(
		padding.right()
			+ _submit->width()
			+ st::ivEditorBottomControlsButtonSkip,
		buttonsTop,
		editorWidth);
	_scroll->setGeometry(
		0,
		toolbarHeight,
		editorWidth,
		contentHeight);
	if (_emojiColumnShown) {
		_emojiColumn->setGeometry(
			editorWidth,
			0,
			emojiWidth,
			height);
	} else {
		_emojiColumn->hide();
	}
	_editor->resizeToWidth(std::max(_scroll->width(), 1));
	updateEditorVisibleTopBottom();
}

void WindowHost::Impl::toggleEmojiColumn() {
	if (_emojiColumnShown) {
		hideEmojiColumn();
	} else {
		showEmojiColumn();
	}
}

void WindowHost::Impl::showEmojiColumn() {
	if (_emojiColumnShown || !_window || !_emojiColumn) {
		return;
	}
	const auto window = not_null<Window*>(_window.get());
	const auto wanted = emojiColumnWidth();
	const auto minimal = std::max(
		minimalWindowWidthWithEmojiColumn() - window->width(),
		0);
	_emojiColumnExtendedBy = 0;
	if (!window->isMaximized() && !window->isFullScreen()) {
		if (CanExtendNoMove(window, wanted)) {
			_emojiColumnExtendedBy = TryToExtendWidthBy(window, wanted);
		} else if (minimal > 0 && CanExtendNoMove(window, minimal)) {
			_emojiColumnExtendedBy = TryToExtendWidthBy(window, minimal);
		} else if (window->width() < minimalWindowWidthWithEmojiColumn()) {
			_emojiColumnExtendedBy = TryToExtendWidthBy(window, minimal);
		}
	}
	window->setMinimumWidth(minimalWindowWidthWithEmojiColumn());
	_emojiColumnShown = true;
	_emojiColumn->setRoundRadius(0);
	setEmojiColumnInteractionActive(true);
	_emojiColumn->showStarted();
	_emojiColumn->show();
	layout();
	_emojiColumn->afterShown();
}

void WindowHost::Impl::hideEmojiColumn(bool skipResize) {
	if (!_emojiColumnShown || !_window || !_emojiColumn) {
		return;
	}
	const auto window = not_null<Window*>(_window.get());
	_emojiColumn->beforeHiding();
	_emojiColumn->hide();
	_emojiColumn->hideFinished();
	_emojiColumnShown = false;
	window->setMinimumWidth(st::ivEditorWindowMinSize.width());
	setEmojiColumnInteractionActive(false);
	if (!skipResize
		&& _emojiColumnExtendedBy > 0
		&& !window->isMaximized()
		&& !window->isFullScreen()) {
		window->resize(
			window->width() - _emojiColumnExtendedBy,
			window->height());
	}
	_emojiColumnExtendedBy = 0;
	layout();
}

void WindowHost::Impl::updateEditorVisibleTopBottom() {
	if (!_scroll || !_editor) {
		return;
	}
	const auto scrollTop = _scroll->scrollTop();
	_editor->setVisibleTopBottom(scrollTop, scrollTop + _scroll->height());
}

void WindowHost::Impl::setEmojiColumnInteractionActive(bool active) {
	if (_emojiColumnInteractionActive == active || !_editor) {
		_emojiColumnInteractionActive = active;
		return;
	}
	_emojiColumnInteractionActive = active;
	_editor->setInlineFieldExternalInteractionActive(active);
}

int WindowHost::Impl::emojiColumnWidth() const {
	return st::emojiPanWidth;
}

int WindowHost::Impl::minimalWindowWidthWithEmojiColumn() const {
	return st::ivEditorWindowMinSize.width() + emojiColumnWidth();
}

bool WindowHost::Impl::handleCloseRequest() {
	if (_closingApproved) {
		return true;
	}
	if (confirmCancel()) {
		finishCloseFromAcceptedEvent();
		return true;
	}
	return false;
}

void WindowHost::Impl::finishCloseFromAcceptedEvent() {
	_closingApproved = true;
	if (_closedNotified) {
		return;
	}
	_closedNotified = true;
	hideEmojiColumn(true);
	if (const auto closed = base::take(_closed)) {
		crl::on_main(closed);
	}
}

void WindowHost::Impl::finishClose() {
	finishCloseFromAcceptedEvent();
	if (_window) {
		_window->close();
	}
}

bool WindowHost::Impl::articleChanged() {
	if (!_state) {
		return false;
	}
	if (_editor
		&& (_editor->commitInlineFieldForClose()
			== State::ApplyResult::Failed)) {
		return true;
	}
	return !RichPagesEqual(_initialPage, _state->richPage());
}

bool WindowHost::Impl::showCloseConfirmation() {
	if (_closeConfirmation) {
		return true;
	} else if (!_show || !_show->valid()) {
		return false;
	}
	const auto window = QPointer<Window>(_window.get());
	const auto close = [=](Fn<void()> closeBox) {
		closeBox();
		if (!window) {
			return;
		} else if (!_cancelled || _cancelled()) {
			finishClose();
		}
	};
	_closeConfirmation = _show->show(Ui::MakeConfirmBox({
		.text = tr::lng_theme_editor_sure_close(),
		.confirmed = close,
		.confirmText = tr::lng_close(),
	}));
	return true;
}

bool WindowHost::Impl::showDiscardConfirmation() {
	if (_discardConfirmation) {
		return true;
	} else if (!_show || !_show->valid()) {
		return false;
	}
	const auto window = QPointer<Window>(_window.get());
	const auto discard = [=](Fn<void()> closeBox) {
		closeBox();
		if (!window) {
			return;
		} else if (!_discarded || _discarded()) {
			finishClose();
		}
	};
	_discardConfirmation = _show->show(Ui::MakeConfirmBox({
		.text = tr::lng_iv_editor_discard_draft_sure(tr::now),
		.confirmed = discard,
		.confirmText = tr::lng_record_lock_discard(),
		.confirmStyle = &st::attentionBoxButton,
	}));
	return true;
}

bool WindowHost::Impl::confirmCancel() {
	if (!articleChanged()) {
		return !_cancelled || _cancelled();
	} else if (_changedCancelled) {
		return _changedCancelled();
	}
	return showCloseConfirmation()
		? false
		: (!_cancelled || _cancelled());
}

void WindowHost::Impl::discard() {
	if (_discard) {
		[[maybe_unused]] const auto shown = showDiscardConfirmation();
	}
}

void WindowHost::Impl::submit() {
	if (!_editor) {
		return;
	}
	if (_editor->commitInlineField() == State::ApplyResult::Failed) {
		return;
	}
	if (!_confirmed || _confirmed()) {
		finishClose();
	}
}

WindowHost::WindowHost(ShowWindowDescriptor descriptor)
: _impl(std::make_unique<Impl>(std::move(descriptor))) {
}

WindowHost::~WindowHost() = default;

void WindowHost::close() {
	_impl->close();
}

std::unique_ptr<WindowHost> ShowWindow(ShowWindowDescriptor descriptor) {
	if (!descriptor.state) {
		descriptor.state = std::make_shared<State>();
	}
	return std::unique_ptr<WindowHost>(new WindowHost(std::move(descriptor)));
}

} // namespace Iv::Editor
