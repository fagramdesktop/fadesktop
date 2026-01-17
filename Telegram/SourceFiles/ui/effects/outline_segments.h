/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {

inline constexpr auto kOutlineSegmentsMax = 50;

struct OutlineSegment {
	QBrush brush;
	float64 width = 0.;
};

void PaintOutlineSegments(
	QPainter &p,
	QRectF ellipse,
	const std::vector<OutlineSegment> &segments,
	float64 fromFullProgress = 1.);

void PaintOutlineSegments(
	QPainter &p,
	QRectF rect,
	float64 radius,
	const std::vector<OutlineSegment> &segments);

[[nodiscard]] QLinearGradient UnreadStoryOutlineGradient(
	QRectF rect,
	const QColor &c1,
	const QColor &c2);
[[nodiscard]] QLinearGradient UnreadStoryOutlineGradient(QRectF rect = {});

} // namespace Ui
