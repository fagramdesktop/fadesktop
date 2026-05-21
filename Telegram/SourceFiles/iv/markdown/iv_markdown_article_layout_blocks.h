/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_article.h"
#include "spellcheck/spellcheck_highlight_syntax.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace Ui {
class DynamicImage;
} // namespace Ui

namespace style {
struct Markdown;
struct QuoteStyle;
struct TextStyle;
} // namespace style

namespace Iv::Markdown {

class InlineFormulaObjectCache;
class CodeBlockSyntaxHighlightTracker {
public:
	virtual ~CodeBlockSyntaxHighlightTracker() = default;

	[[nodiscard]] virtual Spellchecker::HighlightProcessId tryHighlightSyntax(
		const QString &displayText,
		const QString &language,
		TextWithEntities &marked) = 0;
};

struct LaidOutTableCell {
	Ui::Text::String leaf;
	QRect outer;
	QRect textRect;
	int textWidth = 0;
	bool header = false;
	PreparedTableCellVerticalAlignment verticalAlignment
		= PreparedTableCellVerticalAlignment::Top;
	style::align align = style::al_left;
	int column = 0;
	int colspan = 1;
	int rowspan = 1;
	int segmentIndex = -1;
	int tableSegmentIndex = -1;
};

struct LaidOutTableRow {
	std::vector<LaidOutTableCell> cells;
	QRect outer;
	bool header = false;
};

struct LaidOutBlock {
	PreparedBlockKind kind = PreparedBlockKind::Paragraph;
	Ui::Text::String leaf;
	Ui::Text::String labelLeaf;
	Ui::Text::String subtitleLeaf;
	Ui::Text::String actionLeaf;
	Ui::Text::String marker;
	Ui::Text::String fallbackLeaf;
	QString copyText;
	QString labelText;
	QString codeLanguage;
	std::optional<PreparedLink> preparedLink;
	ClickHandlerPtr preparedLinkHandler;
	PreparedPlaceholderBlockId placeholderId;
	Spellchecker::HighlightProcessId syntaxHighlightProcessId = 0;
	std::vector<LaidOutBlock> children;
	std::vector<LaidOutTableRow> tableRows;
	std::vector<int> tableColumnWidths;
	QRect outer;
	QRect headerRect;
	QRect bodyRect;
	QRect iconRect;
	QRect textRect;
	QRect labelRect;
	QRect subtitleRect;
	QRect actionRect;
	QRect markerRect;
	QRect contentRect;
	QRect formulaRect;
	QRect tableRect;
	QRect mediaRect;
	QRect thumbnailRect;
	QRect visibleFormulaRect;
	QRect visibleTableRect;
	QRect visibleMediaRect;
	QPoint markerCenter;
	QString anchorId;
	int textWidth = 0;
	int labelWidth = 0;
	int subtitleWidth = 0;
	int actionWidth = 0;
	int markerWidth = 0;
	int firstLineBaseline = -1;
	int headingLevel = 0;
	ListKind listKind = ListKind::Bullet;
	ListDelimiter listDelimiter = ListDelimiter::None;
	TaskState taskState = TaskState::None;
	int formulaIndex = -1;
	int orderedNumber = 0;
	style::align formulaAlign = style::al_left;
	bool collapsed = false;
	bool overflowed = false;
	bool tableBordered = true;
	bool tableStriped = false;
	bool supplementary = false;
	int segmentIndex = -1;
	int secondarySegmentIndex = -1;
	int tertiarySegmentIndex = -1;
	std::shared_ptr<MediaBlock> mediaBlock;
	std::shared_ptr<PlaceholderBlockRuntime> placeholderRuntime;
	std::shared_ptr<PhotoRuntime> photoRuntime;
	MediaActivation activation;
	uint64 thumbnailPhotoId = 0;
	mutable std::shared_ptr<Ui::DynamicImage> thumbnailImage;
	mutable std::shared_ptr<Ui::DynamicImage> previousThumbnailImage;
	mutable std::shared_ptr<Ui::DynamicImage> subscribedThumbnailImage;
	mutable QSize thumbnailRequestSize;
	mutable QImage colorizedFormulaImage;
	mutable QColor colorizedFormulaColor;
	mutable QSize colorizedFormulaSize;
};

struct LayoutContext {
	int listDepth = 0;
	int quoteDepth = 0;
	int articleLeft = 0;
	int articleWidth = 0;
	bool tightList = false;
	bool useArticleBands = false;
	bool allowAsyncSyntaxHighlighting = true;
	CodeBlockSyntaxHighlightTracker *syntaxHighlightTracker = nullptr;
	std::function<std::shared_ptr<MediaBlock>(const PreparedBlock&)> mediaBlockFactory;
	std::function<std::shared_ptr<PlaceholderBlockRuntime>(
		PreparedPlaceholderBlockId)> placeholderRuntimeFactory;
};

struct TableCellLayoutData {
	LaidOutTableCell cell;
	int preferredWidth = 0;
	int preferredHeight = 0;
	int textHeight = 0;
};

struct TableRowLayoutData {
	std::vector<TableCellLayoutData> cells;
	bool header = false;
};

[[nodiscard]] bool IsAnchorOnlyBlock(const PreparedBlock &block);
[[nodiscard]] bool IsFlowKind(PreparedBlockKind kind);
[[nodiscard]] QString ListMarkerText(const PreparedBlock &block);
[[nodiscard]] int TextLineHeight(const style::TextStyle &style);
[[nodiscard]] QPoint BulletMarkerCenter(
	int left,
	int baseline,
	const style::Markdown &markdown);
[[nodiscard]] QMargins BlockquotePadding(const style::QuoteStyle &style);
[[nodiscard]] Ui::Text::GeometryDescriptor TextGeometry(int width);
[[nodiscard]] int TextMinResizeWidth(int width);
[[nodiscard]] int TableCellTextMinResizeWidth(
	const style::TextStyle &textStyle,
	const style::Markdown &markdown);
[[nodiscard]] QString CodeBlockDisplayText(const QString &text);
[[nodiscard]] int BlockSkip(
	const PreparedBlock &block,
	const style::Markdown &markdown);
[[nodiscard]] int BlockSkip(
	const PreparedBlock &previous,
	const PreparedBlock &block,
	LayoutContext context,
	const style::Markdown &markdown);
[[nodiscard]] const style::TextStyle &TextStyleFor(
	const PreparedBlock &block,
	const style::Markdown &markdown);
[[nodiscard]] int BlockMaxRight(const std::vector<LaidOutBlock> &blocks);
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
	LayoutContext context = {});
