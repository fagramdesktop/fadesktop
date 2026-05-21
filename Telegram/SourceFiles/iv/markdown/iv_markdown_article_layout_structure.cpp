/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_article_layout_structure.h"
#include "iv/markdown/iv_markdown_article_text.h"

#include "styles/style_iv.h"

#include <algorithm>

namespace Iv::Markdown {
namespace {

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
		const style::TextStyle &style) {
	const auto lines = leaf.countLinesGeometry(textRect.width(), true);
	return textRect.y() + (lines.empty()
		? NominalTextBaseline(style, 0)
		: lines.front().baseline);
}

[[nodiscard]] int MarkdownBodyBaseline(
		int top,
		const style::Markdown &markdown) {
	return NominalTextBaseline(markdown.body, top);
}

[[nodiscard]] int BlockBottom(const LaidOutBlock &block) {
	return block.outer.y() + block.outer.height();
}

[[nodiscard]] bool UsesMediaBand(PreparedBlockKind kind) {
	switch (kind) {
	case PreparedBlockKind::Photo:
	case PreparedBlockKind::Video:
	case PreparedBlockKind::Audio:
	case PreparedBlockKind::Map:
	case PreparedBlockKind::Channel:
	case PreparedBlockKind::GroupedMedia:
	case PreparedBlockKind::RelatedArticle:
		return true;
	case PreparedBlockKind::Paragraph:
	case PreparedBlockKind::Heading:
	case PreparedBlockKind::CodeBlock:
	case PreparedBlockKind::Rule:
	case PreparedBlockKind::List:
	case PreparedBlockKind::ListItem:
	case PreparedBlockKind::Quote:
	case PreparedBlockKind::DisplayMath:
	case PreparedBlockKind::Table:
	case PreparedBlockKind::Details:
	case PreparedBlockKind::Placeholder:
	case PreparedBlockKind::EmbedPost:
		return false;
	}
	return false;
}

[[nodiscard]] QRect PaddedBand(
		int left,
		int width,
		QMargins padding) {
	return QRect(
		left + padding.left(),
		0,
		std::max(width - padding.left() - padding.right(), 1),
		0);
}

[[nodiscard]] QRect BlockBand(
		PreparedBlockKind kind,
		const style::Markdown &markdown,
		int left,
		int width,
		LayoutContext context) {
	if (!context.useArticleBands) {
		return QRect(left, 0, std::max(width, 1), 0);
	}
	return UsesMediaBand(kind)
		? PaddedBand(left, width, markdown.mediaPadding)
		: PaddedBand(left, width, markdown.textPadding);
}

[[nodiscard]] bool IsRelatedArticlesHeader(
		const PreparedBlock &block,
		const PreparedBlock *next) {
	return next
		&& (block.kind == PreparedBlockKind::Heading)
		&& (next->kind == PreparedBlockKind::RelatedArticle);
}

[[nodiscard]] const PreparedBlock *NextVisibleBlock(
		const std::vector<PreparedBlock> &blocks,
		int index) {
	for (auto i = index + 1, count = int(blocks.size()); i != count; ++i) {
		if (!IsAnchorOnlyBlock(blocks[i])) {
			return &blocks[i];
		}
	}
	return nullptr;
}

void PrepareNestedContext(
		LayoutContext *context,
		int left,
		int width) {
	context->useArticleBands = false;
	context->articleLeft = left;
	context->articleWidth = std::max(width, 1);
}

[[nodiscard]] bool FirstLineComesFromChildren(const LaidOutBlock &block) {
	switch (block.kind) {
	case PreparedBlockKind::List:
	case PreparedBlockKind::ListItem:
	case PreparedBlockKind::Quote:
	case PreparedBlockKind::EmbedPost:
		return true;
	case PreparedBlockKind::Paragraph:
	case PreparedBlockKind::Heading:
	case PreparedBlockKind::CodeBlock:
	case PreparedBlockKind::Rule:
	case PreparedBlockKind::DisplayMath:
	case PreparedBlockKind::Table:
	case PreparedBlockKind::Photo:
	case PreparedBlockKind::Video:
	case PreparedBlockKind::Audio:
	case PreparedBlockKind::Map:
	case PreparedBlockKind::Channel:
	case PreparedBlockKind::GroupedMedia:
	case PreparedBlockKind::RelatedArticle:
	case PreparedBlockKind::Placeholder:
	case PreparedBlockKind::Details:
		return false;
	}
	return false;
}

[[nodiscard]] int ResolveFirstDisplayedLineBaseline(
		const LaidOutBlock &block,
		const style::Markdown &markdown) {
	if (block.firstLineBaseline >= 0) {
		return block.firstLineBaseline;
	}
	if (FirstLineComesFromChildren(block)) {
		for (const auto &child : block.children) {
			if (child.outer.height() <= 0) {
				continue;
			}
			return ResolveFirstDisplayedLineBaseline(child, markdown);
		}
	}
	return MarkdownBodyBaseline(block.outer.y(), markdown);
}

[[nodiscard]] LaidOutBlock LayoutBlock(
	const PreparedBlock &prepared,
	std::vector<PreparedFormulaSlot> *formulas,
	std::vector<RenderedFormula> *renderedFormulas,
	MathRenderer *renderer,
	InlineFormulaObjectCache *inlineFormulaObjects,
	const std::shared_ptr<MediaRuntime> &mediaRuntime,
	const style::Markdown &markdown,
	int left,
	int top,
	int width,
	LayoutContext context);

[[nodiscard]] LaidOutBlock LayoutListItemBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context,
		bool tight) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::ListItem;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	block.listKind = prepared.listKind;
	block.listDelimiter = prepared.listDelimiter;
	block.taskState = prepared.taskState;
	block.orderedNumber = prepared.orderedNumber;

	const auto &list = markdown.list;
	const auto bodyLineHeight = TextLineHeight(markdown.body);
	const auto task = (prepared.taskState != TaskState::None);
	const auto ordered = !task && (prepared.listKind == ListKind::Ordered);
	const auto markerText = ordered ? ListMarkerText(prepared) : QString();
	auto markerTextWidth = 0;
	auto markerTextHeight = bodyLineHeight;
	if (task) {
		markerTextWidth = list.taskCheck.diameter;
		markerTextHeight = list.taskCheck.diameter;
	} else if (ordered) {
		block.marker.setMarkedText(
			markdown.body,
			TextWithEntities::Simple(markerText),
			TextParseOptions{
				TextParseMultiline,
				0,
				0,
				Qt::LayoutDirectionAuto,
			});
		markerTextWidth = std::max(block.marker.maxWidth(), 1);
		markerTextHeight = std::max(
			block.marker.countHeight(markerTextWidth, true),
			bodyLineHeight);
	}

	block.markerWidth = std::max(list.markerWidth, markerTextWidth);
	const auto bodyLeft = left + block.markerWidth + list.markerSkip;
	const auto bodyWidth = std::max(
		width - block.markerWidth - list.markerSkip,
		1);

	auto childContext = context;
	childContext.tightList = tight;
	PrepareNestedContext(&childContext, bodyLeft, bodyWidth);
	const auto childBottom = LayoutBlocks(
		prepared.children,
		formulas,
		renderedFormulas,
		renderer,
		inlineFormulaObjects,
		mediaRuntime,
		&block.children,
		markdown,
		bodyLeft,
		top,
		bodyWidth,
		childContext);
	const auto markerBaseline = [&] {
		for (const auto &child : block.children) {
			if (child.outer.height() <= 0) {
				continue;
			}
			return ResolveFirstDisplayedLineBaseline(child, markdown);
		}
		return MarkdownBodyBaseline(top, markdown);
	}();
	const auto contentHeight = childBottom - top;
	const auto rowHeight = std::max({
		contentHeight,
		markerTextHeight,
		bodyLineHeight,
	});

	const auto markerTop = top + std::max(
		(bodyLineHeight - markerTextHeight) / 2,
		0);
	if (task) {
		block.markerRect = QRect(
			left,
			markerTop,
			list.taskCheck.diameter,
			list.taskCheck.diameter);
	} else if (ordered) {
		const auto markerLeft = left + block.markerWidth - markerTextWidth;
		const auto markerLeafBaseline = LeafFirstLineBaseline(
			block.marker,
			QRect(0, 0, markerTextWidth, markerTextHeight),
			markdown.body);
		block.markerRect = QRect(
			markerLeft,
			markerBaseline - markerLeafBaseline,
			markerTextWidth,
			markerTextHeight);
	} else {
		block.markerCenter = BulletMarkerCenter(left, markerBaseline, markdown);
	}

	block.contentRect = QRect(bodyLeft, top, bodyWidth, rowHeight);
	block.outer = QRect(left, top, std::max(width, 1), rowHeight);
	block.firstLineBaseline = markerBaseline;
	return block;
}

