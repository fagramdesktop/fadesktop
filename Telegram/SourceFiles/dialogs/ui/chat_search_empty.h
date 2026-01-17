/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Dialogs {

enum class SearchEmptyIcon {
	Search,
	NoResults,
};

class SearchEmpty final : public Ui::RpWidget {
public:
	using Icon = SearchEmptyIcon;

	SearchEmpty(
		QWidget *parent,
		Icon icon,
		rpl::producer<TextWithEntities> text);

	void setMinimalHeight(int minimalHeight);

	void animate();

	[[nodiscard]] rpl::producer<ClickHandlerPtr> handlerActivated() const {
		return _handlerActivated.events();
	}

private:
	void setup(Icon icon, rpl::producer<TextWithEntities> text);

	Fn<void()> _animate;
	rpl::event_stream<ClickHandlerPtr> _handlerActivated;

};

} // namespace Dialogs
