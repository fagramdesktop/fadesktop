/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "data/data_star_gift.h"

namespace Main {
class Session;
} // namespace Main

namespace Data {

struct GiftAuctionBidLevel {
	int64 amount = 0;
	int position = 0;
	TimeId date = 0;
};

struct StarGiftAuctionMyState {
	PeerData *to = nullptr;
	int64 minBidAmount = 0;
	int64 bid = 0;
	TimeId date = 0;
	int gotCount = 0;
	bool returned = false;
};

struct GiftAuctionState {
	std::optional<StarGift> gift;
	StarGiftAuctionMyState my;
	std::vector<GiftAuctionBidLevel> bidLevels;
	std::vector<not_null<UserData*>> topBidders;
	crl::time subscribedTill = 0;
	int64 minBidAmount = 0;
	int64 averagePrice = 0;
	TimeId startDate = 0;
	TimeId endDate = 0;
	TimeId nextRoundAt = 0;
	int currentRound = 0;
	int totalRounds = 0;
	int giftsLeft = 0;
	int version = 0;

	[[nodiscard]] bool finished() const {
		return (averagePrice != 0);
	}
};

struct GiftAcquired {
	not_null<PeerData*> to;
	TextWithEntities message;
	TimeId date = 0;
	int64 bidAmount = 0;
	int round = 0;
	int position = 0;
	bool nameHidden = false;
};

class GiftAuctions final {
public:
	explicit GiftAuctions(not_null<Main::Session*> session);
	~GiftAuctions();

	[[nodiscard]] rpl::producer<GiftAuctionState> state(const QString &slug);

	void apply(const MTPDupdateStarGiftAuctionState &data);
	void apply(const MTPDupdateStarGiftAuctionUserState &data);

	void requestAcquired(
		uint64 giftId, 
		Fn<void(std::vector<Data::GiftAcquired>)> done);

private:
	struct Entry {
		GiftAuctionState state;
		rpl::event_stream<> changes;
		bool requested = false;
	};

	void request(const QString &slug);
	Entry *find(uint64 giftId) const;
	void apply(
		not_null<Entry*> entry,
		const MTPStarGiftAuctionState &state);
	void apply(
		not_null<Entry*> entry,
		const MTPStarGiftAuctionUserState &state);
	void checkSubscriptions();

	const not_null<Main::Session*> _session;

	base::Timer _timer;
	base::flat_map<QString, std::unique_ptr<Entry>> _map;

};

} // namespace Data
