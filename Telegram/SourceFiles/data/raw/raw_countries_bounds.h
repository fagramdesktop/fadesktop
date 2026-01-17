/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QString>

namespace Raw {

struct GeoBounds {
	double minLat = 0.;
	double minLon = 0.;
	double maxLat = 0.;
	double maxLon = 0.;
};

[[nodiscard]] const base::flat_map<QString, GeoBounds> &CountryBounds();

} // namespace Raw
