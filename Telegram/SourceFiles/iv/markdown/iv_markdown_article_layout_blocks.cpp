/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_article_layout_blocks.h"
#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_article_text.h"
#include "lang/lang_keys.h"
#include "spellcheck/spellcheck_highlight_syntax.h"

#include "styles/style_iv.h"
#include "styles/style_widgets.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Iv::Markdown {
namespace {

constexpr auto kIvMarkedTextOptions = TextParseOptions{
	TextParseMultiline,
	0,
	0,
	Qt::LayoutDirectionAuto,
};

constexpr auto kCodeTabColumns = 4;
constexpr auto kCodeTrailingGuard = 0x2060;

[[nodiscard]] style::align CellAlign(TableAlignment alignment) {
	switch (alignment) {
	case TableAlignment::Center:
		return style::al_center;
	case TableAlignment::Right:
		return style::al_right;
	case TableAlignment::None:
	case TableAlignment::Left:
		return style::al_left;
	}
	return style::al_left;
}

[[nodiscard]] const style::TextStyle &TableCellTextStyle(
		const PreparedTableCell &prepared,
		const style::Markdown &) {
	if (prepared.header) {
		return st::defaultTable.defaultLabel.style;
	}
	return st::defaultTable.defaultValue.style;
}

[[nodiscard]] const style::TextStyle &TableCellTextStyle(
		const LaidOutTableCell &cell,
		const style::Markdown &) {
	if (cell.header) {
		return st::defaultTable.defaultLabel.style;
	}
	return st::defaultTable.defaultValue.style;
}

[[nodiscard]] int TableBorder(
		bool bordered,
		const style::Markdown &markdown) {
	return bordered ? markdown.table.border : 0;
}

[[nodiscard]] TableCellLayoutData InitializeTableCellLayout(
		const PreparedTableCell &prepared,
		const std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown) {
	auto result = TableCellLayoutData();
	const auto &textStyle = TableCellTextStyle(prepared, markdown);
	result.cell.header = prepared.header;
	result.cell.verticalAlignment = prepared.verticalAlignment;
	result.cell.align = CellAlign(prepared.alignment);
	result.cell.column = std::max(prepared.column, 0);
	result.cell.colspan = std::max(prepared.colspan, 1);
	result.cell.rowspan = std::max(prepared.rowspan, 1);
	SetTextLeaf(
		&result.cell.leaf,
		textStyle,
		prepared.text,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		TableCellTextMinResizeWidth(textStyle, markdown));
	BindLinks(&result.cell.leaf, prepared.links);
	result.preferredWidth = result.cell.leaf.maxWidth();
	result.preferredHeight = std::max(
		result.cell.leaf.countHeight(std::max(result.preferredWidth, 1), true),
		TextLineHeight(textStyle));
	return result;
}

[[nodiscard]] int TableSpanWidth(
		const std::vector<int> &columnWidths,
		int column,
		int colspan,
		int border) {
	const auto from = std::clamp(column, 0, int(columnWidths.size()));
	const auto to = std::clamp(column + colspan, 0, int(columnWidths.size()));
	if (from >= to) {
		return 0;
	}
	auto result = 0;
	for (auto current = from; current != to; ++current) {
		result += columnWidths[current];
	}
	return result + std::max(to - from - 1, 0) * border;
}

[[nodiscard]] int TableSpanHeight(
		const std::vector<int> &rowHeights,
		int row,
		int rowspan,
		int border) {
	const auto from = std::clamp(row, 0, int(rowHeights.size()));
	const auto to = std::clamp(row + rowspan, 0, int(rowHeights.size()));
	if (from >= to) {
		return 0;
	}
	auto result = 0;
	for (auto current = from; current != to; ++current) {
		result += rowHeights[current];
	}
	return result + std::max(to - from - 1, 0) * border;
}

void DistributeSizeDeficits(
		std::vector<int> *sizes,
		std::vector<int> deficits,
		int *extra) {
	auto remaining = 0;
	for (const auto deficit : deficits) {
		remaining += std::max(deficit, 0);
	}
	while (*extra > 0 && remaining > 0) {
		auto active = 0;
		for (const auto deficit : deficits) {
			if (deficit > 0) {
				++active;
			}
		}
		if (!active) {
			break;
		}
		const auto step = std::max(*extra / active, 1);
		for (auto i = 0, count = int(deficits.size()); i != count && *extra > 0; ++i) {
			if (deficits[i] <= 0) {
				continue;
			}
			const auto delta = std::min({ deficits[i], step, *extra });
			(*sizes)[i] += delta;
			deficits[i] -= delta;
			remaining -= delta;
			*extra -= delta;
		}
	}
}

void DistributeSpanDelta(
		std::vector<int> *sizes,
		int from,
		int to,
		int delta) {
	from = std::clamp(from, 0, int(sizes->size()));
	to = std::clamp(to, 0, int(sizes->size()));
	while (delta > 0 && from < to) {
		const auto active = to - from;
		const auto step = std::max(delta / active, 1);
		for (auto i = from; i != to && delta > 0; ++i) {
			const auto current = std::min(step, delta);
			(*sizes)[i] += current;
			delta -= current;
		}
	}
}

struct TableSpannedCellLayout {
	int row = 0;
	const TableCellLayoutData *cell = nullptr;
};

[[nodiscard]] std::vector<int> ComputeTableColumnWidths(
		const std::vector<TableRowLayoutData> &rows,
		int columnCount,
		int width,
		const style::Markdown &markdown,
		bool bordered,
		bool *overflowed) {
	const auto &padding = markdown.table.cellPadding;
	const auto border = TableBorder(bordered, markdown);
	const auto minimum = markdown.table.minColumnWidth;
	auto result = std::vector<int>(std::max(columnCount, 0), minimum);
	auto singleColumnDeficits = std::vector<int>(std::max(columnCount, 0), 0);
	auto spannedCells = std::vector<TableSpannedCellLayout>();
	for (auto row = 0, rowCount = int(rows.size()); row != rowCount; ++row) {
		for (const auto &cell : rows[row].cells) {
			const auto from = std::clamp(cell.cell.column, 0, columnCount);
			const auto to = std::clamp(
				cell.cell.column + cell.cell.colspan,
				0,
				columnCount);
			if (from >= to) {
				continue;
			}
			const auto preferredWidth = cell.preferredWidth
				+ padding.left()
				+ padding.right();
			if ((to - from) == 1) {
				singleColumnDeficits[from] = std::max(
					singleColumnDeficits[from],
					preferredWidth - minimum);
			} else {
				spannedCells.push_back({ row, &cell });
			}
		}
	}

	const auto availableWidth = std::max(width, 1);
	const auto minimumGridWidth = border
		+ columnCount * (minimum + border);
	*overflowed = (minimumGridWidth > availableWidth);
	if (*overflowed || !columnCount) {
		return result;
	}

	auto extra = availableWidth - minimumGridWidth;
	DistributeSizeDeficits(&result, std::move(singleColumnDeficits), &extra);
	std::sort(
		spannedCells.begin(),
		spannedCells.end(),
		[](const TableSpannedCellLayout &a, const TableSpannedCellLayout &b) {
			const auto aSpan = a.cell ? a.cell->cell.colspan : 0;
			const auto bSpan = b.cell ? b.cell->cell.colspan : 0;
			return (aSpan < bSpan)
				|| ((aSpan == bSpan) && (a.row < b.row))
				|| ((aSpan == bSpan)
					&& (a.row == b.row)
					&& a.cell
					&& b.cell
					&& (a.cell->cell.column < b.cell->cell.column));
		});
	for (const auto &spanned : spannedCells) {
		if (!spanned.cell || extra <= 0) {
			break;
		}
		const auto from = std::clamp(spanned.cell->cell.column, 0, columnCount);
		const auto to = std::clamp(
			spanned.cell->cell.column + spanned.cell->cell.colspan,
			0,
			columnCount);
		if (from >= to) {
			continue;
		}
		const auto preferredWidth = spanned.cell->preferredWidth
			+ padding.left()
			+ padding.right();
		const auto currentWidth = TableSpanWidth(
			result,
			from,
			to - from,
			border);
		const auto delta = std::min(
			std::max(preferredWidth - currentWidth, 0),
			extra);
		DistributeSpanDelta(&result, from, to, delta);
		extra -= delta;
	}

	if (extra > 0) {
		const auto base = extra / columnCount;
		const auto tail = extra % columnCount;
		for (auto column = 0; column != columnCount; ++column) {
			result[column] += base + ((column < tail) ? 1 : 0);
		}
	}
	return result;
}

void SetPlainTextLeaf(
		Ui::Text::String *leaf,
		const style::TextStyle &textStyle,
		const QString &text,
		int width) {
	*leaf = Ui::Text::String(TextMinResizeWidth(width));
	leaf->setMarkedText(
		textStyle,
		TextWithEntities::Simple(text),
		kIvMarkedTextOptions);
}

[[nodiscard]] int LeafHeight(
		const Ui::Text::String &leaf,
		const style::TextStyle &textStyle,
		int width) {
	return std::max(
		leaf.countHeight(width, true),
		TextLineHeight(textStyle));
}

[[nodiscard]] int LeafHeightWithLineLimit(
		const Ui::Text::String &leaf,
		const style::TextStyle &textStyle,
		int width,
		int lines) {
	auto result = LeafHeight(leaf, textStyle, width);
	if (lines > 0) {
		result = std::min(result, lines * TextLineHeight(textStyle));
	}
	return result;
}

[[nodiscard]] QRect PaddedBand(
		int left,
		int width,
		QMargins padding) {
	const auto paddedLeft = left + padding.left();
	const auto paddedWidth = std::max(
		width - padding.left() - padding.right(),
		1);
	return QRect(paddedLeft, 0, paddedWidth, 0);
}

[[nodiscard]] QRect ArticleTextBand(
		int fallbackLeft,
		int fallbackWidth,
		const style::Markdown &markdown,
		const LayoutContext &context) {
	return context.useArticleBands
		? PaddedBand(
			context.articleLeft,
			context.articleWidth,
			markdown.textPadding)
		: QRect(fallbackLeft, 0, std::max(fallbackWidth, 1), 0);
}

[[nodiscard]] int LimitedMediaWidth(
		int availableWidth,
		int intrinsicWidth) {
	const auto limit = (intrinsicWidth > 0)
		? (2 * intrinsicWidth)
		: availableWidth;
	return std::clamp(limit, 1, std::max(availableWidth, 1));
}

void ApplyMediaBlockGeometry(LaidOutBlock *block, QRect geometry) {
	if (!block->mediaBlock) {
		return;
	}
	block->mediaBlock->setGeometry(geometry);
	auto actual = block->mediaBlock->geometry();
	if (actual.width() < geometry.width() && actual.x() == geometry.x()) {
		geometry.moveLeft(
			geometry.x() + std::max((geometry.width() - actual.width()) / 2, 0));
		block->mediaBlock->setGeometry(geometry);
		actual = block->mediaBlock->geometry();
	}
	block->mediaRect = actual;
	block->firstLineBaseline = block->mediaBlock->firstLineBaseline();
	block->visibleMediaRect = block->mediaRect;
}

void LayoutMediaCaptionText(
		LaidOutBlock *block,
		const TextWithEntities &text,
		const std::vector<PreparedLink> &links,
		const std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::TextStyle &textStyle,
		int left,
		int top,
		int width) {
	block->textWidth = std::max(width, 1);
	SetTextLeaf(
		&block->leaf,
		textStyle,
		text,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		block->textWidth);
	BindLinks(&block->leaf, links);
	block->textRect = QRect(
		left,
		top,
		block->textWidth,
		std::max(
			block->leaf.countHeight(block->textWidth, true),
			TextLineHeight(textStyle)));
}

} // namespace

