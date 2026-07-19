// This file is part of Telegram Desktop,
// the official desktop application for the Telegram messaging service.
//
// For license and copyright information please follow this link:
// https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
#pragma once

#include "core/uninstall.h"

#include <cstdint>

namespace Platform::Uninstall {

[[nodiscard]] Core::Uninstall::Lifecycle::ProcessOpenResult OpenProcess(
	std::uint32_t pid);
[[nodiscard]] Core::Uninstall::Lifecycle::ProcessWaitStatus WaitProcess(
	Core::Uninstall::Lifecycle::ProcessHandle handle,
	std::uint32_t timeout);
void CloseProcess(Core::Uninstall::Lifecycle::ProcessHandle handle);

} // namespace Platform::Uninstall
