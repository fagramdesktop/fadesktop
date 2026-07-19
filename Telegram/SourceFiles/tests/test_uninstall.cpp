// This file is part of Telegram Desktop,
// the official desktop application for the Telegram messaging service.
//
// For license and copyright information please follow this link:
// https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

#include "base/basic_types.h"
#include "core/uninstall.h"
#include "platform/win/uninstall_win.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QProcess>
#include <QtCore/QStringList>
#include <QtCore/QTemporaryDir>
#include <QtCore/QUuid>

#include <windows.h>

#include <gsl/util>

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Lifecycle = Core::Uninstall::Lifecycle;
using Result = Lifecycle::Result;

class CooperativeChild final {
public:
	CooperativeChild();
	~CooperativeChild();

	[[nodiscard]] bool start();
	void release();
	[[nodiscard]] bool wait();
	[[nodiscard]] std::uint32_t pid() const;
	[[nodiscard]] HANDLE event() const;
	[[nodiscard]] const QString &childMarkerPath() const;
	[[nodiscard]] const QString &cleanupMarkerPath() const;

private:
	QTemporaryDir _directory;
	QString _eventName;
	QString _childMarkerPath;
	QString _cleanupMarkerPath;
	QProcess _process;
	HANDLE _event = nullptr;

};

struct FakeState {
	std::vector<std::string> events;
	std::string sent;
	Lifecycle::ProcessOpenResult openResult = {
		Lifecycle::ProcessOpenStatus::Opened,
		{ 1 },
	};
	Lifecycle::ProcessWaitStatus waitResult
		= Lifecycle::ProcessWaitStatus::Exited;
	std::uint32_t openedPid = 0;
	int sendCount = 0;
	int openCount = 0;
	int waitCount = 0;
	int closeCount = 0;
	int cleanupCount = 0;
	int cleanupResult = 0;
	bool sendResult = true;
};

struct TestCase {
	const char *name = nullptr;
	void (*method)() = nullptr;
};

void Require(bool condition, std::string_view message) {
	if (!condition) {
		throw std::runtime_error(std::string(message));
	}
}

CooperativeChild::CooperativeChild()
: _eventName(
	u"Local\\TelegramDesktop-Uninstall-Test-"_q
		+ QUuid::createUuid().toString(QUuid::WithoutBraces))
, _childMarkerPath(_directory.filePath(u"child-exited"_q))
, _cleanupMarkerPath(_directory.filePath(u"cleanup-finished"_q)) {
}

CooperativeChild::~CooperativeChild() {
	release();
	if (_process.state() != QProcess::NotRunning) {
		_process.waitForFinished(-1);
	}
	if (_event) {
		CloseHandle(_event);
	}
}

bool CooperativeChild::start() {
	if (!_directory.isValid()) {
		return false;
	}
	_event = CreateEventW(
		nullptr,
		TRUE,
		FALSE,
		reinterpret_cast<LPCWSTR>(_eventName.utf16()));
	if (!_event) {
		return false;
	}
	_process.start(
		QCoreApplication::applicationFilePath(),
		{ u"--cooperative-child"_q, _eventName, _childMarkerPath });
	return _process.waitForStarted(5000);
}

void CooperativeChild::release() {
	if (_event) {
		SetEvent(_event);
	}
}

bool CooperativeChild::wait() {
	if (_process.state() != QProcess::NotRunning
		&& !_process.waitForFinished(5000)) {
		return false;
	}
	return (_process.state() == QProcess::NotRunning)
		&& (_process.exitStatus() == QProcess::NormalExit)
		&& (_process.exitCode() == 0);
}

std::uint32_t CooperativeChild::pid() const {
	const auto result = _process.processId();
	return (result > 0
		&& result <= std::numeric_limits<std::uint32_t>::max())
		? static_cast<std::uint32_t>(result)
		: 0;
}

HANDLE CooperativeChild::event() const {
	return _event;
}

const QString &CooperativeChild::childMarkerPath() const {
	return _childMarkerPath;
}

const QString &CooperativeChild::cleanupMarkerPath() const {
	return _cleanupMarkerPath;
}

[[nodiscard]] Lifecycle MakeLifecycle(FakeState &state) {
	return Lifecycle({
		[&](std::string_view command) {
			state.events.emplace_back("send");
			state.sent.assign(command);
			++state.sendCount;
			return state.sendResult;
		},
		[&](std::uint32_t pid) {
			state.events.emplace_back("open");
			state.openedPid = pid;
			++state.openCount;
			return state.openResult;
		},
		[&](Lifecycle::ProcessHandle) {
			state.events.emplace_back("wait");
			++state.waitCount;
			return state.waitResult;
		},
		[&](Lifecycle::ProcessHandle) {
			state.events.emplace_back("close");
			++state.closeCount;
		},
		[&] {
			state.events.emplace_back("cleanup");
			++state.cleanupCount;
			return state.cleanupResult;
		},
	});
}