void LayoutMediaCaption(
		LaidOutBlock *block,
		const PreparedBlock &prepared,
		const std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		int skip,
		int *bottom,
		LayoutContext context) {
	if (prepared.text.text.isEmpty()) {
		return;
	}
	block->supplementary = prepared.supplementary;
	const auto textBand = ArticleTextBand(left, width, markdown, context);
	LayoutMediaCaptionText(
		block,
		prepared.text,
		prepared.links,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown.body,
		textBand.x(),
		top + skip,
		textBand.width());
	*bottom = block->textRect.y() + block->textRect.height();
}

[[nodiscard]] int SingleDigitOrderedMarkerWidth(
		const style::Markdown &markdown) {
	return std::max(
		markdown.body.font->width(u"8."_q),
		markdown.body.font->width(u"8)"_q));
}

QString CodeBlockDisplayText(const QString &text) {
	auto result = QString();
	result.reserve(text.size());

	auto column = 0;
	for (const auto ch : text) {
		if (ch == QChar::Tabulation) {
			const auto count = kCodeTabColumns - (column % kCodeTabColumns);
			for (auto i = 0; i != count; ++i) {
				result.append(QChar::Space);
			}
			column += count;
			continue;
		}
		result.append(ch);
		if (Ui::Text::IsNewline(ch)) {
			column = 0;
		} else {
			++column;
		}
	}
	if (result.isEmpty() || Ui::Text::IsTrimmed(result.back())) {
		result.append(QChar(kCodeTrailingGuard));
	}
	return result;
}

