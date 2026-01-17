/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "mtproto/sender.h"

class ApiWrap;
class HistoryItem;
struct PollData;

namespace Main {
class Session;
} // namespace Main

namespace Api {

struct SendAction;

class Polls final {
public:
	explicit Polls(not_null<ApiWrap*> api);

	void create(
		const PollData &data,
		SendAction action,
		Fn<void()> done,
		Fn<void()> fail);
	void sendVotes(
		FullMsgId itemId,
		const std::vector<QByteArray> &options);
	void close(not_null<HistoryItem*> item);
	void reloadResults(not_null<HistoryItem*> item);

private:
	const not_null<Main::Session*> _session;
	MTP::Sender _api;

	base::flat_map<FullMsgId, mtpRequestId> _pollVotesRequestIds;
	base::flat_map<FullMsgId, mtpRequestId> _pollCloseRequestIds;
	base::flat_map<FullMsgId, mtpRequestId> _pollReloadRequestIds;

};

} // namespace Api
