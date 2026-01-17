/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

void ShowGiftCreditsBox(
	not_null<Window::SessionController*> controller,
	Fn<void()> gifted);

} // namespace Ui
