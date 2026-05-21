/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_article_selection.h"
#include "lang/lang_keys.h"

#include <algorithm>

namespace Iv::Markdown {
namespace {

constexpr auto kCodeTabColumns = 4;
const auto kPhotoCopyLabel = u"Photo"_q;

struct TableCopySlot {
	const LaidOutTableCell *cell = nullptr;
	bool origin = false;
};

[[nodiscard]] TextForMimeData CopyTextForDisplayMath(const LaidOutBlock &block) {
	return TextForMimeData::Simple(u"$$"_q + block.copyText + u"$$"_q);
}

[[nodiscard]] TextForMimeData CopyTextForCodeBlock(
		const LaidOutBlock &block,
		TextSelection selection = AllTextSelection) {
	if (selection == AllTextSelection) {
		auto rich = tr::marked(block.copyText);
		if (!rich.text.isEmpty()) {
			rich.entities.push_back(EntityInText(
				EntityType::Pre,
				0,
				rich.text.size(),
				block.codeLanguage));
		}
		return TextForMimeData::Rich(std::move(rich));
	}
	auto from = 0;
	auto to = 0;
	auto displayPosition = 0;
	auto column = 0;
	auto found = false;
	const auto &text = block.copyText;
	for (auto i = 0, count = int(text.size()); i != count; ++i) {
		const auto ch = text[i];
		const auto width = (ch == QChar::Tabulation)
			? (kCodeTabColumns - (column % kCodeTabColumns))
			: 1;
		const auto nextDisplayPosition = displayPosition + width;
		if (selection.to <= displayPosition) {
			break;
		}
		if (selection.from < nextDisplayPosition
			&& selection.to > displayPosition) {
			if (!found) {
				from = i;
				found = true;
			}
			to = i + 1;
		}
		displayPosition = nextDisplayPosition;
		if (Ui::Text::IsNewline(ch)) {
			column = 0;
		} else {
			column += width;
		}
	}
	if (!found || to <= from) {
		return TextForMimeData();
	}
	auto rich = tr::marked(text.mid(from, to - from));
	if (!rich.text.isEmpty()) {
		rich.entities.push_back(EntityInText(
			EntityType::Pre,
			0,
			rich.text.size(),
			block.codeLanguage));
	}
	return TextForMimeData::Rich(std::move(rich));
}

[[nodiscard]] TextForMimeData CopyTextForTable(const LaidOutBlock &block) {
	auto result = TextForMimeData();
	const auto rowCount = int(block.tableRows.size());
	const auto columnCount = int(block.tableColumnWidths.size());
	auto grid = std::vector<std::vector<TableCopySlot>>(
		std::max(rowCount, 0),
		std::vector<TableCopySlot>(std::max(columnCount, 0)));
	for (auto rowIndex = 0; rowIndex != rowCount; ++rowIndex) {
		for (const auto &cell : block.tableRows[rowIndex].cells) {
			const auto fromRow = std::clamp(rowIndex, 0, rowCount);
			const auto toRow = std::clamp(rowIndex + cell.rowspan, 0, rowCount);
			const auto fromColumn = std::clamp(cell.column, 0, columnCount);
			const auto toColumn = std::clamp(
				cell.column + cell.colspan,
				0,
				columnCount);
			for (auto currentRow = fromRow; currentRow != toRow; ++currentRow) {
				for (auto currentColumn = fromColumn;
					currentColumn != toColumn;
					++currentColumn) {
					auto &slot = grid[currentRow][currentColumn];
					slot.cell = &cell;
					slot.origin = (currentRow == rowIndex)
						&& (currentColumn == cell.column);
				}
			}
		}
	}
	if (!block.leaf.isEmpty()) {
		result.append(block.leaf.toTextForMimeData());
		if (rowCount > 0 && columnCount > 0) {
			result.append(u"\n"_q);
		}
	}
	for (auto rowIndex = 0; rowIndex != rowCount; ++rowIndex) {
		if (rowIndex) {
			result.append(u"\n"_q);
		}
		for (auto column = 0; column != columnCount; ++column) {
			if (column) {
				result.append(u"\t"_q);
			}
			const auto &slot = grid[rowIndex][column];
			if (slot.origin && slot.cell) {
				result.append(slot.cell->leaf.toTextForMimeData());
			}
		}
	}
	return result;
}

[[nodiscard]] TextForMimeData CopyTextForMediaBlock(
		const QString &label,
		const Ui::Text::String &captionLeaf) {
	auto result = label.isEmpty()
		? TextForMimeData()
		: TextForMimeData::Simple(label);
	if (!captionLeaf.isEmpty()) {
		if (!result.empty()) {
			result.append(u"\n"_q);
		}
		result.append(captionLeaf.toTextForMimeData());
	}
	return result;
}

[[nodiscard]] TextForMimeData CopyTextForSingleMediaBlock(
		const LaidOutBlock &block,
		const QString &fallback = QString()) {
	const auto selectionData = block.mediaBlock
		? block.mediaBlock->selectionData()
		: MediaBlockSelectionData();
	const auto label = !selectionData.copyText.isEmpty()
		? selectionData.copyText
		: !block.copyText.isEmpty()
		? block.copyText
		: !block.labelText.isEmpty()
		? block.labelText
		: fallback;
	return CopyTextForMediaBlock(label, block.leaf);
}

[[nodiscard]] TextForMimeData CopyTextForPhotoBlock(const LaidOutBlock &block) {
	return CopyTextForSingleMediaBlock(block, kPhotoCopyLabel);
}

[[nodiscard]] TextForMimeData CopyTextForPlaceholderBlock(
		const LaidOutBlock &block) {
	return CopyTextForMediaBlock(
		block.labelText.isEmpty() ? block.copyText : block.labelText,
		block.leaf);
}

[[nodiscard]] int AddSelectableSegment(
		std::vector<SelectableSegment> *segments,
		SelectableSegment segment) {
	segment.index = int(segments->size());
	segment.length = std::max(segment.length, 0);
	segments->push_back(std::move(segment));
	return segment.index;
}

[[nodiscard]] int CompareSelectionPositions(
		MarkdownArticleSelectionPosition a,
		MarkdownArticleSelectionPosition b) {
	if (a.segment != b.segment) {
		return (a.segment < b.segment) ? -1 : 1;
	}
	if (a.offset != b.offset) {
		return (a.offset < b.offset) ? -1 : 1;
	}
	return 0;
}

[[nodiscard]] MarkdownArticleSelection NormalizeSelection(
		MarkdownArticleSelection selection) {
	if (selection.empty()) {
		return {};
	}
	if (CompareSelectionPositions(selection.from, selection.to) > 0) {
		std::swap(selection.from, selection.to);
	}
	return selection;
}

[[nodiscard]] int LastTableCellSegmentIndex(
		const std::vector<SelectableSegment> *segments,
		int tableSegmentIndex) {
	auto result = tableSegmentIndex;
	if (!segments) {
		return result;
	}
	for (const auto &segment : *segments) {
		if (segment.tableSegmentIndex == tableSegmentIndex) {
			result = std::max(result, segment.index);
		}
	}
	return result;
}

[[nodiscard]] std::optional<int> SingleTableCellSelection(
		const PaintSelectionState &selectionState,
		int tableSegmentIndex) {
	if (selectionState.empty()
		|| !selectionState.endpoints
		|| tableSegmentIndex < 0) {
		return std::nullopt;
	}
	const auto normalized = NormalizeSelection(selectionState.selection);
	if (normalized.empty()) {
		return std::nullopt;
	}
	const auto lastCellSegment = LastTableCellSegmentIndex(
		selectionState.segments,
		tableSegmentIndex);
	const auto spansWholeTable = (normalized.from.segment < tableSegmentIndex)
		&& (normalized.to.segment > lastCellSegment);
	auto tableHit = false;
	auto cellSegment = -1;
	auto multipleCells = false;
	const auto consider = [&](MarkdownArticleSelectionEndpoint endpoint) {
		if (!endpoint.valid() || !endpoint.direct) {
			return;
		}
		const auto segment = FindSegment(selectionState.segments, endpoint.segment);
		if (!segment) {
			return;
		}
		if (segment->index == tableSegmentIndex) {
			tableHit = true;
			return;
		}
		if (segment->tableSegmentIndex != tableSegmentIndex) {
			return;
		}
		if (cellSegment < 0) {
			cellSegment = segment->index;
		} else if (cellSegment != segment->index) {
			multipleCells = true;
		}
	};
	consider(selectionState.endpoints->from);
	consider(selectionState.endpoints->to);
	if (tableHit || multipleCells || cellSegment < 0 || spansWholeTable) {
		return std::nullopt;
	}
	return cellSegment;
}

[[nodiscard]] std::optional<TextSelection> BaseTextSelectionForSegment(
		const SelectableSegment &segment,
		MarkdownArticleSelection selection) {
	if (selection.empty() || !segment.isTextLeaf()) {
		return std::nullopt;
	}
	selection = NormalizeSelection(selection);
	if (selection.empty()
		|| selection.from.segment > segment.index
		|| selection.to.segment < segment.index) {
		return std::nullopt;
	}
	auto from = 0;
	auto to = SegmentLength(segment);
	if (selection.from.segment == segment.index) {
		from = selection.from.offset;
	}
	if (selection.to.segment == segment.index) {
		to = selection.to.offset;
	}
	from = std::clamp(from, 0, SegmentLength(segment));
	to = std::clamp(to, 0, SegmentLength(segment));
	if (from >= to) {
		return std::nullopt;
	}
	return TextSelection(uint16(from), uint16(to));
}

[[nodiscard]] bool RangeSelectsWholeSegment(
		const SelectableSegment &segment,
		MarkdownArticleSelection selection) {
	selection = NormalizeSelection(selection);
	if (selection.empty()
		|| selection.from.segment > segment.index
		|| selection.to.segment < segment.index) {
		return false;
	}
	auto from = 0;
	auto to = SegmentLength(segment);
	if (selection.from.segment == segment.index) {
		from = selection.from.offset;
	}
	if (selection.to.segment == segment.index) {
		to = selection.to.offset;
	}
	from = std::clamp(from, 0, SegmentLength(segment));
	to = std::clamp(to, 0, SegmentLength(segment));
	return (from < to);
}

} // namespace

void CollectSelectableSegments(
		std::vector<LaidOutBlock> *blocks,
		std::vector<SelectableSegment> *segments) {
	if (!blocks) {
		return;
	}
	for (auto &block : *blocks) {
		block.segmentIndex = -1;
		block.secondarySegmentIndex = -1;
		block.tertiarySegmentIndex = -1;
		switch (block.kind) {
		case PreparedBlockKind::Paragraph:
		case PreparedBlockKind::Heading:
		case PreparedBlockKind::Details: {
			if (block.leaf.isEmpty() && block.textRect.isEmpty()) {
				break;
			}
			auto segment = SelectableSegment();
			segment.kind = SelectableSegmentKind::TextLeaf;
			segment.leaf = &block.leaf;
			segment.block = &block;
			segment.outerRect = (block.kind == PreparedBlockKind::Details)
				? block.headerRect
				: block.textRect;
			segment.textRect = block.textRect;
			segment.textWidth = block.textWidth;
			segment.length = block.leaf.length();
			block.segmentIndex = AddSelectableSegment(
				segments,
				std::move(segment));
		} break;
		case PreparedBlockKind::CodeBlock: {
			auto segment = SelectableSegment();
			segment.kind = SelectableSegmentKind::CodeBlock;
			segment.leaf = &block.leaf;
			segment.block = &block;
			segment.outerRect = block.outer;
			segment.textRect = block.textRect;
			segment.textWidth = block.textWidth;
			segment.length = block.leaf.length();
			block.segmentIndex = AddSelectableSegment(
				segments,
				std::move(segment));
		} break;
		case PreparedBlockKind::DisplayMath: {
			auto segment = SelectableSegment();
			segment.kind = SelectableSegmentKind::DisplayMath;
			segment.block = &block;
			segment.outerRect = block.visibleFormulaRect;
			segment.length = 1;
			block.segmentIndex = AddSelectableSegment(
				segments,
				std::move(segment));
		} break;
		case PreparedBlockKind::Table: {
			if (!block.textRect.isEmpty() && !block.leaf.isEmpty()) {
				auto textSegment = SelectableSegment();
				textSegment.kind = SelectableSegmentKind::TextLeaf;
				textSegment.leaf = &block.leaf;
				textSegment.block = &block;
				textSegment.outerRect = block.textRect;
				textSegment.textRect = block.textRect;
				textSegment.textWidth = block.textWidth;
				textSegment.length = block.leaf.length();
				block.secondarySegmentIndex = AddSelectableSegment(
					segments,
					std::move(textSegment));
			}
			auto segment = SelectableSegment();
			segment.kind = SelectableSegmentKind::Table;
			segment.block = &block;
			segment.outerRect = block.visibleTableRect;
			segment.length = 1;
			block.segmentIndex = AddSelectableSegment(
				segments,
				std::move(segment));
			if (block.secondarySegmentIndex >= 0) {
				(*segments)[block.secondarySegmentIndex].parentSegmentIndex
					= block.segmentIndex;
			}
			for (auto &row : block.tableRows) {
				for (auto &cell : row.cells) {
					auto cellSegment = SelectableSegment();
					cellSegment.kind = SelectableSegmentKind::TextLeaf;
					cellSegment.leaf = &cell.leaf;
					cellSegment.block = &block;
					cellSegment.cell = &cell;
					cellSegment.outerRect = cell.outer;
					cellSegment.textRect = cell.textRect;
					cellSegment.textWidth = cell.textWidth;
					cellSegment.align = cell.align;
					cellSegment.length = cell.leaf.length();
					cellSegment.tableSegmentIndex = block.segmentIndex;
					cell.tableSegmentIndex = block.segmentIndex;
					cell.segmentIndex = AddSelectableSegment(
						segments,
						std::move(cellSegment));
				}
			}
		} break;
		case PreparedBlockKind::RelatedArticle:
		case PreparedBlockKind::Placeholder:
		case PreparedBlockKind::Photo:
		case PreparedBlockKind::Video:
		case PreparedBlockKind::Audio:
		case PreparedBlockKind::Map:
		case PreparedBlockKind::Channel:
		case PreparedBlockKind::GroupedMedia: {
			auto segment = SelectableSegment();
			segment.kind = (block.kind == PreparedBlockKind::Photo)
				? SelectableSegmentKind::Photo
				: (block.kind == PreparedBlockKind::Placeholder)
				? SelectableSegmentKind::Placeholder
				: SelectableSegmentKind::Media;
			segment.block = &block;
			segment.outerRect = block.mediaRect;
			segment.length = 1;
			block.segmentIndex = AddSelectableSegment(
				segments,
				std::move(segment));
			if (!block.textRect.isEmpty() && !block.leaf.isEmpty()) {
				auto textSegment = SelectableSegment();
				textSegment.kind = SelectableSegmentKind::TextLeaf;
				textSegment.leaf = &block.leaf;
				textSegment.block = &block;
				textSegment.outerRect = block.textRect;
				textSegment.textRect = block.textRect;
				textSegment.textWidth = block.textWidth;
				textSegment.length = block.leaf.length();
				textSegment.parentSegmentIndex = block.segmentIndex;
				block.secondarySegmentIndex = AddSelectableSegment(
					segments,
					std::move(textSegment));
			}
		} break;
		case PreparedBlockKind::EmbedPost: {
			if (!block.labelRect.isEmpty() && !block.labelLeaf.isEmpty()) {
				auto authorSegment = SelectableSegment();
				authorSegment.kind = SelectableSegmentKind::TextLeaf;
				authorSegment.leaf = &block.labelLeaf;
				authorSegment.block = &block;
				authorSegment.outerRect = block.labelRect;
				authorSegment.textRect = block.labelRect;
				authorSegment.textWidth = block.labelWidth;
				authorSegment.length = block.labelLeaf.length();
				block.segmentIndex = AddSelectableSegment(
					segments,
					std::move(authorSegment));
			}
			if (!block.subtitleRect.isEmpty() && !block.subtitleLeaf.isEmpty()) {
				auto dateSegment = SelectableSegment();
				dateSegment.kind = SelectableSegmentKind::TextLeaf;
				dateSegment.leaf = &block.subtitleLeaf;
				dateSegment.block = &block;
				dateSegment.outerRect = block.subtitleRect;
				dateSegment.textRect = block.subtitleRect;
				dateSegment.textWidth = block.subtitleWidth;
				dateSegment.length = block.subtitleLeaf.length();
				block.secondarySegmentIndex = AddSelectableSegment(
					segments,
					std::move(dateSegment));
			}
			CollectSelectableSegments(&block.children, segments);
			if (!block.textRect.isEmpty() && !block.leaf.isEmpty()) {
				auto captionSegment = SelectableSegment();
				captionSegment.kind = SelectableSegmentKind::TextLeaf;
				captionSegment.leaf = &block.leaf;
				captionSegment.block = &block;
				captionSegment.outerRect = block.textRect;
				captionSegment.textRect = block.textRect;
				captionSegment.textWidth = block.textWidth;
				captionSegment.length = block.leaf.length();
				block.tertiarySegmentIndex = AddSelectableSegment(
					segments,
					std::move(captionSegment));
			}
			continue;
		}
		case PreparedBlockKind::List:
		case PreparedBlockKind::ListItem:
		case PreparedBlockKind::Quote:
		case PreparedBlockKind::Rule:
			break;
		}
		CollectSelectableSegments(&block.children, segments);
	}
}

void CollectAnchors(
		const std::vector<LaidOutBlock> &blocks,
		std::vector<std::pair<QString, int>> *anchors) {
	if (!anchors) {
		return;
	}
	for (const auto &block : blocks) {
		if (!block.anchorId.isEmpty()) {
			anchors->push_back({ block.anchorId, block.outer.top() });
		}
		CollectAnchors(block.children, anchors);
	}
}

const SelectableSegment *FindSegment(
		const std::vector<SelectableSegment> *segments,
		int index) {
	if (!segments || index < 0 || index >= int(segments->size())) {
		return nullptr;
	}
	return &(*segments)[index];
}

int SegmentLength(const SelectableSegment &segment) {
	return std::max(segment.length, 0);
}

bool TableSegmentSelected(
		const PaintSelectionState &selectionState,
		int tableSegmentIndex) {
	if (selectionState.empty() || tableSegmentIndex < 0) {
		return false;
	}
	if (SingleTableCellSelection(selectionState, tableSegmentIndex)) {
		return false;
	}
	const auto normalized = NormalizeSelection(selectionState.selection);
	if (normalized.empty()) {
		return false;
	}
	auto selectedCells = 0;
	auto selectedCellIndex = -1;
	for (const auto &segment : *selectionState.segments) {
		if (segment.tableSegmentIndex != tableSegmentIndex
			|| segment.index == tableSegmentIndex) {
			continue;
		}
		const auto textSelection = BaseTextSelectionForSegment(
			segment,
			normalized);
		if (!textSelection || textSelection->empty()) {
			continue;
		}
		if (++selectedCells == 1) {
			selectedCellIndex = segment.index;
		} else {
			return true;
		}
	}
	const auto table = FindSegment(selectionState.segments, tableSegmentIndex);
	if (!table || !RangeSelectsWholeSegment(*table, normalized)) {
		return false;
	}
	if (selectedCells != 1) {
		return true;
	}
	if (normalized.from.segment == tableSegmentIndex
		|| normalized.to.segment == tableSegmentIndex) {
		return true;
	}
	const auto lower = std::min(tableSegmentIndex, selectedCellIndex);
	const auto upper = std::max(tableSegmentIndex, selectedCellIndex);
	return (normalized.from.segment < lower)
		&& (normalized.to.segment > upper);
}

std::optional<TextSelection> TextSelectionForSegment(
		const SelectableSegment &segment,
		const PaintSelectionState &selectionState) {
	if (selectionState.empty()) {
		return std::nullopt;
	}
	if (segment.tableSegmentIndex >= 0) {
		if (const auto singleCell = SingleTableCellSelection(
				selectionState,
				segment.tableSegmentIndex);
			singleCell && *singleCell != segment.index) {
			return std::nullopt;
		}
	}
	if (segment.tableSegmentIndex >= 0
		&& TableSegmentSelected(selectionState, segment.tableSegmentIndex)) {
		return std::nullopt;
	}
	return BaseTextSelectionForSegment(segment, selectionState.selection);
}

std::optional<TextSelection> TextSelectionForSegmentIndex(
		const PaintSelectionState &selectionState,
		int index) {
	const auto segment = FindSegment(selectionState.segments, index);
	return segment
		? TextSelectionForSegment(*segment, selectionState)
		: std::nullopt;
}

bool WholeSegmentSelected(
		const SelectableSegment &segment,
		const PaintSelectionState &selectionState) {
	if (selectionState.empty() || segment.isTextLeaf()) {
		return false;
	}
	if (segment.kind == SelectableSegmentKind::Table) {
		return TableSegmentSelected(selectionState, segment.index);
	}
	return RangeSelectsWholeSegment(segment, selectionState.selection);
}

bool WholeSegmentSelected(
		const PaintSelectionState &selectionState,
		int index) {
	const auto segment = FindSegment(selectionState.segments, index);
	return segment
		? WholeSegmentSelected(*segment, selectionState)
		: false;
}

TextForMimeData TextForSegment(
		const SelectableSegment &segment,
		TextSelection selection) {
	switch (segment.kind) {
	case SelectableSegmentKind::TextLeaf:
		return segment.leaf
			? segment.leaf->toTextForMimeData(selection)
			: TextForMimeData();
	case SelectableSegmentKind::CodeBlock:
		return segment.block
			? CopyTextForCodeBlock(*segment.block, selection)
			: TextForMimeData();
	case SelectableSegmentKind::DisplayMath:
		return segment.block
			? CopyTextForDisplayMath(*segment.block)
			: TextForMimeData();
	case SelectableSegmentKind::Table:
		return segment.block
			? CopyTextForTable(*segment.block)
			: TextForMimeData();
	case SelectableSegmentKind::Placeholder:
		return segment.block
			? CopyTextForPlaceholderBlock(*segment.block)
			: TextForMimeData();
	case SelectableSegmentKind::Photo:
		return segment.block
			? CopyTextForPhotoBlock(*segment.block)
			: TextForMimeData();
	case SelectableSegmentKind::Media:
		return segment.block
			? CopyTextForSingleMediaBlock(*segment.block)
			: TextForMimeData();
	}
	return TextForMimeData();
}

TextForMimeData TextForSelectedSegments(
		const std::vector<SelectableSegment> &segments,
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) {
	if (selection.empty()) {
		return TextForMimeData();
	}
	const auto selectionState = PaintSelectionState{
		.segments = &segments,
		.selection = selection,
		.endpoints = endpoints,
	};
	auto pieces = std::vector<TextForMimeData>();
	for (const auto &segment : segments) {
		if (segment.isTextLeaf()) {
			if (segment.parentSegmentIndex >= 0
				&& WholeSegmentSelected(
					selectionState,
					segment.parentSegmentIndex)) {
				continue;
			}
			if (const auto textSelection = TextSelectionForSegment(
					segment,
					selectionState);
				textSelection && !textSelection->empty()) {
				if (auto text = TextForSegment(segment, *textSelection);
					!text.empty()) {
					pieces.push_back(std::move(text));
				}
			}
			continue;
		}
		if (!WholeSegmentSelected(segment, selectionState)) {
			continue;
		}
		if (auto text = TextForSegment(segment); !text.empty()) {
			pieces.push_back(std::move(text));
		}
	}
	if (pieces.empty()) {
		return TextForMimeData();
	} else if (pieces.size() == 1) {
		return std::move(pieces.front());
	}
	auto result = TextForMimeData();
	for (auto i = 0, count = int(pieces.size()); i != count; ++i) {
		if (i) {
			result.append(u"\n"_q);
		}
		result.append(std::move(pieces[i]));
	}
	return result;
}

} // namespace Iv::Markdown
