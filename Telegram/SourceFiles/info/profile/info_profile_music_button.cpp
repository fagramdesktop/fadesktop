/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_music_button.h"

#include "ui/color_contrast.h"
#include "ui/effects/animation_value.h"
#include "ui/effects/ripple_animation.h"
#include "ui/text/text_utilities.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/ui_utility.h"
#include "styles/style_chat.h"
#include "styles/style_info.h"
#include "styles/style_settings.h"

namespace Info::Profile {

MusicButton::MusicButton(
	QWidget *parent,
	MusicButtonData data,
	Fn<void()> handler)
: RippleButton(parent, st::infoMusicButtonRipple)
, _noteSymbol(u"\u266B"_q + QChar(' '))
, _noteWidth(st::normalFont->width(_noteSymbol)) {
	updateData(std::move(data));
	setClickedCallback(std::move(handler));
}

MusicButton::~MusicButton() = default;

void MusicButton::updateData(MusicButtonData data) {
	const auto result = data.name.textWithEntities();
	const auto performerLength = result.entities.empty()
		? 0
		: int(result.entities.front().length());
	_performer.setText(
		st::semiboldTextStyle,
		result.text.mid(0, performerLength));
	_title.setText(
		st::defaultTextStyle,
		result.text.mid(performerLength, result.text.size()));
	setAccessibleName(result.text);
	update();
}

void MusicButton::setOverrideBg(std::optional<QColor> color) {
	_overrideBg = color;
	update();
}

QRectF MusicButton::pillGeometry() const {
	const auto &icon = st::topicButtonArrow;
	const auto iconWidth = icon.width();
	const auto skip = st::normalFont->spacew;

	const auto titleWidth = _title.maxWidth();
	const auto performerWidth = _performer.maxWidth();
	const auto totalNeeded = titleWidth + performerWidth + skip;

	const auto maxPillWidth = std::max(width() - 32, 40);
	const auto pillPaddingH = 14.0;
	const auto maxContentWidth = std::max(float64(maxPillWidth) - 2 * pillPaddingH, 1.0);
	const auto availableForText = std::max(
		maxContentWidth - _noteWidth - skip - iconWidth - skip,
		1.0);

	auto actualTitleWidth = 0;
	auto actualPerformerWidth = 0;
	if (totalNeeded <= availableForText) {
		actualTitleWidth = titleWidth;
		actualPerformerWidth = performerWidth;
	} else {
		const auto ratio = float64(titleWidth) / std::max(totalNeeded, 1);
		actualPerformerWidth = int(availableForText * (1.0 - ratio));
		actualTitleWidth = int(availableForText) - actualPerformerWidth;
	}

	const auto contentWidth = _noteWidth
		+ actualPerformerWidth
		+ skip
		+ actualTitleWidth
		+ skip
		+ iconWidth;
	const auto pillWidth = std::min(
		float64(maxPillWidth),
		float64(contentWidth + 2 * pillPaddingH));
	const auto pillLeft = (width() - pillWidth) / 2.0;
	const auto pillHeight = 28.0;
	const auto pillTop = (height() - pillHeight) / 2.0;

	return QRectF(pillLeft, pillTop, pillWidth, pillHeight);
}

void MusicButton::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	const auto baseBg = _overrideBg ? *_overrideBg : st::boxDividerBg->c;
	p.fillRect(e->rect(), baseBg);

	const auto pill = pillGeometry();
	const auto pillRadius = pill.height() / 2.0;

	const auto isDark = !Ui::IsLightBackground(baseBg);
	auto pillBg = isDark
		? Ui::BlendColors(baseBg, Qt::white, 0.16)
		: QColor(255, 255, 255);
	if (isOver()) {
		pillBg = isDark
			? Ui::BlendColors(pillBg, Qt::white, 0.08)
			: Ui::BlendColors(pillBg, baseBg, 0.12);
	}

