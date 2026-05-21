/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_article_paint.h"
#include "iv/markdown/iv_markdown_article_text.h"
#include "ui/dynamic_image.h"
#include "ui/widgets/checkbox.h"

#include "styles/palette.h"
#include "styles/style_iv.h"
#include "styles/style_widgets.h"

#include <QtGui/QPainterPath>

#include <algorithm>
#include <cmath>

namespace Iv::Markdown {
namespace {

[[nodiscard]] QRectF CenterCropSourceRect(QSize source, QSize target) {
	const auto sourceWidth = std::max(source.width(), 1);
	const auto sourceHeight = std::max(source.height(), 1);
	const auto targetWidth = std::max(target.width(), 1);
	const auto targetHeight = std::max(target.height(), 1);
	const auto sourceRatio = float64(sourceWidth) / sourceHeight;
	const auto targetRatio = float64(targetWidth) / targetHeight;
	if (sourceRatio > targetRatio) {
		const auto width = sourceHeight * targetRatio;
		return QRectF(
			(sourceWidth - width) / 2.,
			0.,
			width,
			sourceHeight);
	}
	const auto height = sourceWidth / targetRatio;
	return QRectF(
		0.,
		(sourceHeight - height) / 2.,
		sourceWidth,
		height);
}

void PaintImageCenterCrop(Painter &p, QRect rect, const QImage &image) {
	p.drawImage(
		QRectF(rect),
		image,
		CenterCropSourceRect(image.size(), rect.size()));
}

[[nodiscard]] bool ImageCoversRect(const QImage &image, QRect rect) {
	const auto ratio = std::max(image.devicePixelRatio(), 1.);
	return (image.width() / ratio >= rect.width())
		&& (image.height() / ratio >= rect.height());
}

[[nodiscard]] bool PaintDynamicImage(
		Painter &p,
		const std::shared_ptr<Ui::DynamicImage> &image,
		QRect rect,
		bool requireCovering = false) {
	if (!image || rect.isEmpty()) {
		return false;
	}
	if (const auto frame = image->image(std::max(rect.width(), rect.height()));
		!frame.isNull()) {
		if (requireCovering && !ImageCoversRect(frame, rect)) {
			return false;
		}
		PaintImageCenterCrop(p, rect, frame);
		return true;
	}
	return false;
}

[[nodiscard]] bool PaintThumbnailImage(
		Painter &p,
		QRect rect,
		const std::shared_ptr<Ui::DynamicImage> &thumbnail,
		const std::shared_ptr<Ui::DynamicImage> &previousThumbnail) {
	return PaintDynamicImage(p, thumbnail, rect, true)
		|| PaintDynamicImage(p, previousThumbnail, rect, true)
		|| PaintDynamicImage(p, previousThumbnail, rect)
		|| PaintDynamicImage(p, thumbnail, rect);
}

void RefreshBlockThumbnail(
		const LaidOutBlock &block,
		const MarkdownArticlePaintCaches &caches) {
	if (!block.photoRuntime || block.thumbnailRect.isEmpty()) {
		return;
	}
	const auto size = block.thumbnailRect.size();
	if (size.isEmpty() || block.thumbnailRequestSize == size) {
		return;
	}
	block.thumbnailRequestSize = size;
	if (const auto image = block.photoRuntime->thumbnail(size)) {
		if (image != block.thumbnailImage) {
			block.previousThumbnailImage = std::move(block.thumbnailImage);
			block.thumbnailImage = image;
		}
		if (image != block.subscribedThumbnailImage) {
			block.subscribedThumbnailImage = image;
			const auto repaint = caches.repaint;
			const auto repaintRect = caches.repaintRect;
			const auto rect = block.mediaRect;
			image->subscribeToUpdates([repaint, repaintRect, rect] {
				if (repaintRect && !rect.isEmpty()) {
					repaintRect(rect);
				} else if (repaint) {
					repaint();
				}
			});
		}
	}
}

void PaintTextLeaf(
		Painter &p,
		const Ui::Text::String &leaf,
		const MarkdownArticlePaintCaches &caches,
		QRect rect,
		int width,
		QRect clip,
		style::align align = style::al_left,
		std::optional<TextSelection> selection = std::nullopt,
		int elisionLines = 0) {
	const auto availableWidth = std::max(width, 1);
	leaf.draw(p, {
		.position = rect.topLeft(),
		.availableWidth = availableWidth,
		.geometry = elisionLines
			? Ui::Text::SimpleGeometry(availableWidth, elisionLines, 0, true)
			: TextGeometry(availableWidth),
		.align = align,
		.clip = clip,
		.palette = &p.textPalette(),
		.pre = caches.pre,
		.blockquote = caches.blockquote,
		.colors = caches.colors,
		.spoiler = Ui::Text::DefaultSpoilerCache(),
		.now = crl::now(),
		.selection = selection.value_or(TextSelection()),
		.elisionLines = elisionLines,
	});
}

[[nodiscard]] std::optional<QColor> QuoteSupplementaryColor(
		const MarkdownArticlePaintCaches &caches) {
	if (!caches.blockquote) {
		return {};
	}
	return anim::color(
		caches.blockquote->bg,
		caches.blockquote->icon,
		0.75);
}

void SetTextLeafPen(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches) {
	p.setPen(!block.supplementary
		? markdown.textColor->c
		: caches.supplementaryColorOverride.value_or(
			markdown.supplementaryTextColor->c));
}

void PaintRelatedArticleTextLeaf(
		Painter &p,
		const Ui::Text::String &leaf,
		const MarkdownArticlePaintCaches &caches,
		QRect rect,
		int width,
		QRect clip,
		int elisionLines) {
	const auto textClip = clip.intersected(rect);
	if (textClip.isEmpty()) {
		return;
	}
	p.save();
	p.setClipRect(textClip, Qt::IntersectClip);
	PaintTextLeaf(
		p,
		leaf,
		caches,
		rect,
		width,
		textClip,
		style::al_left,
		std::nullopt,
		elisionLines);
	p.restore();
}

void PaintTaskMarker(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		int outerWidth) {
	const auto rect = block.markerRect;
	if (rect.isEmpty()) {
		return;
	}
	auto view = Ui::CheckView(
		markdown.list.taskCheck,
		block.taskState == TaskState::Checked);
	view.finishAnimating();
	view.paint(p, rect.left(), rect.top(), outerWidth);
}

void PaintBulletMarker(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown) {
	const auto radius = markdown.list.bulletRadius;
	if (radius <= 0) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	p.setBrush(markdown.list.bulletFg->c);
	p.drawEllipse(QPointF(block.markerCenter), radius, radius);
}

[[nodiscard]] const QImage &ColorizedDisplayFormulaImage(
		const LaidOutBlock &block,
		const RenderedFormula &formula,
		QColor color) {
	const auto size = formula.image.size();
	if (block.colorizedFormulaImage.isNull()
		|| (block.colorizedFormulaSize != size)
		|| (block.colorizedFormulaColor != color)) {
		block.colorizedFormulaImage = QImage(
			size,
			QImage::Format_ARGB32_Premultiplied);
		style::colorizeImage(
			formula.image,
			color,
			&block.colorizedFormulaImage,
			QRect(),
			QPoint(),
			true);
		block.colorizedFormulaColor = color;
		block.colorizedFormulaSize = size;
	}
	return block.colorizedFormulaImage;
}

[[nodiscard]] int TableBorder(
		const LaidOutBlock &block,
		const style::Markdown &markdown) {
	return block.tableBordered ? markdown.table.border : 0;
}

struct TableOwnershipSlot {
	const LaidOutTableCell *cell = nullptr;
};

using TableOwnershipGrid = std::vector<std::vector<TableOwnershipSlot>>;

[[nodiscard]] TableOwnershipGrid BuildTableOwnershipGrid(
		const LaidOutBlock &block) {
	const auto rowCount = int(block.tableRows.size());
	const auto columnCount = int(block.tableColumnWidths.size());
	auto result = TableOwnershipGrid(
		std::max(rowCount, 0),
		std::vector<TableOwnershipSlot>(std::max(columnCount, 0)));
	for (auto row = 0; row != rowCount; ++row) {
		for (const auto &cell : block.tableRows[row].cells) {
			const auto fromRow = std::clamp(row, 0, rowCount);
			const auto toRow = std::clamp(row + cell.rowspan, 0, rowCount);
			const auto fromColumn = std::clamp(cell.column, 0, columnCount);
			const auto toColumn = std::clamp(
				cell.column + cell.colspan,
				0,
				columnCount);
			for (auto currentRow = fromRow; currentRow != toRow; ++currentRow) {
				for (auto currentColumn = fromColumn;
					currentColumn != toColumn;
					++currentColumn) {
					result[currentRow][currentColumn].cell = &cell;
				}
			}
		}
	}
	return result;
}

[[nodiscard]] QPainterPath TableShapePath(
		const LaidOutBlock &block,
		int border,
		int radius) {
	auto path = QPainterPath();
	if (block.tableRect.isEmpty()) {
		return path;
	}
	if (border > 0) {
		const auto half = border / 2.;
		path.addRoundedRect(
			QRectF(block.tableRect).marginsRemoved({
				half,
				half,
				half,
				half,
			}),
			radius,
			radius);
	} else {
		path.addRect(block.tableRect);
	}
	return path;
}

void PaintTableBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	if (!block.textRect.isEmpty()) {
		SetTextLeafPen(p, block, markdown, caches);
		PaintTextLeaf(
			p,
			block.leaf,
			caches,
			block.textRect,
			block.textWidth,
			clip,
			style::al_left,
			TextSelectionForSegmentIndex(
				selectionState,
				block.secondarySegmentIndex));
	}