[[nodiscard]] LaidOutBlock LayoutListBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::List;
	block.listKind = prepared.listKind;
	block.listDelimiter = prepared.listDelimiter;

	const auto depthDelta = std::max(prepared.visualDepth - context.listDepth, 0);
	const auto listLeft = left + depthDelta * markdown.list.indent;
	const auto listWidth = std::max(
		width - depthDelta * markdown.list.indent,
		1);

	auto childContext = context;
	childContext.listDepth = prepared.visualDepth;
	childContext.tightList = false;
	PrepareNestedContext(&childContext, listLeft, listWidth);

	auto y = top;
	auto previous = static_cast<const PreparedBlock*>(nullptr);
	for (const auto &child : prepared.children) {
		const auto anchorOnly = IsAnchorOnlyBlock(child);
		if (previous && !anchorOnly) {
			y += prepared.tight ? 0 : BlockSkip(child, markdown);
		}

		auto laidOut = (child.kind == PreparedBlockKind::ListItem)
			? LayoutListItemBlock(
				child,
				formulas,
				renderedFormulas,
				renderer,
				inlineFormulaObjects,
				mediaRuntime,
				markdown,
				listLeft,
				y,
				listWidth,
				childContext,
				prepared.tight)
			: LayoutBlock(
				child,
				formulas,
				renderedFormulas,
				renderer,
				inlineFormulaObjects,
				mediaRuntime,
				markdown,
				listLeft,
				y,
				listWidth,
				childContext);
		y = BlockBottom(laidOut);
		block.children.push_back(std::move(laidOut));
		if (!anchorOnly) {
			previous = &child;
		}
	}

	block.outer = QRect(
		listLeft,
		top,
		listWidth,
		std::max(y - top, 0));
	block.contentRect = block.outer;
	block.firstLineBaseline = ResolveFirstDisplayedLineBaseline(block, markdown);
	return block;
}

