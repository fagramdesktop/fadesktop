/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/lastfm/fa_lastfm_client.h"
#include "fa/settings/fa_settings.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtGui/QImageReader>
#include <QtCore/QBuffer>

namespace Fa::LastFm {

Client::Client(QObject *parent) : QObject(parent) {
}

Client &Client::Instance() {
	static Client instance;
	return instance;
}

QString Client::resolveApiKey(const QString &customApiKey) const {
	const auto trimmed = customApiKey.trimmed();
	if (!trimmed.isEmpty()) {
		return trimmed;
	}
	return QString::fromLatin1(DefaultLastFmApiKey);
}

void Client::clearCache() {
	_trackCache.clear();
	_artCache.clear();
}

void Client::fetchNowPlaying(
		const QString &username,
		Fn<void(std::optional<LastFmTrack>)> callback,
		const QString &customApiKey) {
	const auto trimmedUser = username.trimmed();
	if (trimmedUser.isEmpty()) {
		callback(std::nullopt);
		return;
	}

	const auto now = crl::now();
	const auto it = _trackCache.find(trimmedUser);
	if (it != _trackCache.end() && (now - it.value().fetchedAt < kCacheTtl)) {
		callback(it.value().track);
		return;
	}

	const auto apiKey = resolveApiKey(customApiKey);
	auto url = QUrl(u"https://ws.audioscrobbler.com/2.0/"_q);
	auto query = QUrlQuery();
	query.addQueryItem(u"method"_q, u"user.getrecenttracks"_q);
	query.addQueryItem(u"user"_q, trimmedUser);
	query.addQueryItem(u"api_key"_q, apiKey);
	query.addQueryItem(u"format"_q, u"json"_q);
	query.addQueryItem(u"limit"_q, u"1"_q);
	url.setQuery(query);

	auto request = QNetworkRequest(url);
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setRawHeader("User-Agent", "FAgramDesktop/2.5.0");

	const auto reply = _nam.get(request);
	connect(reply, &QNetworkReply::finished, this, [=] {
		reply->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			callback(std::nullopt);
			return;
		}

		const auto data = reply->readAll();
		const auto parsed = parseRecentTracksJson(data);
		if (parsed) {
			_trackCache[trimmedUser] = CachedTrack{
				.track = *parsed,
				.fetchedAt = crl::now(),
			};
			callback(*parsed);
		} else {
			_trackCache.remove(trimmedUser);
			callback(std::nullopt);
		}
	});
}

std::optional<LastFmTrack> Client::parseRecentTracksJson(
		const QByteArray &data) const {
	auto parseError = QJsonParseError();
	const auto doc = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		return std::nullopt;
	}

	const auto root = doc.object();
	if (!root.contains("recenttracks") || !root["recenttracks"].isObject()) {
		return std::nullopt;
	}

	const auto recenttracks = root["recenttracks"].toObject();
	if (!recenttracks.contains("track")) {
		return std::nullopt;
	}

	QJsonObject trackObj;
	const auto trackVal = recenttracks["track"];
	if (trackVal.isArray()) {
		const auto arr = trackVal.toArray();
		if (arr.isEmpty() || !arr[0].isObject()) {
			return std::nullopt;
		}
		trackObj = arr[0].toObject();
	} else if (trackVal.isObject()) {
		trackObj = trackVal.toObject();
	} else {
		return std::nullopt;
	}

	bool isNowPlaying = false;
	if (trackObj.contains("@attr") && trackObj["@attr"].isObject()) {
		const auto attr = trackObj["@attr"].toObject();
		if (attr.contains("nowplaying")
			&& attr["nowplaying"].toString() == u"true"_q) {
			isNowPlaying = true;
		}
	}

	LastFmTrack track;
	track.isNowPlaying = isNowPlaying;
	track.title = trackObj.value("name").toString();

	if (trackObj.contains("artist")) {
		const auto artistVal = trackObj["artist"];
		if (artistVal.isObject()) {
			track.artist = artistVal.toObject().value("#text").toString();
		} else if (artistVal.isString()) {
			track.artist = artistVal.toString();
		}
	}

	if (trackObj.contains("album")) {
		const auto albumVal = trackObj["album"];
		if (albumVal.isObject()) {
			track.album = albumVal.toObject().value("#text").toString();
		} else if (albumVal.isString()) {
			track.album = albumVal.toString();
		}
	}

	track.url = trackObj.value("url").toString();

	if (trackObj.contains("image") && trackObj["image"].isArray()) {
		const auto images = trackObj["image"].toArray();
		QString largeUrl;
		QString mediumUrl;
		QString anyUrl;
		for (const auto &imgVal : images) {
			if (!imgVal.isObject()) {
				continue;
			}
			const auto imgObj = imgVal.toObject();
			const auto size = imgObj.value("size").toString();
			const auto imgUrl = imgObj.value("#text").toString().trimmed();
			if (imgUrl.isEmpty()) {
				continue;
			}
			if (size == u"large"_q) {
				largeUrl = imgUrl;
			} else if (size == u"medium"_q) {
				mediumUrl = imgUrl;
			} else if (anyUrl.isEmpty()) {
				anyUrl = imgUrl;
			}
		}
		track.imageUrl = !largeUrl.isEmpty()
			? largeUrl
			: (!mediumUrl.isEmpty() ? mediumUrl : anyUrl);
	}

	track.fetchedAt = crl::now();
	return track;
}

void Client::fetchCoverArt(
		const QString &imageUrl,
		Fn<void(QImage)> callback) {
	const auto trimmedUrl = imageUrl.trimmed();
	if (trimmedUrl.isEmpty()) {
		callback(QImage());
		return;
	}

	const auto it = _artCache.find(trimmedUrl);
	if (it != _artCache.end()) {
		callback(it.value());
		return;
	}

	const auto url = QUrl(trimmedUrl);
	if (!url.isValid()) {
		callback(QImage());
		return;
	}

	auto request = QNetworkRequest(url);
	request.setAttribute(
		QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setRawHeader("User-Agent", "FAgramDesktop/2.5.0");

	const auto reply = _nam.get(request);
	connect(reply, &QNetworkReply::finished, this, [=] {
		reply->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			callback(QImage());
			return;
		}

		const auto data = reply->readAll();
		auto image = QImage::fromData(data);
		if (!image.isNull()) {
			_artCache[trimmedUrl] = image;
			callback(image);
		} else {
			callback(QImage());
		}
	});
}

} // namespace Fa::LastFm
