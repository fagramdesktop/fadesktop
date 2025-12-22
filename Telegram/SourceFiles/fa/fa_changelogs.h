/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/weak_ptr.h"
#include "base/basic_types.h"

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

	void processChangelogs();
	void initializeAfterChatsLoaded(bool hasUnshown);

private:
	void addLog(int version, const QString &changes);

	const not_null<Main::Session*> _session;
	const int _oldVersion = 0;

};

} // namespace FA