/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_article_layout_structure.h"
#include "iv/markdown/iv_markdown_article_paint.h"
#include "iv/markdown/iv_markdown_article_selection.h"
#include "iv/markdown/iv_markdown_article_text.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "ui/style/style_core_scale.h"
#include "ui/basic_click_handlers.h"
#include "ui/dynamic_image.h"

#include "styles/style_iv.h"
#include "styles/style_widgets.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Iv::Markdown {
namespace {

struct PendingHighlightKey {
	QString text;
	QString language;
};

[[nodiscard]] bool operator==(
		const PendingHighlightKey &a,
		const PendingHighlightKey &b) {
	return (a.text == b.text) && (a.language == b.language);
}

struct PendingHighlightKeyHasher {
	[[nodiscard]] size_t operator()(
			const PendingHighlightKey &key) const noexcept;
};

size_t PendingHighlightKeyHasher::operator()(
		const PendingHighlightKey &key) const noexcept {
	auto result = size_t(qHash(key.text));
	result = (result * 1315423911U) ^ size_t(qHash(key.language));
	return result;
}

struct PendingHighlightEntry {
	PendingHighlightKey key;
	std::vector<LaidOutBlock*> blocks;
};

struct RelatedArticleThumbnailState {
	std::shared_ptr<Ui::DynamicImage> thumbnailImage;
	std::shared_ptr<Ui::DynamicImage> previousThumbnailImage;
	std::shared_ptr<Ui::DynamicImage> subscribedThumbnailImage;
	QSize thumbnailRequestSize;
};

void StoreRelatedArticleThumbnailState(
		const LaidOutBlock &block,
		std::unordered_map<uint64, RelatedArticleThumbnailState> *states) {
	if (block.thumbnailPhotoId) {
		(*states)[block.thumbnailPhotoId] = {
			.thumbnailImage = block.thumbnailImage,
			.previousThumbnailImage = block.previousThumbnailImage,
			.subscribedThumbnailImage = block.subscribedThumbnailImage,
			.thumbnailRequestSize = block.thumbnailRequestSize,
		};
	}
	for (const auto &child : block.children) {
		StoreRelatedArticleThumbnailState(child, states);
	}
}

void StoreRelatedArticleThumbnailStates(
		const std::vector<LaidOutBlock> &blocks,
		std::unordered_map<uint64, RelatedArticleThumbnailState> *states) {
	for (const auto &block : blocks) {
		StoreRelatedArticleThumbnailState(block, states);
	}
}

void RestoreRelatedArticleThumbnailState(
		LaidOutBlock *block,
		const std::unordered_map<uint64, RelatedArticleThumbnailState> &states) {
	if (block->thumbnailPhotoId) {
		if (const auto i = states.find(block->thumbnailPhotoId);
			i != end(states)) {
			block->thumbnailImage = i->second.thumbnailImage;
			block->previousThumbnailImage = i->second.previousThumbnailImage;
			block->subscribedThumbnailImage = i->second.subscribedThumbnailImage;
			block->thumbnailRequestSize = i->second.thumbnailRequestSize;
		}
	}
	for (auto &child : block->children) {
		RestoreRelatedArticleThumbnailState(&child, states);
	}
}

void RestoreRelatedArticleThumbnailStates(
		std::vector<LaidOutBlock> *blocks,
		const std::unordered_map<uint64, RelatedArticleThumbnailState> &states) {
	for (auto &block : *blocks) {
		RestoreRelatedArticleThumbnailState(&block, states);
	}
}

void CollectPlaceholderIds(
		const std::vector<LaidOutBlock> &blocks,
		std::unordered_set<uint64> *result) {
	if (!result) {
		return;
	}
	for (const auto &block : blocks) {
		if (block.placeholderId) {
			result->emplace(block.placeholderId.value);
		}
		CollectPlaceholderIds(block.children, result);
	}
}

[[nodiscard]] LaidOutBlock *FindPlaceholderBlock(
		std::vector<LaidOutBlock> *blocks,
		PreparedPlaceholderBlockId id) {
	if (!blocks || !id) {
		return nullptr;
	}
	for (auto &block : *blocks) {
		if (block.placeholderId.value == id.value) {
			return &block;
		}
		if (const auto child = FindPlaceholderBlock(&block.children, id)) {
			return child;
		}
	}
	return nullptr;
}

[[nodiscard]] const LaidOutBlock *FindPlaceholderBlock(
		const std::vector<LaidOutBlock> &blocks,
		PreparedPlaceholderBlockId id) {
	if (!id) {
		return nullptr;
	}
	for (const auto &block : blocks) {
		if (block.placeholderId.value == id.value) {
			return &block;
		}
		if (const auto child = FindPlaceholderBlock(block.children, id)) {
			return child;
		}
	}
	return nullptr;
}

[[nodiscard]] PendingHighlightKey PendingHighlightKeyForBlock(
		const LaidOutBlock &block) {
	return {
		.text = CodeBlockDisplayText(block.copyText),
		.language = block.codeLanguage,
	};
}

[[nodiscard]] Ui::Text::StateResult TextStateAtLeaf(
		const Ui::Text::String &leaf,
		QRect rect,
		int width,
		QPoint point,
		Ui::Text::StateRequest::Flags flags,
		style::align align = style::al_left,
		bool clampToRect = false) {
	if (rect.isEmpty()) {
		return {};
	}
	if (!rect.contains(point)) {
		if (!clampToRect) {
			return {};
		}
		point.setX(std::clamp(point.x(), rect.left(), rect.right()));
		point.setY(std::clamp(point.y(), rect.top(), rect.bottom()));
	}
	auto request = Ui::Text::StateRequest();
	request.align = align;
	request.flags = flags | Ui::Text::StateRequest::Flag::BreakEverywhere;
	const auto availableWidth = std::max(width, 1);
	return leaf.getState(
		point - rect.topLeft(),
		TextGeometry(availableWidth),
		request);
}

[[nodiscard]] SegmentSpan FullSegmentSpan(
		const std::vector<SelectableSegment> &segments) {
	return { 0, int(segments.size()) };
}

void RebuildVisibleSegmentLookup(
		const std::vector<SelectableSegment> &segments,
		std::vector<int> *tops,
		std::vector<int> *bottoms) {
	if (!tops || !bottoms) {
		return;
	}
	tops->clear();
	bottoms->clear();
	tops->reserve(segments.size());
	bottoms->reserve(segments.size());
	auto runningBottom = std::numeric_limits<int>::lowest();
	for (const auto &segment : segments) {
		tops->push_back(segment.outerRect.top());
		runningBottom = std::max(runningBottom, segment.outerRect.bottom());
		bottoms->push_back(runningBottom);
	}
}

[[nodiscard]] SegmentSpan LookupVisibleSegmentSpan(
		const std::vector<int> &tops,
		const std::vector<int> &bottoms,
		LogicalVisibleRange range) {
	if (tops.empty() || bottoms.empty() || (range.bottom <= range.top)) {
		return {};
	}
	const auto from = int(std::lower_bound(
		bottoms.begin(),
		bottoms.end(),
		range.top) - bottoms.begin());
	if (from >= int(tops.size())) {
		return {};
	}
	const auto till = int(std::upper_bound(
		tops.begin() + from,
		tops.end(),
		range.bottom - 1) - tops.begin());
	return (from < till) ? SegmentSpan{ from, till } : SegmentSpan();
}

[[nodiscard]] std::optional<PreparedLink> PreparedLinkForMediaActivation(
		const MediaActivation &activation) {
	if (activation.kind != MediaActivationKind::ExternalUrl
		|| activation.url.isEmpty()) {
		return std::nullopt;
	}
	if (const auto prepared = ClassifiedLink(0, activation.url, nullptr);
		prepared.kind == PreparedLinkKind::External) {
		return prepared;
	}
	return PreparedLink{
		.kind = PreparedLinkKind::External,
		.target = activation.url,
		.copyText = UrlClickHandler::EncodeForOpening(activation.url),
		.entityType = EntityType::Url,
		.shown = EntityLinkShown::Full,
	};
}

[[nodiscard]] std::optional<PreparedLink> PreparedLinkForDetailsBlock(
		const SelectableSegment &segment) {
	if (!segment.block
		|| segment.block->kind != PreparedBlockKind::Details
		|| segment.block->anchorId.isEmpty()) {
		return std::nullopt;
	}
	return PreparedLink{
		.kind = PreparedLinkKind::ToggleDetails,
		.target = segment.block->anchorId,
	};
}

[[nodiscard]] MarkdownArticleHitTestResult HitSegmentBoundary(
		const SelectableSegment &segment,
		int offset) {
	auto result = MarkdownArticleHitTestResult();
	result.segmentIndex = segment.index;
	result.forcedOffset = std::clamp(offset, 0, SegmentLength(segment));
	result.state.uponSymbol = true;
	result.state.afterSymbol = (result.forcedOffset > 0);
	return result;
}

[[nodiscard]] MarkdownArticleHitTestResult HitTextSegment(
		const SelectableSegment &segment,
		QPoint point,
		Ui::Text::StateRequest::Flags flags) {
	if (!segment.isTextLeaf() || !segment.outerRect.contains(point)) {
		return {};
	}
	const auto insideText = segment.textRect.contains(point);
	if (!insideText
		&& !(flags & Ui::Text::StateRequest::Flag::LookupSymbol)) {
		return {};
	}
	auto result = MarkdownArticleHitTestResult();
	result.segmentIndex = segment.index;
	result.state = TextStateAtLeaf(
		*segment.leaf,
		segment.textRect,
		segment.textWidth,
		point,
		flags,
		segment.align,
		!insideText);
	if (!insideText) {
		result.state.link = nullptr;
	}
	result.preparedLink = ExtractPreparedLink(result.state.link);
	if (!result.preparedLink
		&& (flags & Ui::Text::StateRequest::Flag::LookupLink)) {
		if (const auto prepared = PreparedLinkForDetailsBlock(segment)) {
			result.preparedLink = prepared;
			if (!insideText) {
				result.state.link = CreatePreparedLinkHandler(*prepared);
			}
		}
	}
	result.direct = true;
	return result;
}

[[nodiscard]] MarkdownArticleHitTestResult HitBlockSegment(
		const SelectableSegment &segment,
		QPoint point,
		Ui::Text::StateRequest::Flags flags) {
	if (segment.isTextLeaf()
		|| !(flags & Ui::Text::StateRequest::Flag::LookupSymbol)
		|| !segment.outerRect.contains(point)) {
		return {};
	}
	const auto after = (point.y() > segment.outerRect.center().y())
		|| ((point.y() == segment.outerRect.center().y())
			&& (point.x() >= segment.outerRect.center().x()));
	auto result = HitSegmentBoundary(
		segment,
		after ? SegmentLength(segment) : 0);
	const auto applyActivation = [&](const MediaActivation &activation) {
		result.mediaActivation = activation;
		if (const auto prepared = PreparedLinkForMediaActivation(activation)) {
			result.preparedLink = prepared;
			result.state.link = CreatePreparedLinkHandler(*prepared);
		}
	};
	if (segment.block) {
		if (segment.block->kind == PreparedBlockKind::RelatedArticle
			&& segment.block->preparedLink) {
			result.preparedLink = segment.block->preparedLink;
			result.state.link = segment.block->preparedLinkHandler;
			result.mediaActivation = {};
		} else if (segment.block->mediaBlock) {
			if (const auto link = segment.block->mediaBlock->linkAt(point)) {
				result.state.link = link;
				result.preparedLink = std::nullopt;
				result.mediaActivation = {};
			} else {
				applyActivation(segment.block->mediaBlock->activationAt(point));
			}
		} else {
			applyActivation(segment.block->activation);
			if (result.mediaActivation.kind == MediaActivationKind::Embed
				&& segment.block->placeholderRuntime) {
				result.state.link = segment.block->placeholderRuntime->clickHandler;
				result.placeholderLocalPoint = point
					- segment.block->mediaRect.topLeft();
			}
		}
	}
	result.direct = true;
	return result;
}

[[nodiscard]] MarkdownArticleHitTestResult HitSegmentFallback(
		const std::vector<SelectableSegment> &segments,
		SegmentSpan span,
		QPoint point) {
	if (segments.empty() || span.empty()) {
		return {};
	}
	for (auto i = span.from; i != span.till; ++i) {
		const auto &segment = segments[i];
		const auto &rect = segment.outerRect;
		if (point.y() < rect.top()) {
			return HitSegmentBoundary(segment, 0);
		}
		if (point.y() <= rect.bottom()) {
			if (point.x() < rect.left()) {
				return HitSegmentBoundary(segment, 0);
			} else if (point.x() > rect.right()) {
				return HitSegmentBoundary(
					segment,
					SegmentLength(segment));
			}
		}
	}
	return HitSegmentBoundary(
		segments[span.till - 1],
		SegmentLength(segments[span.till - 1]));
}

[[nodiscard]] bool ToggleDetailsBlock(
		std::vector<PreparedBlock> *blocks,
		const QString &anchorId) {
	if (!blocks) {
		return false;
	}
	for (auto &block : *blocks) {
		if (block.kind == PreparedBlockKind::Details
			&& block.anchorId == anchorId) {
			block.collapsed = !block.collapsed;
			return true;
		}
		if (ToggleDetailsBlock(&block.children, anchorId)) {
			return true;
		}
	}
	return false;
}

void ClearColorizedFormulaImages(std::vector<LaidOutBlock> *blocks) {
	if (!blocks) {
		return;
	}
	for (auto &block : *blocks) {
		block.colorizedFormulaImage = QImage();
		block.colorizedFormulaColor = QColor();
		block.colorizedFormulaSize = QSize();
		ClearColorizedFormulaImages(&block.children);
	}
}

} // namespace

