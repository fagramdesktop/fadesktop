/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Api {

template <typename Type>
void PerformForUpdate(
		const MTPUpdates &updates,
		Fn<void(const Type &)> callback) {
	updates.match([&](const MTPDupdates &updates) {
		for (const auto &update : updates.vupdates().v) {
			update.match([&](const Type &d) {
				callback(d);
			}, [](const auto &) {
			});
		}
	}, [](const auto &) {
	});
}

} // namespace Api
