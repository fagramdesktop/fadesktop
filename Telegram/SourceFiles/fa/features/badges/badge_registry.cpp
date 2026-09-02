/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/features/badges/badge_registry.h"

namespace FA::Badges {

BadgeRegistry::BadgeRegistry()
: _fagramDevelopers({
	1623457379,
	6204024154,
})
, _fagramOfficialChannels({
	3269939739,
	3307820237,
	3873208333,
})
, _fagramSupporters({
	350243586,
	1837905355,
	6643283669,
	6168975415,
	7870373162,
	469507810,
}) {
}

} // namespace FA::Badges
