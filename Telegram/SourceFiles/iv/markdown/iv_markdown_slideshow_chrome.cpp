/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_slideshow_chrome.h"

#include "ui/image/image_prepare.h"
#include "styles/style_iv.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Iv::Markdown {

int MediaHeightForWidth(
		int width,
		int aspectWidth,
		int aspectHeight) {
	aspectWidth = std::max(aspectWidth, 1);
	aspectHeight = std::max(aspectHeight, 1);
	return std::max(
		int((int64(width) * aspectHeight + aspectWidth - 1) / aspectWidth),
		1);
}

QPainterPath RoundedRectPath(QRect rect, int radius) {
	auto path = QPainterPath();
	path.addRoundedRect(QRectF(rect), radius, radius);
	return path;
}

void PaintRoundButton(
		Painter &p,
		QRect rect,
		const style::color &bg,
		const style::icon &icon) {
	if (rect.isEmpty()) {
		return;
	}
	auto hq = PainterHighQualityEnabler(p);
	p.setPen(Qt::NoPen);
	p.setBrush(bg->c);
	p.drawEllipse(rect);
	icon.paintInCenter(p, rect);
}

QImage PrepareWithBlurredBackground(
		QSize outer,
		QSize inner,
		QImage large,
		QImage blurred) {
	const auto ratio = style::DevicePixelRatio();
	auto background = QImage(
		outer * ratio,
		QImage::Format_ARGB32_Premultiplied);
	background.setDevicePixelRatio(ratio);
	if (blurred.isNull()) {
		background.fill(Qt::black);
		if (large.isNull()) {
			return background;
		}
	}
	auto p = QPainter(&background);
	if (!blurred.isNull()) {
		auto cover = blurred.scaled(
			outer * ratio,
			Qt::KeepAspectRatioByExpanding,
			Qt::SmoothTransformation);
		if (cover.size() != outer * ratio) {
			cover = cover.copy(QRect(
				QPoint(
					(cover.width() - outer.width() * ratio) / 2,
					(cover.height() - outer.height() * ratio) / 2),
				outer * ratio));
		}
		cover = Images::Blur(std::move(cover), true);
		cover.setDevicePixelRatio(ratio);
		p.drawImage(QPoint(), cover);
		p.fillRect(QRect(QPoint(), outer), QColor(0, 0, 0, 48));
	}
	if (!large.isNull()) {
		auto image = large.scaled(
			inner * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		image.setDevicePixelRatio(ratio);
		p.drawImage(
			(outer.width() - inner.width()) / 2,
			(outer.height() - inner.height()) / 2,
			image);
	}
	return background;
}

int SlideshowFrameHeight(
		int width,
		int slideshowMinHeight,
		gsl::span<const QSize> slideOriginalSizes) {
	auto result = std::numeric_limits<int>::max();
	for (const auto &original : slideOriginalSizes) {
		result = std::min(
			result,
			MediaHeightForWidth(width, original.width(), original.height()));
	}
	if (result == std::numeric_limits<int>::max()) {
		result = std::max(slideshowMinHeight, 1);
	}
	return std::max(result, std::max(slideshowMinHeight, 1));
}

SlideshowNavRects ComputeSlideshowNavRects(
		QRect frame,
		int frameHeight,
		int navButtonSize,
		int navButtonSkip) {
	const auto availableWidth = std::max(
		(frame.width() - 2 * navButtonSkip) / 2,
		0);
	const auto size = std::min({
		navButtonSize,
		std::max(frameHeight, 0),
		availableWidth,
	});
	if (size <= 0) {
		return {};
	}
	const auto top = frame.y() + std::max((frameHeight - size) / 2, 0);
	return {
		.previous = QRect(
			frame.x() + navButtonSkip,
			top,
			size,
			size),
		.next = QRect(
			frame.x() + frame.width() - navButtonSkip - size,
			top,
			size,
			size),
	};
}

} // namespace Iv::Markdown
