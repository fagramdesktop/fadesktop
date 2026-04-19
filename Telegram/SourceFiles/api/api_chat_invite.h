/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class ChannelData;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Api {

void CheckChatInvite(
	not_null<Window::SessionController*> controller,
	const QString &hash,
	ChannelData *invitePeekChannel = nullptr,
	Fn<void()> loaded = nullptr);

} // namespace Api
