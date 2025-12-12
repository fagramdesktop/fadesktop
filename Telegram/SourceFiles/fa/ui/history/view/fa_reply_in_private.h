/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace HistoryView {
struct ContextMenuRequest;
class ListWidget;
} // namespace HistoryView

namespace FA {

bool AddReplyInPrivateChatAction(
	not_null<Ui::PopupMenu*> menu,
	const HistoryView::ContextMenuRequest &request,
	not_null<HistoryView::ListWidget*> list);

} // namespace FA
