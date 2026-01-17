/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Ui {
struct RequestsBarContent;
} // namespace Ui

namespace HistoryView {

inline constexpr auto kRecentRequestsLimit = 3;

[[nodiscard]] rpl::producer<Ui::RequestsBarContent> RequestsBarContentByPeer(
	not_null<PeerData*> peer,
	int userpicSize,
	bool showInForum);

} // namespace HistoryView
