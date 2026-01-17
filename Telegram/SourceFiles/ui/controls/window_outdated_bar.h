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

[[nodiscard]] object_ptr<RpWidget> CreateOutdatedBar(
	not_null<QWidget*> parent,
	const QString &workingPath);

} // namespace Ui
