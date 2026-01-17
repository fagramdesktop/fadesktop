/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/object_ptr.h"

namespace Ui {
class BoxContent;
} // namespace Ui

namespace Window {
class SessionNavigation;
} // namespace Window

[[nodiscard]] object_ptr<Ui::BoxContent> EditDiscussionLinkBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> channel,
	not_null<ChannelData*> chat,
	bool canEdit,
	Fn<void(ChannelData*)> callback);

[[nodiscard]] object_ptr<Ui::BoxContent> EditDiscussionLinkBox(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> channel,
	std::vector<not_null<PeerData*>> &&chats,
	Fn<void(ChannelData*)> callback);

void ShowForumForDiscussionError(
	not_null<Window::SessionNavigation*> navigation);
