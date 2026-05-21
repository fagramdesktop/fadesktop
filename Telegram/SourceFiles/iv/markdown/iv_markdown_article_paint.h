/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_article_selection.h"

namespace Iv::Markdown {

void PaintBlocks(
	Painter &p,
	const std::vector<LaidOutBlock> &blocks,
	std::vector<PreparedFormulaSlot> *formulas,
	std::vector<RenderedFormula> *renderedFormulas,
	MathRenderer *renderer,
	int devicePixelRatio,
	int outerWidth,
	const style::Markdown &markdown,
	const MarkdownArticlePaintCaches &caches,
	const PaintSelectionState &selectionState,
	QRect clip);

} // namespace Iv::Markdown
