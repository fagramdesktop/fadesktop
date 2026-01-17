/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {

class RpWidget;

[[nodiscard]] QPainterPath ToggleUpDownArrowPath(
	float64 x,
	float64 y,
	float64 size,
	float64 fourStrokes,
	float64 progress);

void AddToggleUpDownArrowToMoreButton(not_null<Ui::RpWidget*> parent);

} // namespace Ui
