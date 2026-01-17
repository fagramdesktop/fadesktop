/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/widgets/icon_button_with_text.h"

namespace Ui {
class Show;
} // namespace Ui

namespace HistoryView::Controls {

class TTLButton final {
public:
	TTLButton(
		not_null<Ui::RpWidget*> parent,
		std::shared_ptr<Ui::Show> show,
		not_null<PeerData*> peer);

	[[nodiscard]] not_null<PeerData*> peer() const {
		return _peer;
	}

	void show();
	void hide();
	void setVisible(bool visible);
	[[nodiscard]] bool isVisible() const;
	void move(int x, int y);

	[[nodiscard]] int width() const;

private:
	const not_null<PeerData*> _peer;
	Ui::IconButtonWithText _button;

};

} // namespace HistoryView::Controls
