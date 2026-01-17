/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "data/data_channel_earn.h"

namespace Info::ChannelEarn {

[[nodiscard]] QString MajorPart(Data::EarnInt value);
[[nodiscard]] QString MajorPart(CreditsAmount value);
[[nodiscard]] QString MinorPart(Data::EarnInt value);
[[nodiscard]] QString MinorPart(CreditsAmount value);
[[nodiscard]] QString ToUsd(
	Data::EarnInt value,
	float64 rate,
	int afterFloat);
[[nodiscard]] QString ToUsd(
	CreditsAmount value,
	float64 rate,
	int afterFloat);

} // namespace Info::ChannelEarn
