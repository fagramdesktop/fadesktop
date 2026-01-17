/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/layers/box_content.h"

namespace Ui {
class Radiobutton;
} // namespace Ui

class AutoLockBox : public Ui::BoxContent {
public:
	AutoLockBox(QWidget*);

protected:
	void prepare() override;

private:
	void durationChanged(int seconds);

	std::vector<object_ptr<Ui::Radiobutton>> _options;

};