	const auto tableClip = clip.intersected(block.visibleTableRect);
	if (tableClip.isEmpty()) {
		return;
	}

	const auto border = TableBorder(block, markdown);
	const auto radius = st::defaultTable.radius;
	const auto half = border / 2.;
	const auto shapePath = TableShapePath(block, border, radius);

	p.save();
	p.setClipRect(tableClip);
	p.save();
	p.setClipPath(shapePath, Qt::IntersectClip);
	for (auto rowIndex = 0, rowCount = int(block.tableRows.size()); rowIndex != rowCount; ++rowIndex) {
		const auto striped = block.tableStriped && ((rowIndex % 2) == 0);
		for (const auto &cell : block.tableRows[rowIndex].cells) {
			if (!cell.outer.intersects(block.visibleTableRect)) {
				continue;
			}
			if (!cell.header && !striped) {
				continue;
			}
			p.fillRect(cell.outer, markdown.table.headerBg->c);
		}
	}
	p.restore();

	if (border > 0 && !block.tableRect.isEmpty()) {
		const auto ownership = BuildTableOwnershipGrid(block);
		const auto rowCount = int(ownership.size());
		const auto columnCount = rowCount
			? int(ownership.front().size())
			: int(block.tableColumnWidths.size());
		auto columnLefts = std::vector<int>(columnCount, block.tableRect.x() + border);
		auto separatorLeft = block.tableRect.x() + border;
		for (auto column = 0; column != columnCount; ++column) {
			columnLefts[column] = separatorLeft;
			separatorLeft += block.tableColumnWidths[column] + border;
		}
		const auto inner = QRectF(block.tableRect).marginsRemoved({
			half,
			half,
			half,
			half,
		});
		auto path = shapePath;
		for (auto boundaryRow = 1; boundaryRow != rowCount; ++boundaryRow) {
			auto segmentStart = -1;
			for (auto column = 0; column != columnCount; ++column) {
				const auto split = ownership[boundaryRow - 1][column].cell
					!= ownership[boundaryRow][column].cell;
				if (split && (segmentStart < 0)) {
					segmentStart = column;
				} else if (!split && (segmentStart >= 0)) {
					const auto fromX = (segmentStart == 0)
						? inner.x()
						: (columnLefts[segmentStart] - half);
					const auto toX = columnLefts[column] - half;
					const auto y = block.tableRows[boundaryRow].outer.y() - half;
					path.moveTo(fromX, y);
					path.lineTo(toX, y);
					segmentStart = -1;
				}
			}
			if (segmentStart >= 0) {
				const auto fromX = (segmentStart == 0)
					? inner.x()
					: (columnLefts[segmentStart] - half);
				const auto y = block.tableRows[boundaryRow].outer.y() - half;
				path.moveTo(fromX, y);
				path.lineTo(inner.x() + inner.width(), y);
			}
		}
		for (auto boundaryColumn = 1; boundaryColumn != columnCount; ++boundaryColumn) {
			auto segmentStart = -1;
			for (auto row = 0; row != rowCount; ++row) {
				const auto split = ownership[row][boundaryColumn - 1].cell
					!= ownership[row][boundaryColumn].cell;
				if (split && (segmentStart < 0)) {
					segmentStart = row;
				} else if (!split && (segmentStart >= 0)) {
					const auto fromY = (segmentStart == 0)
						? inner.y()
						: (block.tableRows[segmentStart].outer.y() - half);
					const auto toY = block.tableRows[row].outer.y() - half;
					const auto x = columnLefts[boundaryColumn] - half;
					path.moveTo(x, fromY);
					path.lineTo(x, toY);
					segmentStart = -1;
				}
			}
			if (segmentStart >= 0) {
				const auto fromY = (segmentStart == 0)
					? inner.y()
					: (block.tableRows[segmentStart].outer.y() - half);
				const auto x = columnLefts[boundaryColumn] - half;
				path.moveTo(x, fromY);
				path.lineTo(x, inner.y() + inner.height());
			}
		}

		auto hq = PainterHighQualityEnabler(p);
		auto pen = markdown.table.borderFg->p;
		pen.setWidth(border);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawPath(path);
	}

