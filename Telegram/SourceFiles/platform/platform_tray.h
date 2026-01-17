/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Platform {

class Tray;

[[nodiscard]] bool HasMonochromeSetting();

} // namespace Platform

// Platform dependent implementations.

#ifdef Q_OS_WIN
#include "platform/win/tray_win.h"
#elif defined Q_OS_MAC // Q_OS_WIN
#include "platform/mac/tray_mac.h"
#else // Q_OS_WIN || Q_OS_MAC
#include "platform/linux/tray_linux.h"
#endif // else for Q_OS_WIN || Q_OS_MAC