void RequireEvents(
		const FakeState &state,
		std::initializer_list<std::string_view> expected) {
	auto values = std::vector<std::string>();
	values.reserve(expected.size());
	for (const auto value : expected) {
		values.emplace_back(value);
	}
	Require(state.events == values, "operation order mismatch");
}

[[nodiscard]] bool WriteMarker(const QString &path) {
	auto file = QFile(path);
	return file.open(QIODevice::WriteOnly)
		&& (file.write("done") == 4);
}

[[nodiscard]] std::string ResponseFor(std::uint32_t pid) {
	return "RES:" + std::to_string(pid) + "_0;";
}

void TestQuitAndSuccess() {
	auto state = FakeState();
	auto lifecycle = MakeLifecycle(state);
	lifecycle.connected();
	lifecycle.connected();
	Require(state.sent == "CMD:quit;", "quit command bytes mismatch");
	Require(state.sendCount == 1, "quit command sent more than once");
	lifecycle.consumeResponse("RES:123_0;");
	Require(lifecycle.finished(), "successful lifecycle did not finish");
	Require(lifecycle.result() == Result::Success, "success result mismatch");
	Require(Core::Uninstall::ExitCode(lifecycle.result()) == 0, "success exit code mismatch");
	Require(state.openedPid == 123, "response pid mismatch");
	Require(state.closeCount == 1, "successful handle close count mismatch");
	Require(state.cleanupCount == 1, "successful cleanup count mismatch");
	RequireEvents(state, { "send", "open", "wait", "close", "cleanup" });
}

void TestNoInstanceAndGone() {
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.noInstance();
		Require(lifecycle.result() == Result::NoInstance, "no-instance result mismatch");
		Require(Core::Uninstall::ExitCode(lifecycle.result()) == 0, "no-instance exit code mismatch");
		RequireEvents(state, { "cleanup" });
	}
	{
		auto state = FakeState();
		state.openResult = { Lifecycle::ProcessOpenStatus::Gone, {} };
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:44_0;");
		Require(lifecycle.result() == Result::Success, "gone-race result mismatch");
		Require(state.closeCount == 0, "gone race closed a missing handle");
		RequireEvents(state, { "send", "open", "cleanup" });
	}
	{
		auto state = FakeState();
		state.cleanupResult = 12;
		auto lifecycle = MakeLifecycle(state);
		lifecycle.noInstance();
		Require(lifecycle.result() == Result::CleanupFailure, "cleanup failure result mismatch");
		Require(state.cleanupCount == 1, "failed cleanup count mismatch");
	}
}

void TestProcessFailures() {
	{
		auto state = FakeState();
		state.openResult = { Lifecycle::ProcessOpenStatus::Failed, {} };
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:10_0;");
		Require(lifecycle.result() == Result::ProcessOpenFailure, "open failure result mismatch");
		RequireEvents(state, { "send", "open" });
	}
	{
		auto state = FakeState();
		state.openResult = { Lifecycle::ProcessOpenStatus::Opened, {} };
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:10_0;");
		Require(lifecycle.result() == Result::ProcessOpenFailure, "null handle result mismatch");
		RequireEvents(state, { "send", "open" });
	}
	{
		auto state = FakeState();
		state.waitResult = Lifecycle::ProcessWaitStatus::TimedOut;
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:10_0;");
		Require(lifecycle.result() == Result::ProcessWaitTimeout, "wait timeout result mismatch");
		Require(state.closeCount == 1, "timeout handle close count mismatch");
		Require(state.cleanupCount == 0, "timeout ran cleanup");
		RequireEvents(state, { "send", "open", "wait", "close" });
	}
	{
		auto state = FakeState();
		state.waitResult = Lifecycle::ProcessWaitStatus::Failed;
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:10_0;");
		Require(lifecycle.result() == Result::ProcessWaitFailure, "wait failure result mismatch");
		Require(state.closeCount == 1, "failed-wait handle close count mismatch");
		Require(state.cleanupCount == 0, "failed wait ran cleanup");
		RequireEvents(state, { "send", "open", "wait", "close" });
	}
	{
		auto state = FakeState();
		state.cleanupResult = 7;
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:10_0;");
		Require(lifecycle.result() == Result::CleanupFailure, "post-wait cleanup failure mismatch");
		Require(state.closeCount == 1, "cleanup-failure handle close count mismatch");
		RequireEvents(state, { "send", "open", "wait", "close", "cleanup" });
	}
}

