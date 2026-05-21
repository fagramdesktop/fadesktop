/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_common.h"
#include "iv/markdown/iv_markdown_media_block.h"
#include "iv/markdown/iv_markdown_prepare.h"

#include "spellcheck/spellcheck_highlight_syntax.h"
#include "ui/click_handler.h"
#include "ui/effects/radial_animation.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "ui/text/text.h"

#include <memory>
#include <optional>
#include <span>

namespace Iv::Markdown {

struct PlaceholderBlockRuntime {
	explicit PlaceholderBlockRuntime(Fn<void()> repaint);

	ClickHandlerPtr clickHandler;
	bool loading = false;
	Ui::InfiniteRadialAnimation loadingAnimation;
	std::unique_ptr<Ui::RippleAnimation> ripple;
	QSize rippleSize;
};

struct MarkdownArticlePaintCaches {
	Ui::Text::QuotePaintCache *pre = nullptr;
	Ui::Text::QuotePaintCache *blockquote = nullptr;
	std::span<Ui::Text::SpecialColor> colors;
	Fn<void()> repaint;
	Fn<void(QRect)> repaintRect;
	std::optional<QColor> supplementaryColorOverride;
};

struct MarkdownArticleHitTestResult {
	int segmentIndex = -1;
	Ui::Text::StateResult state;
	std::optional<PreparedLink> preparedLink;
	MediaActivation mediaActivation;
	QPoint placeholderLocalPoint;
	int forcedOffset = -1;
	bool direct = false;

	[[nodiscard]] bool valid() const {
		return (segmentIndex >= 0);
	}
};

struct MarkdownArticleSelectionPosition {
	int segment = -1;
	int offset = 0;

	[[nodiscard]] bool valid() const {
		return (segment >= 0);
	}
};

inline bool operator==(
		MarkdownArticleSelectionPosition a,
		MarkdownArticleSelectionPosition b) {
	return (a.segment == b.segment) && (a.offset == b.offset);
}

inline bool operator!=(
		MarkdownArticleSelectionPosition a,
		MarkdownArticleSelectionPosition b) {
	return !(a == b);
}

struct MarkdownArticleSelection {
	MarkdownArticleSelectionPosition from;
	MarkdownArticleSelectionPosition to;

	[[nodiscard]] bool empty() const;
};

inline bool MarkdownArticleSelection::empty() const {
	return !from.valid()
		|| !to.valid()
		|| (from == to);
}

inline bool operator==(
		MarkdownArticleSelection a,
		MarkdownArticleSelection b) {
	return (a.from == b.from) && (a.to == b.to);
}

inline bool operator!=(
		MarkdownArticleSelection a,
		MarkdownArticleSelection b) {
	return !(a == b);
}

struct MarkdownArticleSelectionEndpoint {
	int segment = -1;
	bool direct = false;

	[[nodiscard]] bool valid() const {
		return (segment >= 0);
	}
};

struct MarkdownArticleSelectionEndpoints {
	MarkdownArticleSelectionEndpoint from;
	MarkdownArticleSelectionEndpoint to;
};

class MarkdownArticle {
public:
	explicit MarkdownArticle(std::shared_ptr<MathRenderer> renderer = nullptr);
	~MarkdownArticle();

	MarkdownArticle(MarkdownArticle &&) noexcept;
	MarkdownArticle &operator=(MarkdownArticle &&) noexcept;

	void setRenderer(std::shared_ptr<MathRenderer> renderer);
	void setMediaBlockHost(MediaBlockHost *host);
	void setTextRepaintCallbacks(
		Fn<void()> repaint,
		Fn<void(QRect)> repaintRect);
	void setContent(MarkdownArticleContent content);
	void invalidateLayout();
	[[nodiscard]] int maxWidth() const;
	[[nodiscard]] int resizeGetHeight(int width);
	void setVisibleTopBottom(int visibleTop, int visibleBottom);
	void paint(
		Painter &p,
		QRect clip,
		MarkdownArticlePaintCaches caches,
		MarkdownArticleSelection selection = {},
		const MarkdownArticleSelectionEndpoints *endpoints = nullptr) const;
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

private:
	class Impl;

	std::unique_ptr<Impl> _impl;

};

} // namespace Iv::Markdown
