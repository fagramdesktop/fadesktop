/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#include "fa/changelog/fa_changelog_peer.h"

#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_peer.h"
#include "data/data_histories.h"
#include "data/data_types.h"
#include "data/data_thread.h"
#include "history/history.h"
#include "history/history_item.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "window/notifications_manager.h"
#include "base/unixtime.h"
#include "api/api_text_entities.h"
#include "ui/text/text_utilities.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <set>

namespace FA::Changelog {
namespace {

constexpr auto kChangelogPeerName = "What's New!";
constexpr auto kMaxChangelogEntries = 15;

constexpr auto kChangelogPeerIdShift = (0xFEULL << 32);
constexpr auto kChangelogPeerIdBase = 696969ULL;

MsgId _nextLocalMsgId = MsgId(1);
bool _storageLoaded = false;

QJsonObject _cachedStorage;
bool _storageCacheValid = false;

void InvalidateStorageCache() {
	_cachedStorage = QJsonObject();
	_storageCacheValid = false;
}

QString StoragePath() {
	return cWorkingDir() + u"tdata/fa-changelog-history.json"_q;
}

bool EnsureStorageDirectory() {
	const auto path = StoragePath();
	const auto dir = QFileInfo(path).absolutePath();
	return QDir().mkpath(dir);
}

QJsonObject ReadStorage() {
	if (_storageCacheValid) {
		return _cachedStorage;
	}

	QFile file(StoragePath());
	if (!file.exists()) {
		_cachedStorage = QJsonObject();
		_storageCacheValid = true;
		return _cachedStorage;
	}
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QJsonObject();
	}
	const auto data = file.readAll();
	file.close();

	QJsonParseError error;
	const auto doc = QJsonDocument::fromJson(data, &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject()) {
		return QJsonObject();
	}

	_cachedStorage = doc.object();
	_storageCacheValid = true;
	return _cachedStorage;
}

void WriteStorage(const QJsonObject &root) {
	EnsureStorageDirectory();

	QFile file(StoragePath());
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}
	const auto doc = QJsonDocument(root);
	file.write(doc.toJson(QJsonDocument::Indented));
	file.close();

	_cachedStorage = root;
	_storageCacheValid = true;
}

void UpdateNextMsgIdFromStorage() {
	const auto root = ReadStorage();
	const auto messages = root["messages"].toArray();

	MsgId maxId = MsgId(0);
	for (const auto &value : messages) {
		if (value.isObject()) {
			const auto obj = value.toObject();
			const auto id = MsgId(obj["id"].toVariant().toLongLong());
			if (id > maxId) {
				maxId = id;
			}
		}
	}
	_nextLocalMsgId = MsgId(maxId.bare + 1);
	if (_nextLocalMsgId < MsgId(1)) {
		_nextLocalMsgId = MsgId(1);
	}
}

} // namespace

PeerId GetChangelogPeerId() {
	return peerFromUser(UserId(kChangelogPeerIdShift + kChangelogPeerIdBase));
}

QString GetChangelogPeerName() {
	return QString::fromUtf8(kChangelogPeerName);
}

QString GetChangelogStoragePath() {
	return StoragePath();
}

void InitializeChangelogPeer(not_null<Main::Session*> session) {
	const auto peerId = GetChangelogPeerId();

	if (session->data().peerLoaded(peerId)) {
		return;
	}

	session->data().processUser(MTP_user(
		MTP_flags(
			MTPDuser::Flag::f_first_name
			| MTPDuser::Flag::f_verified
			| MTPDuser::Flag::f_support),
		MTP_long(peerToUser(peerId).bare),
		MTPlong(),
		MTP_string(GetChangelogPeerName()),
		MTPstring(),
		MTPstring(),
		MTPstring(),
		MTP_userProfilePhotoEmpty(),
		MTP_userStatusRecently(MTP_flags(0)),
		MTPint(),
		MTPVector<MTPRestrictionReason>(),
		MTPstring(),
		MTPstring(),
		MTPEmojiStatus(),
		MTPVector<MTPUsername>(),
		MTPRecentStory(),
		MTPPeerColor(),
		MTPPeerColor(),
		MTPint(),
		MTPlong(),
		MTPlong()));
}

bool IsChangelogPeerInitialized(not_null<Main::Session*> session) {
	return session->data().peerLoaded(GetChangelogPeerId()) != nullptr;
}

History* GetChangelogHistory(not_null<Main::Session*> session) {
	InitializeChangelogPeer(session);
	return session->data().history(GetChangelogPeerId());
}