[[nodiscard]] LaidOutBlock LayoutQuoteBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Quote;

	const auto depthDelta = std::max(
		prepared.visualDepth - context.quoteDepth,
		0);
	const auto quoteLeft = left + depthDelta * markdown.quoteIndent;
	const auto quoteWidth = std::max(
		width - depthDelta * markdown.quoteIndent,
		1);
	const auto &quoteStyle = markdown.body.blockquote;
	const auto padding = BlockquotePadding(quoteStyle);
	const auto contentLeft = quoteLeft + padding.left();
	const auto contentTop = top + padding.top();
	const auto contentWidth = std::max(
		quoteWidth - padding.left() - padding.right(),
		1);

	auto childContext = context;
	childContext.quoteDepth = prepared.visualDepth;
	childContext.tightList = false;
	PrepareNestedContext(&childContext, contentLeft, contentWidth);
	const auto childBottom = LayoutBlocks(
		prepared.children,
		formulas,
		renderedFormulas,
		renderer,
		inlineFormulaObjects,
		mediaRuntime,
		&block.children,
		markdown,
		contentLeft,
		contentTop,
		contentWidth,
		childContext);
	const auto contentHeight = std::max(
		childBottom - contentTop,
		prepared.children.empty()
			? TextLineHeight(markdown.body)
			: 0);
	const auto quoteHeight = padding.top() + contentHeight + padding.bottom();

	block.outer = QRect(quoteLeft, top, quoteWidth, quoteHeight);
	block.contentRect = QRect(
		contentLeft,
		contentTop,
		contentWidth,
		contentHeight);
	block.firstLineBaseline = ResolveFirstDisplayedLineBaseline(block, markdown);
	return block;
}

