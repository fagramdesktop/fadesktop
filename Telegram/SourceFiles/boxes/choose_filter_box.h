/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

class History;

class ChooseFilterValidator final {
public:
	ChooseFilterValidator(not_null<History*> history);
	struct LimitData {
		const bool reached = false;
		const int count = 0;
	};

	[[nodiscard]] bool canAdd() const;
	[[nodiscard]] bool canRemove(FilterId filterId) const;
	[[nodiscard]] LimitData limitReached(
		FilterId filterId,
		bool always) const;

	void add(FilterId filterId) const;
	void remove(FilterId filterId) const;

private:
	const not_null<History*> _history;

};

void FillChooseFilterMenu(
	not_null<Window::SessionController*> controller,
	not_null<Ui::PopupMenu*> menu,
	not_null<History*> history);
