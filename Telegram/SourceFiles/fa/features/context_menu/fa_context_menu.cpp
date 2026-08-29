/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "stdafx.h"

#include "fa/features/context_menu/fa_context_menu.h"

#include "fa/settings/fa_settings.h"
#include "fa_lang_auto.h"
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
#include "data/data_user.h"
#include "data/data_changes.h"
#include "data/data_drafts.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/widgets/menu/menu_item_base.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/layers/generic_box.h"
#include "lang/lang_keys.h"
#include "boxes/translate_box.h"
#include "window/window_session_controller.h"
#include "window/window_peer_menu.h"
#include "window/window_controller.h"
#include "base/unixtime.h"
#include "styles/style_chat.h"
#include "styles/style_menu_icons.h"
#include "styles/style_fa_styles.h"

#include <QAction>
#include <QMenu>

namespace FA::Features::ContextMenu {
namespace {

[[nodiscard]] int ShortcutButtonSize() {
	return style::ConvertScale(
		FASettings::FASettings::getInstance().contextMenuShortcutButtonSize());
}

[[nodiscard]] int ShortcutCornerRadius() {
	return style::ConvertScale(
		FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius());
}

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
			const auto radius = ShortcutCornerRadius();
			p.drawRoundedRect(rect(), radius, radius);
		}

		paintRipple(p, 0, 0);

		const auto iconX = (width() - _icon.width()) / 2;
		const auto iconY = (height() - _icon.height()) / 2;

		const auto color = (over || down) ? _st.itemFgOver->c : _st.itemFg->c;
		_icon.paint(p, iconX, iconY, width(), color);
	}

private:
	const style::Menu &_st;
	const style::icon &_icon;
};

class ContextMenuShortcuts final : public Ui::Menu::ItemBase {
public:
	ContextMenuShortcuts(
		not_null<Ui::Menu::Menu*> parent,
		const style::Menu &st,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote = {},
		std::vector<not_null<HistoryItem*>> saveItems = {});

	bool isEnabled() const override {
		return true;
	}
	not_null<QAction*> action() const override {
		return _dummyAction;
	}

	[[nodiscard]] rpl::producer<bool> forceShown() const {
		return _forceShown.events();
	}
	[[nodiscard]] const std::set<ShortcutType>& addedShortcuts() const {
		return _addedShortcuts;
	}

protected:
	int contentHeight() const override {
		return _height;
	}

private:
	void prepare();
	void paint(Painter &p, const QRect &clip);
	void createButtons();
	void updateButtonsLayout();

	const not_null<QAction*> _dummyAction;
	const style::Menu &_st;
	const not_null<HistoryItem*> _item;
	const not_null<Window::SessionController*> _controller;
	ShortcutCallbacks _callbacks;
	HistoryView::SelectedQuote _quote;
	std::vector<not_null<HistoryItem*>> _saveItems;

	std::vector<object_ptr<Ui::AbstractButton>> _buttons;
	std::set<ShortcutType> _addedShortcuts;

	int _height = 0;
	rpl::event_stream<bool> _forceShown;
};

ContextMenuShortcuts::ContextMenuShortcuts(
	not_null<Ui::Menu::Menu*> parent,
	const style::Menu &st,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	ShortcutCallbacks callbacks,
	HistoryView::SelectedQuote quote,
	std::vector<not_null<HistoryItem*>> saveItems)
: ItemBase(parent, st)
, _dummyAction(Ui::CreateChild<QAction>(this))
, _st(st)
, _item(item)
, _controller(controller)
, _callbacks(std::move(callbacks))
, _quote(std::move(quote))
, _saveItems(saveItems.empty() ? std::vector<not_null<HistoryItem*>>{ item } : std::move(saveItems))
, _height(0) {
	setAcceptBoth(true);
	fitToMenuWidth();
	prepare();
	enableMouseSelecting();
}

