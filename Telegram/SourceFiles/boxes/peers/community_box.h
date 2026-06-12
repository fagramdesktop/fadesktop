/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class ChannelData;
class PeerData;

namespace Ui {
class Show;
} // namespace Ui

namespace Window {
class SessionNavigation;
} // namespace Window

void ShowCommunityBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> community);

void ShowChooseChatToAddBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> community);

void BanFromCommunityWithWarning(
	std::shared_ptr<Ui::Show> show,
	not_null<ChannelData*> community,
	not_null<PeerData*> participant);
