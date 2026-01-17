/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Data {
class ChatFilter;
struct ChatFilterTitle;
} // namespace Data

namespace Api {

void SaveNewFilterPinned(
	not_null<Main::Session*> session,
	FilterId filterId);

void CheckFilterInvite(
	not_null<Window::SessionController*> controller,
	const QString &slug);

void ProcessFilterUpdate(
	base::weak_ptr<Window::SessionController> weak,
	FilterId filterId,
	std::vector<not_null<PeerData*>> missing);

void ProcessFilterRemove(
	base::weak_ptr<Window::SessionController> weak,
	Data::ChatFilterTitle title,
	QString iconEmoji,
	std::vector<not_null<PeerData*>> all,
	std::vector<not_null<PeerData*>> suggest,
	Fn<void(std::vector<not_null<PeerData*>>)> done);

[[nodiscard]] std::vector<not_null<PeerData*>> ExtractSuggestRemoving(
	const Data::ChatFilter &filter);

} // namespace Api
