/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include "fa/data/entities.h"
#include "main/main_session.h"

#include "base/unique_qptr.h"

// thanks ayugram

namespace Ui {
namespace Menu {
class Menu;
class ItemBase;
} // namespace Menu

class PopupMenu;

[[nodiscard]] base::unique_qptr<Menu::ItemBase> ContextActionWithSubText(
	not_null<Menu::Menu*> menu,
	const style::icon &icon,
	const QString &title,
	const QString &subtext,
	Fn<void()> callback = nullptr);

[[nodiscard]] base::unique_qptr<Menu::ItemBase> ContextActionStickerAuthor(
	not_null<Menu::Menu*> menu,
	not_null<Main::Session*> session,
	ID authorId);

} // namespace Ui