// This file is part of Telegram Desktop,
// the official desktop application for the Telegram messaging service.
//
// For license and copyright information please follow this link:
// https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Core::Uninstall {

class Lifecycle final {
public:
	enum class Result {
		Pending,
		Success,
		NoInstance,
		SendFailure,
		MalformedResponse,
		SocketFailure,
		IpcTimeout,
		ProcessOpenFailure,
		ProcessWaitTimeout,
		ProcessWaitFailure,
		CleanupFailure,
	};

	struct ProcessHandle {
		std::uintptr_t value = 0;

		explicit operator bool() const {
			return value != 0;
		}
	};

	enum class ProcessOpenStatus {
		Opened,
		Gone,
		Failed,
	};

	struct ProcessOpenResult {
		ProcessOpenStatus status = ProcessOpenStatus::Failed;
		ProcessHandle handle;
	};

	enum class ProcessWaitStatus {
		Exited,
		TimedOut,
		Failed,
	};

	struct Operations {
		std::function<bool(std::string_view)> sendQuit;
		std::function<ProcessOpenResult(std::uint32_t)> openProcess;
		std::function<ProcessWaitStatus(ProcessHandle)> waitProcess;
		std::function<void(ProcessHandle)> closeProcess;
		std::function<int()> cleanup;
	};

	explicit Lifecycle(Operations operations);

	void connected();
	void consumeResponse(std::string_view fragment);
	void noInstance();
	void socketFailure();
	void disconnected();
	void ipcTimeout();

	[[nodiscard]] bool finished() const;
	[[nodiscard]] Result result() const;

private:
	void processResponse();
	void processPid(std::uint32_t pid);
	void finishWithCleanup(Result result);
	void finish(Result result);

	Operations _operations;
	std::string _response;
	Result _result = Result::Pending;
	bool _quitSent = false;

};

[[nodiscard]] int ExitCode(Lifecycle::Result result);

} // namespace Core::Uninstall
