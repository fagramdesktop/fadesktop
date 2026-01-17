/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/effects/animation_value.h"

namespace Statistic {

class LinesFilterController final {
public:
	LinesFilterController();

	void setEnabled(int id, bool enabled, crl::time now);
	[[nodiscard]] bool isEnabled(int id) const;
	[[nodiscard]] bool isFinished() const;
	[[nodiscard]] float64 alpha(int id) const;

	void tick(float64 dtSpeed);

private:
	struct Entry final {
		bool enabled = false;
		crl::time startedAt = 0;
		float64 alpha = 1.;
		anim::value anim;
		float64 dtCurrent = 0.;
	};

	base::flat_map<int, Entry> _entries;
	bool _isFinished = true;

};

} // namespace Statistic