[[nodiscard]] LaidOutBlock LayoutDetailsBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::Details;
	block.anchorId = prepared.anchorId;
	block.collapsed = prepared.collapsed;
	block.supplementary = prepared.supplementary;
	const auto &details = markdown.details;
	const auto headerWidth = std::max(width, 1);
	const auto iconWidth = details.icon.width();
	const auto iconHeight = details.icon.height();
	const auto iconSkip = iconWidth ? details.iconSkip : 0;
	const auto textLeft = left
		+ details.headerPadding.left()
		+ iconWidth
		+ iconSkip;
	block.textWidth = std::max(
		headerWidth
			- details.headerPadding.left()
			- details.headerPadding.right()
			- iconWidth
			- iconSkip,
		1);

	SetTextLeaf(
		&block.leaf,
		details.summaryStyle,
		prepared.text,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		block.textWidth);
	BindLinks(&block.leaf, prepared.links);

	const auto summaryHeight = std::max(
		block.leaf.countHeight(block.textWidth, true),
		TextLineHeight(details.summaryStyle));
	const auto headerContentHeight = std::max(summaryHeight, iconHeight);
	const auto headerHeight = details.headerPadding.top()
		+ headerContentHeight
		+ details.headerPadding.bottom();
	block.headerRect = QRect(left, top, headerWidth, headerHeight);
	if (iconWidth > 0 && iconHeight > 0) {
		block.iconRect = QRect(
			left + details.headerPadding.left(),
			top + (headerHeight - iconHeight) / 2,
			iconWidth,
			iconHeight);
	}
	block.textRect = QRect(
		textLeft,
		top + details.headerPadding.top()
			+ std::max((headerContentHeight - summaryHeight) / 2, 0),
		block.textWidth,
		summaryHeight);
	block.firstLineBaseline = LeafFirstLineBaseline(
		block.leaf,
		block.textRect,
		details.summaryStyle);

	auto bottom = top + headerHeight;
	if (!prepared.collapsed) {
		const auto childLeft = left + details.bodyPadding.left();
		const auto childTop = bottom + details.bodyPadding.top();
		const auto childWidth = std::max(
			headerWidth
				- details.bodyPadding.left()
				- details.bodyPadding.right(),
			1);
		auto childContext = context;
		PrepareNestedContext(&childContext, childLeft, childWidth);
		const auto childBottom = LayoutBlocks(
			prepared.children,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			&block.children,
			markdown,
			childLeft,
			childTop,
			childWidth,
			childContext);
		const auto contentHeight = std::max(childBottom - childTop, 0);
		const auto bodyHeight = details.bodyPadding.top()
			+ contentHeight
			+ details.bodyPadding.bottom();
		block.bodyRect = QRect(left, bottom, headerWidth, bodyHeight);
		block.contentRect = QRect(
			childLeft,
			childTop,
			childWidth,
			contentHeight);
		bottom += bodyHeight;
	}
	block.outer = QRect(
		left,
		top,
		headerWidth,
		std::max(bottom - top, headerHeight));
	return block;
}

