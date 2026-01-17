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
class GenericBox;

enum class ShowOrPremium : uchar {
	LastSeen,
	ReadTime,
};
void ShowOrPremiumBox(
	not_null<GenericBox*> box,
	ShowOrPremium type,
	QString shortName,
	Fn<void()> justShow,
	Fn<void()> toPremium);

[[nodiscard]] object_ptr<RpWidget> MakeShowOrLabel(
	not_null<RpWidget*> parent,
	rpl::producer<QString> text);

} // namespace Ui
