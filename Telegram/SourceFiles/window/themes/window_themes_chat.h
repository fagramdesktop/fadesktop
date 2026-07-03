/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "data/data_cloud_themes.h"

namespace Ui {
struct ChatThemeBubblesData;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Window::Theme {

[[nodiscard]] Ui::ChatThemeBubblesData PrepareBubblesData(
	const Data::CloudTheme &theme,
	Data::CloudThemeType type);

void ApplyChatTheme(
	not_null<SessionController*> controller,
	const Data::CloudTheme &theme,
	bool dark);

} // namespace Window::Theme
