/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "fa/data/entities.h"
#include "fa/features/badges/badge_registry.h"
#include "info/profile/info_profile_badge.h"

namespace FA::Badges {

[[nodiscard]] FaID getBareID(not_null<PeerData*> peer);

[[nodiscard]] bool isFAgramPeer(FaID peerId);
[[nodiscard]] bool isFAgramSupporterPeer(FaID peerId);

[[nodiscard]] base::flags<Info::Profile::BadgeType> BadgeTypes();
[[nodiscard]] Info::Profile::Badge::Content ComputeBadgeContent(
	not_null<PeerData*> peer);
[[nodiscard]] rpl::producer<Info::Profile::Badge::Content> BadgeContentForPeer(
	not_null<PeerData*> peer);
[[nodiscard]] Fn<void()> badgeClickHandler(not_null<PeerData*> peer);

} // namespace FA::Badges
