/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "previews.h"
#include "fa/settings/fa_settings.h"
#include "core/application.h"
#include "styles/style_layers.h"

#include "main/main_domain.h"
#include "styles/style_fa_styles.h"
#include "ui/painter.h"
#include "window/main_window.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"

IconPackCheck::IconPackCheck(const style::icon *icon, bool checked)
: Ui::AbstractCheckView(st::defaultRadio.duration, checked, nullptr)
, _icon(icon)
, _radio(st::defaultRadio, checked, [this] { update(); }) {
}

QSize IconPackCheck::getSize() const {
	return st::settingsThemePreviewSize;
}

void IconPackCheck::paint(QPainter &p, int left, int top, int outerWidth) {
	PainterHighQualityEnabler hq(p);
	p.setPen(Qt::NoPen);

	const auto height = getSize().height();
	const auto rect = QRect(0, 0, outerWidth, height);
	const auto radius = st::roundRadiusLarge;
	
	p.setBrush(st::boxBg);
	p.drawRoundedRect(rect, radius, radius);
	
	if (_icon) {
		_icon->paintInCenter(p, rect);
	}
	
	const auto radio = _radio.getSize();
	_radio.paint(
		p,
		(outerWidth - radio.width()) / 2,
		height - radio.height() - st::settingsThemeRadioBottom,
		outerWidth);
		
	const auto toggled = currentAnimationValue();
	if (toggled > 0.) {
		const auto width = float64(st::settingsThemeOutlineWidth);
		const auto inset = width / 2.;
		const auto outlineRadius = st::roundRadiusLarge - inset;
		auto pen = QPen(st::windowActiveTextFg);
		pen.setWidthF(width);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.setOpacity(toggled);
		p.drawRoundedRect(
			QRectF(0, 0, outerWidth, height).adjusted(
				inset,
				inset,
				-inset,
				-inset),
			outlineRadius,
			outlineRadius);
		p.setOpacity(1.);
	}
}

QImage IconPackCheck::prepareRippleMask() const { return QImage(); }
bool IconPackCheck::checkRippleStartPosition(QPoint position) const { return false; }

void IconPackCheck::checkedChangedHook(anim::type animated) {
	_radio.setChecked(checked(), animated);
}

RoundnessPreview::RoundnessPreview(QWidget *parent) : RpWidget(parent) {
    auto sectionHeight = st::rndPreviewSize;
    setMinimumSize(st::boxWidth, sectionHeight);
}

void RoundnessPreview::paintEvent(QPaintEvent *e)  {
    Painter p(this);
    PainterHighQualityEnabler hq(p);

    auto size = st::rndPreviewSize;
    auto radius = size * (FASettings::FASettings::getInstance().roundness() / 100.);

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(st::rndPreviewFill));
    p.drawRoundedRect(
        0, 0,
        size, size, 
        radius, radius
    );

    p.setBrush(QBrush(st::rndSkeletonFill));
    auto skeletonWidth = st::boxWidth - (3 * st::rndPreviewSize);
    auto skeletonHeight = st::rndPreviewSize / 5;
    p.drawRoundedRect(
        st::rndPreviewSize * 1.33,
        skeletonHeight,
        skeletonWidth / 2,
        skeletonHeight,
        skeletonHeight / 2,
        skeletonHeight / 2
    );

    p.drawRoundedRect(
        st::rndPreviewSize * 1.33,
        skeletonHeight * 3,
        skeletonWidth,
        skeletonHeight,
        skeletonHeight / 2,
        skeletonHeight / 2
    );
}

// thanks rabbitGram