/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/layers/generic_box.h"

namespace Ui {

void AutoDeleteSettingsBox(
	not_null<Ui::GenericBox*> box,
	TimeId ttlPeriod,
	rpl::producer<QString> about,
	Fn<void(TimeId)> callback);

} // namespace Ui
