/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/weak_ptr.h"

namespace Main {
class Session;
} // namespace Main

namespace FA {

[[nodiscard]] QString FormatFAVersionDisplay(int version);

class Changelogs final : public base::has_weak_ptr {
public:
	Changelogs(not_null<Main::Session*> session, int oldVersion);

	static std::unique_ptr<Changelogs> Create(
		not_null<Main::Session*> session);

private:
	void requestChanges();
	void showChangelog();
	void addLog(int version, const QString &changes);

	const not_null<Main::Session*> _session;
	const int _oldVersion = 0;
	rpl::lifetime _chatsSubscription;

};

} // namespace FA