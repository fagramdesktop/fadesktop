/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class HistoryItem;

namespace Data {
struct Group;
} // namespace Data

struct HistorySelectedTextEntry {
	not_null<HistoryItem*> item;
	const Data::Group *group = nullptr;
};

TextForMimeData HistoryItemText(not_null<HistoryItem*> item);
TextForMimeData HistoryGroupText(not_null<const Data::Group*> group);
TextForMimeData HistorySelectedItemsText(
	const std::vector<HistorySelectedTextEntry> &entries,
	bool richContext);