void TestResponseParsing() {
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:4");
		Require(!lifecycle.finished(), "fragment completed prematurely");
		lifecycle.consumeResponse("2_18446744073709551615;RES:99_0;");
		lifecycle.consumeResponse("RES:100_0;");
		Require(lifecycle.result() == Result::Success, "fragmented response failed");
		Require(state.openedPid == 42, "fragmented pid mismatch");
		Require(state.openCount == 1, "duplicate response reopened process");
		Require(state.waitCount == 1, "duplicate response repeated wait");
		Require(state.closeCount == 1, "duplicate response repeated close");
		Require(state.cleanupCount == 1, "duplicate response repeated cleanup");
	}
	const auto invalid = std::array<std::string_view, 9>{
		"RES:0_0;",
		"RES:4294967296_0;",
		"RES:1_18446744073709551616;",
		"RES:_0;",
		"RES:1_;",
		"RES:1_0junk;",
		"junkRES:1_0;",
		"RES:1_0_extra;",
		"RES:+1_0;",
	};
	for (const auto frame : invalid) {
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse(frame);
		Require(lifecycle.result() == Result::MalformedResponse, "malformed response accepted");
		Require(state.openCount == 0, "malformed response opened process");
		Require(state.cleanupCount == 0, "malformed response ran cleanup");
	}
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse(std::string(129, 'x'));
		Require(lifecycle.result() == Result::MalformedResponse, "over-limit response accepted");
	}
}

void TestTerminalInputs() {
	{
		auto state = FakeState();
		state.sendResult = false;
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.connected();
		Require(lifecycle.result() == Result::SendFailure, "send failure result mismatch");
		Require(state.sendCount == 1, "failed send repeated");
	}
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.disconnected();
		Require(lifecycle.result() == Result::SocketFailure, "empty disconnect result mismatch");
	}
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.consumeResponse("RES:1_");
		lifecycle.disconnected();
		Require(lifecycle.result() == Result::MalformedResponse, "incomplete disconnect result mismatch");
	}
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.socketFailure();
		lifecycle.noInstance();
		lifecycle.ipcTimeout();
		Require(lifecycle.result() == Result::SocketFailure, "socket terminal result changed");
		Require(state.cleanupCount == 0, "terminal socket failure ran cleanup");
	}
	{
		auto state = FakeState();
		auto lifecycle = MakeLifecycle(state);
		lifecycle.connected();
		lifecycle.ipcTimeout();
		Require(lifecycle.result() == Result::IpcTimeout, "ipc timeout result mismatch");
		Require(state.cleanupCount == 0, "ipc timeout ran cleanup");
	}
}

void TestExitCodes() {
	const auto failures = std::array{
		Result::SendFailure,
		Result::MalformedResponse,
		Result::SocketFailure,
		Result::IpcTimeout,
		Result::ProcessOpenFailure,
		Result::ProcessWaitTimeout,
		Result::ProcessWaitFailure,
		Result::CleanupFailure,
		Result::Pending,
	};
	for (auto i = std::size_t(0); i != failures.size(); ++i) {
		const auto code = Core::Uninstall::ExitCode(failures[i]);
		Require(code != 0, "failure exit code is zero");
		for (auto j = std::size_t(0); j != i; ++j) {
			Require(code != Core::Uninstall::ExitCode(failures[j]), "failure exit code is unstable");
		}
	}
}

