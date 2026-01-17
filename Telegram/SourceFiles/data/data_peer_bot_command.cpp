/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "data/data_peer_bot_command.h"

namespace Data {

BotCommand BotCommandFromTL(const MTPBotCommand &result) {
	return result.match([](const MTPDbotCommand &data) {
		return BotCommand {
			.command = qs(data.vcommand().v),
			.description = qs(data.vdescription().v),
		};
	});
}

} // namespace Data