bool IsFlowKind(PreparedBlockKind kind) {
	return (kind == PreparedBlockKind::Paragraph)
		|| (kind == PreparedBlockKind::Heading);
}

bool IsAnchorOnlyBlock(const PreparedBlock &block) {
	return (block.kind == PreparedBlockKind::Paragraph)
		&& !block.anchorId.isEmpty()
		&& block.text.text.isEmpty()
		&& block.text.entities.empty()
		&& block.links.empty()
		&& block.children.empty();
}

QString ListMarkerText(const PreparedBlock &block) {
	if (block.listKind == ListKind::Ordered) {
		const auto delimiter = (block.listDelimiter == ListDelimiter::Parenthesis)
			? u")"_q
			: u"."_q;
		return QString::number(block.orderedNumber) + delimiter;
	}
	return QString();
}

int TextLineHeight(const style::TextStyle &style) {
	return std::max(style.lineHeight, style.font->height);
}

[[nodiscard]] int NominalTextBaseline(
		const style::TextStyle &style,
		int top) {
	const auto lineHeight = TextLineHeight(style);
	const auto textTop = top
		+ (std::max(lineHeight - style.font->height, 0) / 2);
	return textTop + style.font->ascent;
}

[[nodiscard]] int LeafFirstLineBaseline(
		const Ui::Text::String &leaf,
		const QRect &textRect,
		const style::TextStyle &style,
		bool breakEverywhere = true) {
	const auto lines = leaf.countLinesGeometry(textRect.width(), breakEverywhere);
	return textRect.y() + (lines.empty()
		? NominalTextBaseline(style, 0)
		: lines.front().baseline);
}

QPoint BulletMarkerCenter(
		int left,
		int baseline,
		const style::Markdown &markdown) {
	const auto &list = markdown.list;
	const auto lineHeight = TextLineHeight(markdown.body);
	const auto markerWidth = SingleDigitOrderedMarkerWidth(markdown);
	const auto nominalBaseline = NominalTextBaseline(markdown.body, 0);
	return QPoint(
		left + list.markerWidth - list.bulletLeftShift - ((markerWidth + 1) / 2),
		baseline + (lineHeight / 2) - nominalBaseline);
}

QMargins BlockquotePadding(const style::QuoteStyle &style) {
	return style.padding
		+ QMargins(0, style.header + style.verticalSkip, 0, style.verticalSkip);
}

Ui::Text::GeometryDescriptor TextGeometry(int width) {
	auto result = Ui::Text::SimpleGeometry(std::max(width, 1), 0, 0, false);
	result.breakEverywhere = true;
	return result;
}

int TextMinResizeWidth(int width) {
	return std::max(width, 1);
}

int TableCellTextMinResizeWidth(
		const style::TextStyle &textStyle,
		const style::Markdown &markdown) {
	const auto &padding = markdown.table.cellPadding;
	return std::max({
		markdown.table.minColumnWidth - padding.left() - padding.right(),
		textStyle.font->spacew,
		1,
	});
}

int BlockSkip(
		const PreparedBlock &block,
		const style::Markdown &markdown) {
	if (IsAnchorOnlyBlock(block)) {
		return 0;
	}
	const auto &skips = markdown.blockSkips;
	switch (block.kind) {
	case PreparedBlockKind::Paragraph:
		return skips.paragraph;
	case PreparedBlockKind::Heading:
		return skips.heading;
	case PreparedBlockKind::CodeBlock:
		return skips.code;
	case PreparedBlockKind::Rule:
		return skips.rule;
	case PreparedBlockKind::List:
	case PreparedBlockKind::ListItem:
		return skips.paragraph;
	case PreparedBlockKind::Quote:
		return skips.quote;
	case PreparedBlockKind::DisplayMath:
		return skips.displayMath;
	case PreparedBlockKind::Table:
		return skips.table;
	case PreparedBlockKind::Photo:
		return skips.photo;
	case PreparedBlockKind::Video:
		return skips.video;
	case PreparedBlockKind::Audio:
		return skips.audio;
	case PreparedBlockKind::Map:
		return skips.map;
	case PreparedBlockKind::Channel:
		return skips.channel;
	case PreparedBlockKind::RelatedArticle:
		return skips.relatedArticle;
	case PreparedBlockKind::EmbedPost:
		return skips.embedPost;
	case PreparedBlockKind::Placeholder:
		return skips.placeholder;
	case PreparedBlockKind::Details:
		return skips.paragraph;
	case PreparedBlockKind::GroupedMedia:
		return skips.groupedMedia;
	}
	return 0;
}

