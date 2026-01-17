/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class WindowListDelegate;

namespace HistoryView {
class ListWidget;
} // namespace HistoryView

namespace Ui {
class RpWidget;
class ScrollArea;
} // namespace Ui

namespace Window {

class SectionWidget;

void SetupSwipeBackSection(
	not_null<Ui::RpWidget*> parent,
	not_null<Ui::ScrollArea*> scroll,
	not_null<HistoryView::ListWidget*> list);

} // namespace Window
