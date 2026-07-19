// This file is part of Telegram Desktop,
// the official desktop application for the Telegram messaging service.
//
// For license and copyright information please follow this link:
// https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
#include "platform/win/uninstall_win.h"

#include <windows.h>

namespace Platform::Uninstall {

Core::Uninstall::Lifecycle::ProcessOpenResult OpenProcess(
		std::uint32_t pid) {
	using Lifecycle = Core::Uninstall::Lifecycle;
	if (!pid) {
		return { Lifecycle::ProcessOpenStatus::Failed, {} };
	}
	const auto handle = ::OpenProcess(SYNCHRONIZE, FALSE, pid);
	if (handle) {
		return {
			Lifecycle::ProcessOpenStatus::Opened,
			{ reinterpret_cast<std::uintptr_t>(handle) },
		};
	}
	return {
		(GetLastError() == ERROR_INVALID_PARAMETER)
			? Lifecycle::ProcessOpenStatus::Gone
			: Lifecycle::ProcessOpenStatus::Failed,
		{},
	};
}

Core::Uninstall::Lifecycle::ProcessWaitStatus WaitProcess(
		Core::Uninstall::Lifecycle::ProcessHandle handle,
		std::uint32_t timeout) {
	using Status = Core::Uninstall::Lifecycle::ProcessWaitStatus;
	if (!handle) {
		return Status::Failed;
	}
	const auto result = WaitForSingleObject(
		reinterpret_cast<HANDLE>(handle.value),
		timeout);
	return (result == WAIT_OBJECT_0)
		? Status::Exited
		: (result == WAIT_TIMEOUT)
		? Status::TimedOut
		: Status::Failed;
}

void CloseProcess(Core::Uninstall::Lifecycle::ProcessHandle handle) {
	if (handle) {
		CloseHandle(reinterpret_cast<HANDLE>(handle.value));
	}
}

} // namespace Platform::Uninstall
