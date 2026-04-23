/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "editor/photo_editor_common.h"

#include "editor/scene/scene.h"
#include "ui/painter.h"
#include "ui/userpic_view.h"

namespace Editor {
namespace {

void ApplyShapeMask(QImage &image, EditorData::CropType type) {
	if (type == EditorData::CropType::Rect) {
		return;
	}
	if (image.format() != QImage::Format_ARGB32_Premultiplied) {
		image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	}
	auto mask = QImage(image.size(), QImage::Format_ARGB32_Premultiplied);
	mask.fill(Qt::transparent);
	{
		auto p = QPainter(&mask);
		auto hq = PainterHighQualityEnabler(p);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		const auto rect = QRectF(QPointF(), QSizeF(image.size()));
		if (type == EditorData::CropType::Ellipse) {
			p.drawEllipse(rect);
		} else {
			const auto radius = std::min(rect.width(), rect.height())
				* Ui::ForumUserpicRadiusMultiplier();
			p.drawRoundedRect(rect, radius, radius);
		}
	}
	auto p = QPainter(&image);
	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawImage(0, 0, mask);
}

} // namespace

QImage ImageModified(QImage image, const PhotoModifications &mods) {
	Expects(!image.isNull());

	if (!mods) {
		return image;
	}
	if (mods.paint) {
		if (image.format() != QImage::Format_ARGB32_Premultiplied) {
			image = image.convertToFormat(
				QImage::Format_ARGB32_Premultiplied);
		}

		Painter p(&image);
		PainterHighQualityEnabler hq(p);

		mods.paint->render(&p, image.rect());
	}
	auto cropped = mods.crop.isValid()
		? image.copy(mods.crop)
		: image;
	ApplyShapeMask(cropped, mods.cropType);
	QTransform transform;
	if (mods.flipped) {
		transform.scale(-1, 1);
	}
	if (mods.angle) {
		transform.rotate(mods.angle);
	}
	return cropped.transformed(transform);
}

bool PhotoModifications::empty() const {
	return !angle
		&& !flipped
		&& !crop.isValid()
		&& cropType == EditorData::CropType::Rect
		&& !paint;
}

PhotoModifications::operator bool() const {
	return !empty();
}

PhotoModifications::~PhotoModifications() {
	if (paint && (paint.use_count() == 1)) {
		paint->deleteLater();
	}
}

} // namespace Editor
