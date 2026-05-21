/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_prepare_state.h"

namespace Iv::Markdown {

[[nodiscard]] int CountPreparedBlocks(const std::vector<PreparedBlock> &blocks);
[[nodiscard]] int FormulaSlotCount(const PreparedDocument &document);
void MeasurePreparedFormulas(PrepareState *state);

} // namespace Iv::Markdown