PlaceholderBlockRuntime::PlaceholderBlockRuntime(Fn<void()> repaint)
: clickHandler(std::make_shared<LambdaClickHandler>([] {
}))
, loadingAnimation(
	[repaint = std::move(repaint)] {
		if (repaint) {
			repaint();
		}
	},
	st::defaultInfiniteRadialAnimation) {
}

class MarkdownArticle::Impl final : public CodeBlockSyntaxHighlightTracker {
public:
	explicit Impl(std::shared_ptr<MathRenderer> renderer);

	void setRenderer(std::shared_ptr<MathRenderer> renderer);

	void setMediaBlockHost(MediaBlockHost *host);

	void setTextRepaintCallbacks(
		Fn<void()> repaint,
		Fn<void(QRect)> repaintRect);

	void setContent(MarkdownArticleContent content);

	[[nodiscard]] int maxWidth();

	[[nodiscard]] int resizeGetHeight(int width);

	void setVisibleTopBottom(int visibleTop, int visibleBottom);

	void paint(
		Painter &p,
		QRect clip,
		MarkdownArticlePaintCaches caches,
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints);

	[[nodiscard]] MarkdownArticleHitTestResult hitTest(
		QPoint point,
		Ui::Text::StateRequest::Flags flags) const;

	[[nodiscard]] int anchorTop(const QString &anchorId) const;

