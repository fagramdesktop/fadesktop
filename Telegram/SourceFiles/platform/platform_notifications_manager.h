/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "window/notifications_manager.h"

namespace Platform {
namespace Notifications {

[[nodiscard]] bool SkipToastForCustom();
void MaybePlaySoundForCustom(Fn<void()> playSound);
void MaybeFlashBounceForCustom(Fn<void()> flashBounce);
[[nodiscard]] bool WaitForInputForCustom();

[[nodiscard]] bool Supported();
[[nodiscard]] bool Enforced();
[[nodiscard]] bool ByDefault();
[[nodiscard]] bool VolumeSupported();
void Create(Window::Notifications::System *system);

} // namespace Notifications
} // namespace Platform

// Platform dependent implementations.

#ifdef Q_OS_WIN
#include "platform/win/notifications_manager_win.h"
#elif defined Q_OS_MAC // Q_OS_MAC
#include "platform/mac/notifications_manager_mac.h"
#else // Q_OS_WIN || Q_OS_MAC
#include "platform/linux/notifications_manager_linux.h"
#endif // else for Q_OS_WIN || Q_OS_MAC