int BlockSkip(
		const PreparedBlock &previous,
		const PreparedBlock &block,
		LayoutContext context,
		const style::Markdown &markdown) {
	if (context.tightList
		&& IsFlowKind(previous.kind)
		&& IsFlowKind(block.kind)) {
		return 0;
	}
	return BlockSkip(block, markdown);
}

const style::TextStyle &TextStyleFor(
		const PreparedBlock &block,
		const style::Markdown &markdown) {
	if (block.kind == PreparedBlockKind::CodeBlock) {
		return markdown.code;
	} else if (block.kind != PreparedBlockKind::Heading) {
		return markdown.body;
	}
	switch (std::clamp(block.headingLevel, 1, 6)) {
	case 1: return markdown.heading1;
	case 2: return markdown.heading2;
	case 3: return markdown.heading3;
	case 4: return markdown.heading4;
	case 5: return markdown.heading5;
	case 6: return markdown.heading6;
	}
	return markdown.heading6;
}

int BlockMaxRight(const std::vector<LaidOutBlock> &blocks) {
	auto result = 0;
	for (const auto &block : blocks) {
		result = std::max(result, block.outer.right() + 1);
		result = std::max(result, BlockMaxRight(block.children));
	}
	return result;
}

void RepopulateCodeBlockLeaf(
		LaidOutBlock &block,
		const style::Markdown &markdown,
		bool allowAsyncSyntaxHighlighting,
		CodeBlockSyntaxHighlightTracker *syntaxHighlightTracker) {
	auto marked = tr::marked(CodeBlockDisplayText(block.copyText));
	if (!marked.text.isEmpty()) {
		marked.entities.push_back(EntityInText(
			EntityType::Pre,
			0,
			marked.text.size(),
			block.codeLanguage));
	}
	block.syntaxHighlightProcessId = allowAsyncSyntaxHighlighting
		? (syntaxHighlightTracker
			? syntaxHighlightTracker->tryHighlightSyntax(
				marked.text,
				block.codeLanguage,
				marked)
			: Spellchecker::TryHighlightSyntax(marked))
		: 0;
	block.leaf.setMarkedText(
		markdown.code,
		std::move(marked),
		kIvMarkedTextOptions);
}

LaidOutBlock LayoutFlowBlock(
		const PreparedBlock &prepared,
		const std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = prepared.kind;
	block.anchorId = prepared.anchorId;
	block.headingLevel = prepared.headingLevel;
	block.supplementary = prepared.supplementary;
	block.textWidth = std::max(width, 1);
	if (IsAnchorOnlyBlock(prepared)) {
		block.textRect = QRect(left, top, block.textWidth, 0);
		block.outer = block.textRect;
		return block;
	}

	const auto &textStyle = TextStyleFor(prepared, markdown);
	SetTextLeaf(
		&block.leaf,
		textStyle,
		prepared.text,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		block.textWidth);
	BindLinks(&block.leaf, prepared.links);

	const auto height = std::max(
		block.leaf.countHeight(block.textWidth, true),
		TextLineHeight(textStyle));
	block.textRect = QRect(left, top, block.textWidth, height);
	block.outer = QRect(left, top, block.textWidth, height);
	block.firstLineBaseline = LeafFirstLineBaseline(
		block.leaf,
		block.textRect,
		textStyle);
	return block;
}

LaidOutBlock LayoutCodeBlock(
		const PreparedBlock &prepared,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		bool allowAsyncSyntaxHighlighting,
		CodeBlockSyntaxHighlightTracker *syntaxHighlightTracker) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::CodeBlock;
	block.copyText = prepared.text.text;
	block.codeLanguage = prepared.codeLanguage;
	block.textWidth = std::max(width, 1);
	block.leaf = Ui::Text::String(TextMinResizeWidth(block.textWidth));
	RepopulateCodeBlockLeaf(
		block,
		markdown,
		allowAsyncSyntaxHighlighting,
		syntaxHighlightTracker);
	const auto height = std::max(
		block.leaf.countHeight(block.textWidth, true),
		TextLineHeight(markdown.code));
	block.textRect = QRect(left, top, block.textWidth, height);
	block.outer = block.textRect;
	block.firstLineBaseline = LeafFirstLineBaseline(
		block.leaf,
		block.textRect,
		markdown.code);
	return block;
}

LaidOutBlock LayoutRuleBlock(
		const style::Markdown &markdown,
		int left,
		int top,
		int width) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Rule;
	block.outer = QRect(
		left,
		top,
		std::max(width, 1),
		markdown.rule.height);
	block.textRect = block.outer;
	return block;
}

