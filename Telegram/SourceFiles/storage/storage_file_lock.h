/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include <QtCore/QFile>

namespace Storage {

class FileLock {
public:
	FileLock();

	bool lock(QFile &file, QIODevice::OpenMode mode);
	void unlock();

	static constexpr auto kSkipBytes = size_type(4);

	~FileLock();

private:
	class Lock;
	struct Descriptor;
	struct LockingPid;

	static constexpr auto kLockOffset = index_type(0);
	static constexpr auto kLockLimit = kSkipBytes;

	std::unique_ptr<Lock> _lock;

};

} // namespace Storage
