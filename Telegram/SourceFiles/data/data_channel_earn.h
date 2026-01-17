/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QDateTime>

#include "data/data_credits.h"
#include "data/data_statistics_chart.h"

namespace Data {

using EarnInt = uint64;

struct EarnHistorySlice final {
	using OffsetToken = QString;
	std::vector<CreditsHistoryEntry> list;
	int total = 0;
	bool allLoaded = false;
	OffsetToken token;
};

struct EarnStatistics final {
	explicit operator bool() const {
		return !!usdRate;
	}
	Data::StatisticalGraph topHoursGraph;
	Data::StatisticalGraph revenueGraph;
	CreditsAmount currentBalance;
	CreditsAmount availableBalance;
	CreditsAmount overallRevenue;
	float64 usdRate = 0.;
	bool switchedOff = false;

	EarnHistorySlice firstHistorySlice;
};

} // namespace Data
