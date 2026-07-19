// This file is part of Telegram Desktop,
// the official desktop application for the Telegram messaging service.
//
// For license and copyright information please follow this link:
// https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
#include "core/uninstall.h"

#include <charconv>
#include <system_error>
#include <utility>

#include <gsl/util>

namespace Core::Uninstall {
namespace {

constexpr auto kQuitCommand = std::string_view("CMD:quit;");
constexpr auto kResponsePrefix = std::string_view("RES:");
constexpr auto kMaxResponseSize = std::size_t(128);

template <typename Value>
[[nodiscard]] bool ParseUnsigned(std::string_view text, Value &value) {
	if (text.empty()) {
		return false;
	}
	const auto begin = text.data();
	const auto end = begin + text.size();
	const auto parsed = std::from_chars(begin, end, value);
	return (parsed.ec == std::errc()) && (parsed.ptr == end);
}

} // namespace

Lifecycle::Lifecycle(Operations operations)
: _operations(std::move(operations)) {
}

void Lifecycle::connected() {
	if (finished() || _quitSent) {
		return;
	}
	_quitSent = true;
	if (!_operations.sendQuit || !_operations.sendQuit(kQuitCommand)) {
		finish(Result::SendFailure);
	}
}

void Lifecycle::consumeResponse(std::string_view fragment) {
	if (finished()) {
		return;
	}
	for (const auto ch : fragment) {
		if (_response.size() == kMaxResponseSize) {
			finish(Result::MalformedResponse);
			return;
		}
		_response.push_back(ch);
		if (ch == ';') {
			processResponse();
			return;
		}
	}
}

void Lifecycle::noInstance() {
	if (!finished()) {
		finishWithCleanup(Result::NoInstance);
	}
}

void Lifecycle::socketFailure() {
	if (!finished()) {
		finish(Result::SocketFailure);
	}
}

void Lifecycle::disconnected() {
	if (finished()) {
		return;
	}
	finish(!_response.empty()
		? Result::MalformedResponse
		: Result::SocketFailure);
}

void Lifecycle::ipcTimeout() {
	if (!finished()) {
		finish(Result::IpcTimeout);
	}
}

bool Lifecycle::finished() const {
	return _result != Result::Pending;
}

Lifecycle::Result Lifecycle::result() const {
	return _result;
}

void Lifecycle::processResponse() {
	const auto frame = std::string_view(_response);
	if (frame.size() <= kResponsePrefix.size() + 2
		|| frame.substr(0, kResponsePrefix.size()) != kResponsePrefix
		|| frame.back() != ';') {
		finish(Result::MalformedResponse);
		return;
	}
	const auto values = frame.substr(
		kResponsePrefix.size(),
		frame.size() - kResponsePrefix.size() - 1);
	const auto separator = values.find('_');
	if (separator == std::string_view::npos
		|| values.find('_', separator + 1) != std::string_view::npos) {
		finish(Result::MalformedResponse);
		return;
	}
	auto pid = std::uint32_t(0);
	auto windowId = std::uint64_t(0);
	if (!ParseUnsigned(values.substr(0, separator), pid)
		|| !pid
		|| !ParseUnsigned(values.substr(separator + 1), windowId)) {
		finish(Result::MalformedResponse);
		return;
	}
	processPid(pid);
}

void Lifecycle::processPid(std::uint32_t pid) {
	const auto opened = _operations.openProcess
		? _operations.openProcess(pid)
		: ProcessOpenResult();
	if (opened.status == ProcessOpenStatus::Gone) {
		finishWithCleanup(Result::Success);
		return;
	} else if (opened.status != ProcessOpenStatus::Opened
		|| !opened.handle
		|| !_operations.waitProcess
		|| !_operations.closeProcess) {
		finish(Result::ProcessOpenFailure);
		return;
	}
	const auto waited = [&] {
		const auto guard = gsl::finally([&] {
			_operations.closeProcess(opened.handle);
		});
		return _operations.waitProcess(opened.handle);
	}();
	if (waited == ProcessWaitStatus::Exited) {
		finishWithCleanup(Result::Success);
	} else if (waited == ProcessWaitStatus::TimedOut) {
		finish(Result::ProcessWaitTimeout);
	} else {
		finish(Result::ProcessWaitFailure);
	}
}

void Lifecycle::finishWithCleanup(Result result) {
	if (!_operations.cleanup || _operations.cleanup() != 0) {
		finish(Result::CleanupFailure);
	} else {
		finish(result);
	}
}

void Lifecycle::finish(Result result) {
	if (!finished()) {
		_result = result;
	}
}

int ExitCode(Lifecycle::Result result) {
	switch (result) {
	case Lifecycle::Result::Success:
	case Lifecycle::Result::NoInstance:
		return 0;
	case Lifecycle::Result::SendFailure:
		return 1;
	case Lifecycle::Result::MalformedResponse:
		return 2;
	case Lifecycle::Result::SocketFailure:
		return 3;
	case Lifecycle::Result::IpcTimeout:
		return 4;
	case Lifecycle::Result::ProcessOpenFailure:
		return 5;
	case Lifecycle::Result::ProcessWaitTimeout:
		return 6;
	case Lifecycle::Result::ProcessWaitFailure:
		return 7;
	case Lifecycle::Result::CleanupFailure:
		return 8;
	case Lifecycle::Result::Pending:
		return 9;
	}
	return 9;
}

} // namespace Core::Uninstall