	p.setPen(markdown.textColor->c);
	for (const auto &row : block.tableRows) {
		if (!row.outer.intersects(block.visibleTableRect)) {
			continue;
		}
		for (const auto &cell : row.cells) {
			if (!cell.textRect.intersects(block.visibleTableRect)) {
				continue;
			}
			PaintTextLeaf(
				p,
				cell.leaf,
				caches,
				cell.textRect,
				cell.textWidth,
				tableClip,
				cell.align,
				TextSelectionForSegmentIndex(
					selectionState,
					cell.segmentIndex));
		}
	}

	if (block.segmentIndex >= 0
		&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
		p.save();
		p.setClipPath(shapePath, Qt::IntersectClip);
		p.fillRect(block.tableRect, p.textPalette().selectOverlay);
		p.restore();
	}

	if (block.overflowed) {
		const auto indicatorWidth = std::min(
			std::max(markdown.table.overflowWidth, 1),
			block.visibleTableRect.width());
		p.save();
		p.setClipPath(shapePath, Qt::IntersectClip);
		p.fillRect(
			QRect(
				block.visibleTableRect.x()
					+ block.visibleTableRect.width()
					- indicatorWidth,
				block.visibleTableRect.y(),
				indicatorWidth,
				block.visibleTableRect.height()),
			markdown.table.overflowFg->c);
		p.restore();
	}

