/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/toast/toast.h"

class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace ChatHelpers {

struct ForwardedMessagePhraseArgs final {
	size_t toCount = 0;
	bool singleMessage = false;
	PeerData *to1 = nullptr;
	PeerData *to2 = nullptr;
	bool toSelfWithPremiumIsEmpty = true;
};

[[nodiscard]] rpl::producer<TextWithEntities> ForwardedMessagePhrase(
	const ForwardedMessagePhraseArgs &args);

[[nodiscard]] QString ForwardedMessagePhraseIcon(
	const ForwardedMessagePhraseArgs &args);

[[nodiscard]] Ui::Toast::ClickHandlerFilter ForwardedToSavedMessagesFilter(
	not_null<Main::Session*> session);

} // namespace ChatHelpers