	[[nodiscard]] bool toggleDetails(const QString &anchorId);

	[[nodiscard]] bool segmentIsText(int index) const;

	[[nodiscard]] int segmentLength(int index) const;

	[[nodiscard]] int selectionOffsetFromHit(
		const MarkdownArticleHitTestResult &result,
		TextSelectType selectionType) const;

	[[nodiscard]] TextSelection adjustSelection(
		int segmentIndex,
		TextSelection selection,
		TextSelectType selectionType) const;

	[[nodiscard]] bool selectionContains(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints,
		const MarkdownArticleHitTestResult &result) const;

	[[nodiscard]] TextForMimeData textForContext(
		const MarkdownArticleHitTestResult &result) const;

	[[nodiscard]] TextForMimeData textForSelection(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) const;

	[[nodiscard]] bool highlightProcessDone(
		Spellchecker::HighlightProcessId processId);

	void invalidatePaletteCache();

	void invalidateRasterCache();

	[[nodiscard]] MediaBlockHost *mediaBlockHost() const;

	void setPlaceholderLoading(PreparedPlaceholderBlockId id);
	void clearPlaceholderLoading(PreparedPlaceholderBlockId id);
	void clearAllPlaceholderLoading();
	void addPlaceholderRipple(PreparedPlaceholderBlockId id, QPoint point);
	void stopPlaceholderRipple(PreparedPlaceholderBlockId id);

	void invalidateLayout();

private:
	[[nodiscard]] int currentDevicePixelRatio() const;

	void rebuildVisibleSegmentLookup();

	void refreshVisibleSegmentSpan();

	void clearMediaBlocks();

	void refreshMediaBlockHosts();