LaidOutBlock LayoutDisplayMathBlock(
		const PreparedBlock &prepared,
		const std::vector<PreparedFormulaSlot> &formulas,
		const style::Markdown &markdown,
		int left,
		int top,
		int width) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::DisplayMath;
	block.formulaIndex = prepared.formulaIndex;
	block.copyText = prepared.formulaTex;

	const auto &padding = markdown.displayMath.padding;
	const auto contentLeft = left + padding.left();
	const auto contentTop = top + padding.top();
	const auto contentWidth = std::max(
		width - padding.left() - padding.right(),
		1);
	const auto formula = PreparedFormulaFor(formulas, prepared.formulaIndex);

	auto formulaWidth = 0;
	auto formulaHeight = 0;
	if (formula && formula->measured.success) {
		formulaWidth = std::max(formula->measured.logicalSize.width(), 1);
		formulaHeight = std::max(formula->measured.logicalSize.height(), 1);
	} else {
		const auto &fallbackPadding = markdown.displayMath.fallbackPadding;
		const auto fallbackPaddingWidth = fallbackPadding.left()
			+ fallbackPadding.right();
		const auto fallbackText = formula
			? formula->measured.fallbackText
			: prepared.formulaTex.trimmed();
		block.textWidth = std::max(contentWidth - fallbackPaddingWidth, 1);
		block.fallbackLeaf = Ui::Text::String(
			TextMinResizeWidth(block.textWidth));
		block.fallbackLeaf.setMarkedText(
			markdown.displayMath.fallbackStyle,
			TextWithEntities::Simple(fallbackText),
			kIvMarkedTextOptions);
		block.textWidth = std::min(
			block.textWidth,
			std::max(block.fallbackLeaf.maxWidth(), 1));
		auto textHeight = std::max(
			block.fallbackLeaf.countHeight(block.textWidth, true),
			TextLineHeight(markdown.displayMath.fallbackStyle));
		formulaWidth = std::min(
			block.textWidth + fallbackPaddingWidth,
			contentWidth);
		block.textWidth = std::max(formulaWidth - fallbackPaddingWidth, 1);
		textHeight = std::max(
			block.fallbackLeaf.countHeight(block.textWidth, true),
			TextLineHeight(markdown.displayMath.fallbackStyle));
		formulaHeight = fallbackPadding.top()
			+ textHeight
			+ fallbackPadding.bottom();
		block.textRect.setSize(QSize(block.textWidth, textHeight));
	}

	const auto centered = (markdown.displayMath.align == ::style::al_center)
		&& (formulaWidth <= contentWidth);
	block.formulaAlign = centered ? ::style::al_center : ::style::al_left;

	block.contentRect = QRect(
		contentLeft,
		contentTop,
		contentWidth,
		formulaHeight);
	block.formulaRect = QRect(
		centered
			? (contentLeft + ((contentWidth - formulaWidth) / 2))
			: contentLeft,
		contentTop,
		formulaWidth,
		formulaHeight);
	block.visibleFormulaRect = block.formulaRect.intersected(block.contentRect);
	block.outer = QRect(
		left,
		top,
		std::max(width, 1),
		padding.top() + formulaHeight + padding.bottom());
	block.overflowed = formula
		&& formula->measured.success
		&& (block.formulaRect.width() > block.visibleFormulaRect.width());

	if (!(formula && formula->measured.success)) {
		const auto &fallbackPadding = markdown.displayMath.fallbackPadding;
		block.textRect.moveTo(
			block.formulaRect.x() + fallbackPadding.left(),
			block.formulaRect.y() + fallbackPadding.top());
		block.firstLineBaseline = LeafFirstLineBaseline(
			block.fallbackLeaf,
			block.textRect,
			markdown.displayMath.fallbackStyle);
	}
	return block;
}

LaidOutBlock LayoutTableBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Table;
	block.anchorId = prepared.anchorId;
	block.tableBordered = prepared.tableBordered;
	block.tableStriped = prepared.tableStriped;
	block.supplementary = prepared.supplementary;

	auto tableTop = top;
	if (!prepared.text.text.isEmpty()) {
		LayoutMediaCaptionText(
			&block,
			prepared.text,
			prepared.links,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown.body,
			left,
			top,
			width);
		block.firstLineBaseline = LeafFirstLineBaseline(
			block.leaf,
			block.textRect,
			markdown.body);
		tableTop = block.textRect.y()
			+ block.textRect.height()
			+ markdown.table.captionSkip;
	}

	const auto columnCount = prepared.tableColumnCount;
	if (!columnCount || prepared.tableRows.empty()) {
		if (!block.textRect.isEmpty()) {
			block.contentRect = block.textRect;
			block.outer = block.textRect;
		} else {
			block.outer = QRect(left, top, std::max(width, 1), 0);
		}
		return block;
	}

	auto rows = std::vector<TableRowLayoutData>();
	rows.reserve(prepared.tableRows.size());
	for (const auto &preparedRow : prepared.tableRows) {
		auto row = TableRowLayoutData();
		row.header = preparedRow.header;
		row.cells.reserve(preparedRow.cells.size());
		for (const auto &preparedCell : preparedRow.cells) {
			row.cells.push_back(InitializeTableCellLayout(
				preparedCell,
				formulas,
				inlineFormulaObjects,
				mediaRuntime,
				markdown));
		}
		rows.push_back(std::move(row));
	}

	block.tableColumnWidths = ComputeTableColumnWidths(
		rows,
		columnCount,
		width,
		markdown,
		block.tableBordered,
		&block.overflowed);

	const auto &padding = markdown.table.cellPadding;
	const auto border = TableBorder(block.tableBordered, markdown);
	auto tableWidth = border;
	for (const auto columnWidth : block.tableColumnWidths) {
		tableWidth += columnWidth + border;
	}
	auto columnLefts = std::vector<int>(columnCount, left + border);
	auto x = left + border;
	for (auto column = 0; column != columnCount; ++column) {
		columnLefts[column] = x;
		x += block.tableColumnWidths[column] + border;
	}

	auto rowHeights = std::vector<int>(rows.size(), 0);
	auto rowSpans = std::vector<TableSpannedCellLayout>();
	for (auto rowIndex = 0, rowCount = int(rows.size()); rowIndex != rowCount; ++rowIndex) {
		for (auto &cellData : rows[rowIndex].cells) {
			const auto spanWidth = TableSpanWidth(
				block.tableColumnWidths,
				cellData.cell.column,
				cellData.cell.colspan,
				border);
			cellData.cell.textWidth = std::max(
				spanWidth - padding.left() - padding.right(),
				1);
			const auto &textStyle = TableCellTextStyle(cellData.cell, markdown);
			cellData.textHeight = (cellData.cell.textWidth >= cellData.preferredWidth)
				? cellData.preferredHeight
				: std::max(
					cellData.cell.leaf.countHeight(
						cellData.cell.textWidth,
						true),
					TextLineHeight(textStyle));
			const auto outerHeight = cellData.textHeight
				+ padding.top()
				+ padding.bottom();
			if (cellData.cell.rowspan == 1) {
				rowHeights[rowIndex] = std::max(rowHeights[rowIndex], outerHeight);
			} else {
				rowSpans.push_back({ rowIndex, &cellData });
			}
		}
	}
	std::sort(
		rowSpans.begin(),
		rowSpans.end(),
		[](const TableSpannedCellLayout &a, const TableSpannedCellLayout &b) {
			const auto aSpan = a.cell ? a.cell->cell.rowspan : 0;
			const auto bSpan = b.cell ? b.cell->cell.rowspan : 0;
			return (aSpan < bSpan)
				|| ((aSpan == bSpan) && (a.row < b.row))
				|| ((aSpan == bSpan)
					&& (a.row == b.row)
					&& a.cell
					&& b.cell
					&& (a.cell->cell.column < b.cell->cell.column));
		});
	for (const auto &spanned : rowSpans) {
		if (!spanned.cell) {
			continue;
		}
		const auto outerHeight = spanned.cell->textHeight
			+ padding.top()
			+ padding.bottom();
		const auto currentHeight = TableSpanHeight(
			rowHeights,
			spanned.row,
			spanned.cell->cell.rowspan,
			border);
		DistributeSpanDelta(
			&rowHeights,
			spanned.row,
			spanned.row + spanned.cell->cell.rowspan,
			std::max(outerHeight - currentHeight, 0));
	}

	auto y = tableTop + border;
	block.tableRows.reserve(rows.size());
	for (auto rowIndex = 0, rowCount = int(rows.size()); rowIndex != rowCount; ++rowIndex) {
		auto &rowData = rows[rowIndex];
		const auto rowHeight = rowHeights[rowIndex];
		auto row = LaidOutTableRow();
		row.header = rowData.header;
		row.outer = QRect(
			left + border,
			y,
			std::max(tableWidth - (2 * border), 0),
			rowHeight);

		row.cells.reserve(rowData.cells.size());
		for (auto &cellData : rowData.cells) {
			auto cell = std::move(cellData.cell);
			const auto column = std::clamp(cell.column, 0, columnCount - 1);
			const auto spanWidth = TableSpanWidth(
				block.tableColumnWidths,
				cell.column,
				cell.colspan,
				border);
			const auto spanHeight = TableSpanHeight(
				rowHeights,
				rowIndex,
				cell.rowspan,
				border);
			const auto cellTop = y;
			const auto contentHeight = std::max(
				spanHeight - padding.top() - padding.bottom(),
				0);
			auto textTop = cellTop + padding.top();
			switch (cell.verticalAlignment) {
			case PreparedTableCellVerticalAlignment::Middle:
				textTop += std::max((contentHeight - cellData.textHeight) / 2, 0);
				break;
			case PreparedTableCellVerticalAlignment::Bottom:
				textTop = cellTop
					+ spanHeight
					- padding.bottom()
					- cellData.textHeight;
				break;
			case PreparedTableCellVerticalAlignment::Top:
				break;
			}
			cell.outer = QRect(
				columnLefts[column],
				cellTop,
				spanWidth,
				spanHeight);
			cell.textRect = QRect(
				columnLefts[column] + padding.left(),
				textTop,
				cell.textWidth,
				cellData.textHeight);
			row.cells.push_back(std::move(cell));
		}
		block.tableRows.push_back(std::move(row));
		y += rowHeight + border;
	}

	const auto tableHeight = std::max(y - tableTop, border);
	block.tableRect = QRect(left, tableTop, tableWidth, tableHeight);
	block.visibleTableRect = QRect(
		left,
		tableTop,
		std::min(tableWidth, std::max(width, 1)),
		tableHeight);
	block.contentRect = block.textRect.isEmpty()
		? block.visibleTableRect
		: block.visibleTableRect.isEmpty()
		? block.textRect
		: block.textRect.united(block.visibleTableRect);
	block.outer = block.contentRect;
	if (!block.textRect.isEmpty()) {
		return block;
	}
	for (const auto &row : block.tableRows) {
		for (const auto &cell : row.cells) {
			if (cell.leaf.isEmpty()) {
				continue;
			}
			block.firstLineBaseline = LeafFirstLineBaseline(
				cell.leaf,
				cell.textRect,
				TableCellTextStyle(cell, markdown));
			return block;
		}
	}
	return block;
}

