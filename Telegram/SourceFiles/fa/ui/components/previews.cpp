/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/ui/components/previews.h"
#include "fa/ui/components/fa_ui_components.h"
#include "fa/settings/fa_settings.h"
#include "fa_lang_auto.h"
#include "core/application.h"
#include "styles/style_layers.h"

#include "main/main_domain.h"
#include "styles/style_fa_styles.h"
#include "ui/painter.h"
#include "window/main_window.h"
#include "styles/style_settings.h"
#include "styles/style_window.h"
#include "fa/ui/components/svg_assets.h"
#include <QSvgRenderer>

IconPackCheck::IconPackCheck(bool isMaterial, bool checked)
: Ui::AbstractCheckView(st::defaultRadio.duration, checked, nullptr)
, _isMaterial(isMaterial) {
}

QSize IconPackCheck::getSize() const {
	return QSize(
		st::settingsThemePreviewSize.width(),
		76);
}

void IconPackCheck::paint(QPainter &p, int left, int top, int outerWidth) {
	PainterHighQualityEnabler hq(p);
	p.setPen(Qt::NoPen);

	const auto rectHeight = 76;
	const auto rect = QRect(0, 0, outerWidth, rectHeight);
	const auto radius = rectHeight / 2.0;
	
	p.setBrush(st::settingsThemeNotSupportedBg);
	p.drawRoundedRect(rect, radius, radius);
	
	const auto slotSize = 28.0;
	const auto spacing = 14.0;
	const auto totalWidth = (slotSize * 3) + (spacing * 2);
	const auto startX = (outerWidth - totalWidth) / 2.0;
	const auto centerY = rectHeight / 2.0;
	
	const auto active = currentAnimationValue();
	const auto iconColor = anim::color(
		st::windowSubTextFg,
		st::windowActiveTextFg,
		active).name().toUtf8();
	
	auto drawSvg = [&](std::string_view svg, QRectF bounds) {
		QByteArray data = QByteArray::fromRawData(svg.data(), svg.size()).trimmed();
		data.replace("#FFFFFF", iconColor);
		data.replace("#ffffff", iconColor);
		data.replace("#E3E3E3", iconColor);
		data.replace("#e3e3e3", iconColor);
		QSvgRenderer renderer(data);
		renderer.render(&p, bounds);
	};
	
	const auto slotCenter1 = startX + slotSize / 2.0;
	const auto slotCenter2 = startX + slotSize + spacing + slotSize / 2.0;
	const auto slotCenter3 = startX + (slotSize + spacing) * 2 + slotSize / 2.0;
	
	if (_isMaterial) {
		drawSvg(fa::svg::material_dock, QRectF(slotCenter1 - 12, centerY - 12, 24, 24));
		drawSvg(fa::svg::material_chat, QRectF(slotCenter2 - 11, centerY - 11, 22, 22));
		drawSvg(fa::svg::material_send, QRectF(slotCenter3 - 12, centerY - 12, 24, 24));
	} else {
		drawSvg(fa::svg::default_custom, QRectF(slotCenter1 - 16, centerY - 16, 32, 32));
		drawSvg(fa::svg::default_all, QRectF(slotCenter2 - 16, centerY - 16, 32, 32));
		drawSvg(fa::svg::default_airplane, QRectF(slotCenter3 - 16, centerY - 16, 32, 32));
	}
		
	if (active > 0.) {
		const auto width = float64(st::settingsThemeOutlineWidth);
		const auto inset = width / 2.;
		const auto outlineRadius = (rectHeight - width) / 2.0;
		auto pen = QPen(st::windowActiveTextFg);
		pen.setWidthF(width);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.setOpacity(active);
		p.drawRoundedRect(
			QRectF(0, 0, outerWidth, rectHeight).adjusted(
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

RoundnessPreview::RoundnessPreview(QWidget *parent) : RpWidget(parent) {
	resize(width(), 60);
}

int RoundnessPreview::resizeGetHeight(int newWidth) {
	return 60;
}

void RoundnessPreview::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);

	const auto avatarSize = 40;
	const auto roundness = FASettings::FASettings::getInstance().roundness();
	const auto radius = avatarSize * (roundness / 100.);
	const auto avatarX = 16;
	const auto avatarY = (height() - avatarSize) / 2;

	p.setPen(Qt::NoPen);
	p.setBrush(st::windowActiveTextFg);
	p.drawRoundedRect(
		QRectF(avatarX, avatarY, avatarSize, avatarSize),
		radius, radius
	);

	p.setPen(Qt::NoPen);
	p.setBrush(st::settingsThemeNotSupportedBg);
	p.drawEllipse(QRectF(avatarX + 14, avatarY + 8, 12, 12));
	{
		PainterHighQualityEnabler hqBody(p);
		p.save();
		auto clipPath = QPainterPath();
		clipPath.addRoundedRect(QRectF(avatarX, avatarY, avatarSize, avatarSize), radius, radius);
		p.setClipPath(clipPath);
		p.drawEllipse(QRectF(avatarX + 6, avatarY + 23, 28, 24));
		p.restore();
	}

	const auto textX = avatarX + avatarSize + 14;
	const auto textWidth = width() - textX - 16;

	const auto titleFontMetrics = QFontMetrics(st::semiboldFont->f);
	const auto titleH = titleFontMetrics.height();
	const auto descFont = FA::Ui::DescriptionFont();
	const auto descMetrics = QFontMetrics(descFont);
	const auto descH = descMetrics.height();
	const auto spacing = 3;
	const auto textBlockH = titleH + spacing + descH;
	const auto textTop = (height() - textBlockH) / 2;

	p.setFont(st::semiboldFont);
	p.setPen(st::windowFg);
	p.drawText(
		QRect(textX, textTop, textWidth, titleH),
		Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
		fatr::fa_preview_contact_title(fatr::now));

	p.setFont(descFont);
	p.setPen(st::windowSubTextFg);
	p.drawText(
		QRect(textX, textTop + titleH + spacing, textWidth, descH),
		Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
		fatr::fa_preview_contact_subtitle(fatr::now));
}