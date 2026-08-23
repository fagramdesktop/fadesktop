/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "fa/lastfm/fa_lastfm_config.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <optional>
#include "base/basic_types.h"

namespace Fa::LastFm {

class Client final : public QObject {
	Q_OBJECT

public:
	static Client &Instance();

	void fetchNowPlaying(
		const QString &username,
		Fn<void(std::optional<LastFmTrack>)> callback,
		const QString &customApiKey = QString());

	void fetchCoverArt(
		const QString &imageUrl,
		Fn<void(QImage)> callback);

	void clearCache();

private:
	explicit Client(QObject *parent = nullptr);

	[[nodiscard]] QString resolveApiKey(const QString &customApiKey) const;
	[[nodiscard]] std::optional<LastFmTrack> parseRecentTracksJson(
		const QByteArray &data) const;

	QNetworkAccessManager _nam;

	struct CachedTrack {
		LastFmTrack track;
		crl::time fetchedAt = 0;
	};
	QHash<QString, CachedTrack> _trackCache;
	QHash<QString, QImage> _artCache;
};

} // namespace Fa::LastFm
