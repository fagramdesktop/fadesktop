/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"

namespace Dialogs {
class Entry;
} // namespace Dialogs

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace HistoryView {

struct ChatPreviewAction {
	FullMsgId openItemId;
	bool cancel = false;
	bool openInfo = false;
	bool markRead = false;
	bool markUnread = false;
};

struct ChatPreview {
	base::unique_qptr<Ui::PopupMenu> menu;
	rpl::producer<ChatPreviewAction> actions;
};

[[nodiscard]] ChatPreview MakeChatPreview(
	QWidget *parent,
	not_null<Dialogs::Entry*> entry);

} // namespace HistoryView