void TestRealAdapterExit() {
	auto child = CooperativeChild();
	Require(child.start(), "cooperative child did not start");
	Require(child.pid() != 0, "cooperative child pid invalid");
	auto sent = std::string();
	auto closeCount = 0;
	auto cleanupCount = 0;
	auto cleanupObservedExit = false;
	auto lifecycle = Lifecycle({
		[&](std::string_view command) {
			sent.assign(command);
			return true;
		},
		[](std::uint32_t pid) {
			return Platform::Uninstall::OpenProcess(pid);
		},
		[](Lifecycle::ProcessHandle handle) {
			return Platform::Uninstall::WaitProcess(handle, 5000);
		},
		[&](Lifecycle::ProcessHandle handle) {
			++closeCount;
			Platform::Uninstall::CloseProcess(handle);
		},
		[&] {
			++cleanupCount;
			cleanupObservedExit = QFileInfo::exists(child.childMarkerPath());
			return (cleanupObservedExit
				&& WriteMarker(child.cleanupMarkerPath())) ? 0 : 1;
		},
	});
	lifecycle.connected();
	auto releaser = std::jthread([event = child.event()] {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		SetEvent(event);
	});
	lifecycle.consumeResponse(ResponseFor(child.pid()));
	Require(child.wait(), "cooperative child did not exit normally");
	Require(sent == "CMD:quit;", "real adapter quit bytes mismatch");
	Require(lifecycle.result() == Result::Success, "real adapter exit result mismatch");
	Require(closeCount == 1, "real adapter close count mismatch");
	Require(cleanupCount == 1, "real adapter cleanup count mismatch");
	Require(cleanupObservedExit, "cleanup ran before child exit marker");
	Require(QFileInfo::exists(child.cleanupMarkerPath()), "cleanup marker missing");
	Require(
		Platform::Uninstall::OpenProcess(0).status
			== Lifecycle::ProcessOpenStatus::Failed,
		"zero pid adapter result mismatch");
	Require(
		Platform::Uninstall::OpenProcess(
			std::numeric_limits<std::uint32_t>::max()).status
			== Lifecycle::ProcessOpenStatus::Gone,
		"gone pid adapter result mismatch");
}

void TestRealAdapterTimeout() {
	auto child = CooperativeChild();
	Require(child.start(), "timeout child did not start");
	auto closeCount = 0;
	auto cleanupCount = 0;
	auto lifecycle = Lifecycle({
		[](std::string_view) {
			return true;
		},
		[](std::uint32_t pid) {
			return Platform::Uninstall::OpenProcess(pid);
		},
		[](Lifecycle::ProcessHandle handle) {
			return Platform::Uninstall::WaitProcess(handle, 10);
		},
		[&](Lifecycle::ProcessHandle handle) {
			++closeCount;
			Platform::Uninstall::CloseProcess(handle);
		},
		[&] {
			++cleanupCount;
			return WriteMarker(child.cleanupMarkerPath()) ? 0 : 1;
		},
	});
	lifecycle.connected();
	lifecycle.consumeResponse(ResponseFor(child.pid()));
	Require(lifecycle.result() == Result::ProcessWaitTimeout, "real adapter timeout result mismatch");
	Require(closeCount == 1, "real timeout close count mismatch");
	Require(cleanupCount == 0, "real timeout ran cleanup");
	Require(!QFileInfo::exists(child.cleanupMarkerPath()), "real timeout wrote cleanup marker");
	child.release();
	Require(child.wait(), "timeout child teardown failed");
	Require(QFileInfo::exists(child.childMarkerPath()), "timeout child marker missing");
}

int RunCooperativeChild(const QStringList &arguments) {
	if (arguments.size() != 4) {
		return 2;
	}
	const auto event = OpenEventW(
		SYNCHRONIZE,
		FALSE,
		reinterpret_cast<LPCWSTR>(arguments[2].utf16()));
	if (!event) {
		return 3;
	}
	const auto close = gsl::finally([&] {
		CloseHandle(event);
	});
	const auto result = WaitForSingleObject(event, INFINITE);
	if (result != WAIT_OBJECT_0) {
		return 4;
	}
	auto marker = QFile(arguments[3]);
	if (!marker.open(QIODevice::WriteOnly)) {
		return 5;
	}
	return (marker.write("exited") == 6) ? 0 : 6;
}

} // namespace

int main(int argc, char *argv[]) {
	auto application = QCoreApplication(argc, argv);
	const auto arguments = application.arguments();
	if (arguments.size() > 1 && arguments[1] == u"--cooperative-child"_q) {
		return RunCooperativeChild(arguments);
	}
	const auto tests = std::array{
		TestCase{ "quit-and-success", &TestQuitAndSuccess },
		TestCase{ "no-instance-and-gone", &TestNoInstanceAndGone },
		TestCase{ "process-failures", &TestProcessFailures },
		TestCase{ "response-parsing", &TestResponseParsing },
		TestCase{ "terminal-inputs", &TestTerminalInputs },
		TestCase{ "exit-codes", &TestExitCodes },
		TestCase{ "real-adapter-exit", &TestRealAdapterExit },
		TestCase{ "real-adapter-timeout", &TestRealAdapterTimeout },
	};
	auto failures = 0;
	for (const auto &test : tests) {
		try {
			test.method();
			std::cout << "PASS: " << test.name << '\n';
		} catch (const std::exception &error) {
			++failures;
			std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
		}
	}
	std::cout << "RESULT: " << (tests.size() - failures) << '/'
		<< tests.size() << " passed\n";
	return failures ? 1 : 0;
}
