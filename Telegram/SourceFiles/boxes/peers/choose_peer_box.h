/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

struct RequestPeerQuery;

namespace Main {
class Session;
class SessionShow;
} // namespace Main

namespace Ui {
class BoxContent;
} // namespace Ui

namespace Window {
class SessionNavigation;
} // namespace Window

void ShowChoosePeerBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<UserData*> bot,
	RequestPeerQuery query,
	Fn<void(std::vector<not_null<PeerData*>>)> chosen,
	Fn<void()> cancelled = nullptr);

void ShowChoosePeerBox(
	std::shared_ptr<Main::SessionShow> show,
	not_null<UserData*> bot,
	RequestPeerQuery query,
	Fn<void(std::vector<not_null<PeerData*>>)> chosen,
	Fn<void()> cancelled = nullptr);