	p.restore();
}

void PaintDisplayMathBlock(
		Painter &p,
		const LaidOutBlock &block,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		int devicePixelRatio,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto formulaClip = clip.intersected(block.visibleFormulaRect);
	if (formulaClip.isEmpty()) {
		return;
	}

	p.save();
	p.setClipRect(formulaClip);

	const auto formula = PreparedFormulaFor(formulas, block.formulaIndex);
	p.setPen(markdown.textColor->c);
	const auto rendered = EnsureFormulaRendered(
		formula,
		FormulaRasterSlot(renderedFormulas, block.formulaIndex),
		renderer,
		devicePixelRatio);
	if (rendered.success) {
		p.drawImage(
			block.formulaRect.topLeft(),
			ColorizedDisplayFormulaImage(
				block,
				rendered,
				p.pen().color()));
	}
	if (!rendered.success) {
		const auto radius = markdown.displayMath.fallbackRadius;
		p.setPen(Qt::NoPen);
		p.setBrush(markdown.displayMath.fallbackBg->c);
		if (radius > 0) {
			auto hq = PainterHighQualityEnabler(p);
			p.drawRoundedRect(block.formulaRect, radius, radius);
		} else {
			p.fillRect(block.formulaRect, markdown.displayMath.fallbackBg->c);
		}
		p.setPen(markdown.textColor->c);
		PaintTextLeaf(
			p,
			block.fallbackLeaf,
			caches,
			block.textRect,
			block.textWidth,
			formulaClip);
	}

	if (block.segmentIndex >= 0
		&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
		p.fillRect(block.visibleFormulaRect, p.textPalette().selectOverlay);
	}

	if (block.overflowed) {
		const auto indicatorWidth = std::min(
			std::max(markdown.displayMath.overflowWidth, 1),
			block.visibleFormulaRect.width());
		p.fillRect(
			QRect(
				block.visibleFormulaRect.x()
					+ block.visibleFormulaRect.width()
					- indicatorWidth,
				block.visibleFormulaRect.y(),
				indicatorWidth,
				block.visibleFormulaRect.height()),
			markdown.displayMath.overflowFg->c);
	}

	p.restore();
}

void PaintQuoteBlock(
		Painter &p,
		const LaidOutBlock &block,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		int devicePixelRatio,
		int outerWidth,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto quoteClip = clip.intersected(block.outer);
	if (quoteClip.isEmpty()) {
		return;
	}

	if (caches.blockquote) {
		const auto &quoteStyle = markdown.body.blockquote;
		Ui::Text::ValidateQuotePaintCache(*caches.blockquote, quoteStyle);

		p.save();
		p.setClipRect(quoteClip);
		Ui::Text::FillQuotePaint(
			p,
			block.outer,
			*caches.blockquote,
			quoteStyle);
		p.restore();
	}

	auto overriden = caches;
	overriden.supplementaryColorOverride = QuoteSupplementaryColor(caches);
	PaintBlocks(
		p,
		block.children,
		formulas,
		renderedFormulas,
		renderer,
		devicePixelRatio,
		outerWidth,
		markdown,
		overriden,
		selectionState,
		clip.intersected(block.contentRect));
}

void PaintPlaceholderBlock(
		Painter &p,
		const LaidOutBlock &block,
		int outerWidth,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto visible = clip.intersected(block.visibleMediaRect);
	if (!visible.isEmpty()) {
		p.save();
		p.setClipRect(visible);
		auto hq = PainterHighQualityEnabler(p);
		if (block.activation.kind == MediaActivationKind::Embed
			&& block.placeholderRuntime) {
			const auto border = markdown.placeholder.border;
			const auto radius = markdown.placeholder.radius;
			const auto borderSkip = border / 2;
			const auto borderRect = block.mediaRect.marginsRemoved(QMargins(
				borderSkip,
				borderSkip,
				borderSkip,
				borderSkip));
			const auto active = ClickHandler::showAsActive(
				block.placeholderRuntime->clickHandler);
			const auto pressed = ClickHandler::showAsPressed(
				block.placeholderRuntime->clickHandler);
			if (active || pressed) {
				p.setPen(Qt::NoPen);
				p.setBrush(st::windowBgOver);
				p.drawRoundedRect(block.mediaRect, radius, radius);
			}
			if (const auto &ripple = block.placeholderRuntime->ripple) {
				ripple->paint(
					p,
					block.mediaRect.x(),
					block.mediaRect.y(),
					outerWidth,
					&st::windowBgRipple->c);
			}
			auto pen = QPen(st::windowActiveTextFg->c);
			pen.setWidth(border);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(borderRect, radius, radius);
			if (block.placeholderRuntime->loading) {
				const auto size = QSize(
					markdown.placeholder.spinnerSize,
					markdown.placeholder.spinnerSize);
				const auto spinner = style::centerrect(
					block.mediaRect,
					QRect(QPoint(), size));
				Ui::InfiniteRadialAnimation::Draw(
					p,
					block.placeholderRuntime->loadingAnimation.computeState(),
					spinner.topLeft(),
					spinner.size(),
					outerWidth,
					QPen(st::windowActiveTextFg->c),
					markdown.placeholder.spinnerWidth);
			} else {
				p.setPen(st::windowActiveTextFg->c);
				PaintTextLeaf(
					p,
					block.labelLeaf,
					caches,
					block.labelRect,
					block.labelWidth,
					visible,
					style::al_center);
			}
		} else {
			const auto max = block.labelLeaf.maxWidth();
			const auto radius = markdown.placeholder.padding.left();
			p.setBrush(st::windowBgOver);
			p.setPen(Qt::NoPen);
			const auto skip = (max < block.labelRect.width())
				? ((block.labelRect.width() - max) / 2)
				: 0;
			p.drawRoundedRect(
				block.labelRect.marginsRemoved(
					{ skip, 0, skip, 0 }
				).marginsAdded(markdown.placeholder.padding),
				radius,
				radius);
			p.setPen(st::windowSubTextFg->c);
			PaintTextLeaf(
				p,
				block.labelLeaf,
				caches,
				block.labelRect,
				block.labelWidth,
				visible,
				style::al_center);
		}
		if (block.segmentIndex >= 0
			&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
			p.fillRect(block.visibleMediaRect, p.textPalette().selectOverlay);
		}
		p.restore();
	}
	if (!block.textRect.isEmpty()) {
		SetTextLeafPen(p, block, markdown, caches);
		PaintTextLeaf(
			p,
			block.leaf,
			caches,
			block.textRect,
			block.textWidth,
			clip,
			style::al_left,
			TextSelectionForSegmentIndex(
				selectionState,
				block.secondarySegmentIndex));
	}
}

[[nodiscard]] QPainterPath RoundedRectPath(QRect rect, int radius) {
	auto path = QPainterPath();
	path.addRoundedRect(QRectF(rect), radius, radius);
	return path;
}

void PaintEmbedPostBlock(
		Painter &p,
		const LaidOutBlock &block,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		int devicePixelRatio,
		int outerWidth,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto mediaClip = clip.intersected(block.mediaRect);
	const auto &style = markdown.embedPost;
	if (!mediaClip.isEmpty()) {
		RefreshBlockThumbnail(block, caches);

		p.save();
		p.setClipRect(mediaClip);
		if (style.accentWidth > 0) {
			p.fillRect(
				QRect(
					block.mediaRect.x(),
					block.mediaRect.y(),
					style.accentWidth,
					block.mediaRect.height()),
				style.accentFg->c);
		}
		if (block.photoRuntime && !block.thumbnailRect.isEmpty()) {
			auto hq = PainterHighQualityEnabler(p);
			const auto avatarPath = RoundedRectPath(
				block.thumbnailRect,
				style.avatarRadius);
			p.fillPath(avatarPath, st::windowBg->c);
			p.save();
			p.setClipPath(
				avatarPath,
				Qt::IntersectClip);
			(void)PaintThumbnailImage(
				p,
				block.thumbnailRect,
				block.thumbnailImage,
				block.previousThumbnailImage);
			p.restore();
		}
		if (!block.labelRect.isEmpty()) {
			p.setPen(style.authorFg->c);
			PaintTextLeaf(
				p,
				block.labelLeaf,
				caches,
				block.labelRect,
				block.labelWidth,
				mediaClip,
				style::al_left,
				TextSelectionForSegmentIndex(
					selectionState,
					block.segmentIndex));
		}
		if (!block.subtitleRect.isEmpty()) {
			p.setPen(style.dateFg->c);
			PaintTextLeaf(
				p,
				block.subtitleLeaf,
				caches,
				block.subtitleRect,
				block.subtitleWidth,
				mediaClip,
				style::al_left,
				TextSelectionForSegmentIndex(
					selectionState,
					block.secondarySegmentIndex));
		}
		p.restore();
	}
	if (!block.bodyRect.isEmpty()) {
		PaintBlocks(
			p,
			block.children,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip.intersected(block.bodyRect));
	}
	if (!block.textRect.isEmpty()) {
		SetTextLeafPen(p, block, markdown, caches);
		PaintTextLeaf(
			p,
			block.leaf,
			caches,
			block.textRect,
			block.textWidth,
			clip,
			style::al_left,
			TextSelectionForSegmentIndex(
				selectionState,
				block.tertiarySegmentIndex));
	}
}

void PaintMediaCaption(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	if (block.textRect.isEmpty()) {
		return;
	}
	SetTextLeafPen(p, block, markdown, caches);
	PaintTextLeaf(
		p,
		block.leaf,
		caches,
		block.textRect,
		block.textWidth,
		clip,
		style::al_left,
		TextSelectionForSegmentIndex(
			selectionState,
			block.secondarySegmentIndex));
}

void PaintPersistentMediaBlock(
		Painter &p,
		const LaidOutBlock &block,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	if (!block.mediaBlock) {
		return;
	}
	block.mediaBlock->paint(p, clip, caches);
	if (block.segmentIndex >= 0
		&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
		const auto visible = clip.intersected(block.visibleMediaRect);
		if (!visible.isEmpty()) {
			p.save();
			p.setClipRect(visible);
			p.fillRect(block.mediaRect, p.textPalette().selectOverlay);
			p.restore();
		}
	}
}

void PaintCardSurface(
		Painter &p,
		QRect rect,
		int border,
		const style::color &borderFg,
		const style::color &bg,
		int radius) {
	if (rect.isEmpty()) {
		return;
	}
	if (border <= 0) {
		p.fillRect(rect, bg->c);
		return;
	}
	const auto half = border / 2.;
	const auto inner = QRectF(rect).marginsRemoved({
		half,
		half,
		half,
		half,
	});
	if (radius > 0) {
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(QPen(borderFg->c, border));
		p.setBrush(bg->c);
		p.drawRoundedRect(inner, radius, radius);
	} else {
		p.fillRect(rect, bg->c);
		p.setPen(QPen(borderFg->c, border));
		p.setBrush(Qt::NoBrush);
		p.drawRect(inner);
	}
}

void PaintAudioBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	PaintPersistentMediaBlock(p, block, caches, selectionState, clip);
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintChannelBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	PaintPersistentMediaBlock(p, block, caches, selectionState, clip);
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintRelatedArticleBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto visible = clip.intersected(block.visibleMediaRect);
	if (visible.isEmpty()) {
		return;
	}
	const auto &style = markdown.relatedArticle;
	RefreshBlockThumbnail(block, caches);