LaidOutBlock LayoutPlaceholderBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Placeholder;
	block.anchorId = prepared.anchorId;
	block.copyText = prepared.placeholder.copyText;
	block.labelText = prepared.placeholder.label;
	block.placeholderId = prepared.placeholder.id;
	if (block.placeholderId && context.placeholderRuntimeFactory) {
		block.placeholderRuntime = context.placeholderRuntimeFactory(
			block.placeholderId);
	}
	block.supplementary = prepared.supplementary;

	const auto &style = markdown.placeholder;
	const auto blockWidth = std::max(width, 1);
	const auto contentLeft = left + style.padding.left();
	const auto contentWidth = std::max(
		blockWidth - style.padding.left() - style.padding.right(),
		1);
	block.labelWidth = contentWidth;
	block.labelLeaf = Ui::Text::String(TextMinResizeWidth(contentWidth));
	block.labelLeaf.setMarkedText(
		style.labelStyle,
		TextWithEntities::Simple(block.labelText),
		kIvMarkedTextOptions);
	const auto labelHeight = std::max(
		block.labelLeaf.countHeight(contentWidth, true),
		TextLineHeight(style.labelStyle));
	const auto mediaHeight = std::max(
		style.minHeight,
		labelHeight + style.padding.top() + style.padding.bottom());
	block.mediaRect = QRect(left, top, blockWidth, mediaHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.placeholderRuntime
		&& block.placeholderRuntime->ripple
		&& block.placeholderRuntime->rippleSize != block.mediaRect.size()) {
		block.placeholderRuntime->ripple = nullptr;
		block.placeholderRuntime->rippleSize = QSize();
	}
	block.labelRect = QRect(
		contentLeft,
		top + std::max((mediaHeight - labelHeight) / 2, 0),
		contentWidth,
		labelHeight);
	block.firstLineBaseline = LeafFirstLineBaseline(
		block.labelLeaf,
		block.labelRect,
		style.labelStyle);
	if (prepared.placeholder.embed) {
		block.activation.kind = MediaActivationKind::Embed;
		block.activation.embed = *prepared.placeholder.embed;
		block.activation.placeholderId = block.placeholderId;
	}

	auto bottom = top + mediaHeight;
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		contentLeft,
		bottom,
			contentWidth,
			style.captionSkip,
			&bottom,
			context);

	block.contentRect = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, mediaHeight));
	block.outer = block.contentRect;
	return block;
}

