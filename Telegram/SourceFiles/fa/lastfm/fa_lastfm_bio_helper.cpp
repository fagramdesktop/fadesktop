/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/lastfm/fa_lastfm_bio_helper.h"
#include "fa/lastfm/fa_lastfm_config.h"

#include "main/main_session.h"
#include "data/data_user.h"
#include "apiwrap.h"
#include "base/timer.h"
#include "ui/text/text_utilities.h"

namespace Fa::LastFm {
namespace {

struct ScheduledSync {
	base::weak_ptr<Main::Session> session;
	bool showOnProfile = false;
	QString lastFmUsername;
};

std::unique_ptr<base::Timer> g_syncTimer;
std::optional<ScheduledSync> g_scheduledSync;

void PerformSync(const ScheduledSync &data) {
	const auto session = data.session.get();
	if (!session) {
		return;
	}
	SyncBioTag(session, data.showOnProfile, data.lastFmUsername);
}

} // namespace

std::optional<QString> ExtractLastFmUsername(const QString &about) {
	if (about.isEmpty()) {
		return std::nullopt;
	}
	const auto &regex = BioTagRegex();
	const auto match = regex.match(about);
	if (match.hasMatch()) {
		const auto username = match.captured(1).trimmed();
		if (!username.isEmpty()) {
			return username;
		}
	}
	return std::nullopt;
}

QString StripBioTag(const QString &about) {
	if (about.isEmpty()) {
		return QString();
	}
	auto result = about;
	result.remove(BioTagRegex());
	return result.trimmed();
}

void SyncBioTag(
		not_null<Main::Session*> session,
		bool showOnProfile,
		const QString &lastFmUsername) {
	const auto user = session->user();
	const auto currentAbout = user ? user->about() : QString();
	const auto cleanAbout = StripBioTag(currentAbout);

	QString targetAbout;
	const auto trimmedUsername = lastFmUsername.trimmed();
	if (showOnProfile && !trimmedUsername.isEmpty()) {
		const auto tag = u"#np:"_q + trimmedUsername;
		const auto maxLen = session->premium() ? 140 : 70;
		if (cleanAbout.isEmpty()) {
			targetAbout = tag.left(maxLen);
		} else {
			const auto combined = cleanAbout + ' ' + tag;
			if (combined.size() <= maxLen) {
				targetAbout = combined;
			} else {
				const auto maxClean = maxLen - 1 - tag.size();
				if (maxClean > 0) {
					targetAbout = cleanAbout.left(maxClean).trimmed() + ' ' + tag;
				} else {
					targetAbout = tag.left(maxLen);
				}
			}
		}
	} else {
		targetAbout = cleanAbout;
	}

	if (targetAbout != currentAbout) {
		session->api().saveSelfBio(
			TextUtilities::PrepareForSending(targetAbout));
	}
}

void ScheduleBioTagSync(
		not_null<Main::Session*> session,
		bool showOnProfile,
		const QString &lastFmUsername,
		int delayMs) {
	g_scheduledSync = ScheduledSync{
		.session = base::make_weak(session.get()),
		.showOnProfile = showOnProfile,
		.lastFmUsername = lastFmUsername,
	};

	if (!g_syncTimer) {
		g_syncTimer = std::make_unique<base::Timer>([] {
			if (g_scheduledSync) {
				const auto syncData = *g_scheduledSync;
				g_scheduledSync.reset();
				PerformSync(syncData);
			}
		});
	}

	g_syncTimer->callOnce(delayMs);
}

void FlushBioTagSync() {
	if (g_syncTimer && g_syncTimer->isActive()) {
		g_syncTimer->cancel();
		if (g_scheduledSync) {
			const auto syncData = *g_scheduledSync;
			g_scheduledSync.reset();
			PerformSync(syncData);
		}
	}
}

} // namespace Fa::LastFm