	p.save();
	p.setClipRect(visible);
	PaintCardSurface(
		p,
		block.mediaRect,
		style.border,
		style.borderFg,
		style.bg,
		style.radius);
	if (!block.thumbnailRect.isEmpty()) {
		p.fillRect(block.thumbnailRect, st::windowBg->c);
		if (style.thumbnailRadius > 0) {
			auto hq = PainterHighQualityEnabler(p);
			auto path = RoundedRectPath(block.thumbnailRect, style.thumbnailRadius);
			p.save();
			p.setClipPath(path, Qt::IntersectClip);
			(void)PaintThumbnailImage(
				p,
				block.thumbnailRect,
				block.thumbnailImage,
				block.previousThumbnailImage);
			p.restore();
		} else {
			(void)PaintThumbnailImage(
				p,
				block.thumbnailRect,
				block.thumbnailImage,
				block.previousThumbnailImage);
		}
	}
	if (!block.labelRect.isEmpty()) {
		p.setPen(markdown.textColor->c);
		PaintRelatedArticleTextLeaf(
			p,
			block.labelLeaf,
			caches,
			block.labelRect,
			block.labelWidth,
			visible,
			style.titleLines);
	}
	if (!block.subtitleRect.isEmpty()) {
		p.setPen(markdown.textColor->c);
		PaintRelatedArticleTextLeaf(
			p,
			block.subtitleLeaf,
			caches,
			block.subtitleRect,
			block.subtitleWidth,
			visible,
			style.subtitleLines);
	}
	if (!block.actionRect.isEmpty()) {
		p.setPen(markdown.supplementaryTextColor->c);
		PaintRelatedArticleTextLeaf(
			p,
			block.actionLeaf,
			caches,
			block.actionRect,
			block.actionWidth,
			visible,
			style.footerLines);
	}
	if (block.segmentIndex >= 0
		&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
		p.fillRect(block.visibleMediaRect, p.textPalette().selectOverlay);
	}
	if (style.separator > 0) {
		p.fillRect(
			QRect(
				block.mediaRect.x(),
				block.mediaRect.y() + block.mediaRect.height() - style.separator,
				block.mediaRect.width(),
				style.separator),
			style.separatorFg->c);
	}
	p.restore();
}

void PaintPhotoBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	PaintPersistentMediaBlock(p, block, caches, selectionState, clip);
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintVideoBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	PaintPersistentMediaBlock(p, block, caches, selectionState, clip);
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintMapBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	PaintPersistentMediaBlock(p, block, caches, selectionState, clip);
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintGroupedMediaBlock(
		Painter &p,
		const LaidOutBlock &block,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	if (block.mediaBlock) {
		block.mediaBlock->paint(p, clip, caches);
		if ((block.segmentIndex >= 0)
			&& WholeSegmentSelected(selectionState, block.segmentIndex)) {
			const auto visible = clip.intersected(block.visibleMediaRect);
			if (!visible.isEmpty()) {
				const auto &style = markdown.groupedMedia;
				auto overlay = p.textPalette().selectOverlay->c;
				overlay.setAlphaF(std::clamp(style.overlayOpacity, 0., 1.));
				p.save();
				p.setClipRect(visible);
				p.fillPath(RoundedRectPath(block.mediaRect, style.radius), overlay);
				p.restore();
			}
		}
		PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
		return;
	}
	PaintMediaCaption(p, block, markdown, caches, selectionState, clip);
}

void PaintDetailsBlock(
		Painter &p,
		const LaidOutBlock &block,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		int devicePixelRatio,
		int outerWidth,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	const auto visible = clip.intersected(block.outer);
	if (visible.isEmpty()) {
		return;
	}

	const auto &details = markdown.details;
	const auto half = details.border / 2.;
	const auto outer = QRectF(block.outer).marginsRemoved({
		half,
		half,
		half,
		half,
	});
	auto outerPath = QPainterPath();
	outerPath.addRoundedRect(outer, details.radius, details.radius);

	p.save();
	p.setClipRect(visible);
	{
		auto hq = PainterHighQualityEnabler(p);
		p.fillPath(
			outerPath,
			(block.bodyRect.isEmpty() ? details.headerBg : details.bodyBg)->c);
		p.save();
		p.setClipPath(outerPath, Qt::IntersectClip);
		p.fillRect(block.headerRect, details.headerBg->c);
		if (!block.bodyRect.isEmpty()) {
			p.setPen(QPen(details.borderFg->c, details.border));
			const auto separatorY = block.bodyRect.top() + half;
			p.drawLine(
				QPointF(outer.left(), separatorY),
				QPointF(outer.right(), separatorY));
		}
		p.restore();

		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(details.borderFg->c, details.border));
		p.drawPath(outerPath);
	}
	if (!block.iconRect.isEmpty()) {
		details.icon.paint(
			p,
			block.iconRect.x(),
			block.iconRect.y(),
			outerWidth);
	}
	p.setPen(details.summaryFg->c);
	PaintTextLeaf(
		p,
		block.leaf,
		caches,
		block.textRect,
		block.textWidth,
		clip,
		style::al_left,
		TextSelectionForSegmentIndex(
			selectionState,
			block.segmentIndex));
	p.restore();

	if (!block.bodyRect.isEmpty()) {
		PaintBlocks(
			p,
			block.children,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip.intersected(block.bodyRect));
	}
}

