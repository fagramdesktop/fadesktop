/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "window/main_window.h"

namespace Platform {

class MainWindow;

} // namespace Platform

// Platform dependent implementations.

#ifdef Q_OS_WIN
#include "platform/win/main_window_win.h"
#elif defined Q_OS_MAC // Q_OS_WIN
#include "platform/mac/main_window_mac.h"
#else // Q_OS_WIN || Q_OS_MAC
#include "platform/linux/main_window_linux.h"
#endif // else Q_OS_WIN || Q_OS_MAC
