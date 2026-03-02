/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Data {

class ChannelAdminChanges {
public:
	ChannelAdminChanges(not_null<ChannelData*> channel);

	void add(UserId userId, const QString &rank);
	void remove(UserId userId);

	~ChannelAdminChanges();

private:
	not_null<ChannelData*> _channel;
	base::flat_set<UserId> &_admins;
	base::flat_set<UserId> _changes;
	UserData *_oldCreator = nullptr;

};

class ChannelMemberRankChanges {
public:
	ChannelMemberRankChanges(not_null<ChannelData*> channel);

	void feed(UserId userId, const QString &rank);

	~ChannelMemberRankChanges();

private:
	not_null<ChannelData*> _channel;
	base::flat_map<UserId, QString> &_memberRanks;
	base::flat_set<UserId> _changes;

};

} // namespace Data
