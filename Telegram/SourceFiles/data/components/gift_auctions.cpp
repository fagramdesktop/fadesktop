/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/components/gift_auctions.h"

#include "api/api_premium.h"
#include "api/api_text_entities.h"
#include "apiwrap.h"
#include "data/data_session.h"
#include "main/main_session.h"

namespace Data {

GiftAuctions::GiftAuctions(not_null<Main::Session*> session)
: _session(session)
, _timer([=] { checkSubscriptions(); }) {
}

GiftAuctions::~GiftAuctions() = default;

rpl::producer<GiftAuctionState> GiftAuctions::state(const QString &slug) {
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();

		auto &entry = _map[slug];
		if (!entry) {
			entry = std::make_unique<Entry>();
		}
		const auto raw = entry.get();

		raw->changes.events() | rpl::start_with_next([=] {
			consumer.put_next_copy(raw->state);
		}, lifetime);

		const auto now = crl::now();
		if (raw->state.subscribedTill < 0
			|| raw->state.subscribedTill >= now) {
			consumer.put_next_copy(raw->state);
		} else if (raw->state.subscribedTill >= 0) {
			request(slug);
		}

		return lifetime;
	};
}

void GiftAuctions::apply(const MTPDupdateStarGiftAuctionState &data) {
	if (const auto entry = find(data.vgift_id().v)) {
		apply(entry, data.vstate());
	}
}

void GiftAuctions::apply(const MTPDupdateStarGiftAuctionUserState &data) {
	if (const auto entry = find(data.vgift_id().v)) {
		apply(entry, data.vuser_state());
	}
}

void GiftAuctions::requestAcquired(
		uint64 giftId,
		Fn<void(std::vector<Data::GiftAcquired>)> done) {
	Expects(done != nullptr);

	_session->api().request(MTPpayments_GetStarGiftAuctionAcquiredGifts(
		MTP_long(giftId)
	)).done([=](const MTPpayments_StarGiftAuctionAcquiredGifts &result) {
		const auto &data = result.data();

		const auto owner = &_session->data();
		owner->processUsers(data.vusers());
		owner->processChats(data.vchats());

		const auto &list = data.vgifts().v;
		auto gifts = std::vector<Data::GiftAcquired>();
		gifts.reserve(list.size());
		for (const auto &gift : list) {
			const auto &data = gift.data();
			gifts.push_back({
				.to = owner->peer(peerFromMTP(data.vpeer())),
				.message = (data.vmessage()
					? Api::ParseTextWithEntities(_session, *data.vmessage())
					: TextWithEntities()),
				.date = data.vdate().v,
				.bidAmount = int64(data.vbid_amount().v),
				.round = data.vround().v,
				.position = data.vpos().v,
				.nameHidden = data.is_name_hidden(),
			});
		}
		if (const auto entry = find(giftId)) {
			const auto count = int(gifts.size());
			if (entry->state.my.gotCount != count) {
				entry->state.my.gotCount = count;
				entry->changes.fire({});
			}
		}
		done(std::move(gifts));
	}).fail([=] {
		done({});
	}).send();
}

void GiftAuctions::checkSubscriptions() {
	const auto now = crl::now();
	auto next = crl::time();
	for (const auto &[slug, entry] : _map) {
		const auto raw = entry.get();
		const auto till = raw->state.subscribedTill;
		if (till <= 0 || !raw->changes.has_consumers()) {
			continue;
		} else if (till <= now) {
			request(slug);
		} else {
			const auto timeout = till - now;
			if (!next || timeout < next) {
				next = timeout;
			}
		}
	}
	if (next) {
		_timer.callOnce(next);
	}
}

void GiftAuctions::request(const QString &slug) {
	auto &entry = _map[slug];
	Assert(entry != nullptr);

	const auto raw = entry.get();
	if (raw->requested) {
		return;
	}
	raw->requested = true;
	_session->api().request(MTPpayments_GetStarGiftAuctionState(
		MTP_inputStarGiftAuctionSlug(MTP_string(slug)),
		MTP_int(raw->state.version)
	)).done([=](const MTPpayments_StarGiftAuctionState &result) {
		raw->requested = false;
		const auto &data = result.data();

		raw->state.gift = Api::FromTL(_session, data.vgift());
		if (!raw->state.gift) {
			return;
		}
		const auto timeout = data.vtimeout().v;
		const auto ms = timeout * crl::time(1000);
		raw->state.subscribedTill = ms ? (crl::now() + ms) : -1;

		_session->data().processUsers(data.vusers());

		apply(raw, data.vstate());
		apply(raw, data.vuser_state());
		if (raw->changes.has_consumers()) {
			raw->changes.fire({});
			if (ms && (!_timer.isActive() || _timer.remainingTime() > ms)) {
				_timer.callOnce(ms);
			}
		}
	}).send();
}

GiftAuctions::Entry *GiftAuctions::find(uint64 giftId) const {
	for (const auto &[slug, entry] : _map) {
		if (entry->state.gift && entry->state.gift->id == giftId) {
			return entry.get();
		}
	}
	return nullptr;
}

void GiftAuctions::apply(
		not_null<Entry*> entry,
		const MTPStarGiftAuctionState &state) {
	Expects(entry->state.gift.has_value());

	const auto raw = &entry->state;
	state.match([&](const MTPDstarGiftAuctionState &data) {
		const auto version = data.vversion().v;
		if (raw->version >= version) {
			return;
		}
		const auto owner = &_session->data();
		raw->startDate = data.vstart_date().v;
		raw->endDate = data.vend_date().v;
		raw->minBidAmount = data.vmin_bid_amount().v;
		const auto &levels = data.vbid_levels().v;
		raw->bidLevels.clear();
		raw->bidLevels.reserve(levels.size());
		for (const auto &level : levels) {
			auto &entry = raw->bidLevels.emplace_back();
			const auto &data = level.data();
			entry.amount = data.vamount().v;
			entry.position = data.vpos().v;
			entry.date = data.vdate().v;
		}
		const auto &top = data.vtop_bidders().v;
		raw->topBidders.clear();
		raw->topBidders.reserve(top.size());
		for (const auto &user : top) {
			raw->topBidders.push_back(owner->user(UserId(user.v)));
		}
		raw->nextRoundAt = data.vnext_round_at().v;
		raw->giftsLeft = data.vgifts_left().v;
		raw->currentRound = data.vcurrent_round().v;
		raw->totalRounds = data.vtotal_rounds().v;
		raw->averagePrice = 0;
	}, [&](const MTPDstarGiftAuctionStateFinished &data) {
		raw->averagePrice = data.vaverage_price().v;
		raw->startDate = data.vstart_date().v;
		raw->endDate = data.vend_date().v;
		raw->minBidAmount = 0;
		raw->nextRoundAt
			= raw->currentRound
			= raw->totalRounds
			= raw->giftsLeft
			= raw->version
			= 0;
	}, [&](const MTPDstarGiftAuctionStateNotModified &data) {
	});
}

void GiftAuctions::apply(
		not_null<Entry*> entry,
		const MTPStarGiftAuctionUserState &state) {
	const auto &data = state.data();
	const auto raw = &entry->state.my;
	raw->to = data.vbid_peer()
		? _session->data().peer(peerFromMTP(*data.vbid_peer())).get()
		: nullptr;
	raw->minBidAmount = data.vmin_bid_amount().value_or(0);
	raw->bid = data.vbid_amount().value_or(0);
	raw->date = data.vbid_date().value_or(0);
	raw->gotCount = data.vacquired_count().v;
	raw->returned = data.is_returned();
}

} // namespace Data