[[nodiscard]] LaidOutBlock LayoutEmbedPostBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto block = LaidOutBlock();
	block.kind = PreparedBlockKind::EmbedPost;
	block.anchorId = prepared.anchorId;
	block.supplementary = prepared.supplementary;
	block.thumbnailPhotoId = prepared.embedPost.authorPhotoId;
	if (prepared.embedPost.authorPhotoId && mediaRuntime) {
		block.photoRuntime = mediaRuntime->resolvePhoto(
			prepared.embedPost.authorPhotoId);
	}

	const auto parseOptions = TextParseOptions{
		TextParseMultiline,
		0,
		0,
		Qt::LayoutDirectionAuto,
	};
	const auto &style = markdown.embedPost;
	const auto blockWidth = std::max(width, 1);
	const auto contentLeft = left
		+ style.accentWidth
		+ style.accentSkip
		+ style.padding.left();
	const auto contentTop = top + style.padding.top();
	const auto contentWidth = std::max(
		blockWidth
			- style.accentWidth
			- style.accentSkip
			- style.padding.left()
			- style.padding.right(),
		1);
	const auto hasAvatar = (block.photoRuntime != nullptr);
	const auto avatarSize = hasAvatar ? std::max(style.avatarSize, 1) : 0;
	const auto headerGap = hasAvatar ? style.headerGap : 0;
	const auto textLeft = contentLeft + avatarSize + headerGap;
	const auto textWidth = std::max(contentWidth - avatarSize - headerGap, 1);

	auto authorHeight = 0;
	if (!prepared.embedPost.author.isEmpty()) {
		block.labelWidth = textWidth;
		block.labelLeaf = Ui::Text::String(TextMinResizeWidth(textWidth));
		block.labelLeaf.setMarkedText(
			style.authorStyle,
			TextWithEntities::Simple(prepared.embedPost.author),
			parseOptions);
		authorHeight = std::max(
			block.labelLeaf.countHeight(textWidth, true),
			TextLineHeight(style.authorStyle));
	}

	auto dateHeight = 0;
	if (!prepared.embedPost.dateText.isEmpty()) {
		block.subtitleWidth = textWidth;
		block.subtitleLeaf = Ui::Text::String(TextMinResizeWidth(textWidth));
		block.subtitleLeaf.setMarkedText(
			style.dateStyle,
			TextWithEntities::Simple(prepared.embedPost.dateText),
			parseOptions);
		dateHeight = std::max(
			block.subtitleLeaf.countHeight(textWidth, true),
			TextLineHeight(style.dateStyle));
	}

	const auto textHeight = authorHeight + dateHeight;
	const auto headerHeight = std::max(textHeight, avatarSize);
	if (headerHeight > 0) {
		block.headerRect = QRect(contentLeft, contentTop, contentWidth, headerHeight);
	}
	if (avatarSize > 0) {
		block.thumbnailRect = QRect(
			contentLeft,
			contentTop + std::max((headerHeight - avatarSize) / 2, 0),
			avatarSize,
			avatarSize);
	}

	auto textTop = contentTop + std::max((headerHeight - textHeight) / 2, 0);
	if (authorHeight > 0) {
		block.labelRect = QRect(textLeft, textTop, textWidth, authorHeight);
		textTop += authorHeight;
	}
	if (dateHeight > 0) {
		block.subtitleRect = QRect(textLeft, textTop, textWidth, dateHeight);
	}

	auto wrapperBottom = contentTop + headerHeight;
	if (!prepared.children.empty()) {
		const auto bodyTop = wrapperBottom + ((headerHeight > 0) ? style.bodySkip : 0);
		auto childContext = context;
		PrepareNestedContext(&childContext, contentLeft, contentWidth);
		const auto childBottom = LayoutBlocks(
			prepared.children,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			&block.children,
			markdown,
			contentLeft,
			bodyTop,
			contentWidth,
			childContext);
		const auto bodyHeight = std::max(childBottom - bodyTop, 0);
		block.bodyRect = QRect(contentLeft, bodyTop, contentWidth, bodyHeight);
		wrapperBottom = std::max(wrapperBottom, childBottom);
	}

	block.contentRect = QRect(
		contentLeft,
		contentTop,
		contentWidth,
		std::max(wrapperBottom - contentTop, 0));
	block.mediaRect = QRect(
		left,
		top,
		blockWidth,
		style.padding.top()
			+ block.contentRect.height()
			+ style.padding.bottom());

	auto bottom = block.mediaRect.y() + block.mediaRect.height();
	LayoutMediaCaption(
		&block,
		prepared,
		formulas,
		inlineFormulaObjects,
		mediaRuntime,
		markdown,
		left,
		bottom,
		blockWidth,
		style.captionSkip,
		&bottom,
		context);
	block.outer = QRect(
		left,
		top,
		blockWidth,
		std::max(bottom - top, block.mediaRect.height()));

	if (!block.labelRect.isEmpty() && !block.labelLeaf.isEmpty()) {
		block.firstLineBaseline = LeafFirstLineBaseline(
			block.labelLeaf,
			block.labelRect,
			style.authorStyle);
	} else if (!block.subtitleRect.isEmpty() && !block.subtitleLeaf.isEmpty()) {
		block.firstLineBaseline = LeafFirstLineBaseline(
			block.subtitleLeaf,
			block.subtitleRect,
			style.dateStyle);
	} else {
		for (const auto &child : block.children) {
			if (child.outer.height() <= 0) {
				continue;
			}
			block.firstLineBaseline = ResolveFirstDisplayedLineBaseline(
				child,
				markdown);
			break;
		}
		if (block.firstLineBaseline < 0) {
			block.firstLineBaseline = MarkdownBodyBaseline(top, markdown);
		}
	}
	return block;
}