LaidOutBlock LayoutRelatedArticleBlock(
		const PreparedBlock &prepared,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		const std::shared_ptr<MediaRuntime> &mediaRuntime) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::RelatedArticle;
	block.anchorId = prepared.anchorId;
	block.copyText = prepared.relatedArticle.copyText;
	block.labelText = prepared.relatedArticle.title;
	block.preparedLink = prepared.relatedArticle.link;
	block.preparedLinkHandler = CreatePreparedLinkHandler(
		prepared.relatedArticle.link);
	if (prepared.relatedArticle.photoId && mediaRuntime) {
		block.photoRuntime = mediaRuntime->resolvePhoto(
			prepared.relatedArticle.photoId);
	}

	const auto &card = markdown.relatedArticle;
	const auto blockWidth = std::max(width, 1);
	const auto hasThumbnail = (prepared.relatedArticle.photoId != 0);
	block.thumbnailPhotoId = prepared.relatedArticle.photoId;
	const auto thumbnailSize = hasThumbnail
		? std::max(card.thumbnailSize, 1)
		: 0;
	const auto thumbnailSkip = hasThumbnail ? card.thumbnailSkip : 0;
	const auto contentLeft = left + card.padding.left();
	const auto contentWidth = std::max(
		blockWidth
			- card.padding.left()
			- card.padding.right()
			- thumbnailSize
			- thumbnailSkip,
		1);

	auto titleHeight = 0;
	if (!prepared.relatedArticle.title.isEmpty()) {
		block.labelWidth = contentWidth;
		SetPlainTextLeaf(
			&block.labelLeaf,
			card.titleStyle,
			prepared.relatedArticle.title,
			block.labelWidth);
		titleHeight = LeafHeightWithLineLimit(
			block.labelLeaf,
			card.titleStyle,
			block.labelWidth,
			card.titleLines);
	}

	auto subtitleHeight = 0;
	if (!prepared.relatedArticle.description.isEmpty()) {
		block.subtitleWidth = contentWidth;
		SetPlainTextLeaf(
			&block.subtitleLeaf,
			card.subtitleStyle,
			prepared.relatedArticle.description,
			block.subtitleWidth);
		subtitleHeight = LeafHeightWithLineLimit(
			block.subtitleLeaf,
			card.subtitleStyle,
			block.subtitleWidth,
			card.subtitleLines);
	}

	auto footerHeight = 0;
	if (!prepared.relatedArticle.footer.isEmpty()) {
		block.actionWidth = contentWidth;
		SetPlainTextLeaf(
			&block.actionLeaf,
			card.footerStyle,
			prepared.relatedArticle.footer,
			block.actionWidth);
		footerHeight = LeafHeightWithLineLimit(
			block.actionLeaf,
			card.footerStyle,
			block.actionWidth,
			card.footerLines);
	}

	auto textHeight = 0;
	if (titleHeight) {
		textHeight += titleHeight;
	}
	if (subtitleHeight) {
		textHeight += subtitleHeight + (textHeight ? card.textSkip : 0);
	}
	if (footerHeight) {
		textHeight += footerHeight
			+ (textHeight ? card.footerSkip : 0);
	}
	const auto cardContentHeight = std::max(textHeight, thumbnailSize);
	const auto cardHeight = card.padding.top()
		+ cardContentHeight
		+ card.padding.bottom()
		+ card.separator;

	block.mediaRect = QRect(left, top, blockWidth, cardHeight);
	block.visibleMediaRect = block.mediaRect;
	if (hasThumbnail) {
		block.thumbnailRect = QRect(
			left + blockWidth - card.padding.right() - thumbnailSize,
			top + card.padding.top()
				+ std::max((cardContentHeight - thumbnailSize) / 2, 0),
			thumbnailSize,
			thumbnailSize);
	}

	auto textTop = top + card.padding.top()
		+ std::max((cardContentHeight - textHeight) / 2, 0);
	if (titleHeight) {
		block.labelRect = QRect(
			contentLeft,
			textTop,
			block.labelWidth,
			titleHeight);
		textTop += titleHeight;
	}
	if (subtitleHeight) {
		textTop += block.labelRect.isEmpty() ? 0 : card.textSkip;
		block.subtitleRect = QRect(
			contentLeft,
			textTop,
			block.subtitleWidth,
			subtitleHeight);
		textTop += subtitleHeight;
	}
	if (footerHeight) {
		textTop += (block.labelRect.isEmpty() && block.subtitleRect.isEmpty())
			? 0
			: card.footerSkip;
		block.actionRect = QRect(
			contentLeft,
			textTop,
			block.actionWidth,
			footerHeight);
	}

	const auto setBaseline = [&](const Ui::Text::String &leaf,
			QRect rect,
			const style::TextStyle &textStyle) {
		if (rect.isEmpty() || leaf.isEmpty()) {
			return false;
		}
		block.firstLineBaseline = LeafFirstLineBaseline(
			leaf,
			rect,
			textStyle);
		return true;
	};
	if (!setBaseline(block.labelLeaf, block.labelRect, card.titleStyle)
		&& !setBaseline(
			block.subtitleLeaf,
			block.subtitleRect,
			card.subtitleStyle)
		&& !setBaseline(block.actionLeaf, block.actionRect, card.footerStyle)) {
		block.firstLineBaseline = top + card.padding.top();
	}
	block.contentRect = block.mediaRect;
	block.outer = block.mediaRect;
	return block;
}

