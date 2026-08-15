/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_slider.h"

#include "base/timer.h"
#include "ui/painter.h"
#include "styles/style_basic.h"
#include "styles/style_widgets.h"

namespace FA::Ui {

MaterialSlider::MaterialSlider(QWidget *parent)
: ContinuousSlider(parent) {
	resize(width(), 32);
}

int MaterialSlider::resizeGetHeight(int newWidth) {
	return 32;
}

QSize MaterialSlider::getSeekDecreaseSize() const {
	if (!_alwaysDisplayMarker) {
		return QSize();
	}
	const auto trackThickness = 16;
	return QSize(trackThickness, trackThickness);
}

float64 MaterialSlider::getOverDuration() const {
	return 150.0;
}

void MaterialSlider::addDivider(float64 atValue) {
	_dividers.push_back(atValue);
	update();
}

void MaterialSlider::clearDividers() {
	_dividers.clear();
	update();
}

void MaterialSlider::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	p.setPen(Qt::NoPen);
	p.setOpacity(fadeOpacity());

	const auto horizontal = isHorizontal();
	const auto trackThickness = 16.0;
	const auto radius = trackThickness / 2.0;
	const auto disabled = isDisabled();
	const auto over = getCurrentOverFactor();

	const auto value = horizontal
		? getCurrentValue()
		: (1. - getCurrentValue());

	const auto from = 0.0;
	const auto length = float64(horizontal ? width() : height());

	const auto handleThickness = 4.0;
	const auto handleLength = 28.0;
	const auto seekPadding = _alwaysDisplayMarker ? radius : (radius * over);
	const auto mid = from + seekPadding + value * (length - 2.0 * seekPadding);

	const auto activeFg = disabled
		? st::windowSubTextFg->c
		: st::windowActiveTextFg->c;
	const auto inactiveFg = disabled
		? st::windowBg->c
		: st::sliderBgInactive->c;
	const auto seekFg = activeFg;

	const auto markerSizeRatio = disabled
		? 0.0
		: (_alwaysDisplayMarker ? 1.0 : over);

	const auto gap = 4.0;
	const auto effectiveGap = gap * markerSizeRatio;
	const auto effectiveHandleThickness = handleThickness * markerSizeRatio;

