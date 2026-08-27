/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/features/avatar_shape/avatar_shape.h"

#include "fa/ui/md3/svg_assets.h"
#include "fa/settings/fa_settings.h"
#include "ui/image/image_prepare.h"
#include "ui/painter.h"
#include "ui/userpic_view.h"
#include "styles/style_basic.h"

#include <QtSvg/QSvgRenderer>

#include <algorithm>
#include <map>

namespace FA::Features::AvatarShape {

namespace {

QImage MaterialShapeMask(QSize size, int shapeIndex) {
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
	if (!color.isValid()) {
		color = st::activeButtonBgOver->c;
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
	const auto size = image.size();
	const auto outlineColor = st::activeButtonBgOver->c;

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

struct MaskKey {
	QSize size;
	int index = 0;

	friend inline bool operator<(const MaskKey &a, const MaskKey &b) {
		if (a.size.width() != b.size.width()) {
			return a.size.width() < b.size.width();
		}
		if (a.size.height() != b.size.height()) {
			return a.size.height() < b.size.height();
		}
		return a.index < b.index;
	}
};

std::map<MaskKey, QImage> &MaskCache() {
	static std::map<MaskKey, QImage> cache;
	return cache;
}

constexpr auto kMaxMaskCache = 64;

} // namespace

bool IsMaterial() {
	const auto value = FASettings::FASettings::getInstance().avatarShape();
	return (value >= 1) && (value <= 7);
}

int Roundness() {
	return FASettings::FASettings::getInstance().roundness();
}

int CornerRadius(int size, int dpr) {
	const auto roundness = FASettings::FASettings::getInstance().roundness();
	const auto result = (size * roundness) / 100.0 / std::max(1, dpr);
	return std::max(0, int(result));
}

QImage Mask(QSize size) {
	const auto index = FASettings::FASettings::getInstance().avatarShape();
	auto &cache = MaskCache();
	const auto key = MaskKey{ size, index };
	const auto it = cache.find(key);
	if (it != cache.end()) {
		return it->second;
	}
	auto result = MaterialShapeMask(size, index);
	if (cache.size() >= kMaxMaskCache) {
		cache.clear();
	}
	cache.emplace(key, result);
	return result;
}

QImage Apply(QImage image) {
	if (!IsMaterial()) {
		return image;
	}
	const auto shapeIndex = FASettings::FASettings::getInstance().avatarShape();
	return ApplyMaterialShape(std::move(image), shapeIndex);
}

Ui::PeerUserpicShape Resolve(Ui::PeerUserpicShape base) {
	return IsMaterial() ? Ui::PeerUserpicShape::Material : base;
}

} // namespace FA::Features::AvatarShape
