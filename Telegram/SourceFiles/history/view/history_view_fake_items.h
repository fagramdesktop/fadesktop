/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "history/admin_log/history_admin_log_item.h"

namespace HistoryView {

[[nodiscard]] AdminLog::OwnedItem GenerateItem(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<History*> history,
	PeerId from,
	FullMsgId replyTo,
	const QString &text,
	EffectId effectId = 0);

[[nodiscard]] PeerId GenerateUser(
	not_null<History*> history,
	const QString &name);

} // namespace HistoryView
