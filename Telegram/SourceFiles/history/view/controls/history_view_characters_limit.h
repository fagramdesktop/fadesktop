/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/widgets/labels.h"

namespace HistoryView::Controls {

class CharactersLimitLabel final : public Ui::FlatLabel {
public:
	CharactersLimitLabel(
		not_null<Ui::RpWidget*> parent,
		not_null<Ui::RpWidget*> widgetToAlign,
		style::align align,
		QMargins margins = {});

	void setLeft(int value);

private:
	int _lastValue = 0;
	not_null<Ui::RpWidget*> _widgetToAlign;
	Fn<void(int, const QRect &)> _position;

};

} // namespace HistoryView::Controls
