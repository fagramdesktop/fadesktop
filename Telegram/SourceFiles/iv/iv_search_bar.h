/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "ui/widgets/shadow.h"
#include "ui/wrap/slide_wrap.h"

namespace Ui {
class CrossButton;
class FlatLabel;
class IconButton;
class MultiSelect;
} // namespace Ui

namespace Iv {

class SearchBar final {
public:
	explicit SearchBar(not_null<Ui::RpWidget*> parent);

	void toggle(bool shown, anim::type animated);
	void show(anim::type animated);
	void hide(anim::type animated);
	[[nodiscard]] bool shown() const;
	void setInnerFocus();
	void raise();
	void move(int x, int y);

	void setResults(int current, int total);

	[[nodiscard]] rpl::producer<QString> queryChanges() const;
	[[nodiscard]] rpl::producer<> submits() const;
	[[nodiscard]] rpl::producer<int> navigateRequests() const;
	[[nodiscard]] rpl::producer<> closeRequests() const;
	[[nodiscard]] rpl::producer<int> heightValue() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	void setup(not_null<Ui::RpWidget*> parent);
	void updateControlsGeometry();

	Ui::SlideWrap<Ui::RpWidget> _wrap;
	Ui::PlainShadow _shadow;
	Ui::MultiSelect *_select = nullptr;
	Ui::FlatLabel *_counter = nullptr;
	Ui::IconButton *_up = nullptr;
	Ui::IconButton *_down = nullptr;
	Ui::CrossButton *_close = nullptr;
	rpl::event_stream<QString> _queryChanges;
	rpl::event_stream<> _submits;
	rpl::event_stream<> _closeRequests;

};

} // namespace Iv
