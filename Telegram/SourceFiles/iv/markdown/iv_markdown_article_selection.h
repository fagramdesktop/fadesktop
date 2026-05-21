/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_article_layout_blocks.h"

namespace Iv::Markdown {

enum class SelectableSegmentKind {
	TextLeaf,
	CodeBlock,
	DisplayMath,
	Table,
	Placeholder,
	Photo,
	Media,
};

struct SelectableSegment {
	SelectableSegmentKind kind = SelectableSegmentKind::TextLeaf;
	const Ui::Text::String *leaf = nullptr;
	const LaidOutBlock *block = nullptr;
	const LaidOutTableCell *cell = nullptr;
	QRect outerRect;
	QRect textRect;
	int textWidth = 0;
	style::align align = style::al_left;
	int index = -1;
	int length = 0;
	int tableSegmentIndex = -1;
	int parentSegmentIndex = -1;

	[[nodiscard]] bool isTextLeaf() const {
		return (leaf != nullptr);
	}
};

struct PaintSelectionState {
	const std::vector<SelectableSegment> *segments = nullptr;
	MarkdownArticleSelection selection;
	const MarkdownArticleSelectionEndpoints *endpoints = nullptr;

	[[nodiscard]] bool empty() const {
		return !segments || selection.empty();
	}
};

struct LogicalVisibleRange {
	int top = 0;
	int bottom = 0;
};

struct SegmentSpan {
	int from = 0;
	int till = 0;

	[[nodiscard]] bool empty() const {
		return (from >= till);
	}
};

void CollectSelectableSegments(
	std::vector<LaidOutBlock> *blocks,
	std::vector<SelectableSegment> *segments);
void CollectAnchors(
	const std::vector<LaidOutBlock> &blocks,
	std::vector<std::pair<QString, int>> *anchors);
[[nodiscard]] const SelectableSegment *FindSegment(
	const std::vector<SelectableSegment> *segments,
	int index);
[[nodiscard]] int SegmentLength(const SelectableSegment &segment);
[[nodiscard]] std::optional<TextSelection> TextSelectionForSegment(
	const SelectableSegment &segment,
	const PaintSelectionState &selectionState);
[[nodiscard]] std::optional<TextSelection> TextSelectionForSegmentIndex(
	const PaintSelectionState &selectionState,
	int index);
[[nodiscard]] bool WholeSegmentSelected(
	const SelectableSegment &segment,
	const PaintSelectionState &selectionState);
[[nodiscard]] bool WholeSegmentSelected(
	const PaintSelectionState &selectionState,
	int index);
[[nodiscard]] bool TableSegmentSelected(
	const PaintSelectionState &selectionState,
	int tableSegmentIndex);
[[nodiscard]] TextForMimeData TextForSegment(
	const SelectableSegment &segment,
	TextSelection selection = AllTextSelection);
[[nodiscard]] TextForMimeData TextForSelectedSegments(
	const std::vector<SelectableSegment> &segments,
	MarkdownArticleSelection selection,
	const MarkdownArticleSelectionEndpoints *endpoints);

} // namespace Iv::Markdown
