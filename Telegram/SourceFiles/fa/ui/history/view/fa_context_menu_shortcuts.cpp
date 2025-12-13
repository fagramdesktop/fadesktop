/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#include "stdafx.h"

#include "fa/ui/history/view/fa_context_menu_shortcuts.h"

#include "fa/settings/fa_settings.h"
#include "main/main_session.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_list_widget.h"
#include "history/history_item.h"
#include "history/history_item_text.h"
#include "history/history.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_document.h"
#include "data/data_file_click_handler.h"
#include "data/data_session.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/layers/generic_box.h"
#include "lang/lang_keys.h"
#include "boxes/translate_box.h"
#include "ui/layers/box_content.h"
#include "window/window_session_controller.h"
#include "window/window_peer_menu.h"
#include "window/window_controller.h"
#include "base/unixtime.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"

#include <QtGui/QAction>
#include <QtGui/QPainterPath>

namespace FaHistoryView {
namespace {

constexpr auto kShortcutButtonSize = 40;
constexpr auto kShortcutButtonIconSize = 24;
constexpr auto kShortcutButtonSpacing = 8;
constexpr auto kShortcutVerticalPadding = 4;
constexpr auto kShortcutHorizontalPadding = 7;
constexpr auto kShortcutCornerRadius = 6;

class ShortcutButton final : public Ui::RippleButton {
public:
	ShortcutButton(
		not_null<Ui::RpWidget*> parent,
		const style::Menu &st,
		const style::icon &icon)
	: Ui::RippleButton(parent, st.ripple)
	, _st(st)
	, _icon(icon) {
		setMouseTracking(true);
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = Painter(this);
		const auto over = isOver();
		const auto down = isDown();

		if (over || down) {
			p.setRenderHint(QPainter::Antialiasing);
			p.setPen(Qt::NoPen);
			p.setBrush(_st.itemBgOver);
			p.drawRoundedRect(rect(), 4, 4);
		}

		paintRipple(p, 0, 0);

		const auto iconSize = kShortcutButtonIconSize;
		const auto iconX = (width() - iconSize) / 2;
		const auto iconY = (height() - iconSize) / 2;

		const auto color = (over || down) ? _st.itemFgOver->c : _st.itemFg->c;
		_icon.paint(p, iconX, iconY, width(), color);
	}

private:
	const style::Menu &_st;
	const style::icon &_icon;
};

} // namespace

ContextMenuShortcuts::ContextMenuShortcuts(
	not_null<Ui::RpWidget*> parent,
	const style::Menu &st,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	ShortcutCallbacks callbacks)
: ItemBase(parent, st)
, _dummyAction(Ui::CreateChild<QAction>(this))
, _st(st)
, _item(item)
, _controller(controller)
, _callbacks(std::move(callbacks))
, _height(0) {
	setAcceptBoth(true);
	initResizeHook(parent->sizeValue());
	prepare();
	enableMouseSelecting();
}

bool ContextMenuShortcuts::isEnabled() const {
	return true;
}

not_null<QAction*> ContextMenuShortcuts::action() const {
	return _dummyAction;
}

rpl::producer<bool> ContextMenuShortcuts::forceShown() const {
	return _forceShown.events();
}

int ContextMenuShortcuts::contentHeight() const {
	return _height;
}

void ContextMenuShortcuts::prepare() {
	createButtons();

	paintRequest(
	) | rpl::start_with_next([=](const QRect &clip) {
		auto p = Painter(this);
		paint(p, clip);
	}, lifetime());

	sizeValue(
	) | rpl::start_with_next([=](QSize size) {
		updateButtonsLayout();
	}, lifetime());
}

void ContextMenuShortcuts::paint(Painter &p, const QRect &clip) {
	Q_UNUSED(clip);
	p.fillRect(0, 0, width(), _height, _st.itemBg);
}

void ContextMenuShortcuts::createButtons() {
	_buttons.clear();
	_addedShortcuts.clear();

	const auto item = _item;

	const auto canReply = item->isRegular();
	const auto hasCopyRestriction = _callbacks.hasCopyRestriction
		? _callbacks.hasCopyRestriction(item)
		: false;
	const auto canCopy = !item->clipboardText().empty() && !hasCopyRestriction;
	const auto canLink = item->hasDirectLink();
	const auto canForward = item->allowsForward();
	const auto media = item->media();
	const auto photo = media ? media->photo() : nullptr;
	const auto document = media ? media->document() : nullptr;
	const auto canGallery = photo
		|| (document && (document->isVideoFile() || document->isGifv()));

	const auto hasDocumentOnly = document && !canGallery;
	const auto hasText = !item->originalText().text.isEmpty();

	const auto canEdit = item->allowsEdit(base::unixtime::now());
	const auto canPin = item->canPin();
	const auto isPinned = item->isPinned();

	auto addButton = [&](const style::icon &icon,
		Fn<void()> callback,
		ShortcutType type) {
		auto button = object_ptr<ShortcutButton>(
			this,
			_st,
			icon);
		button->setClickedCallback(std::move(callback));
		_buttons.push_back(std::move(button));
		_addedShortcuts.insert(type);
	};

	// Reply (always if available)
	if (canReply) {
		addButton(
			st::menuIconReply,
			[=] {
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
				if (_callbacks.replyToMessage) {
					_callbacks.replyToMessage({
						.messageId = item->fullId(),
					}, false);
				}
			},
			ShortcutType::Reply);
	}

	// Copy or Edit
	if (canCopy) {
		addButton(
			st::menuIconCopy,
			[=] {
				bool restricted = _callbacks.showCopyRestriction
					? _callbacks.showCopyRestriction(item)
					: false;
				if (!restricted) {
					TextUtilities::SetClipboardText(item->clipboardText());
				}
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
			},
			ShortcutType::Copy);
	} else if (canEdit) {
		addButton(
			st::menuIconEdit,
			[=] {
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
				if (_callbacks.editMessage) {
					_callbacks.editMessage(item->fullId());
				}
			},
			ShortcutType::Edit);
	}

	// Copy Link or Pin
	if (canLink) {
		addButton(
			st::menuIconLink,
			[=] {
				const auto context = _callbacks.elementContext
					? _callbacks.elementContext()
					: HistoryView::Context::History;
				const auto show = _callbacks.uiShow
					? _callbacks.uiShow()
					: _controller->uiShow();
				HistoryView::CopyPostLink(
					show,
					item->fullId(),
					context,
					std::nullopt);
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
			},
			ShortcutType::CopyLink);
	} else if (canPin) {
		const auto pinItemId = item->fullId();
		addButton(
			isPinned ? st::menuIconUnpin : st::menuIconPin,
			[=] {
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
				Window::ToggleMessagePinned(_controller, pinItemId, !isPinned);
			},
			isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	// Based on media type, or Forward as fallback
	bool addedFourthButton = false;

	if (canGallery && !hasCopyRestriction) {
		addButton(
			st::menuIconDownload,
			[=] {
				if (photo && _callbacks.savePhoto) {
					_callbacks.savePhoto(photo);
				} else if (document) {
					DocumentSaveClickHandler::SaveAndTrack(
						item->fullId(),
						document,
						DocumentSaveClickHandler::Mode::ToNewFile);
				}
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
			},
			ShortcutType::SaveFile);
		addedFourthButton = true;
	} else if (hasDocumentOnly && !hasCopyRestriction) {
		addButton(
			st::menuIconDownload,
			[=] {
				if (!document) {
					return;
				}
				DocumentSaveClickHandler::SaveAndTrack(
					item->fullId(),
					document,
					DocumentSaveClickHandler::Mode::ToNewFile);
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
			},
			ShortcutType::SaveFile);
		addedFourthButton = true;
	} else if (hasText && !Ui::SkipTranslate(item->originalText())) {
		addButton(
			st::menuIconTranslate,
			[=] {
				_controller->show(Box(
					Ui::TranslateBox,
					item->history()->peer,
					item->fullId().msg,
					item->originalText(),
					hasCopyRestriction));
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
			},
			ShortcutType::Translate);
		addedFourthButton = true;
	}

	// If no fourth button and we can pin (and pin wasn't already added in slot 3), add Pin
	if (!addedFourthButton && canPin && !canLink) {
		// Pin was already added in the third slot when !canLink
	} else if (!addedFourthButton && canPin && canLink) {
		// canLink took the third slot, so we can add Pin in the fourth slot
		const auto pinItemId = item->fullId();
		addButton(
			isPinned ? st::menuIconUnpin : st::menuIconPin,
			[=] {
				if (_callbacks.hideMenu) {
					_callbacks.hideMenu();
				}
				Window::ToggleMessagePinned(_controller, pinItemId, !isPinned);
			},
			isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	if (_buttons.empty()) {
		_height = 0;
		resize(width(), 0);
		return;
	}

	_height = kShortcutButtonSize + kShortcutVerticalPadding * 2;

	const auto numButtons = int(_buttons.size());
	const auto totalWidth = kShortcutHorizontalPadding * 2
		+ (kShortcutButtonSize * numButtons)
		+ (kShortcutButtonSpacing * (numButtons - 1));

	setMinWidth(std::max(totalWidth, _st.widthMin));

	for (auto &button : _buttons) {
		button->resize(kShortcutButtonSize, kShortcutButtonSize);
		button->show();
	}

	resize(width(), _height);
	update();
}

void ContextMenuShortcuts::updateButtonsLayout() {
	if (_buttons.empty()) {
		return;
	}

	const auto numButtons = int(_buttons.size());
	const auto contentWidth = (kShortcutButtonSize * numButtons)
		+ (kShortcutButtonSpacing * (numButtons - 1));

	auto x = std::max(kShortcutHorizontalPadding, (width() - contentWidth) / 2);

	for (auto &button : _buttons) {
		button->setGeometry(
			x,
			kShortcutVerticalPadding,
			kShortcutButtonSize,
			kShortcutButtonSize);
		x += kShortcutButtonSize + kShortcutButtonSpacing;
	}
}

AddContextMenuShortcutsResult AddContextMenuShortcuts(
	not_null<Ui::Menu::Menu*> menu,
	const HistoryView::ContextMenuRequest &request,
	not_null<HistoryView::ListWidget*> list) {

	if (!FASettings::JsonSettings::GetBool("context_menu_use_shortcuts")) {
		return { nullptr, {} };
	}

	if (!request.item) {
		return { nullptr, {} };
	}

	ShortcutCallbacks callbacks;
	callbacks.hasCopyRestriction = [list](HistoryItem *item) {
		return list->hasCopyRestriction(item);
	};
	callbacks.showCopyRestriction = [list](HistoryItem *item) {
		return list->showCopyRestriction(item);
	};
	callbacks.replyToMessage = [list](FullReplyTo to, bool b) {
		list->replyToMessageRequestNotify(to, b);
	};
	callbacks.editMessage = [list](FullMsgId id) {
		list->editMessageRequestNotify(id);
	};
	callbacks.forwardMessage = [list](FullMsgId id) {
		if (const auto item = list->session().data().message(id)) {
			Window::ShowForwardMessagesBox(
				list->controller(),
				MessageIdsList{ 1, id });
		}
	};
	callbacks.uiShow = [list]() {
		return list->controller()->uiShow();
	};
	callbacks.elementContext = [list]() {
		return list->elementContext();
	};
	callbacks.openPhoto = [list](not_null<PhotoData*> photo, FullMsgId context) {
		list->elementOpenPhoto(photo, context);
	};
	callbacks.openDocument = [list](not_null<DocumentData*> document, FullMsgId context, bool showInMediaView) {
		list->elementOpenDocument(document, context, showInMediaView);
	};
	callbacks.savePhoto = [](not_null<PhotoData*> photo) {
		const auto media = photo->activeMediaView();
		if (photo->isNull() || !media || !media->loaded()) {
			return;
		}
		FileDialog::GetWritePath(
			Core::App().getFileDialogParent(),
			tr::lng_save_photo(tr::now),
			u"JPEG Image (*.jpg);;"_q + FileDialog::AllFilesFilter(),
			filedialogDefaultName(u"photo"_q, u".jpg"_q),
			crl::guard(&photo->session(), [=](const QString &result) {
				if (!result.isEmpty()) {
					media->saveToFile(result);
				}
			}));
	};

	auto widget = base::make_unique_q<ContextMenuShortcuts>(
		menu,
		menu->st(),
		request.item,
		list->controller(),
		std::move(callbacks));

	auto addedShortcuts = widget ? widget->addedShortcuts() : std::set<ShortcutType>{};
	return { std::move(widget), std::move(addedShortcuts) };
}


AddContextMenuShortcutsResult AddContextMenuShortcuts(
	not_null<Ui::Menu::Menu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	ShortcutCallbacks callbacks) {

	if (!FASettings::JsonSettings::GetBool("context_menu_use_shortcuts")) {
		return { nullptr, {} };
	}

	auto widget = base::make_unique_q<ContextMenuShortcuts>(
		menu,
		menu->st(),
		item,
		controller,
		std::move(callbacks));

	auto addedShortcuts = widget ? widget->addedShortcuts() : std::set<ShortcutType>{};
	return { std::move(widget), std::move(addedShortcuts) };
}

std::set<ShortcutType> GetAvailableShortcuts(
	not_null<HistoryItem*> item,
	Fn<bool(HistoryItem*)> hasCopyRestriction) {

	if (!FASettings::JsonSettings::GetBool("context_menu_use_shortcuts")) {
		return {};
	}

	std::set<ShortcutType> result;

	const auto canReply = item->isRegular();
	const auto copyRestriction = hasCopyRestriction
		? hasCopyRestriction(item)
		: false;
	const auto canCopy = !item->clipboardText().empty() && !copyRestriction;
	const auto canLink = item->hasDirectLink();
	const auto canForward = item->allowsForward();
	const auto media = item->media();
	const auto photo = media ? media->photo() : nullptr;
	const auto document = media ? media->document() : nullptr;
	const auto canGallery = photo
		|| (document && (document->isVideoFile() || document->isGifv()));

	const auto hasDocumentOnly = document && !canGallery;
	const auto hasText = !item->originalText().text.isEmpty();

	const auto canEdit = item->allowsEdit(base::unixtime::now());
	const auto canPin = item->canPin();
	const auto isPinned = item->isPinned();

	// Reply
	if (canReply) {
		result.insert(ShortcutType::Reply);
	}

	// Copy or Edit
	if (canCopy) {
		result.insert(ShortcutType::Copy);
	} else if (canEdit) {
		result.insert(ShortcutType::Edit);
	}

	// Copy link or Pin
	if (canLink) {
		result.insert(ShortcutType::CopyLink);
	} else if (canPin) {
		result.insert(isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	// Based on media type, or Forward
	bool addedFourthButton = false;

	if (canGallery && !copyRestriction) {
		result.insert(ShortcutType::SaveFile);
		addedFourthButton = true;
	} else if (hasDocumentOnly && !copyRestriction) {
		result.insert(ShortcutType::SaveFile);
		addedFourthButton = true;
	} else if (hasText && !Ui::SkipTranslate(item->originalText())) {
		result.insert(ShortcutType::Translate);
		addedFourthButton = true;
	}

	// If no fourth button and we can pin (and pin wasn't already added in slot 3), add Pin
	if (!addedFourthButton && canPin && canLink) {
		// canLink took the third slot, so we can add Pin in the fourth slot
		result.insert(isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	return result;
}

} // namespace FaHistoryView
