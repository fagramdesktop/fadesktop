/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "menu/menu_check_item.h"

#include "ui/widgets/checkbox.h"
#include "styles/style_media_player.h"

namespace Menu {

void ItemWithCheck::init(bool checked) {
	enableMouseSelecting();

	ItemBase::setPreventClose(true);

	_checkView = std::make_unique<Ui::CheckView>(st::defaultCheck, false);
	_checkView->checkedChanges(
	) | rpl::on_next([=](bool checked) {
		setIcon(checked ? &st::mediaPlayerMenuCheck : nullptr);
	}, lifetime());

	_checkView->setChecked(checked, anim::type::normal);
	ItemBase::clicks() | rpl::on_next([=] {
		_checkView->setChecked(!_checkView->checked(), anim::type::normal);
	}, lifetime());
}

not_null<Ui::CheckView*> ItemWithCheck::checkView() const {
	return _checkView.get();
}

} // namespace Menu
