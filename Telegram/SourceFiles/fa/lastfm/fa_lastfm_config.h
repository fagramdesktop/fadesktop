/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QRegularExpression>
#include "crl/crl_time.h"

namespace Fa::LastFm {

#if defined TDESKTOP_LASTFM_API_KEY
inline constexpr auto DefaultLastFmApiKey = QT_STRINGIFY(TDESKTOP_LASTFM_API_KEY);
#else
inline constexpr auto DefaultLastFmApiKey = "";
#endif

inline constexpr auto kPollInterval = 15 * crl::time(1000);
inline constexpr auto kCacheTtl = 15 * crl::time(1000);
inline constexpr auto kBioDebounceTimeout = 1000;

struct LastFmTrack {
	QString title;
	QString artist;
	QString album;
	QString url;
	QString imageUrl;
	bool isNowPlaying = false;
	crl::time fetchedAt = 0;
};

[[nodiscard]] inline const QRegularExpression &BioTagRegex() {
	static const auto result = QRegularExpression(
		u"(?:^|\\s)#np:([a-zA-Z0-9_\\-]+)"_q);
	return result;
}

} // namespace Fa::LastFm
