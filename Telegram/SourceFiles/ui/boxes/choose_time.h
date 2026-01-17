/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/object_ptr.h"

namespace Ui {

class RpWidget;

struct ChooseTimeResult {
	object_ptr<RpWidget> widget;
	rpl::producer<TimeId> secondsValue;
};

ChooseTimeResult ChooseTimeWidget(
	not_null<RpWidget*> parent,
	TimeId startSeconds,
	bool hiddenDaysInput = false);

} // namespace Ui
