/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include "data/data_peer.h"
#include "ui/text/text_utilities.h"
#include "fa/settings/fa_settings.h"

[[nodiscard]] QString IDString(not_null<PeerData*> peer);

[[nodiscard]] rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer);

[[nodiscard]] QString parseRegistrationTime(const QString &prefix, int64 regTime);

[[nodiscard]] QString findRegistrationTime(int64 userId);

[[nodiscard]] rpl::producer<TextWithEntities> RegistrationValue(not_null<PeerData*> peer);
