/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] Type SessionsId();

void AddSessionInfoRow(
	not_null<Ui::VerticalLayout*> container,
	rpl::producer<QString> label,
	const QString &value,
	const style::icon &icon);

} // namespace Settings
