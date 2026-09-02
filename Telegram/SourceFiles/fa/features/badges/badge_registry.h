/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "fa/data/entities.h"
#include "data/data_types.h"

#include <unordered_set>

namespace FA::Badges {

class BadgeRegistry final {
public:
	static BadgeRegistry &getInstance() {
		static BadgeRegistry instance;
		return instance;
	}

	BadgeRegistry(const BadgeRegistry &) = delete;
	BadgeRegistry &operator=(const BadgeRegistry &) = delete;
	BadgeRegistry(BadgeRegistry &&) = delete;
	BadgeRegistry &operator=(BadgeRegistry &&) = delete;

	[[nodiscard]] const std::unordered_set<FaID> &fagramDevelopers() const {
		return _fagramDevelopers;
	}

	[[nodiscard]] const std::unordered_set<FaID> &fagramOfficialChannels() const {
		return _fagramOfficialChannels;
	}

	[[nodiscard]] const std::unordered_set<FaID> &fagramSupporters() const {
		return _fagramSupporters;
	}

	[[nodiscard]] const std::unordered_set<FaID> &fagramSupporterChannels() const {
		return _fagramSupporterChannels;
	}

private:
	BadgeRegistry();

	const std::unordered_set<FaID> _fagramDevelopers;
	const std::unordered_set<FaID> _fagramOfficialChannels;
	const std::unordered_set<FaID> _fagramSupporters;
	const std::unordered_set<FaID> _fagramSupporterChannels = {};
};

} // namespace FA::Badges
