/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"
#include "base/object_ptr.h"
#include "ui/widgets/menu/menu_action.h"
#include "history/view/history_view_element.h"
#include "data/data_msg_id.h"
#include "main/session/session_show.h"

#include <set>

namespace Ui {
class PopupMenu;
namespace Menu {
class Menu;
} // namespace Menu
} // namespace Ui

namespace HistoryView {
struct ContextMenuRequest;
class ListWidget;
} // namespace HistoryView

namespace Window {
class SessionController;
} // namespace Window

class HistoryItem;
class PhotoData;
class DocumentData;
class Painter;

namespace FaHistoryView {

enum class ShortcutType {
	Reply,
	Copy,
	CopyLink,
	Edit,
	Pin,
	Unpin,
	OpenMedia,
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

class ContextMenuShortcuts final : public Ui::Menu::ItemBase {
public:
	ContextMenuShortcuts(
		not_null<Ui::Menu::Menu*> parent,
		const style::Menu &st,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller,
		ShortcutCallbacks callbacks,
		HistoryView::SelectedQuote quote = {});

	bool isEnabled() const override;
	not_null<QAction*> action() const override;

	[[nodiscard]] rpl::producer<bool> forceShown() const;
	[[nodiscard]] const std::set<ShortcutType>& addedShortcuts() const {
		return _addedShortcuts;
	}

protected:
	int contentHeight() const override;

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
	
	std::vector<object_ptr<Ui::AbstractButton>> _buttons;
	std::set<ShortcutType> _addedShortcuts;
	
	int _height = 0;

	rpl::event_stream<bool> _forceShown;
};

struct AddContextMenuShortcutsResult {
	base::unique_qptr<Ui::Menu::ItemBase> widget;
	std::set<ShortcutType> addedShortcuts;
};

[[nodiscard]] std::set<ShortcutType> GetAvailableShortcuts(
	not_null<HistoryItem*> item,
	Fn<bool(HistoryItem*)> hasCopyRestriction = nullptr);

[[nodiscard]] AddContextMenuShortcutsResult AddContextMenuShortcuts(
	not_null<Ui::Menu::Menu*> menu,
	const HistoryView::ContextMenuRequest &request,
	not_null<HistoryView::ListWidget*> list);

[[nodiscard]] AddContextMenuShortcutsResult AddContextMenuShortcuts(
	not_null<Ui::Menu::Menu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller,
	ShortcutCallbacks callbacks,
	HistoryView::SelectedQuote quote = {});

[[nodiscard]] inline bool HasShortcut(
	const std::set<ShortcutType>& shortcuts,
	ShortcutType type) {
	return shortcuts.find(type) != shortcuts.end();
}

} // namespace FaHistoryView