MsgId GetNextChangelogMessageId(not_null<Data::Session*> data) {
	if (!_storageLoaded) {
		UpdateNextMsgIdFromStorage();
		_storageLoaded = true;
	}
	return _nextLocalMsgId++;
}

std::vector<StoredMessage> GetStoredMessages() {
	std::vector<StoredMessage> result;

	const auto root = ReadStorage();
	const auto messages = root["messages"].toArray();

	for (const auto &value : messages) {
		if (!value.isObject()) {
			continue;
		}
		const auto obj = value.toObject();

		StoredMessage msg;
		msg.id = MsgId(obj["id"].toVariant().toLongLong());
		msg.version = obj["version"].toInt();
		msg.text = obj["text"].toString();
		msg.date = TimeId(obj["date"].toVariant().toLongLong());
		msg.unread = obj["unread"].toBool(false);

		if (msg.id > MsgId(0) && !msg.text.isEmpty()) {
			result.push_back(std::move(msg));
		}
	}

	std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
		return a.date < b.date;
	});

	return result;
}

bool IsVersionStored(int version) {
	const auto root = ReadStorage();
	const auto messages = root["messages"].toArray();

	for (const auto &value : messages) {
		if (value.isObject()) {
			if (value.toObject()["version"].toInt() == version) {
				return true;
			}
		}
	}
	return false;
}

void PruneOldChangelogEntries(QJsonArray &messages) {
	if (messages.size() <= kMaxChangelogEntries) {
		return;
	}

	std::vector<std::pair<qint64, int>> dateIndexPairs;
	dateIndexPairs.reserve(messages.size());
	for (int i = 0; i < messages.size(); ++i) {
		if (messages[i].isObject()) {
			const auto date = messages[i].toObject()["date"].toVariant().toLongLong();
			dateIndexPairs.push_back({date, i});
		}
	}

	std::sort(dateIndexPairs.begin(), dateIndexPairs.end());

	const auto toRemove = messages.size() - kMaxChangelogEntries;
	std::set<int> indicesToRemove;
	for (size_t i = 0; i < toRemove && i < dateIndexPairs.size(); ++i) {
		indicesToRemove.insert(dateIndexPairs[i].second);
	}

	QJsonArray prunedMessages;
	for (int i = 0; i < messages.size(); ++i) {
		if (indicesToRemove.find(i) == indicesToRemove.end()) {
			prunedMessages.append(messages[i]);
		}
	}
	messages = prunedMessages;
}

void SaveMessageToStorage(const StoredMessage &message) {
	auto root = ReadStorage();
	auto messages = root["messages"].toArray();

	for (int i = 0; i < messages.size(); ++i) {
		if (messages[i].isObject()) {
			if (messages[i].toObject()["id"].toVariant().toLongLong() == message.id.bare) {
				QJsonObject obj;
				obj["id"] = qint64(message.id.bare);
				obj["version"] = message.version;
				obj["text"] = message.text;
				obj["date"] = qint64(message.date);
				obj["unread"] = message.unread;
				messages[i] = obj;
				root["messages"] = messages;
				WriteStorage(root);
				return;
			}
		}
	}

	QJsonObject obj;
	obj["id"] = qint64(message.id.bare);
	obj["version"] = message.version;
	obj["text"] = message.text;
	obj["date"] = qint64(message.date);
	obj["unread"] = message.unread;
	messages.append(obj);

	PruneOldChangelogEntries(messages);

	root["messages"] = messages;
	WriteStorage(root);
}

void MarkMessageReadInStorage(MsgId id) {
	auto root = ReadStorage();
	auto messages = root["messages"].toArray();

	for (int i = 0; i < messages.size(); ++i) {
		if (messages[i].isObject()) {
			auto obj = messages[i].toObject();
			if (obj["id"].toVariant().toLongLong() == id.bare) {
				obj["unread"] = false;
				messages[i] = obj;
				root["messages"] = messages;
				WriteStorage(root);
				return;
			}
		}
	}
}

