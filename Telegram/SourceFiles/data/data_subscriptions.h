/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Data {

struct PeerSubscription final {
	uint64 credits = 0;
	int period = 0;

	explicit operator bool() const {
		return credits > 0 && period > 0;
	}
};

struct SubscriptionEntry final {
	explicit operator bool() const {
		return !id.isEmpty();
	}

	QString id;
	QString inviteHash;
	QString title;
	QString slug;
	QDateTime until;
	PeerSubscription subscription;
	uint64 barePeerId = 0;
	uint64 photoId = 0;
	bool cancelled = false;
	bool cancelledByBot = false;
	bool expired = false;
	bool canRefulfill = false;
};

} // namespace Data
