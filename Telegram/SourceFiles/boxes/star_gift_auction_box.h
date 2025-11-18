/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Data {
struct GiftAuctionState;
} // namespace Data

namespace Info::PeerGifts {
struct GiftSendDetails;
} // namespace Info::PeerGifts

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

class BoxContent;
class RoundButton;

[[nodiscard]] rpl::lifetime ShowStarGiftAuction(
	not_null<Window::SessionController*> controller,
	PeerData *peer,
	QString slug,
	Fn<void()> finishRequesting,
	Fn<void()> boxClosed);

struct AuctionBidBoxArgs {
	not_null<PeerData*> peer;
	std::shared_ptr<ChatHelpers::Show> show;
	rpl::producer<Data::GiftAuctionState> state;
	std::unique_ptr<Info::PeerGifts::GiftSendDetails> details;
};
[[nodiscard]] object_ptr<BoxContent> MakeAuctionBidBox(
	AuctionBidBoxArgs &&args);

enum class AuctionButtonCountdownType {
	Join,
	Place,
};
void SetAuctionButtonCountdownText(
	not_null<RoundButton*> button,
	AuctionButtonCountdownType type,
	rpl::producer<Data::GiftAuctionState> value);

} // namespace Ui