void ContextMenuShortcuts::prepare() {
	createButtons();

	paintRequest(
	) | rpl::on_next([=](const QRect &clip) {
		auto p = Painter(this);
		paint(p, clip);
	}, lifetime());

	sizeValue(
	) | rpl::on_next([=](QSize size) {
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
		button->setClickedCallback([this, callback = std::move(callback)] {
			callback();
			setClicked(Ui::Menu::TriggeredSource::Mouse);
		});
		_buttons.push_back(std::move(button));
		_addedShortcuts.insert(type);
	};

	// 1. Reply
	if (canReply) {
		const auto hasQuote = static_cast<bool>(_quote);
		const auto replyToId = hasQuote ? _quote.item->fullId() : item->fullId();
		const auto quoteText = _quote.highlight.quote;
		const auto quoteOffset = _quote.highlight.quoteOffset;
		addButton(
			st::menuIconReply,
			[=] {
				if (_callbacks.replyToMessage) {
					auto replyTo = FullReplyTo();
					replyTo.messageId = replyToId;
					replyTo.quote = quoteText;
					replyTo.quoteOffset = quoteOffset;
					_callbacks.replyToMessage(std::move(replyTo), false);
				}
				if (hasQuote && _callbacks.clearSelection) {
					_callbacks.clearSelection();
				}
			},
			ShortcutType::Reply);
	}

	// 2. Copy or Edit
	const auto hasQuoteText = _quote && !_quote.highlight.quote.empty();
	if (canCopy || hasQuoteText) {
		const auto quoteText = _quote.highlight.quote;
		addButton(
			st::menuIconCopy,
			[=] {
				const auto restricted = _callbacks.showCopyRestriction
					? _callbacks.showCopyRestriction(item)
					: false;
				if (!restricted) {
					if (hasQuoteText) {
						TextUtilities::SetClipboardText(
							TextForMimeData::Rich(TextWithEntities{ quoteText }));
					} else {
						TextUtilities::SetClipboardText(item->clipboardText());
					}
				}
			},
			ShortcutType::Copy);
	} else if (canEdit) {
		addButton(
			st::menuIconEdit,
			[=] {
				if (_callbacks.editMessage) {
					_callbacks.editMessage(item->fullId());
				}
			},
			ShortcutType::Edit);
	}

	// 3. Copy Link or Pin
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
			},
			ShortcutType::CopyLink);
	} else if (canPin) {
		const auto pinItemId = item->fullId();
		addButton(
			isPinned ? st::menuIconUnpin : st::menuIconPin,
			[=] {
				Window::ToggleMessagePinned(_controller, pinItemId, !isPinned);
			},
			isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	// 4. Media Save or Translate or Pin
	bool addedFourthButton = false;

	auto hasSaveable = [&] {
		for (const auto &saveItem : _saveItems) {
			const auto restricted = _callbacks.hasCopyRestriction
				? _callbacks.hasCopyRestriction(saveItem)
				: false;
			if (restricted) {
				continue;
			}
			const auto media = saveItem->media();
			if (media && (media->photo() || media->document())) {
				return true;
			}
		}
		return false;
	};
	if (hasSaveable()) {
		auto saveAll = [=] {
			for (const auto &saveItem : _saveItems) {
				const auto restricted = _callbacks.hasCopyRestriction
					? _callbacks.hasCopyRestriction(saveItem)
					: false;
				if (restricted) {
					continue;
				}
				const auto media = saveItem->media();
				const auto photo = media ? media->photo() : nullptr;
				const auto document = media ? media->document() : nullptr;
				if (photo) {
					if (_callbacks.savePhoto) {
						_callbacks.savePhoto(photo);
					}
				} else if (document) {
					DocumentSaveClickHandler::SaveAndTrack(
						saveItem->fullId(),
						document,
						DocumentSaveClickHandler::Mode::ToNewFile);
				}
			}
		};
		addButton(
			st::menuIconDownload,
			std::move(saveAll),
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
			},
			ShortcutType::Translate);
		addedFourthButton = true;
	}

	if (!addedFourthButton && canPin && canLink) {
		const auto pinItemId = item->fullId();
		addButton(
			isPinned ? st::menuIconUnpin : st::menuIconPin,
			[=] {
				Window::ToggleMessagePinned(_controller, pinItemId, !isPinned);
			},
			isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
	}

	if (_buttons.empty()) {
		_height = 0;
		resize(width(), 0);
		return;
	}

	const auto buttonSize = ShortcutButtonSize();
	const auto autoVPadding = style::ConvertScale(4);
	_height = buttonSize + autoVPadding * 2;

	const auto numButtons = int(_buttons.size());
	const auto minSpacing = style::ConvertScale(6);
	const auto minMargin = style::ConvertScale(10);
	const auto contentMinWidth = (buttonSize * numButtons)
		+ (minSpacing * (numButtons - 1))
		+ (minMargin * 2);

	setMinWidth(std::max(contentMinWidth, _st.widthMin));

	for (auto &button : _buttons) {
		button->resize(buttonSize, buttonSize);
		button->show();
	}

	resize(width(), _height);
	update();
}

void ContextMenuShortcuts::updateButtonsLayout() {
	if (_buttons.empty()) {
		return;
	}

	const auto buttonSize = ShortcutButtonSize();
	const auto numButtons = int(_buttons.size());
	const auto totalButtonsWidth = buttonSize * numButtons;
	const auto autoVPadding = (_height > buttonSize)
		? (_height - buttonSize) / 2
		: 0;

	if (numButtons == 1) {
		_buttons[0]->setGeometry(
			(width() - buttonSize) / 2,
			autoVPadding,
			buttonSize,
			buttonSize);
		return;
	}

	// Dynamic spacing and padding calculation
	const auto sideMargin = std::max(
		style::ConvertScale(10),
		_st.itemIconPosition.x());
	const auto availableForGaps = width() - totalButtonsWidth - (sideMargin * 2);

	int spacing = 0;
	int startX = 0;

	if (availableForGaps > 0) {
		const auto rawSpacing = availableForGaps / (numButtons - 1);
		const auto maxSpacing = style::ConvertScale(20);
		spacing = std::min(rawSpacing, maxSpacing);

		const auto contentWidth = totalButtonsWidth + (spacing * (numButtons - 1));
		startX = (width() - contentWidth) / 2;
	} else {
		spacing = std::max(
			style::ConvertScale(2),
			(width() - totalButtonsWidth) / (numButtons - 1));
		const auto contentWidth = totalButtonsWidth + (spacing * (numButtons - 1));
		startX = std::max(0, (width() - contentWidth) / 2);
	}

	auto x = startX;
	for (int i = 0; i < numButtons; ++i) {
		_buttons[i]->setGeometry(
			x,
			autoVPadding,
			buttonSize,
			buttonSize);
		x += buttonSize + spacing;
	}
}

[[nodiscard]] UserData* GetReplyableUser(HistoryItem *item) {
	if (!item || !item->isRegular()) {
		return nullptr;
	}

	const auto displayFrom = item->displayFrom();
	const auto from = displayFrom ? displayFrom : item->from().get();
	if (!from || !from->isUser()) {
		return nullptr;
	}

	const auto user = from->asUser();
	if (from == item->history()->peer) {
		return nullptr;
	}
	if (user->isSelf() || user->isInaccessible()) {
		return nullptr;
	}
	return user;
}

void ExecuteReplyInPrivate(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user,
		FullMsgId messageId,
		const TextWithEntities &quote,
		int quoteOffset) {
	const auto history = user->owner().history(user);
	auto reply = FullReplyTo{
		.messageId = messageId,
		.quote = quote,
		.quoteOffset = quoteOffset,
	};

	const auto existingDraft = history->localDraft(MsgId(0), PeerId(0));
	const auto textWithTags = existingDraft
		? existingDraft->textWithTags
		: TextWithTags();
	const auto cursor = existingDraft
		? existingDraft->cursor
		: MessageCursor();

	history->setLocalDraft(std::make_unique<Data::Draft>(
		textWithTags,
		reply,
		SuggestOptions(),
		cursor,
		Data::WebPageDraft()));

	history->clearLocalEditDraft(MsgId(0), PeerId(0));
	history->session().changes().entryUpdated(
		history,
		Data::EntryUpdate::Flag::LocalDraftSet);

	controller->showPeerHistory(
		user,
		Window::SectionShow::Way::Forward,
		ShowAtUnreadMsgId);
}

} // namespace

