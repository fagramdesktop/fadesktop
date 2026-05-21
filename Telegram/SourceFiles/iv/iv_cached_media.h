/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <QtCore/QString>

#include <memory>

struct WebPageData;

namespace Main {
class Session;
} // namespace Main

namespace Iv::Markdown {
class MediaRuntime;
} // namespace Iv::Markdown

namespace Iv {

[[nodiscard]] auto CreateCachedPageMediaRuntime(
	not_null<Main::Session*> session,
	not_null<WebPageData*> page,
	Fn<void(QString)> openChannel,
	Fn<void(QString)> joinChannel)
-> std::shared_ptr<Markdown::MediaRuntime>;

} // namespace Iv
