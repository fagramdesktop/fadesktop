/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_switch.h"

#include "ui/painter.h"
#include "ui/effects/animation_value_f.h"
#include "styles/style_basic.h"
#include "styles/style_window.h"

#include <QtGui/QPainterPath>

namespace FA::Ui {

void PaintMd3Switch(
		QPainter &p,
		float64 x,
		float64 y,
		float64 toggled,
		float64 switchWidth,
		float64 switchHeight) {
	PainterHighQualityEnabler hq(p);

	const auto trackRadius = switchHeight / 2.0;

	if (toggled > 0.0) {
		p.setOpacity(toggled);
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowActiveTextFg);
		p.drawRoundedRect(
			QRectF(x, y, switchWidth, switchHeight),
			trackRadius,
			trackRadius);
	}

	if (toggled < 1.0) {
		p.setOpacity(1.0 - toggled);
		p.setPen(QPen(st::windowSubTextFg->c, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(
			QRectF(x + 1.0, y + 1.0, switchWidth - 2.0, switchHeight - 2.0),
			(switchHeight - 2.0) / 2.0,
			(switchHeight - 2.0) / 2.0);
	}
	p.setOpacity(1.0);

	const auto thumbDiameter = std::round(switchHeight * 0.72);
	const auto margin = (switchHeight - thumbDiameter) / 2.0;
	const auto thumbX = anim::interpolateF(x + margin, x + switchWidth - margin - thumbDiameter, toggled);
	const auto thumbY = y + margin;
	const auto thumbColor = anim::color(st::windowSubTextFg->c, st::windowBg->c, toggled);

	p.setPen(Qt::NoPen);
	p.setBrush(thumbColor);
	p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));

	const auto cx = thumbX + thumbDiameter / 2.0;
	const auto cy = thumbY + thumbDiameter / 2.0;
	const auto scale = thumbDiameter / 20.0;

	if (toggled < 0.95) {
		p.setOpacity(1.0 - toggled);
		p.setPen(QPen(st::windowBg->c, 1.8 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.setBrush(Qt::NoBrush);

		const auto crossRadius = 3.2 * scale;
		p.drawLine(
			QPointF(cx - crossRadius, cy - crossRadius),
			QPointF(cx + crossRadius, cy + crossRadius));
		p.drawLine(
			QPointF(cx + crossRadius, cy - crossRadius),
			QPointF(cx - crossRadius, cy + crossRadius));
	}

	if (toggled > 0.05) {
		p.setOpacity(toggled);
		p.setPen(QPen(st::windowActiveTextFg->c, 1.8 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.setBrush(Qt::NoBrush);

		auto check = QPainterPath();
		check.moveTo(cx - 3.5 * scale, cy - 0.2 * scale);
		check.lineTo(cx - 1.0 * scale, cy + 2.5 * scale);
		check.lineTo(cx + 4.0 * scale, cy - 2.5 * scale);
		p.drawPath(check);
	}
	p.setOpacity(1.0);
}

MaterialSwitch::MaterialSwitch(QWidget *parent, bool checked)
: RippleButton(parent, st::defaultSettingsButton.ripple)
, _checked(checked) {
	addClickHandler([this] {
		setChecked(!_checked);
	});
	resize(sizeHint());
}

void MaterialSwitch::setChecked(bool checked, anim::type animated) {
	if (_checked == checked) {
		return;
	}
	_checked = checked;
	_checkedChanges.fire_copy(_checked);
	if (animated == anim::type::instant) {
		_animation.stop();
		update();
	} else {
		_animation.start(
			[this] { update(); },
			_checked ? 0.0 : 1.0,
			_checked ? 1.0 : 0.0,
			200,
			anim::easeOutCubic);
	}
}

void MaterialSwitch::paintEvent(QPaintEvent *e) {
	Painter p(this);
	paintRipple(p, 0, 0);

	const auto switchWidth = 48.0;
	const auto switchHeight = 28.0;
	const auto toggleX = (width() - switchWidth) / 2.0;
	const auto toggleY = (height() - switchHeight) / 2.0;
	const auto toggled = _animation.value(_checked ? 1.0 : 0.0);

	PaintMd3Switch(p, toggleX, toggleY, toggled, switchWidth, switchHeight);
}

QSize MaterialSwitch::sizeHint() const {
	return QSize(48, 28);
}

} // namespace FA::Ui
