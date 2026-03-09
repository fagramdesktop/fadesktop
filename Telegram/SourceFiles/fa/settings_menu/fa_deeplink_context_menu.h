/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class QWidget;

namespace Window {
class SessionController;
} // namespace Window

namespace Settings::FADeepLinkMenu {

void AttachSettingsContextMenu(
	not_null<QWidget*> widget,
	const QString &controlId,
	not_null<Window::SessionController*> controller);

} // namespace Settings::FADeepLinkMenu
