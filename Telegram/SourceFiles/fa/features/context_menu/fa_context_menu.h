/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"
#include "data/data_msg_id.h"
#include "history/view/history_view_element.h"
#include "main/session/session_show.h"

#include <set>
#include <vector>

namespace Ui {
class PopupMenu;
namespace Menu {
class Menu;
class ItemBase;
} // namespace Menu
} // namespace Ui

namespace HistoryView {
struct ContextMenuRequest;
class ListWidget;
} // namespace HistoryView

namespace Window {
class SessionController;
class SessionNavigation;
} // namespace Window

class HistoryItem;
class PhotoData;
class DocumentData;

namespace FA::Features::ContextMenu {

enum class ShortcutType {
	Reply,
	Copy,
	CopyLink,
	Edit,
	Pin,
	Unpin,
	SaveFile,
	Translate,
	Forward,
};

struct ShortcutCallbacks {
	Fn<bool(HistoryItem*)> hasCopyRestriction = nullptr;
	Fn<bool(HistoryItem*)> showCopyRestriction = nullptr;
	Fn<void(FullReplyTo, bool)> replyToMessage = nullptr;
	Fn<void(FullMsgId)> editMessage = nullptr;
	Fn<void(FullMsgId)> forwardMessage = nullptr;
	Fn<std::shared_ptr<Main::SessionShow>()> uiShow = nullptr;
	Fn<HistoryView::Context()> elementContext = nullptr;
	Fn<void(not_null<PhotoData*>, FullMsgId)> openPhoto = nullptr;
	Fn<void(not_null<DocumentData*>, FullMsgId, bool)> openDocument = nullptr;
	Fn<void(not_null<PhotoData*>)> savePhoto = nullptr;
	Fn<void()> hideMenu = nullptr;
	Fn<void()> clearSelection = nullptr;
};

struct ShortcutsResult {
	base::unique_qptr<Ui::Menu::ItemBase> widget;
	std::set<ShortcutType> addedShortcuts;
};

struct AvailableShortcuts {
	bool reply = false;
	bool copy = false;
	bool edit = false;
	bool pin = false;
	bool copyLink = false;
	bool translate = false;
	bool forward = false;
	bool saveFile = false;
};

[[nodiscard]] AvailableShortcuts GetAvailableShortcutsFlags(
	const std::set<ShortcutType> &shortcuts);

template <typename Container>
[[nodiscard]] std::vector<not_null<HistoryItem*>> CollectItems(const Container &items) {
	std::vector<not_null<HistoryItem*>> result;
	result.reserve(items.size());
	for (const auto &item : items) {
		result.push_back(item);
	}
	return result;
}

[[nodiscard]] bool HasShortcut(
	const std::set<ShortcutType> &shortcuts,
	ShortcutType type);

[[nodiscard]] std::set<ShortcutType> GetAvailableShortcuts(
	not_null<HistoryItem*> item,
	Fn<bool(HistoryItem*)> hasCopyRestriction = nullptr,
	std::vector<not_null<HistoryItem*>> items = {});

[[nodiscard]] ShortcutsResult CreateShortcutsWidget(
		not_null<Ui::Menu::Menu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote = {},
		std::vector<not_null<HistoryItem*>> saveItems = {});

ShortcutsResult SetupShortcuts(
		not_null<Ui::PopupMenu*> menu,
		const HistoryView::ContextMenuRequest &request,
		not_null<HistoryView::ListWidget*> list);

ShortcutsResult SetupShortcuts(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote = {},
		std::vector<not_null<HistoryItem*>> saveItems = {});

bool AddReplyInPrivate(
	not_null<Ui::PopupMenu*> menu,
	const HistoryView::ContextMenuRequest &request,
	not_null<HistoryView::ListWidget*> list);

bool AddReplyInPrivate(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	HistoryView::SelectedQuote quote = {});

bool AddForwardSubmenu(
	not_null<Ui::PopupMenu*> menu,
	const QString &text,
	MessageIdsList ids,
	not_null<Window::SessionNavigation*> navigation,
	Fn<void()> callback = nullptr,
	bool hasMediaWithCaption = false);

bool AddForwardSubmenu(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionNavigation*> navigation,
	bool asGroup = true,
	Fn<void()> callback = nullptr);

} // namespace FA::Features::ContextMenu
