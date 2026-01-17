/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class UserData;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Info {
namespace Profile {

[[nodiscard]] bool IsCollectiblePhone(not_null<UserData*> user);

void AddPhoneMenu(not_null<Ui::PopupMenu*> menu, not_null<UserData*> user);

} // namespace Profile
} // namespace Info
