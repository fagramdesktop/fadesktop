/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_music_button.h"

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

	if (_overrideBg) {
		p.fillRect(e->rect(), *_overrideBg);
	} else {
		p.fillRect(e->rect(), st::boxDividerBg);
	}

	const auto pill = pillGeometry();
	const auto pillRadius = pill.height() / 2.0;

	p.setPen(Qt::NoPen);
	if (_overrideBg) {
		p.setBrush(Ui::BlendColors(
			*_overrideBg,
			Qt::black,
			st::infoProfileTopBarActionButtonBgOpacity));
	} else {
		p.setBrush(st::settingsThemeNotSupportedBg);
	}
	p.drawRoundedRect(pill, pillRadius, pillRadius);

	paintRipple(p, pill.topLeft().toPoint());

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

	p.setPen(_overrideBg ? st::groupCallMembersFg : st::windowBoldFg);
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

	p.setPen(_overrideBg ? st::groupCallVideoSubTextFg : st::windowSubTextFg);
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
	icon.paint(p, int(iconLeft), int(iconTop), iconWidth, p.pen().color());
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