	void clearPlaceholderRuntimes();

	[[nodiscard]] std::shared_ptr<PlaceholderBlockRuntime>
	getOrCreatePlaceholderRuntime(PreparedPlaceholderBlockId id);

	void prunePlaceholderRuntimes();

	void requestPlaceholderRepaint(PreparedPlaceholderBlockId id);

	[[nodiscard]] std::shared_ptr<MediaBlock> getOrCreateMediaBlock(
		const PreparedBlock &prepared);

	template <typename Factory>
	[[nodiscard]] std::shared_ptr<MediaBlock> getOrCreateMediaBlock(
		PreparedMediaBlockId id,
		Factory &&factory);

	[[nodiscard]] Spellchecker::HighlightProcessId tryHighlightSyntax(
		const QString &displayText,
		const QString &language,
		TextWithEntities &marked) override;

	[[nodiscard]] SegmentSpan candidateSegmentSpan(QPoint point) const;

	void clearPendingHighlightBlockPointers();

	void registerPendingHighlightProcess(
		const PendingHighlightKey &key,
		Spellchecker::HighlightProcessId processId);

	void registerPendingHighlightBlock(LaidOutBlock &block);

	void registerPendingHighlightBlocks(std::vector<LaidOutBlock> &blocks);

	void resetFormulaRasterCache();

	void setPlaceholderLoadingValue(
		PreparedPlaceholderBlockId id,
		bool loading);

	void relayout(int width);

	mutable MarkdownArticleContent _content;
	std::vector<RenderedFormula> _formulaRenders;
	std::shared_ptr<MathRenderer> _renderer;
	std::shared_ptr<InlineFormulaObjectCache> _inlineFormulaObjects;
	MediaBlockHost *_mediaBlockHost = nullptr;
	Fn<void()> _textRepaint;
	Fn<void(QRect)> _textRepaintRect;
	int _width = -1;
	int _height = 0;
	std::vector<LaidOutBlock> _blocks;
	std::unordered_map<uint64, std::shared_ptr<MediaBlock>> _mediaBlocks;
	std::unordered_map<uint64, std::shared_ptr<PlaceholderBlockRuntime>>
		_placeholderRuntimes;
	std::unordered_map<
		uint64,
		RelatedArticleThumbnailState> _relatedArticleThumbnails;
	std::unordered_map<
		PendingHighlightKey,
		Spellchecker::HighlightProcessId,
		PendingHighlightKeyHasher> _pendingHighlightProcesses;
	std::unordered_map<
		Spellchecker::HighlightProcessId,
		PendingHighlightEntry> _pendingHighlightEntries;
	std::vector<std::pair<QString, int>> _anchors;
	std::vector<SelectableSegment> _segments;
	std::optional<LogicalVisibleRange> _visibleRange;
	SegmentSpan _visibleSegmentSpan;
	std::vector<int> _segmentTops;
	std::vector<int> _segmentBottoms;

};

MarkdownArticle::Impl::Impl(std::shared_ptr<MathRenderer> renderer)
: _renderer(std::move(renderer))
, _inlineFormulaObjects(CreateInlineFormulaObjectCache(_renderer)) {
}

void MarkdownArticle::Impl::setRenderer(std::shared_ptr<MathRenderer> renderer) {
	_renderer = std::move(renderer);
	SetInlineFormulaObjectCacheRenderer(_inlineFormulaObjects, _renderer);
	invalidateRasterCache();
	invalidateLayout();
}

void MarkdownArticle::Impl::setMediaBlockHost(MediaBlockHost *host) {
	_mediaBlockHost = host;
	refreshMediaBlockHosts();
}

void MarkdownArticle::Impl::setTextRepaintCallbacks(
		Fn<void()> repaint,
		Fn<void(QRect)> repaintRect) {
	_textRepaint = std::move(repaint);
	_textRepaintRect = std::move(repaintRect);
}

void MarkdownArticle::Impl::setContent(MarkdownArticleContent content) {
	clearMediaBlocks();
	clearPlaceholderRuntimes();
	_relatedArticleThumbnails.clear();
	_content = std::move(content);
	ClearInlineFormulaObjectCache(_inlineFormulaObjects);
	resetFormulaRasterCache();
	invalidateLayout();
}

int MarkdownArticle::Impl::maxWidth() {
	const auto &markdown = st::defaultMarkdown;
	return std::max(
		markdown.pageMaxWidth,
		markdown.pagePadding.left()
			+ markdown.pagePadding.right()
			+ 1);
}

int MarkdownArticle::Impl::resizeGetHeight(int width) {
	relayout(width);
	return std::max(_height, 1);
}

void MarkdownArticle::Impl::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	if (visibleBottom <= visibleTop) {
		_visibleRange = std::nullopt;
		_visibleSegmentSpan = {};
		return;
	}
	_visibleRange = LogicalVisibleRange{
		.top = visibleTop,
		.bottom = visibleBottom,
	};
	refreshVisibleSegmentSpan();
}

void MarkdownArticle::Impl::paint(
		Painter &p,
		QRect clip,
		MarkdownArticlePaintCaches caches,
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) {
	const auto &markdown = st::defaultMarkdown;
	const auto selectionState = PaintSelectionState{
		.segments = &_segments,
		.selection = selection,
		.endpoints = endpoints,
	};
	PaintBlocks(
		p,
		_blocks,
		&_content.formulas,
		&_formulaRenders,
		_renderer.get(),
		currentDevicePixelRatio(),
		std::max(_width, 1),
		markdown,
		caches,
		selectionState,
		clip);
}