LaidOutBlock LayoutPhotoBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Photo;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &style = markdown.photo;
	const auto blockWidth = std::max(width, 1);
	const auto availableLeft = left + style.padding.left();
	const auto mediaTop = top + style.padding.top();
	const auto availableWidth = std::max(
		blockWidth - style.padding.left() - style.padding.right(),
		1);
	const auto mediaWidth = LimitedMediaWidth(
		availableWidth,
		prepared.photo.width);
	const auto mediaLeft = availableLeft
		+ std::max((availableWidth - mediaWidth) / 2, 0);
	const auto mediaHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(mediaWidth)
		: 0;
	block.mediaRect = QRect(mediaLeft, mediaTop, mediaWidth, mediaHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}

	auto bottom = block.mediaRect.y() + block.mediaRect.height()
		+ style.padding.bottom();
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		block.mediaRect.x(),
		bottom,
		std::max(block.mediaRect.width(), 1),
		style.captionSkip,
		&bottom,
		context);

	block.contentRect = QRect(
		block.mediaRect.x(),
		block.mediaRect.y(),
		block.mediaRect.width(),
		std::max(bottom - block.mediaRect.y(), block.mediaRect.height()));
	block.outer = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, block.mediaRect.bottom() - top + 1));
	return block;
}

LaidOutBlock LayoutVideoBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Video;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &style = markdown.photo;
	const auto blockWidth = std::max(width, 1);
	const auto availableLeft = left + style.padding.left();
	const auto mediaTop = top + style.padding.top();
	const auto availableWidth = std::max(
		blockWidth - style.padding.left() - style.padding.right(),
		1);
	const auto mediaWidth = LimitedMediaWidth(
		availableWidth,
		prepared.video.media.width);
	const auto mediaLeft = availableLeft
		+ std::max((availableWidth - mediaWidth) / 2, 0);
	const auto mediaHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(mediaWidth)
		: 0;
	block.mediaRect = QRect(mediaLeft, mediaTop, mediaWidth, mediaHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}

	auto bottom = block.mediaRect.y() + block.mediaRect.height()
		+ style.padding.bottom();
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		block.mediaRect.x(),
		bottom,
		std::max(block.mediaRect.width(), 1),
		style.captionSkip,
		&bottom,
		context);

	block.contentRect = QRect(
		block.mediaRect.x(),
		block.mediaRect.y(),
		block.mediaRect.width(),
		std::max(bottom - block.mediaRect.y(), block.mediaRect.height()));
	block.outer = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, block.mediaRect.bottom() - top + 1));
	return block;
}

LaidOutBlock LayoutAudioBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Audio;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &card = markdown.audio;
	const auto blockWidth = std::max(width, 1);
	const auto cardHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(blockWidth)
		: 0;
	block.mediaRect = QRect(left, top, blockWidth, cardHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}

	auto bottom = top + cardHeight;
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		left + card.padding.left(),
		bottom,
		std::max(
			blockWidth - card.padding.left() - card.padding.right(),
			1),
		markdown.audio.captionSkip,
		&bottom,
		context);
	block.contentRect = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, cardHeight));
	block.outer = block.contentRect;
	return block;
}

LaidOutBlock LayoutMapBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Map;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &style = markdown.photo;
	const auto blockWidth = std::max(width, 1);
	const auto mediaLeft = left + style.padding.left();
	const auto mediaTop = top + style.padding.top();
	const auto mediaWidth = std::max(
		blockWidth - style.padding.left() - style.padding.right(),
		1);
	const auto mediaHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(mediaWidth)
		: 0;
	block.mediaRect = QRect(mediaLeft, mediaTop, mediaWidth, mediaHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}

	auto bottom = block.mediaRect.y() + block.mediaRect.height()
		+ style.padding.bottom();
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		block.mediaRect.x(),
		bottom,
		std::max(block.mediaRect.width(), 1),
		style.captionSkip,
		&bottom,
		context);

	block.contentRect = QRect(
		block.mediaRect.x(),
		block.mediaRect.y(),
		block.mediaRect.width(),
		std::max(bottom - block.mediaRect.y(), block.mediaRect.height()));
	block.outer = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, block.mediaRect.bottom() - top + 1));
	return block;
}

LaidOutBlock LayoutChannelBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Channel;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &card = markdown.channel;
	const auto blockWidth = std::max(width, 1);
	const auto cardHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(blockWidth)
		: 0;
	block.mediaRect = QRect(left, top, blockWidth, cardHeight);
	block.visibleMediaRect = block.mediaRect;
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}

	auto bottom = top + cardHeight;
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		left + card.padding.left(),
		bottom,
		std::max(blockWidth - card.padding.left() - card.padding.right(), 1),
		markdown.audio.captionSkip,
		&bottom,
		context);
	block.contentRect = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, cardHeight));
	block.outer = block.contentRect;
	return block;
}

LaidOutBlock LayoutGroupedMediaBlock(
		const PreparedBlock &prepared,
		const std::vector<PreparedFormulaSlot> *formulas,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::GroupedMedia;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	if (context.mediaBlockFactory) {
		block.mediaBlock = context.mediaBlockFactory(prepared);
	}
	if (block.mediaBlock) {
		block.copyText = block.mediaBlock->selectionData().copyText;
	}

	const auto &style = markdown.groupedMedia;
	const auto blockWidth = std::max(width, 1);
	const auto mediaLeft = left + style.padding.left();
	const auto mediaTop = top + style.padding.top();
	const auto mediaWidth = std::max(
		blockWidth - style.padding.left() - style.padding.right(),
		1);
	const auto mediaHeight = block.mediaBlock
		? block.mediaBlock->resizeGetHeight(mediaWidth)
		: 0;
	block.mediaRect = QRect(mediaLeft, mediaTop, mediaWidth, mediaHeight);
	if (block.mediaBlock) {
		ApplyMediaBlockGeometry(&block, block.mediaRect);
	}
	block.visibleMediaRect = block.mediaRect;

	auto bottom = block.mediaRect.y() + block.mediaRect.height()
		+ style.padding.bottom();
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		mediaLeft,
		bottom,
		std::max(block.mediaRect.width(), 1),
		style.captionSkip,
		&bottom,
		context);

	block.contentRect = QRect(
		mediaLeft,
		mediaTop,
		block.mediaRect.width(),
		std::max(bottom - mediaTop, block.mediaRect.height()));
	block.outer = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, block.mediaRect.height()));
	return block;
}

} // namespace Iv::Markdown
