/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#include "fa/settings/fa_settings.h"

#include "ui/userpic_view.h"

#include "ui/empty_userpic.h"
#include "ui/painter.h"
#include "ui/image/image_prepare.h"

namespace Ui {

float64 ForumUserpicRadiusMultiplier() {
	return 0.3;
}

bool PeerUserpicLoading(const PeerUserpicView &view) {
	return view.cloud && view.cloud->isNull();
}

void ValidateUserpicCache(
		PeerUserpicView &view,
		const QImage *cloud,
		const EmptyUserpic *empty,
		int size,
		PeerUserpicShape shape) {
	Expects(cloud != nullptr || empty != nullptr);

	const auto full = QSize(size, size);
	const auto version = style::PaletteVersion();
	const auto shapeValue = static_cast<uint32>(shape) & 3;
	const auto regenerate = (view.cached.size() != QSize(size, size))
		|| (view.shape != shapeValue)
		|| (cloud && !view.empty.null())
		|| (empty && empty != view.empty.get())
		|| (empty && view.paletteVersion != version);
	bool use_default_rounding = FASettings::JsonSettings::GetBool("use_default_rounding");
	if (!regenerate) {
		return;
	}
	view.empty = empty;
	view.shape = shapeValue;
	view.paletteVersion = version;

	const auto forum = (shape == PeerUserpicShape::Forum);
	const auto radius = size * FASettings::JsonSettings::GetInt("roundness") / 100 / style::DevicePixelRatio();

	if (cloud) {
		view.cached = cloud->scaled(
			full,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		if (shape == PeerUserpicShape::Monoforum) {
			view.cached = Ui::ApplyMonoforumShape(std::move(view.cached));
		} else if (forum) {
			if (use_default_rounding) {
				view.cached = Images::Round(
					std::move(view.cached),
					Images::CornersMask(size
						* Ui::ForumUserpicRadiusMultiplier()
						/ style::DevicePixelRatio()));
			} else {
				view.cached = Images::Round(
					std::move(view.cached),
					Images::CornersMask(radius));
			}
		} else {
			if (use_default_rounding) {
				view.cached = Images::Circle(std::move(view.cached));
			} else {
				view.cached = Images::Round(
					std::move(view.cached),
					Images::CornersMask(radius));
			}
		}
	} else {
		if (view.cached.size() != full) {
			view.cached = QImage(full, QImage::Format_ARGB32_Premultiplied);
		}
		view.cached.fill(Qt::transparent);

		auto p = QPainter(&view.cached);
		if (shape == PeerUserpicShape::Monoforum) {
			empty->paintMonoforum(p, 0, 0, size, size);
		} else if (forum) {
			if (use_default_rounding) {
				empty->paintRounded(
					p,
					0,
					0,
					size,
					size,
					size * Ui::ForumUserpicRadiusMultiplier());
			} else {
				empty->paintRounded(
					p,
					0,
					0,
					size,
					size,
					radius);
			}
		} else {
			if (use_default_rounding) {
				empty->paintCircle(p, 0, 0, size, size);
			} else {
				empty->paintRounded(p, 0, 0, size, size, radius);
			}
		}
	}
}

} // namespace Ui
