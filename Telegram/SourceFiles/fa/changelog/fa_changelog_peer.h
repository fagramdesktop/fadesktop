/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "base/weak_ptr.h"
#include "data/data_peer_id.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <vector>

namespace Main {
class Session;
} // namespace Main

namespace Data {
class Session;
} // namespace Data

class History;
class HistoryItem;

namespace FA::Changelog {

[[nodiscard]] PeerId GetChangelogPeerId();

[[nodiscard]] QString GetChangelogPeerName();

[[nodiscard]] QString GetChangelogStoragePath();

struct StoredMessage {
	MsgId id = 0;
	int version = 0;
	QString text;
	TimeId date = 0;
	bool unread = false;
};

void InitializeChangelogPeer(not_null<Main::Session*> session);

[[nodiscard]] bool IsChangelogPeerInitialized(not_null<Main::Session*> session);

[[nodiscard]] History* GetChangelogHistory(not_null<Main::Session*> session);

not_null<HistoryItem*> AddChangelogMessage(
	not_null<Main::Session*> session,
	int version,
	const QString &text,
	TimeId date = 0);

void LoadStoredMessages(not_null<Main::Session*> session);

void SaveMessageToStorage(const StoredMessage &message);

void MarkMessageReadInStorage(MsgId id);

[[nodiscard]] std::vector<StoredMessage> GetStoredMessages();

[[nodiscard]] bool IsVersionStored(int version);

[[nodiscard]] MsgId GetNextChangelogMessageId(not_null<Data::Session*> data);

void TriggerChangelogNotification(
	not_null<Main::Session*> session,
	not_null<HistoryItem*> item);

void RequestChangelogDialogEntry(not_null<Main::Session*> session);

} // namespace FA::Changelog
