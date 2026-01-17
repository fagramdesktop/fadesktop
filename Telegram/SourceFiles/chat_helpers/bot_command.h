/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class PeerData;
class UserData;

namespace Bot {

struct SendCommandRequest {
	not_null<PeerData*> peer;
	QString command;
	FullMsgId context;
	FullReplyTo replyTo;
};

[[nodiscard]] QString WrapCommandInChat(
	not_null<PeerData*> peer,
	const QString &command,
	const FullMsgId &context);
[[nodiscard]] QString WrapCommandInChat(
	not_null<PeerData*> peer,
	const QString &command,
	not_null<UserData*> bot);

} // namespace Bot