void RepopulateCodeBlockLeaf(
	LaidOutBlock &block,
	const style::Markdown &markdown,
	bool allowAsyncSyntaxHighlighting,
	CodeBlockSyntaxHighlightTracker *syntaxHighlightTracker = nullptr);

[[nodiscard]] LaidOutBlock LayoutFlowBlock(
	const PreparedBlock &prepared,
	const std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutCodeBlock(
	const PreparedBlock &prepared,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	bool allowAsyncSyntaxHighlighting,
	CodeBlockSyntaxHighlightTracker *syntaxHighlightTracker = nullptr);
[[nodiscard]] LaidOutBlock LayoutRuleBlock(
	const style::Markdown &markdown,
	int left,
	int top,
	int width);
[[nodiscard]] LaidOutBlock LayoutDisplayMathBlock(
	const PreparedBlock &prepared,
	const std::vector<PreparedFormulaSlot> &formulas,
	const style::Markdown &markdown,
	int left,
	int top,
	int width);
[[nodiscard]] LaidOutBlock LayoutTableBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width);
[[nodiscard]] LaidOutBlock LayoutPlaceholderBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutRelatedArticleBlock(
	const PreparedBlock &prepared,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] LaidOutBlock LayoutPhotoBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutVideoBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutAudioBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutMapBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutChannelBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});
[[nodiscard]] LaidOutBlock LayoutGroupedMediaBlock(
	const PreparedBlock &prepared,
	const std::vector<PreparedFormulaSlot> *formulas,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context = {});

} // namespace Iv::Markdown
