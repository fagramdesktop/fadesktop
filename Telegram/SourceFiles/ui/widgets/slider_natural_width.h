/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/widgets/discrete_sliders.h"

namespace Ui {

class CustomWidthSlider final : public SettingsSlider {
public:
	using Ui::SettingsSlider::SettingsSlider;
	using SettingsSlider::setNaturalWidth;

};

} // namespace Ui