MarkdownArticleHitTestResult MarkdownArticle::Impl::hitTest(
		QPoint point,
		Ui::Text::StateRequest::Flags flags) const {
	const auto span = candidateSegmentSpan(point);
	for (auto i = span.from; i != span.till; ++i) {
		const auto &segment = _segments[i];
		if (const auto result = HitTextSegment(segment, point, flags);
			result.valid()) {
			return result;
		}
	}
	for (auto i = span.from; i != span.till; ++i) {
		const auto &segment = _segments[i];
		if (const auto result = HitBlockSegment(segment, point, flags);
			result.valid()) {
			return result;
		}
	}
	if (flags & Ui::Text::StateRequest::Flag::LookupSymbol) {
		return HitSegmentFallback(_segments, span, point);
	}
	return {};
}

int MarkdownArticle::Impl::anchorTop(const QString &anchorId) const {
	for (const auto &entry : _anchors) {
		if (entry.first == anchorId) {
			return entry.second;
		}
	}
	return -1;
}

bool MarkdownArticle::Impl::toggleDetails(const QString &anchorId) {
	if (!ToggleDetailsBlock(&_content.blocks.blocks, anchorId)) {
		return false;
	}
	invalidateLayout();
	return true;
}

bool MarkdownArticle::Impl::segmentIsText(int index) const {
	const auto segment = FindSegment(&_segments, index);
	return segment && segment->isTextLeaf();
}

int MarkdownArticle::Impl::segmentLength(int index) const {
	const auto segment = FindSegment(&_segments, index);
	return segment ? SegmentLength(*segment) : 0;
}

int MarkdownArticle::Impl::selectionOffsetFromHit(
		const MarkdownArticleHitTestResult &result,
		TextSelectType selectionType) const {
	const auto segment = FindSegment(&_segments, result.segmentIndex);
	if (!segment) {
		return 0;
	}
	if (result.forcedOffset >= 0) {
		return std::clamp(result.forcedOffset, 0, SegmentLength(*segment));
	}
	auto offset = int(result.state.symbol);
	if (selectionType == TextSelectType::Letters
		&& result.state.afterSymbol) {
		++offset;
	}
	return std::clamp(offset, 0, SegmentLength(*segment));
}

TextSelection MarkdownArticle::Impl::adjustSelection(
		int segmentIndex,
		TextSelection selection,
		TextSelectType selectionType) const {
	const auto segment = FindSegment(&_segments, segmentIndex);
	if (!segment || !segment->isTextLeaf()) {
		return selection;
	}
	return segment->leaf->adjustSelection(selection, selectionType);
}

bool MarkdownArticle::Impl::selectionContains(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints,
		const MarkdownArticleHitTestResult &result) const {
	const auto segment = FindSegment(&_segments, result.segmentIndex);
	if (!segment || selection.empty() || !result.valid()) {
		return false;
	}
	const auto selectionState = PaintSelectionState{
		.segments = &_segments,
		.selection = selection,
		.endpoints = endpoints,
	};
	if (segment->tableSegmentIndex >= 0
		&& TableSegmentSelected(selectionState, segment->tableSegmentIndex)) {
		return true;
	}
	if (!segment->isTextLeaf()) {
		return WholeSegmentSelected(*segment, selectionState);
	}
	const auto textSelection = TextSelectionForSegment(*segment, selectionState);
	if (!textSelection || textSelection->empty()) {
		return false;
	}
	const auto offset = selectionOffsetFromHit(result, TextSelectType::Letters);
	return (offset >= textSelection->from) && (offset < textSelection->to);
}

TextForMimeData MarkdownArticle::Impl::textForContext(
		const MarkdownArticleHitTestResult &result) const {
	if (!result.valid() || !result.direct) {
		return TextForMimeData();
	}
	const auto segment = FindSegment(&_segments, result.segmentIndex);
	return segment ? TextForSegment(*segment) : TextForMimeData();
}

TextForMimeData MarkdownArticle::Impl::textForSelection(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) const {
	return TextForSelectedSegments(_segments, selection, endpoints);
}

bool MarkdownArticle::Impl::highlightProcessDone(
		Spellchecker::HighlightProcessId processId) {
	const auto i = _pendingHighlightEntries.find(processId);
	if (i == end(_pendingHighlightEntries)) {
		return false;
	}
	auto entry = std::move(i->second);
	_pendingHighlightEntries.erase(i);
	_pendingHighlightProcesses.erase(entry.key);

	auto rebuilt = false;
	for (const auto block : entry.blocks) {
		RepopulateCodeBlockLeaf(*block, st::defaultMarkdown, true, this);
		registerPendingHighlightBlock(*block);
		rebuilt = true;
	}
	return rebuilt;
}

void MarkdownArticle::Impl::invalidatePaletteCache() {
	InvalidateInlineFormulaPaletteCache(_inlineFormulaObjects);
	ClearColorizedFormulaImages(&_blocks);
}

void MarkdownArticle::Impl::invalidateRasterCache() {
	resetFormulaRasterCache();
	InvalidateInlineFormulaRasterCache(_inlineFormulaObjects);
	ClearColorizedFormulaImages(&_blocks);
}

MediaBlockHost *MarkdownArticle::Impl::mediaBlockHost() const {
	return _mediaBlockHost;
}

void MarkdownArticle::Impl::setPlaceholderLoading(
		PreparedPlaceholderBlockId id) {
	setPlaceholderLoadingValue(id, true);
}

void MarkdownArticle::Impl::clearPlaceholderLoading(
		PreparedPlaceholderBlockId id) {
	setPlaceholderLoadingValue(id, false);
}

