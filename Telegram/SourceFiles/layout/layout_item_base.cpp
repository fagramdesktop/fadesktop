/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "layout/layout_item_base.h"

#include "history/view/history_view_cursor_state.h"

[[nodiscard]] HistoryView::TextState LayoutItemBase::getState(
		QPoint point,
		StateRequest request) const {
	return {};
}

[[nodiscard]] TextSelection LayoutItemBase::adjustSelection(
		TextSelection selection,
		TextSelectType type) const {
	return selection;
}
