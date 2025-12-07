/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#pragma once

#include "data/data_peer.h"
#include "ui/text/text_utilities.h"
#include "fa/settings/fa_settings.h"

QString IDString(not_null<PeerData*> peer);

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer);

QString parseRegistrationTime(QString prefix, long long regTime);

QString findRegistrationTime(long long userId);

[[nodiscard]] rpl::producer<TextWithEntities> RegistrationValue(not_null<PeerData*> peer_id);
