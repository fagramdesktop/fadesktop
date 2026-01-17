/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Data {
class ChatFilter;
} // namespace Data

void EditFilterBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> window,
	const Data::ChatFilter &filter,
	Fn<void(const Data::ChatFilter &)> doneCallback,
	Fn<void(const Data::ChatFilter &, Fn<void(Data::ChatFilter)>)> saveAnd);

void EditExistingFilter(
	not_null<Window::SessionController*> window,
	FilterId id);
