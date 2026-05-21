/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_prepare_native_richtext.h"

#include <QtCore/QVector>

namespace Iv::Markdown {

[[nodiscard]] bool PrepareNativeIvBlocks(
	const QVector<MTPPageBlock> &blocks,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);

} // namespace Iv::Markdown