void PaintBlock(
		Painter &p,
		const LaidOutBlock &block,
		std::vector<PreparedFormulaSlot> *formulas,
		std::vector<RenderedFormula> *renderedFormulas,
		MathRenderer *renderer,
		int devicePixelRatio,
		int outerWidth,
		const style::Markdown &markdown,
		const MarkdownArticlePaintCaches &caches,
		const PaintSelectionState &selectionState,
		QRect clip) {
	if (!block.outer.intersects(clip)) {
		return;
	}

	switch (block.kind) {
	case PreparedBlockKind::Paragraph:
	case PreparedBlockKind::Heading:
		if (!block.headerRect.isEmpty()) {
			p.fillRect(block.headerRect, markdown.relatedArticle.headerBg->c);
		}
		SetTextLeafPen(p, block, markdown, caches);
		PaintTextLeaf(
			p,
			block.leaf,
			caches,
			block.textRect,
			block.textWidth,
			clip,
			style::al_left,
			TextSelectionForSegmentIndex(
				selectionState,
				block.segmentIndex));
		break;
	case PreparedBlockKind::CodeBlock:
		p.setPen(markdown.textColor->c);
		PaintTextLeaf(
			p,
			block.leaf,
			caches,
			block.textRect,
			block.textWidth,
			clip,
			style::al_left,
			TextSelectionForSegmentIndex(
				selectionState,
				block.segmentIndex));
		break;
	case PreparedBlockKind::Rule:
		p.fillRect(block.outer, st::defaultTable.borderFg->c);
		break;
	case PreparedBlockKind::List:
		PaintBlocks(
			p,
			block.children,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::ListItem:
		if (block.taskState != TaskState::None) {
			PaintTaskMarker(p, block, markdown, outerWidth);
		} else if (block.listKind == ListKind::Ordered
			&& !block.markerRect.isEmpty()) {
			p.setPen(markdown.textColor->c);
			PaintTextLeaf(
				p,
				block.marker,
				caches,
				block.markerRect,
				block.markerWidth,
				clip);
		} else if (block.listKind == ListKind::Bullet) {
			PaintBulletMarker(p, block, markdown);
		}
		PaintBlocks(
			p,
			block.children,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Quote:
		PaintQuoteBlock(
			p,
			block,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::DisplayMath:
		PaintDisplayMathBlock(
			p,
			block,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Table:
		PaintTableBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Photo:
		PaintPhotoBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Video:
		PaintVideoBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Audio:
		PaintAudioBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Map:
		PaintMapBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Channel:
		PaintChannelBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::RelatedArticle:
		PaintRelatedArticleBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::EmbedPost:
		PaintEmbedPostBlock(
			p,
			block,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Placeholder:
		PaintPlaceholderBlock(
			p,
			block,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::GroupedMedia:
		PaintGroupedMediaBlock(
			p,
			block,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	case PreparedBlockKind::Details:
		PaintDetailsBlock(
			p,
			block,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
		break;
	}
}

} // namespace

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
		QRect clip) {
	for (const auto &block : blocks) {
		if (block.outer.bottom() < clip.top()) {
			continue;
		} else if (block.outer.top() > clip.bottom()) {
			break;
		}
		PaintBlock(
			p,
			block,
			formulas,
			renderedFormulas,
			renderer,
			devicePixelRatio,
			outerWidth,
			markdown,
			caches,
			selectionState,
			clip);
	}
}

} // namespace Iv::Markdown
