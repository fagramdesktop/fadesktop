/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/fa_changelogs.h"

#include "fa/fa_version.h"
#include "fa/settings/fa_settings.h"
#include "fa/changelog/fa_changelog_peer.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_folder.h"
#include "data/data_peer.h"
#include "base/unixtime.h"
#include "base/qt/qt_common_adapters.h"
#include "base/call_delayed.h"
#include "ui/text/text_utilities.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>
#include <QtCore/QDir>

#include <set>

namespace FA {
namespace {

struct BuiltInChangelog {
	int version;
	const char *changes;
};

const std::vector<BuiltInChangelog> &GetBuiltInChangelogs() {
	static const std::vector<BuiltInChangelog> entries = {
		{
			2001005,
			"- Added option to auto parse markdown hyperlinks\n"
			"- Added option to hide sidebar tab titles\n"
			"- Added option to show online status dot only\n"
			"- New category for context menu settings options\n"
			"- Now black dot shows for any user with 'Last seen a long time ago' status\n"
			"- Show forward without captions option on single-item forward submenu\n"
			"- Updated userpic/avatar rounding to apply in group comments preview too\n"
			"- What's New! chat is now toggleable\n"
			"- Updated translations\n"
			"- Updated source URLs\n"
			"- Some code optimizations\n\n"
			"@FAgramDesktop\n"
		},
		{
			2001006,
			"- Updated tdesktop to v6.4.4.beta\n"
			"- Support search in settings for FAgram Preferences\n"
			"- Fixed copy selected text on context menu shortcut icons mode\n\n"
			"@FAgramDesktop\n"
		},
		{
			2001007,
			"- Updated tdesktop to v6.5.0\n"
			"- Fixed DM button visibility in channels\n\n"
			"@FAgramDesktop\n"
		},
		{
			2001008,
			"- Updated tdesktop to v6.5.1\n"
			"- Fixed some UI related bugs\n\n"
			"@FAgramDesktop\n"
		},
	};
	return entries;
}

QString FormatChangelogText(int version, const QString &changes) {
	static const auto simple = u"\n- "_q;
	static const auto separator = QString::fromUtf8("\n\xE2\x80\xA2 ");
	auto result = changes.trimmed();
	if (result.startsWith(base::StringViewMid(simple, 1))) {
		result = separator.mid(1) + result.mid(simple.size() - 1);
	}
	result = result.replace(simple, separator);

	const auto versionStr = FormatFAVersionDisplay(version);
	return u"FAgram Desktop %1:\n\n%2"_q.arg(versionStr).arg(result);
}

std::vector<std::pair<int, QString>> GetUnshownChangelogs(int oldVersion) {
	std::vector<std::pair<int, QString>> result;

	for (const auto &builtin : GetBuiltInChangelogs()) {
		if (Changelog::IsVersionStored(builtin.version)) {
			continue;
		}

		if (builtin.version <= oldVersion || builtin.version > AppFAVersion) {
			continue;
		}

		result.emplace_back(builtin.version, QString::fromUtf8(builtin.changes));
	}

	std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
		return a.first < b.first;
	});

	return result;
}

} // namespace

Changelogs::Changelogs(not_null<Main::Session*> session, int oldVersion)
: _session(session)
, _oldVersion(oldVersion) {
}

std::unique_ptr<Changelogs> Changelogs::Create(
		not_null<Main::Session*> session) {
	if (!FASettings::JsonSettings::GetBool("enable_whats_new_chat")) {
		return nullptr;
	}

	const auto oldVersion = FASettings::JsonSettings::GetInt(
		"fa_last_changelog_version");

	if (oldVersion < AppFAVersion) {
		FASettings::JsonSettings::Set(
			"fa_last_changelog_version",
			AppFAVersion);
		FASettings::JsonSettings::Write();
	}

	const auto unshown = GetUnshownChangelogs(oldVersion);
	const auto hasUnshown = oldVersion > 0 && !unshown.empty();

	auto changelogs = std::make_unique<Changelogs>(session, oldVersion);

	const auto raw = changelogs.get();
	if (session->data().chatsListLoaded()) {
		raw->initializeAfterChatsLoaded(hasUnshown);
	} else {
		using namespace rpl::mappers;
		session->data().chatsListLoadedEvents(
		) | rpl::filter(_1 == nullptr) | rpl::take(1) | rpl::on_next([=] {
			raw->initializeAfterChatsLoaded(hasUnshown);
		}, session->lifetime());
	}

	return changelogs;
}

void Changelogs::initializeAfterChatsLoaded(bool hasUnshown) {
	Changelog::InitializeChangelogPeer(_session);
	Changelog::LoadStoredMessages(_session);

	if (hasUnshown) {
		processChangelogs();
	} else {
		const auto storedMessages = Changelog::GetStoredMessages();
		if (!storedMessages.empty()) {
			Changelog::RequestChangelogDialogEntry(_session);
		}
	}
}

void Changelogs::processChangelogs() {
	const auto entries = GetUnshownChangelogs(_oldVersion);

	for (const auto &[version, changes] : entries) {
		addLog(version, changes);
	}

	Changelog::RequestChangelogDialogEntry(_session);
}

void Changelogs::addLog(int version, const QString &changes) {
	const auto text = FormatChangelogText(version, changes);
	const auto date = base::unixtime::now();

	const auto item = Changelog::AddChangelogMessage(
		_session,
		version,
		text,
		date);

	Changelog::TriggerChangelogNotification(_session, item);
}

QString FormatFAVersionDisplay(int version) {
	return QString::number(version / 1000000)
		+ '.' + QString::number((version % 1000000) / 1000)
		+ ((version % 1000)
			? ('.' + QString::number(version % 1000))
			: QString());
}

} // namespace FA
