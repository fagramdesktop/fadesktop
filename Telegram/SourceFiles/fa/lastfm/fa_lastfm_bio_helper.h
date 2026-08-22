/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <optional>
#include "base/basic_types.h"

namespace Main {
class Session;
} // namespace Main

namespace Fa::LastFm {

[[nodiscard]] std::optional<QString> ExtractLastFmUsername(const QString &about);

[[nodiscard]] QString StripBioTag(const QString &about);

void SyncBioTag(
	not_null<Main::Session*> session,
	bool showOnProfile,
	const QString &lastFmUsername);

void ScheduleBioTagSync(
	not_null<Main::Session*> session,
	bool showOnProfile,
	const QString &lastFmUsername,
	int delayMs = 1000);

void FlushBioTagSync();

} // namespace Fa::LastFm