void LoadStoredMessages(not_null<Main::Session*> session) {
	const auto history = GetChangelogHistory(session);
	if (!history) {
		return;
	}

	if (!history->folderKnown()) {
		history->clearFolder();
	}

	const auto messages = GetStoredMessages();
	if (messages.empty()) {
		return;
	}

	const auto peerId = GetChangelogPeerId();

	for (const auto &stored : messages) {
		if (const auto existing = history->owner().message(peerId, stored.id)) {
			if (existing->mainView()) {
				continue;
			}
		}

		const auto flags = MTPDmessage::Flag::f_entities
			| MTPDmessage::Flag::f_from_id;
		const auto localFlags = MessageFlag::Local;

		auto textWithEntities = TextWithEntities{ stored.text };
		TextUtilities::ParseEntities(
			textWithEntities,
			TextParseLinks | TextParseMentions | TextParseHashtags);

		history->addNewMessage(
			stored.id,
			MTP_message(
				MTP_flags(flags),
				MTP_int(int(stored.id.bare & 0x7FFFFFFF)),
				peerToMTP(peerId),
				MTPint(),
				peerToMTP(peerId),
				MTPPeer(),
				MTPMessageFwdHeader(),
				MTPlong(),
				MTPlong(),
				MTPMessageReplyHeader(),
				MTP_int(stored.date),
				MTP_string(stored.text),
				MTP_messageMediaEmpty(),
				MTPReplyMarkup(),
				Api::EntitiesToMTP(session, textWithEntities.entities),
				MTPint(),
				MTPint(),
				MTPMessageReplies(),
				MTPint(),
				MTPstring(),
				MTPlong(),
				MTPMessageReactions(),
				MTPVector<MTPRestrictionReason>(),
				MTPint(),
				MTPint(),
				MTPlong(),
				MTPFactCheck(),
				MTPint(),
				MTPlong(),
				MTPSuggestedPost(),
				MTPint(),
				MTPstring()),
			localFlags,
			NewMessageType::Unread);
	}

	session->data().sendHistoryChangeNotifications();

	if (history->lastMessage() || !history->isEmpty()) {
		history->updateChatListSortPosition();
		history->updateChatListExistence();
		history->updateChatListEntry();
	}
}

not_null<HistoryItem*> AddChangelogMessage(
		not_null<Main::Session*> session,
		int version,
		const QString &text,
		TimeId date) {
	InitializeChangelogPeer(session);

	const auto history = GetChangelogHistory(session);
	Expects(history != nullptr);

	if (!history->folderKnown()) {
		history->clearFolder();
	}

	const auto peerId = GetChangelogPeerId();
	const auto msgId = GetNextChangelogMessageId(&session->data());
	const auto msgDate = date ? date : base::unixtime::now();

	StoredMessage stored;
	stored.id = msgId;
	stored.version = version;
	stored.text = text;
	stored.date = msgDate;
	stored.unread = true;
	SaveMessageToStorage(stored);

	const auto flags = MTPDmessage::Flag::f_entities
		| MTPDmessage::Flag::f_from_id;
	const auto localFlags = MessageFlag::ClientSideUnread
		| MessageFlag::Local;

	auto textWithEntities = TextWithEntities{ text };
	TextUtilities::ParseEntities(
		textWithEntities,
		TextParseLinks | TextParseMentions | TextParseHashtags);

	const auto item = history->addNewMessage(
		msgId,
		MTP_message(
			MTP_flags(flags),
			MTP_int(int(msgId.bare & 0x7FFFFFFF)),
			peerToMTP(peerId),
			MTPint(),
			peerToMTP(peerId),
			MTPPeer(),
			MTPMessageFwdHeader(),
			MTPlong(),
			MTPlong(),
			MTPMessageReplyHeader(),
			MTP_int(msgDate),
			MTP_string(text),
			MTP_messageMediaEmpty(),
			MTPReplyMarkup(),
			Api::EntitiesToMTP(session, textWithEntities.entities),
			MTPint(),
			MTPint(),
			MTPMessageReplies(),
			MTPint(),
			MTPstring(),
			MTPlong(),
			MTPMessageReactions(),
			MTPVector<MTPRestrictionReason>(),
			MTPint(),
			MTPint(),
			MTPlong(),
			MTPFactCheck(),
			MTPint(),
			MTPlong(),
			MTPSuggestedPost(),
			MTPint(),
			MTPstring()),
		localFlags,
		NewMessageType::Unread);

	session->data().sendHistoryChangeNotifications();

	history->updateChatListSortPosition();
	history->updateChatListExistence();
	history->updateChatListEntry();

	return item;
}

void TriggerChangelogNotification(
		not_null<Main::Session*> session,
		not_null<HistoryItem*> item) {
	Core::App().notifications().schedule(Data::ItemNotification{
		.item = item,
		.reactionSender = nullptr,
		.type = Data::ItemNotificationType::Message,
	});
}

void RequestChangelogDialogEntry(not_null<Main::Session*> session) {
	const auto history = GetChangelogHistory(session);
	if (!history) {
		return;
	}

	if (!history->folderKnown()) {
		history->clearFolder();
	}

	if (history->lastMessage() || !history->isEmpty()) {
		history->updateChatListSortPosition();
		history->updateChatListExistence();
		history->updateChatListEntry();
	}
}

} // namespace FA::Changelog
