/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include "base/object_ptr.h"

namespace Ui {
class VerticalLayout;
class RpWidget;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace FA::Features::HideArchiveChats {

[[nodiscard]] bool ShouldHide();

[[nodiscard]] not_null<::Ui::RpWidget*> AddToggle(
	not_null<::Ui::VerticalLayout*> card,
	not_null<Window::SessionController*> controller);

} // namespace FA::Features::HideArchiveChats