void MarkdownArticle::Impl::clearAllPlaceholderLoading() {
	auto repaintIds = std::vector<PreparedPlaceholderBlockId>();
	repaintIds.reserve(_placeholderRuntimes.size());
	for (const auto &[value, runtime] : _placeholderRuntimes) {
		if (!runtime || !runtime->loading) {
			continue;
		}
		runtime->loading = false;
		runtime->loadingAnimation.stop(anim::type::instant);
		repaintIds.push_back({ .value = value });
	}
	for (const auto id : repaintIds) {
		requestPlaceholderRepaint(id);
	}
}

void MarkdownArticle::Impl::addPlaceholderRipple(
		PreparedPlaceholderBlockId id,
		QPoint point) {
	const auto block = FindPlaceholderBlock(&_blocks, id);
	if (!block) {
		return;
	}
	auto runtime = block->placeholderRuntime
		? block->placeholderRuntime
		: getOrCreatePlaceholderRuntime(id);
	if (!runtime) {
		return;
	}
	block->placeholderRuntime = runtime;
	const auto size = block->mediaRect.size();
	if (!runtime->ripple || runtime->rippleSize != size) {
		runtime->ripple = std::make_unique<Ui::RippleAnimation>(
			st::defaultRippleAnimation,
			Ui::RippleAnimation::RoundRectMask(
				size,
				st::defaultMarkdown.placeholder.radius),
			[=] {
				requestPlaceholderRepaint(id);
			});
		runtime->rippleSize = size;
	}
	point.setX(std::clamp(point.x(), 0, std::max(size.width() - 1, 0)));
	point.setY(std::clamp(point.y(), 0, std::max(size.height() - 1, 0)));
	runtime->ripple->add(point);
	requestPlaceholderRepaint(id);
}

void MarkdownArticle::Impl::stopPlaceholderRipple(
		PreparedPlaceholderBlockId id) {
	if (!id) {
		return;
	}
	const auto i = _placeholderRuntimes.find(id.value);
	if (i == end(_placeholderRuntimes)
		|| !i->second
		|| !i->second->ripple) {
		return;
	}
	i->second->ripple->lastStop();
	requestPlaceholderRepaint(id);
}

void MarkdownArticle::Impl::invalidateLayout() {
	_width = -1;
	_height = 0;
	clearPendingHighlightBlockPointers();
	_blocks.clear();
	_anchors.clear();
	_segments.clear();
	_visibleSegmentSpan = {};
	_segmentTops.clear();
	_segmentBottoms.clear();
}

int MarkdownArticle::Impl::currentDevicePixelRatio() const {
	return std::max(style::DevicePixelRatio(), 1);
}

void MarkdownArticle::Impl::rebuildVisibleSegmentLookup() {
	RebuildVisibleSegmentLookup(
		_segments,
		&_segmentTops,
		&_segmentBottoms);
	refreshVisibleSegmentSpan();
}

void MarkdownArticle::Impl::refreshVisibleSegmentSpan() {
	_visibleSegmentSpan = _visibleRange
		? LookupVisibleSegmentSpan(
			_segmentTops,
			_segmentBottoms,
			*_visibleRange)
		: SegmentSpan();
}

void MarkdownArticle::Impl::clearMediaBlocks() {
	for (const auto &[id, block] : _mediaBlocks) {
		if (block) {
			block->setHost(nullptr);
		}
	}
	_mediaBlocks.clear();
}

void MarkdownArticle::Impl::clearPlaceholderRuntimes() {
	_placeholderRuntimes.clear();
}

void MarkdownArticle::Impl::refreshMediaBlockHosts() {
	for (const auto &[id, block] : _mediaBlocks) {
		if (block) {
			block->setHost(_mediaBlockHost);
		}
	}
}

std::shared_ptr<PlaceholderBlockRuntime>
MarkdownArticle::Impl::getOrCreatePlaceholderRuntime(
		PreparedPlaceholderBlockId id) {
	if (!id) {
		return nullptr;
	}
	if (const auto i = _placeholderRuntimes.find(id.value);
		i != end(_placeholderRuntimes)) {
		return i->second;
	}
	auto runtime = std::make_shared<PlaceholderBlockRuntime>([=] {
		requestPlaceholderRepaint(id);
	});
	_placeholderRuntimes.emplace(id.value, runtime);
	return runtime;
}

void MarkdownArticle::Impl::prunePlaceholderRuntimes() {
	auto live = std::unordered_set<uint64>();
	CollectPlaceholderIds(_blocks, &live);
	for (auto i = _placeholderRuntimes.begin(); i != _placeholderRuntimes.end();) {
		if (live.find(i->first) != end(live)) {
			++i;
		} else {
			i = _placeholderRuntimes.erase(i);
		}
	}
}

void MarkdownArticle::Impl::requestPlaceholderRepaint(
		PreparedPlaceholderBlockId id) {
	if (const auto block = FindPlaceholderBlock(_blocks, id)) {
		if (_textRepaintRect) {
			_textRepaintRect(block->mediaRect);
		} else if (_textRepaint) {
			_textRepaint();
		}
	} else if (_textRepaint) {
		_textRepaint();
	}
}

