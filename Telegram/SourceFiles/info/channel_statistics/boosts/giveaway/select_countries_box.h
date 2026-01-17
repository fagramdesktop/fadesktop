/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {

class GenericBox;

void SelectCountriesBox(
	not_null<Ui::GenericBox*> box,
	const std::vector<QString> &selected,
	Fn<void(std::vector<QString>)> doneCallback,
	Fn<bool(int)> checkErrorCallback);

} // namespace Ui
