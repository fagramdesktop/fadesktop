/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Platform {

inline std::unique_ptr<OverlayWidgetHelper> CreateOverlayWidgetHelper(
		not_null<Ui::RpWindow*> window,
		Fn<void(bool)> maximize) {
	return std::make_unique<DefaultOverlayWidgetHelper>(
		window,
		std::move(maximize));
}

} // namespace Platform