std::shared_ptr<MediaBlock> MarkdownArticle::Impl::getOrCreateMediaBlock(
		const PreparedBlock &prepared) {
	switch (prepared.kind) {
	case PreparedBlockKind::Photo:
		return getOrCreateMediaBlock(
			prepared.photo.id,
			[=] {
				return CreatePhotoMediaBlock(
					prepared.photo,
					_content.mediaRuntime);
			});
	case PreparedBlockKind::Video:
		return getOrCreateMediaBlock(
			prepared.video.id,
			[=] {
				return CreateVideoMediaBlock(
					prepared.video,
					_content.mediaRuntime);
			});
	case PreparedBlockKind::Map:
		return getOrCreateMediaBlock(
			prepared.map.id,
			[=] {
				return CreateMapMediaBlock(
					prepared.map,
					_content.mediaRuntime);
			});
	case PreparedBlockKind::Audio:
		return getOrCreateMediaBlock(
			prepared.audio.id,
			[=] {
				return CreateAudioMediaBlock(
					prepared.audio,
					_content.mediaRuntime);
			});
	case PreparedBlockKind::Channel:
		return getOrCreateMediaBlock(
			prepared.channel.id,
			[=] {
				return CreateChannelMediaBlock(
					prepared.channel,
					_content.mediaRuntime);
			});
	case PreparedBlockKind::GroupedMedia:
		return getOrCreateMediaBlock(
			prepared.groupedMedia.id,
			[=] {
				return CreateGroupedMediaBlock(
					prepared.groupedMedia,
					_content.mediaRuntime);
			});
	default:
		return nullptr;
	}
}

template <typename Factory>
std::shared_ptr<MediaBlock> MarkdownArticle::Impl::getOrCreateMediaBlock(
		PreparedMediaBlockId id,
		Factory &&factory) {
	if (!id) {
		return nullptr;
	}
	if (const auto i = _mediaBlocks.find(id.value);
		i != end(_mediaBlocks)) {
		return i->second;
	}
	auto block = factory();
	if (block) {
		block->setHost(_mediaBlockHost);
	}
	_mediaBlocks.emplace(id.value, block);
	return block;
}

Spellchecker::HighlightProcessId MarkdownArticle::Impl::tryHighlightSyntax(
		const QString &displayText,
		const QString &language,
		TextWithEntities &marked) {
	const auto key = PendingHighlightKey{
		.text = displayText,
		.language = language,
	};
	if (const auto i = _pendingHighlightProcesses.find(key);
		i != end(_pendingHighlightProcesses)) {
		return i->second;
	}
	const auto processId = Spellchecker::TryHighlightSyntax(marked);
	if (processId) {
		registerPendingHighlightProcess(key, processId);
	}
	return processId;
}

SegmentSpan MarkdownArticle::Impl::candidateSegmentSpan(QPoint point) const {
	if (_visibleRange
		&& (_visibleRange->top <= point.y())
		&& (point.y() < _visibleRange->bottom)) {
		return _visibleSegmentSpan.empty()
			? FullSegmentSpan(_segments)
			: _visibleSegmentSpan;
	}
	return FullSegmentSpan(_segments);
}

void MarkdownArticle::Impl::clearPendingHighlightBlockPointers() {
	for (auto &entry : _pendingHighlightEntries) {
		entry.second.blocks.clear();
	}
}

void MarkdownArticle::Impl::registerPendingHighlightProcess(
		const PendingHighlightKey &key,
		Spellchecker::HighlightProcessId processId) {
	_pendingHighlightProcesses[key] = processId;
	auto &entry = _pendingHighlightEntries[processId];
	entry.key = key;
}

void MarkdownArticle::Impl::registerPendingHighlightBlock(LaidOutBlock &block) {
	if (!block.syntaxHighlightProcessId) {
		return;
	}
	if (!_pendingHighlightEntries.contains(block.syntaxHighlightProcessId)) {
		registerPendingHighlightProcess(
			PendingHighlightKeyForBlock(block),
			block.syntaxHighlightProcessId);
	}
	_pendingHighlightEntries[block.syntaxHighlightProcessId].blocks.push_back(
		&block);
}

void MarkdownArticle::Impl::registerPendingHighlightBlocks(std::vector<LaidOutBlock> &blocks) {
	for (auto &block : blocks) {
		registerPendingHighlightBlock(block);
		registerPendingHighlightBlocks(block.children);
	}
}

void MarkdownArticle::Impl::resetFormulaRasterCache() {
	_formulaRenders.clear();
	_formulaRenders.resize(_content.formulas.size());
}

void MarkdownArticle::Impl::setPlaceholderLoadingValue(
		PreparedPlaceholderBlockId id,
		bool loading) {
	if (!id) {
		return;
	}
	const auto runtime = loading
		? getOrCreatePlaceholderRuntime(id)
		: [&]() -> std::shared_ptr<PlaceholderBlockRuntime> {
			if (const auto i = _placeholderRuntimes.find(id.value);
				i != end(_placeholderRuntimes)) {
				return i->second;
			}
			return nullptr;
		}();
	if (!runtime || runtime->loading == loading) {
		return;
	}
	runtime->loading = loading;
	if (loading) {
		runtime->loadingAnimation.start();
	} else {
		runtime->loadingAnimation.stop(anim::type::instant);
	}
	requestPlaceholderRepaint(id);
}

void MarkdownArticle::Impl::relayout(int width) {
	width = std::max(width, 1);
	if (_width == width) {
		return;
	}
	_width = width;
	StoreRelatedArticleThumbnailStates(
		_blocks,
		&_relatedArticleThumbnails);
	clearPendingHighlightBlockPointers();
	_blocks.clear();
	_anchors.clear();
	_segments.clear();
	_visibleSegmentSpan = {};
	_segmentTops.clear();
	_segmentBottoms.clear();

	const auto &markdown = st::defaultMarkdown;
	const auto &page = markdown.pagePadding;
	const auto innerWidth = std::max(width - page.left() - page.right(), 1);
	auto repaintScope = InlineIvImageRepaintScope(
		_textRepaint,
		_textRepaintRect);
	auto context = LayoutContext{
		.articleLeft = page.left(),
		.articleWidth = innerWidth,
		.useArticleBands = true,
		.syntaxHighlightTracker = this,
	};
	context.mediaBlockFactory = [=](const PreparedBlock &prepared) {
		return getOrCreateMediaBlock(prepared);
	};
	context.placeholderRuntimeFactory = [=](PreparedPlaceholderBlockId id) {
		return getOrCreatePlaceholderRuntime(id);
	};
	const auto y = LayoutBlocks(
		_content.blocks.blocks,
		&_content.formulas,
		&_formulaRenders,
		_renderer.get(),
		_inlineFormulaObjects.get(),
		_content.mediaRuntime,
		&_blocks,
		markdown,
		page.left(),
		page.top(),
		innerWidth,
		context);
	prunePlaceholderRuntimes();
	RestoreRelatedArticleThumbnailStates(
		&_blocks,
		_relatedArticleThumbnails);
	_height = y + page.bottom();
	registerPendingHighlightBlocks(_blocks);
	CollectAnchors(_blocks, &_anchors);
	CollectSelectableSegments(&_blocks, &_segments);
	rebuildVisibleSegmentLookup();
}

