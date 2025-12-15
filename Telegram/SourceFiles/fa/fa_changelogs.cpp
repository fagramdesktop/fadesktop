/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#include "fa/fa_changelogs.h"

#include "fa/fa_version.h"
#include "fa/settings/fa_settings.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "base/qt/qt_common_adapters.h"

namespace FA {
namespace {

std::map<int, const char*> FALogs() {
	return {
	{
		2000009,
		"- Updated to tdesktop v6.3.9\n"
		"- New context menu shortcuts\n"
		"- Service notifications for changelogs\n"
		"- Reply in Private Chat\n"
		"- Advanced forward options\n"
		"- Support tg://openmessage links\n"
		"- Fixed registration timestamps\n\n"
		"@FAgramDesktop\n"
	},
	};
}

} // namespace

Changelogs::Changelogs(not_null<Main::Session*> session, int oldVersion)
: _session(session)
, _oldVersion(oldVersion) {
	_session->data().chatsListChanges(
	) | rpl::filter([](Data::Folder *folder) {
		return !folder;
	}) | rpl::on_next([=] {
		requestChanges();
	}, _chatsSubscription);
}

std::unique_ptr<Changelogs> Changelogs::Create(
		not_null<Main::Session*> session) {
	const auto oldVersion = FASettings::JsonSettings::GetInt(
		"fa_last_changelog_version");

	if (oldVersion < AppFAVersion) {
		FASettings::JsonSettings::Set(
			"fa_last_changelog_version",
			AppFAVersion);
		FASettings::JsonSettings::Write();
	}

	return (oldVersion > 0 && oldVersion < AppFAVersion)
		? std::make_unique<Changelogs>(session, oldVersion)
		: nullptr;
}

void Changelogs::requestChanges() {
	_chatsSubscription.destroy();
	showChangelog();
}

void Changelogs::showChangelog() {
	for (const auto &[version, changes] : FALogs()) {
		if (_oldVersion < version && version <= AppFAVersion) {
			addLog(version, QString::fromUtf8(changes));
		}
	}
}

void Changelogs::addLog(int version, const QString &changes) {
	const auto text = [&] {
		static const auto simple = u"\n- "_q;
		static const auto separator = QString::fromUtf8("\n\xE2\x80\xA2 ");
		auto result = changes.trimmed();
		if (result.startsWith(base::StringViewMid(simple, 1))) {
			result = separator.mid(1) + result.mid(simple.size() - 1);
		}
		return result.replace(simple, separator);
	}();

	const auto versionStr = FormatFAVersionDisplay(version);
	const auto log = u"FAgram Desktop %1:\n\n%2"_q.arg(versionStr).arg(text);

	auto textWithEntities = TextWithEntities{ log };
	TextUtilities::ParseEntities(
		textWithEntities,
		TextParseLinks | TextParseMentions);
	_session->data().serviceNotification(textWithEntities);
}

QString FormatFAVersionDisplay(int version) {
	return QString::number(version / 1000000)
		+ '.' + QString::number((version % 1000000) / 1000)
		+ ((version % 1000)
			? ('.' + QString::number(version % 1000))
			: QString());
}

} // namespace FA