[[nodiscard]] LaidOutBlock LayoutBlock(
		const PreparedBlock &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	switch (prepared.kind) {
	case PreparedBlockKind::Paragraph:
	case PreparedBlockKind::Heading:
		return LayoutFlowBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::CodeBlock:
		return LayoutCodeBlock(
			prepared,
			markdown,
			left,
			top,
			width,
			context.allowAsyncSyntaxHighlighting,
			context.syntaxHighlightTracker);
	case PreparedBlockKind::Rule:
		return LayoutRuleBlock(markdown, left, top, width);
	case PreparedBlockKind::List:
		return LayoutListBlock(
			prepared,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::ListItem:
		return LayoutListItemBlock(
			prepared,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context,
			false);
	case PreparedBlockKind::Quote:
		return LayoutQuoteBlock(
			prepared,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::DisplayMath:
		return LayoutDisplayMathBlock(prepared, *formulas, markdown, left, top, width);
	case PreparedBlockKind::Table:
		return LayoutTableBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width);
	case PreparedBlockKind::Photo:
		return LayoutPhotoBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::Video:
		return LayoutVideoBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::Audio:
		return LayoutAudioBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::Map:
		return LayoutMapBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::Channel:
		return LayoutChannelBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::GroupedMedia:
		return LayoutGroupedMediaBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::RelatedArticle:
		return LayoutRelatedArticleBlock(
			prepared,
			markdown,
			left,
			top,
			width,
			mediaRuntime);
	case PreparedBlockKind::Placeholder:
		return LayoutPlaceholderBlock(
			prepared,
			formulas,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::Details:
		return LayoutDetailsBlock(
			prepared,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	case PreparedBlockKind::EmbedPost:
		return LayoutEmbedPostBlock(
			prepared,
			formulas,
			renderedFormulas,
			renderer,
			inlineFormulaObjects,
			mediaRuntime,
			markdown,
			left,
			top,
			width,
			context);
	}
	Unexpected("Unknown markdown article block kind.");
}

} // namespace

int LayoutBlocks(
		const std::vector<PreparedBlock> &prepared,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		InlineFormulaObjectCache *inlineFormulaObjects,
		const std::shared_ptr<MediaRuntime> &mediaRuntime,
		std::vector<LaidOutBlock> *blocks,
		const style::Markdown &markdown,
		int left,
		int top,
		int width,
		LayoutContext context) {
	auto y = top;
	auto previous = static_cast<const PreparedBlock*>(nullptr);
	for (auto i = 0, count = int(prepared.size()); i != count; ++i) {
		const auto &block = prepared[i];
		const auto anchorOnly = IsAnchorOnlyBlock(block);
		const auto next = NextVisibleBlock(prepared, i);
		if (previous && !anchorOnly) {
			y += BlockSkip(*previous, block, context, markdown);
		}
		const auto band = BlockBand(
			block.kind,
			markdown,
			left,
			std::max(width, 1),
			context);
		auto laidOut = IsRelatedArticlesHeader(block, next)
			? LayoutFlowBlock(
				block,
				formulas,
				inlineFormulaObjects,
				mediaRuntime,
				markdown,
				left + markdown.relatedArticle.headerPadding.left(),
				y + markdown.relatedArticle.headerPadding.top(),
				std::max(
					width
						- markdown.relatedArticle.headerPadding.left()
						- markdown.relatedArticle.headerPadding.right(),
					1),
				context)
			: LayoutBlock(
				block,
				formulas,
				renderedFormulas,
				renderer,
				inlineFormulaObjects,
				mediaRuntime,
				markdown,
				band.x(),
				y,
				band.width(),
				context);
		if (IsRelatedArticlesHeader(block, next)) {
			laidOut.headerRect = QRect(
				left,
				y,
				std::max(width, 1),
				laidOut.outer.height()
					+ markdown.relatedArticle.headerPadding.top()
					+ markdown.relatedArticle.headerPadding.bottom());
			laidOut.outer = laidOut.headerRect;
			laidOut.contentRect = laidOut.headerRect;
		}
		y = BlockBottom(laidOut);
		blocks->push_back(std::move(laidOut));
		if (!anchorOnly) {
			previous = &block;
		}
	}
	return y;
}

} // namespace Iv::Markdown