	p.setPen(Qt::NoPen);
	p.setBrush(pillBg);
	p.drawRoundedRect(pill, pillRadius, pillRadius);

	const auto pillIsDark = !Ui::IsLightBackground(pillBg);
	const auto rippleColor = _overrideBg
		? std::make_optional(anim::with_alpha(
			(pillIsDark
				? st::groupCallMembersFg->c
				: QColor(Qt::black)),
			st::infoProfileTopBarBackdropRippleOpacity))
		: std::nullopt;
	paintRipple(p, pill.topLeft().toPoint(), rippleColor ? &*rippleColor : nullptr);

	const auto &icon = st::topicButtonArrow;
	const auto iconWidth = icon.width();
	const auto iconHeight = icon.height();

	const auto skip = st::normalFont->spacew;
	const auto pillPaddingH = 14.0;

	const auto titleWidth = _title.maxWidth();
	const auto performerWidth = _performer.maxWidth();
	const auto totalNeeded = titleWidth + performerWidth + skip;
	const auto maxContentWidth = std::max(float64(pill.width() - 2 * pillPaddingH), 1.0);
	const auto availableForText = std::max(
		maxContentWidth - _noteWidth - skip - iconWidth - skip,
		1.0);

	auto actualTitleWidth = 0;
	auto actualPerformerWidth = 0;
	if (totalNeeded <= availableForText) {
		actualTitleWidth = titleWidth;
		actualPerformerWidth = performerWidth;
	} else {
		const auto ratio = float64(titleWidth) / std::max(totalNeeded, 1);
		actualPerformerWidth = int(availableForText * (1.0 - ratio));
		actualTitleWidth = int(availableForText) - actualPerformerWidth;
	}

	const auto contentStartX = pill.left() + pillPaddingH;
	const auto textTop = pill.top() + (pill.height() - st::normalFont->height) / 2.0;

	const auto primaryFg = pillIsDark
		? st::groupCallMembersFg->c
		: (!_overrideBg
			? st::windowBoldFg->c
			: QColor(Qt::black));
	const auto secondaryFg = pillIsDark
		? st::groupCallVideoSubTextFg->c
		: (!_overrideBg
			? st::windowSubTextFg->c
			: anim::with_alpha(
				QColor(Qt::black),
				st::groupCallVideoSubTextFg->c.alphaF()));

	p.setPen(primaryFg);
	p.setFont(st::normalFont);
	p.drawText(
		int(contentStartX),
		int(textTop + st::normalFont->ascent),
		_noteSymbol);

	_performer.draw(p, {
		.position = { int(contentStartX + _noteWidth), int(textTop) },
		.availableWidth = actualPerformerWidth,
		.now = crl::now(),
		.elisionLines = 1,
		.elisionMiddle = true,
	});

	p.setPen(secondaryFg);
	_title.draw(p, {
		.position = QPoint(
			int(contentStartX + _noteWidth + actualPerformerWidth + skip),
			int(textTop)),
		.availableWidth = actualTitleWidth,
		.now = crl::now(),
		.elisionLines = 1,
		.elisionMiddle = true,
	});

	const auto iconLeft = contentStartX
		+ _noteWidth
		+ actualPerformerWidth
		+ skip
		+ actualTitleWidth
		+ skip;
	const auto iconTop = pill.top() + (pill.height() - iconHeight) / 2.0;
	icon.paint(p, int(iconLeft), int(iconTop), iconWidth, secondaryFg);
}

int MusicButton::resizeGetHeight(int newWidth) {
	return 38;
}

QImage MusicButton::prepareRippleMask() const {
	const auto pill = pillGeometry();
	return Ui::RippleAnimation::RoundRectMask(
		pill.size().toSize(),
		int(pill.height() / 2.0));
}

QPoint MusicButton::prepareRippleStartPosition() const {
	const auto pill = pillGeometry();
	return mapFromGlobal(QCursor::pos()) - pill.topLeft().toPoint();
}

} // namespace Info::Profile