MarkdownArticle::MarkdownArticle(std::shared_ptr<MathRenderer> renderer)
: _impl(std::make_unique<Impl>(std::move(renderer))) {
}

MarkdownArticle::~MarkdownArticle() = default;
MarkdownArticle::MarkdownArticle(MarkdownArticle &&) noexcept = default;
MarkdownArticle &MarkdownArticle::operator=(MarkdownArticle &&) noexcept = default;

void MarkdownArticle::setRenderer(std::shared_ptr<MathRenderer> renderer) {
	_impl->setRenderer(std::move(renderer));
}

void MarkdownArticle::setMediaBlockHost(MediaBlockHost *host) {
	_impl->setMediaBlockHost(host);
}

void MarkdownArticle::setTextRepaintCallbacks(
		Fn<void()> repaint,
		Fn<void(QRect)> repaintRect) {
	_impl->setTextRepaintCallbacks(
		std::move(repaint),
		std::move(repaintRect));
}

void MarkdownArticle::setContent(MarkdownArticleContent content) {
	_impl->setContent(std::move(content));
}

void MarkdownArticle::invalidateLayout() {
	_impl->invalidateLayout();
}

int MarkdownArticle::maxWidth() const {
	return const_cast<Impl*>(_impl.get())->maxWidth();
}

int MarkdownArticle::resizeGetHeight(int width) {
	return _impl->resizeGetHeight(width);
}

void MarkdownArticle::setVisibleTopBottom(int visibleTop, int visibleBottom) {
	_impl->setVisibleTopBottom(visibleTop, visibleBottom);
}

void MarkdownArticle::paint(
		Painter &p,
		QRect clip,
		MarkdownArticlePaintCaches caches,
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) const {
	_impl->paint(
		p,
		clip,
		caches,
		selection,
		endpoints);
}

MarkdownArticleHitTestResult MarkdownArticle::hitTest(
		QPoint point,
		Ui::Text::StateRequest::Flags flags) const {
	return _impl->hitTest(point, flags);
}

int MarkdownArticle::anchorTop(const QString &anchorId) const {
	return _impl->anchorTop(anchorId);
}

bool MarkdownArticle::toggleDetails(const QString &anchorId) {
	return _impl->toggleDetails(anchorId);
}

bool MarkdownArticle::segmentIsText(int index) const {
	return _impl->segmentIsText(index);
}

int MarkdownArticle::segmentLength(int index) const {
	return _impl->segmentLength(index);
}

int MarkdownArticle::selectionOffsetFromHit(
		const MarkdownArticleHitTestResult &result,
		TextSelectType selectionType) const {
	return _impl->selectionOffsetFromHit(result, selectionType);
}

TextSelection MarkdownArticle::adjustSelection(
		int segmentIndex,
		TextSelection selection,
		TextSelectType selectionType) const {
	return _impl->adjustSelection(segmentIndex, selection, selectionType);
}

bool MarkdownArticle::selectionContains(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints,
		const MarkdownArticleHitTestResult &result) const {
	return _impl->selectionContains(selection, endpoints, result);
}

TextForMimeData MarkdownArticle::textForContext(
		const MarkdownArticleHitTestResult &result) const {
	return _impl->textForContext(result);
}

TextForMimeData MarkdownArticle::textForSelection(
		MarkdownArticleSelection selection,
		const MarkdownArticleSelectionEndpoints *endpoints) const {
	return _impl->textForSelection(selection, endpoints);
}

bool MarkdownArticle::highlightProcessDone(
		Spellchecker::HighlightProcessId processId) {
	return _impl->highlightProcessDone(processId);
}

void MarkdownArticle::invalidatePaletteCache() {
	_impl->invalidatePaletteCache();
}

void MarkdownArticle::invalidateRasterCache() {
	_impl->invalidateRasterCache();
}

MediaBlockHost *MarkdownArticle::mediaBlockHost() const {
	return _impl->mediaBlockHost();
}

void MarkdownArticle::setPlaceholderLoading(PreparedPlaceholderBlockId id) {
	_impl->setPlaceholderLoading(id);
}

void MarkdownArticle::clearPlaceholderLoading(PreparedPlaceholderBlockId id) {
	_impl->clearPlaceholderLoading(id);
}

void MarkdownArticle::clearAllPlaceholderLoading() {
	_impl->clearAllPlaceholderLoading();
}

void MarkdownArticle::addPlaceholderRipple(
		PreparedPlaceholderBlockId id,
		QPoint point) {
	_impl->addPlaceholderRipple(id, point);
}

void MarkdownArticle::stopPlaceholderRipple(PreparedPlaceholderBlockId id) {
	_impl->stopPlaceholderRipple(id);
}

} // namespace Iv::Markdown
