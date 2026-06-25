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
class RoundButton;
class Show;
class VerticalLayout;
} // namespace Ui

namespace Main {
class SessionShow;
} // namespace Main

namespace Window {
class SessionController;
class SessionNavigation;
} // namespace Window

void SetupCommunityContent(
	not_null<Ui::VerticalLayout*> container,
	not_null<Window::SessionController*> controller,
	not_null<ChannelData*> community,
	std::shared_ptr<Main::SessionShow> show);

void ShowChooseChatToAddBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> community);

[[nodiscard]] not_null<Ui::RoundButton*> MakeCommunityAddChatButton(
	not_null<QWidget*> parent,
	Fn<void()> clicked);

void BanFromCommunityWithWarning(
	std::shared_ptr<Ui::Show> show,
	not_null<ChannelData*> community,
	not_null<PeerData*> participant);

void ShowCommunityAdminBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> community);