	if (horizontal) {
		const auto trackY = (height() - trackThickness) / 2.0;
		const auto activeEnd = mid - (effectiveHandleThickness / 2.0) - effectiveGap;
		const auto inactiveStart = mid + (effectiveHandleThickness / 2.0) + effectiveGap;

		if (value <= 0.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawRoundedRect(QRectF(from, trackY, length, trackThickness), radius, radius);
		} else if (value >= 1.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawRoundedRect(QRectF(from, trackY, length, trackThickness), radius, radius);
		} else {
			if (activeEnd > from) {
				const auto x = from;
				const auto y = trackY;
				const auto w = activeEnd - from;
				const auto h = trackThickness;
				const auto rLeft = h / 2.0;
				const auto rRight = (inactiveStart >= length) ? (h / 2.0) : 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(activeFg);
				if (w <= rLeft || (rLeft == rRight && w <= h)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rLeft, y);
					path.lineTo(x + w - rRight, y);
					path.quadTo(x + w, y, x + w, y + rRight);
					path.lineTo(x + w, y + h - rRight);
					path.quadTo(x + w, y + h, x + w - rRight, y + h);
					path.lineTo(x + rLeft, y + h);
					path.quadTo(x, y + h, x, y + h - rLeft);
					path.lineTo(x + rLeft, y);
					path.quadTo(x, y, x + rLeft, y);
					p.drawPath(path);
				}
			}

			if (inactiveStart < length) {
				const auto x = inactiveStart;
				const auto y = trackY;
				const auto w = length - inactiveStart;
				const auto h = trackThickness;
				const auto rLeft = (activeEnd <= from) ? (h / 2.0) : 2.0;
				const auto rRight = h / 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(inactiveFg);
				if (w <= rRight || (rLeft == rRight && w <= h)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rLeft, y);
					path.lineTo(x + w - rRight, y);
					path.quadTo(x + w, y, x + w, y + rRight);
					path.lineTo(x + w, y + h - rRight);
					path.quadTo(x + w, y + h, x + w - rRight, y + h);
					path.lineTo(x + rLeft, y + h);
					path.quadTo(x, y + h, x, y + h - rLeft);
					path.lineTo(x, y + rLeft);
					path.quadTo(x, y, x + rLeft, y);
					p.drawPath(path);
				}
			}
		}

		const auto dotCenterX = length - radius;
		const auto dotCenterY = trackY + radius;
		const auto dotRadius = 2.0;
		if (value < 0.95 && inactiveStart + 4.0 < dotCenterX - dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}

		if (value > 0.05 && from + radius + dotRadius < activeEnd - 4.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawEllipse(QPointF(from + radius, dotCenterY), dotRadius, dotRadius);
		}

		if (markerSizeRatio > 0.0) {
			const auto hW = std::max(effectiveHandleThickness, 1.0);
			const auto hH = handleLength * markerSizeRatio;
			const auto hX = mid - hW / 2.0;
			const auto hY = (height() - hH) / 2.0;
			const auto hRadius = hW / 2.0;
			p.setPen(Qt::NoPen);
			p.setBrush(seekFg);
			p.drawRoundedRect(QRectF(hX, hY, hW, hH), hRadius, hRadius);
		}
	} else {
		const auto trackX = (width() - trackThickness) / 2.0;
		const auto inactiveEnd = mid - (effectiveHandleThickness / 2.0) - effectiveGap;
		const auto activeStart = mid + (effectiveHandleThickness / 2.0) + effectiveGap;

		if (value <= 0.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawRoundedRect(QRectF(trackX, from, trackThickness, length), radius, radius);
		} else if (value >= 1.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawRoundedRect(QRectF(trackX, from, trackThickness, length), radius, radius);
		} else {
			if (inactiveEnd > from) {
				const auto x = trackX;
				const auto y = from;
				const auto w = trackThickness;
				const auto h = inactiveEnd - from;
				const auto rTop = w / 2.0;
				const auto rBottom = (activeStart >= length) ? (w / 2.0) : 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(inactiveFg);
				if (h <= rTop || (rTop == rBottom && h <= w)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rTop, y);
					path.quadTo(x + w, y, x + w, y + rTop);
					path.lineTo(x + w, y + h - rBottom);
					path.quadTo(x + w, y + h, x + w - rBottom, y + h);
					path.lineTo(x + rBottom, y + h);
					path.quadTo(x, y + h, x, y + h - rBottom);
					path.lineTo(x, y + rTop);
					path.quadTo(x, y, x + rTop, y);
					p.drawPath(path);
				}
			}

			if (activeStart < length) {
				const auto x = trackX;
				const auto y = activeStart;
				const auto w = trackThickness;
				const auto h = length - activeStart;
				const auto rTop = (inactiveEnd <= from) ? (w / 2.0) : 2.0;
				const auto rBottom = w / 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(activeFg);
				if (h <= rBottom || (rTop == rBottom && h <= w)) {
					p.drawRoundedRect(QRectF(x, y, w, h), h / 2.0, h / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rTop, y);
					path.lineTo(x + w - rTop, y);
					path.quadTo(x + w, y, x + w, y + rTop);
					path.lineTo(x + w, y + h - rBottom);
					path.quadTo(x + w, y + h, x + w - rBottom, y + h);
					path.lineTo(x + rBottom, y + h);
					path.quadTo(x, y + h, x, y + h - rBottom);
					path.lineTo(x, y + rTop);
					path.quadTo(x, y, x + rTop, y);
					p.drawPath(path);
				}
			}
		}

		const auto dotCenterX = trackX + radius;
		const auto dotCenterY = from + radius;
		const auto dotRadius = 2.0;
		if (value < 0.95 && inactiveEnd - 4.0 > dotCenterY + dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}

		if (value > 0.05 && length - radius - dotRadius > activeStart + 4.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawEllipse(QPointF(dotCenterX, length - radius), dotRadius, dotRadius);
		}

		if (markerSizeRatio > 0.0) {
			const auto hW = handleLength * markerSizeRatio;
			const auto hH = std::max(effectiveHandleThickness, 1.0);
			const auto hX = (width() - hW) / 2.0;
			const auto hY = mid - hH / 2.0;
			const auto hRadius = hH / 2.0;
			p.setPen(Qt::NoPen);
			p.setBrush(seekFg);
			p.drawRoundedRect(QRectF(hX, hY, hW, hH), hRadius, hRadius);
		}
	}

	if (!_dividers.empty()) {
		for (const auto &divValue : _dividers) {
			const auto dividerValue = horizontal
				? divValue
				: (1.0 - divValue);
			const auto dividerMid = from + dividerValue * length;
			const auto dist = std::abs(dividerMid - mid);
			if (dist > (effectiveHandleThickness / 2.0) + effectiveGap + 2.0) {
				const auto isPassed = (horizontal ? (dividerValue <= value) : (dividerValue >= value));
				p.setBrush(isPassed ? inactiveFg : activeFg);
				const auto dRadius = 2.0;
				if (horizontal) {
					p.drawEllipse(QPointF(dividerMid, (height() / 2.0)), dRadius, dRadius);
				} else {
					p.drawEllipse(QPointF((width() / 2.0), dividerMid), dRadius, dRadius);
				}
			}
		}
	}
}

not_null<MaterialSlider*> AddCardSlider(
		not_null<::Ui::VerticalLayout*> card,
		const style::margins &margin) {
	return card->add(
		object_ptr<MaterialSlider>(card),
		margin);
}

} // namespace FA::Ui
