/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class History;

namespace Support {

// Returns histories().request, not api().request.
[[nodiscard]] int SendPreloadRequest(
	not_null<History*> history,
	Fn<void()> retry);

} // namespace Support
