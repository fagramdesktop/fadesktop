/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common.h"
#include "settings/settings_type.h"

enum class PremiumFeature;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] Type BusinessId();

void ShowBusiness(not_null<Window::SessionController*> controller);

[[nodiscard]] std::vector<PremiumFeature> BusinessFeaturesOrder(
	not_null<::Main::Session*> session);

} // namespace Settings
