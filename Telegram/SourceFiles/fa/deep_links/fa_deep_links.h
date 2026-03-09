/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Core::DeepLinks {

class Router;

void RegisterFAHandlers(Router &router);

[[nodiscard]] QString FASettingsDeepLink(const QString &controlId);

} // namespace Core::DeepLinks
