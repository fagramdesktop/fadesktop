/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_avatar_shape.h"
#include "fa/ui/md3/svg_assets.h"
#include "fa/settings/fa_settings.h"
#include "ui/image/image_prepare.h"
#include "ui/painter.h"
#include "styles/style_basic.h"

#include <QtSvg/QSvgRenderer>

namespace FA::Ui {

QImage MaterialShapeMask(QSize size, int shapeIndex) {
	if (shapeIndex < 0) {
		shapeIndex = FASettings::FASettings::getInstance().avatarShape();
	}
	if (shapeIndex <= 0 || shapeIndex > 7) {
		return QImage();
	}
	auto result = QImage(size, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);

	std::string_view svgData;
	switch (shapeIndex) {
	case 1: svgData = fa::svg::material_shape1; break;
	case 2: svgData = fa::svg::material_shape2; break;
	case 3: svgData = fa::svg::material_shape3; break;
	case 4: svgData = fa::svg::material_shape4; break;
	case 5: svgData = fa::svg::material_shape5; break;
	case 6: svgData = fa::svg::material_shape6; break;
	case 7: svgData = fa::svg::material_shape7; break;
	default: svgData = fa::svg::material_shape1; break;
	}

	auto svg = QSvgRenderer(QByteArray::fromRawData(
		svgData.data(),
		svgData.size()));

	const auto margin = float(size.width()) * 0.04f;
	const auto targetRect = QRectF(
		margin,
		margin,
		size.width() - 2 * margin,
		size.height() - 2 * margin);

	QPainter p(&result);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, targetRect);
	p.end();

	return result;
}

QImage MaterialShapeOutline(
		QSize size,
		int shapeIndex,
		QColor color,
		float strokeWidth) {
	if (size.isEmpty()) {
		return QImage();
	}
	if (shapeIndex < 0) {
		shapeIndex = FASettings::FASettings::getInstance().avatarShape();
	}
	if (!color.isValid()) {
		color = st::activeButtonBgOver->c;
	}
	if (shapeIndex == 0) {
		auto result = QImage(size, QImage::Format_ARGB32_Premultiplied);
		result.fill(Qt::transparent);
		const auto roundness = FASettings::FASettings::getInstance().roundness();
		if (strokeWidth <= 0.0f) {
			strokeWidth = std::max(2.0f, float(size.width()) * 0.03f);
		}
		const auto margin = strokeWidth;
		const auto radius = (size.width() - 2 * margin) * roundness / 100.0;
		const auto innerRect = QRectF(
			margin,
			margin,
			size.width() - 2 * margin,
			size.height() - 2 * margin);
		const auto inset = strokeWidth / 2.0;
		auto p = QPainter(&result);
		p.setRenderHint(QPainter::Antialiasing);
		auto pen = QPen(color);
		pen.setWidthF(strokeWidth);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(
			innerRect.adjusted(inset, inset, -inset, -inset),
			std::max(0.0, radius - inset),
			std::max(0.0, radius - inset));
		p.end();
		return result;
	}
	if (shapeIndex < 1 || shapeIndex > 7) {
		return QImage();
	}

	std::string_view svgData;
	switch (shapeIndex) {
	case 1: svgData = fa::svg::material_shape1; break;
	case 2: svgData = fa::svg::material_shape2; break;
	case 3: svgData = fa::svg::material_shape3; break;
	case 4: svgData = fa::svg::material_shape4; break;
	case 5: svgData = fa::svg::material_shape5; break;
	case 6: svgData = fa::svg::material_shape6; break;
	case 7: svgData = fa::svg::material_shape7; break;
	default: svgData = fa::svg::material_shape1; break;
	}

	if (strokeWidth <= 0.0f) {
		strokeWidth = 10.0f;
	}
	const auto colorName = color.name(QColor::HexRgb).toUtf8();
	QByteArray data = QByteArray::fromRawData(svgData.data(), svgData.size()).trimmed();
	const auto strokeAttr = "fill=\"none\" stroke=\"" + colorName + "\" stroke-width=\"" + QByteArray::number(strokeWidth) + "\" stroke-linejoin=\"round\"";
	data.replace("fill=\"#FFFFFF\"", strokeAttr);
	data.replace("fill=\"#ffffff\"", strokeAttr);

	auto svg = QSvgRenderer(data);
	auto result = QImage(size, QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);

	const auto margin = float(size.width()) * 0.04f;
	const auto targetRect = QRectF(
		margin,
		margin,
		size.width() - 2 * margin,
		size.height() - 2 * margin);

	QPainter p(&result);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, targetRect);
	p.end();

	return result;
}

QImage ApplyMaterialShape(QImage image, int shapeIndex) {
	if (shapeIndex < 0) {
		shapeIndex = FASettings::FASettings::getInstance().avatarShape();
	}
	const auto size = image.size();
	const auto outlineColor = st::activeButtonBgOver->c;

	if (shapeIndex == 0) {
		const auto roundness = FASettings::FASettings::getInstance().roundness();
		const auto strokeWidth = std::max(2.0, size.width() * 0.03);
		const auto margin = strokeWidth;
		const auto radius = (size.width() - 2 * margin) * roundness / 100.0;
		const auto innerRect = QRectF(
			margin,
			margin,
			size.width() - 2 * margin,
			size.height() - 2 * margin);

		auto mask = QImage(size, QImage::Format_ARGB32_Premultiplied);
		mask.fill(Qt::transparent);
		{
			auto q = QPainter(&mask);
			q.setRenderHint(QPainter::Antialiasing);
			q.setPen(Qt::NoPen);
			q.setBrush(Qt::white);
			q.drawRoundedRect(innerRect, radius, radius);
		}

		constexpr auto format = QImage::Format_ARGB32_Premultiplied;
		if (image.format() != format) {
			image = std::move(image).convertToFormat(format);
		}
		auto p = QPainter(&image);
		p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
		p.drawImage(QRect(QPoint(), image.size()), mask);

		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.setRenderHint(QPainter::Antialiasing);
		auto pen = QPen(outlineColor);
		pen.setWidthF(strokeWidth);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		const auto inset = strokeWidth / 2.0;
		p.drawRoundedRect(
			innerRect.adjusted(inset, inset, -inset, -inset),
			std::max(0.0, radius - inset),
			std::max(0.0, radius - inset));
		p.end();

		return image;
	}

	auto mask = MaterialShapeMask(size, shapeIndex);
	if (mask.isNull()) {
		return image;
	}

	auto outline = MaterialShapeOutline(size, shapeIndex, outlineColor, 10.0f);

	constexpr auto format = QImage::Format_ARGB32_Premultiplied;
	if (image.format() != format) {
		image = std::move(image).convertToFormat(format);
	}
	auto p = QPainter(&image);
	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawImage(QRect(QPoint(), image.size()), mask);
	if (!outline.isNull()) {
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		p.drawImage(QRect(QPoint(), image.size()), outline);
	}
	p.end();

	return image;
}

} // namespace FA::Ui