bool HasShortcut(
		const std::set<ShortcutType> &shortcuts,
		ShortcutType type) {
	return shortcuts.find(type) != shortcuts.end();
}

AvailableShortcuts GetAvailableShortcutsFlags(
		const std::set<ShortcutType> &shortcuts) {
	auto result = AvailableShortcuts();
	result.reply = HasShortcut(shortcuts, ShortcutType::Reply);
	result.copy = HasShortcut(shortcuts, ShortcutType::Copy);
	result.edit = HasShortcut(shortcuts, ShortcutType::Edit);
	result.pin = HasShortcut(shortcuts, ShortcutType::Pin)
		|| HasShortcut(shortcuts, ShortcutType::Unpin);
	result.copyLink = HasShortcut(shortcuts, ShortcutType::CopyLink);
	result.translate = HasShortcut(shortcuts, ShortcutType::Translate);
	result.forward = HasShortcut(shortcuts, ShortcutType::Forward);
	result.saveFile = HasShortcut(shortcuts, ShortcutType::SaveFile);
	return result;
}

std::set<ShortcutType> GetAvailableShortcuts(
		not_null<HistoryItem*> item,
		Fn<bool(HistoryItem*)> hasCopyRestriction,
		std::vector<not_null<HistoryItem*>> items) {
	if (!FASettings::FASettings::getInstance().contextMenuUseShortcuts()) {
		return {};
	}
	if (items.empty()) {
		items.push_back(item);
	}

	std::set<ShortcutType> result;
	for (const auto &it : items) {
		const auto canReply = it->isRegular();
		const auto copyRestriction = hasCopyRestriction
			? hasCopyRestriction(it)
			: false;
		const auto canCopy = !it->clipboardText().empty() && !copyRestriction;
		const auto canLink = it->hasDirectLink();
		const auto media = it->media();
		const auto photo = media ? media->photo() : nullptr;
		const auto document = media ? media->document() : nullptr;
		const auto canGallery = photo
			|| (document && (document->isVideoFile() || document->isGifv()));

		const auto hasDocumentOnly = document && !canGallery;
		const auto hasText = !it->originalText().text.isEmpty();
		const auto canEdit = it->allowsEdit(base::unixtime::now());
		const auto canPin = it->canPin();
		const auto isPinned = it->isPinned();

		if (canReply) {
			result.insert(ShortcutType::Reply);
		}
		if (canCopy) {
			result.insert(ShortcutType::Copy);
		} else if (canEdit) {
			result.insert(ShortcutType::Edit);
		}
		if (canLink) {
			result.insert(ShortcutType::CopyLink);
		} else if (canPin) {
			result.insert(isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
		}

		bool addedFourthButton = false;
		if (canGallery && !copyRestriction) {
			result.insert(ShortcutType::SaveFile);
			addedFourthButton = true;
		} else if (hasDocumentOnly && !copyRestriction) {
			result.insert(ShortcutType::SaveFile);
			addedFourthButton = true;
		} else if (hasText && !Ui::SkipTranslate(it->originalText())) {
			result.insert(ShortcutType::Translate);
			addedFourthButton = true;
		}

		if (!addedFourthButton && canPin && canLink) {
			result.insert(isPinned ? ShortcutType::Unpin : ShortcutType::Pin);
		}
	}

	return result;
}

ShortcutsResult CreateShortcutsWidget(
		not_null<Ui::Menu::Menu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote,
		std::vector<not_null<HistoryItem*>> saveItems) {
	if (!FASettings::FASettings::getInstance().contextMenuUseShortcuts()) {
		return { nullptr, {} };
	}

	auto widget = base::make_unique_q<ContextMenuShortcuts>(
		menu,
		menu->st(),
		item,
		controller,
		std::move(callbacks),
		std::move(quote),
		std::move(saveItems));

	auto addedShortcuts = widget ? widget->addedShortcuts() : std::set<ShortcutType>{};
	return { std::move(widget), std::move(addedShortcuts) };
}

ShortcutsResult SetupShortcuts(
		not_null<Ui::PopupMenu*> menu,
		const HistoryView::ContextMenuRequest &request,
		not_null<HistoryView::ListWidget*> list) {
	if (!FASettings::FASettings::getInstance().contextMenuUseShortcuts() || !request.item) {
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
	callbacks.clearSelection = [list]() {
		list->cancelSelection();
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

	std::vector<not_null<HistoryItem*>> saveItems;
	if (request.overSelection && !request.selectedItems.empty()) {
		saveItems.reserve(request.selectedItems.size());
		for (const auto &selected : request.selectedItems) {
			if (const auto item = list->session().data().message(selected.msgId)) {
				saveItems.push_back(item);
			}
		}
	}

	auto result = CreateShortcutsWidget(
		menu->menu(),
		request.item,
		list->controller(),
		std::move(callbacks),
		request.quote,
		std::move(saveItems));

	if (result.widget) {
		if (!FASettings::FASettings::getInstance().contextMenuShortcutsAtBottom()) {
			menu->insertAction(0, std::move(result.widget));
		}
	}
	return result;
}

ShortcutsResult SetupShortcuts(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote,
		std::vector<not_null<HistoryItem*>> saveItems) {
	if (!FASettings::FASettings::getInstance().contextMenuUseShortcuts()) {
		return { nullptr, {} };
	}

	auto result = CreateShortcutsWidget(
		menu->menu(),
		item,
		controller,
		std::move(callbacks),
		std::move(quote),
		std::move(saveItems));

	if (result.widget) {
		if (FASettings::FASettings::getInstance().contextMenuShortcutsAtBottom()) {
			menu->addAction(std::move(result.widget));
		} else {
			menu->insertAction(0, std::move(result.widget));
		}
	}
	return result;
}

bool AddReplyInPrivate(
		not_null<Ui::PopupMenu*> menu,
		const HistoryView::ContextMenuRequest &request,
		not_null<HistoryView::ListWidget*> list) {
	if (!FASettings::FASettings::getInstance().contextMenuReplyInPrivate()) {
		return false;
	}

	const auto item = request.quote.item
		? request.quote.item
		: request.item;

	const auto user = GetReplyableUser(item);
	if (!user || !item->allowsForward()) {
		return false;
	}

	const auto itemId = item->fullId();
	const auto &quote = request.quote;
	const auto controller = list->controller();

	menu->addAction(
		fatr::fa_reply_in_private_chat(fatr::now),
		[=] {
			ExecuteReplyInPrivate(
				controller,
				user,
				itemId,
				quote.highlight.quote,
				quote.highlight.quoteOffset);
		},
		&st::menuIconReply);

	return true;
}

bool AddReplyInPrivate(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		HistoryView::SelectedQuote quote) {
	if (!FASettings::FASettings::getInstance().contextMenuReplyInPrivate()) {
		return false;
	}

	const auto replyToItem = quote.item ? quote.item : item.get();
	const auto user = GetReplyableUser(replyToItem);
	if (!user || !replyToItem->allowsForward()) {
		return false;
	}

	const auto itemId = replyToItem->fullId();
	const auto quoteText = quote.highlight.quote;
	const auto quoteOffset = quote.highlight.quoteOffset;

	menu->addAction(
		fatr::fa_reply_in_private_chat(fatr::now),
		[=] {
			ExecuteReplyInPrivate(
				controller,
				user,
				itemId,
				quoteText,
				quoteOffset);
		},
		&st::menuIconReply);

	return true;
}

bool AddForwardSubmenu(
		not_null<Ui::PopupMenu*> menu,
		const QString &text,
		MessageIdsList ids,
		not_null<Window::SessionNavigation*> navigation,
		Fn<void()> callback,
		bool hasMediaWithCaption) {
	if (ids.empty()) {
		return false;
	}

	if (!FASettings::FASettings::getInstance().contextMenuForwardSubmenu()) {
		menu->addAction(
			text,
			[=] {
				auto idsCopy = ids;
				Window::ShowForwardMessagesBox(navigation, std::move(idsCopy), Fn<void()>(callback));
			},
			&st::menuIconForward);
		return true;
	}

	const auto forwardAction = menu->addAction(
		text,
		[=] {
			auto idsCopy = ids;
			Window::ShowForwardMessagesBox(navigation, std::move(idsCopy), Fn<void()>(callback));
		},
		&st::menuIconForward);

	forwardAction->setMenu(Ui::CreateChild<QMenu>(menu->menu().get()));
	const auto submenu = menu->ensureSubmenu(forwardAction, st::faContextMenu);

	submenu->addAction(
		fatr::fa_forward_with_author(fatr::now),
		[=] {
			auto idsCopy = ids;
			Window::ShowForwardMessagesBox(navigation, std::move(idsCopy), Fn<void()>(callback));
		},
		&st::menuIconForward);

	submenu->addAction(
		fatr::fa_forward_as_copy(fatr::now),
		[=] {
			auto draft = Data::ForwardDraft{
				.ids = ids,
				.options = Data::ForwardOptions::NoSenderNames,
			};
			Window::ShowForwardMessagesBox(navigation, std::move(draft), Fn<void()>(callback));
		},
		&st::menuIconCopy);

	if (hasMediaWithCaption) {
		submenu->addAction(
			fatr::fa_forward_without_caption(fatr::now),
			[=] {
				auto draft = Data::ForwardDraft{
					.ids = ids,
					.options = Data::ForwardOptions::NoNamesAndCaptions,
				};
				Window::ShowForwardMessagesBox(navigation, std::move(draft), Fn<void()>(callback));
			},
			&st::menuIconFile);
	}

	submenu->addAction(
		fatr::fa_forward_to_saved(fatr::now),
		[=] {
			auto draft = Data::ForwardDraft{ .ids = ids };
			Window::ForwardToSelf(
				navigation->uiShow(),
				draft);
			if (callback) {
				callback();
			}
		},
		&st::menuIconSavedMessages);

	submenu->addAction(
		fatr::fa_forward_to_saved_as_copy(fatr::now),
		[=] {
			auto draft = Data::ForwardDraft{
				.ids = ids,
				.options = Data::ForwardOptions::NoSenderNames,
			};
			Window::ForwardToSelf(
				navigation->uiShow(),
				draft);
			if (callback) {
				callback();
			}
		},
		&st::menuIconSavedMessages);

	return true;
}

bool AddForwardSubmenu(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionNavigation*> navigation,
		bool asGroup,
		Fn<void()> callback) {
	if (!item->allowsForward()) {
		return false;
	}

	const auto owner = &item->history()->owner();
	const auto itemId = item->fullId();

	const auto getMessageIds = [=]() -> MessageIdsList {
		if (const auto msg = owner->message(itemId)) {
			return asGroup
				? owner->itemOrItsGroup(msg)
				: MessageIdsList{ 1, itemId };
		}
		return {};
	};

	const auto hasCaption = item->media() && item->media()->allowsEditCaption();

	if (!FASettings::FASettings::getInstance().contextMenuForwardSubmenu()) {
		menu->addAction(
			tr::lng_context_forward_msg(tr::now),
			[=] {
				Window::ShowForwardMessagesBox(navigation, getMessageIds(), Fn<void()>(callback));
			},
			&st::menuIconForward);
		return true;
	}

	const auto forwardAction = menu->addAction(
		tr::lng_context_forward_msg(tr::now),
		[=] {
			Window::ShowForwardMessagesBox(navigation, getMessageIds(), Fn<void()>(callback));
		},
		&st::menuIconForward);

	forwardAction->setMenu(Ui::CreateChild<QMenu>(menu->menu().get()));
	const auto submenu = menu->ensureSubmenu(forwardAction, st::faContextMenu);

	submenu->addAction(
		fatr::fa_forward_with_author(fatr::now),
		[=] {
			Window::ShowForwardMessagesBox(navigation, getMessageIds(), Fn<void()>(callback));
		},
		&st::menuIconForward);

	submenu->addAction(
		fatr::fa_forward_as_copy(fatr::now),
		[=] {
			const auto ids = getMessageIds();
			if (!ids.empty()) {
				auto draft = Data::ForwardDraft{
					.ids = ids,
					.options = Data::ForwardOptions::NoSenderNames,
				};
				Window::ShowForwardMessagesBox(navigation, std::move(draft), Fn<void()>(callback));
			}
		},
		&st::menuIconCopy);

	if (hasCaption) {
		submenu->addAction(
			fatr::fa_forward_without_caption(fatr::now),
			[=] {
				const auto ids = getMessageIds();
				if (!ids.empty()) {
					auto draft = Data::ForwardDraft{
						.ids = ids,
						.options = Data::ForwardOptions::NoNamesAndCaptions,
					};
					Window::ShowForwardMessagesBox(navigation, std::move(draft), Fn<void()>(callback));
				}
			},
			&st::menuIconFile);
	}

	submenu->addAction(
		fatr::fa_forward_to_saved(fatr::now),
		[=] {
			const auto ids = getMessageIds();
			if (!ids.empty()) {
				auto draft = Data::ForwardDraft{ .ids = ids };
				Window::ForwardToSelf(
					navigation->uiShow(),
					draft);
				if (callback) {
					callback();
				}
			}
		},
		&st::menuIconSavedMessages);

	submenu->addAction(
		fatr::fa_forward_to_saved_as_copy(fatr::now),
		[=] {
			const auto ids = getMessageIds();
			if (!ids.empty()) {
				auto draft = Data::ForwardDraft{
					.ids = ids,
					.options = Data::ForwardOptions::NoSenderNames,
				};
				Window::ForwardToSelf(
					navigation->uiShow(),
					draft);
				if (callback) {
					callback();
				}
			}
		},
		&st::menuIconSavedMessages);

	return true;
}

} // namespace FA::Features::ContextMenu
