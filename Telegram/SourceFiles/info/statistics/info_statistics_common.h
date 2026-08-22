/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "data/data_statistics.h"
#include "data/data_statistics_lists.h"

namespace Info::Statistics {

struct SavedState final {
	Data::AnyStatistics stats;
	Data::StatisticsLists lists;
	Data::StatisticalGraph pollVotesGraph;
	base::flat_map<Data::RecentPostId, QImage> recentPostPreviews;
	Data::PublicForwardsSlice publicForwardsFirstSlice;
	int recentPostsExpanded = 0;
};

} // namespace Info::Statistics
