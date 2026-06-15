/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "dialogs/dialogs_main_list.h"
#include "ui/text/text.h"

class ChannelData;
class History;
class PeerData;

namespace Data {

struct CommunityLinkedPeer {
	not_null<PeerData*> peer;
	std::optional<bool> visible;

	friend inline bool operator==(
		const CommunityLinkedPeer &,
		const CommunityLinkedPeer &) = default;
};

class CommunityInfo final {
public:
	explicit CommunityInfo(not_null<ChannelData*> channel);

	[[nodiscard]] not_null<ChannelData*> channel() const {
		return _channel;
	}

	void applyLinkedPeers(const QVector<MTPCommunityPeer> &list);
	[[nodiscard]] auto linkedPeers() const
	-> const std::vector<CommunityLinkedPeer> & {
		return _linkedPeers;
	}
	[[nodiscard]] rpl::producer<> linkedPeersValue() const;

	[[nodiscard]] bool collapsedInDialogs() const;
	void collapsedChanged();
	void ensureRowInChatList();

	void registerOne(not_null<History*> history);
	void unregisterOne(not_null<History*> history);
	[[nodiscard]] auto histories() const
	-> const base::flat_set<not_null<History*>> & {
		return _histories;
	}

	[[nodiscard]] not_null<Dialogs::MainList*> chatsList();

	[[nodiscard]] TimeId chatsListDate() const {
		return _chatsListDate;
	}
	void oneChatsListDateChanged(TimeId was, TimeId now);
	void oneListMessageChanged();
	void oneUnreadStateChanged();

	[[nodiscard]] auto lastHistories() const
	-> const std::vector<not_null<History*>> & {
		return _lastHistories;
	}
	void validateListEntryCache();
	[[nodiscard]] const Ui::Text::String &listEntryCache() const {
		return _listEntryCache;
	}

private:
	void recountChatsListDate();
	void reorderLastHistories();
	void updateRowSortPosition();
	void repaintRow();
	void moveHistory(not_null<History*> history, bool nowCollapsed);

	const not_null<ChannelData*> _channel;
	std::vector<CommunityLinkedPeer> _linkedPeers;
	rpl::event_stream<> _linkedPeersChanges;

	base::flat_set<not_null<History*>> _histories;
	std::vector<not_null<History*>> _lastHistories;
	Ui::Text::String _listEntryCache;
	int _listEntryCacheVersion = 0;
	int _chatListViewVersion = 0;
	TimeId _chatsListDate = 0;
	Dialogs::MainList _chatsList;

	rpl::lifetime _lifetime;

};

} // namespace Data
