/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_nav_drawer.h"

#include "ui/painter.h"
#include "ui/effects/ripple_animation.h"
#include "ui/effects/animation_value_f.h"
#include "styles/style_basic.h"
#include "styles/style_window.h"

namespace FA::Ui {
namespace {

constexpr auto kNavDrawerItemHeight = 50;
constexpr auto kPillMargin = 12.0;
constexpr auto kPillPaddingY = 2.0;

[[nodiscard]] style::margins ComputeNavDrawerPadding(const style::SettingsButton &st) {
	const auto topPadding = (kNavDrawerItemHeight - st.style.font->height) / 2;
	return style::margins(58, topPadding, 28, topPadding);
}

} // namespace

NavDrawerButton::NavDrawerButton(
	QWidget *parent,
	rpl::producer<QString> text,
	const style::SettingsButton &st,
	Settings::IconDescriptor &&descriptor)
: SettingsButton(parent, std::move(text), st)
, _descriptor(std::move(descriptor))
, _icon(_descriptor.icon) {
	setPaddingOverride(ComputeNavDrawerPadding(st));
	initToggleAnimation();
}

NavDrawerButton::NavDrawerButton(
	QWidget *parent,
	rpl::producer<TextWithEntities> text,
	const style::SettingsButton &st,
	Settings::IconDescriptor &&descriptor)
: SettingsButton(parent, std::move(text), st)
, _descriptor(std::move(descriptor))
, _icon(_descriptor.icon) {
	setPaddingOverride(ComputeNavDrawerPadding(st));
	initToggleAnimation();
}

void NavDrawerButton::initToggleAnimation() {
	toggledChanges(
	) | rpl::on_next([this](bool checked) {
		const auto animated = _hasToggled ? anim::type::normal : anim::type::instant;
		_hasToggled = true;
		if (animated == anim::type::instant) {
			_toggleAnimation.stop();
			update();
		} else {
			_toggleAnimation.start(
				[this] { update(); },
				checked ? 0.0 : 1.0,
				checked ? 1.0 : 0.0,
				200,
				anim::easeOutCubic);
		}
	}, lifetime());
}

void NavDrawerButton::setIcon(const style::icon *icon) {
	_icon = icon;
	update();
}

void NavDrawerButton::setDescriptor(Settings::IconDescriptor &&descriptor) {
	_descriptor = std::move(descriptor);
	_icon = _descriptor.icon;
	update();
}

int NavDrawerButton::resizeGetHeight(int newWidth) {
	return kNavDrawerItemHeight;
}

void NavDrawerButton::paintEvent(QPaintEvent *e) {
	Painter p(this);
	const auto outerw = width();
	const auto outerh = height();

	const auto pillRect = QRectF(
		kPillMargin,
		kPillPaddingY,
		outerw - 2.0 * kPillMargin,
		outerh - 2.0 * kPillPaddingY);
	const auto radius = pillRect.height() / 2.0;

	const auto paintOver = (isOver() || isDown()) && !isDisabled();
	if (paintOver) {
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBgOver);
		p.drawRoundedRect(pillRect, radius, radius);
	}

	{
		p.save();
		auto clip = QPainterPath();
		clip.addRoundedRect(pillRect, radius, radius);
		p.setClipPath(clip);
		paintRipple(p, 0, 0);
		p.restore();
	}

	if (_icon) {
		const auto iconLeft = int(pillRect.x() + 16.0);
		const auto iconTop = (outerh - _icon->height()) / 2;
		if (paintOver) {
			_icon->paint(p, QPoint(iconLeft, iconTop), outerw, st::windowBoldFgOver->c);
		} else {
			_icon->paint(p, QPoint(iconLeft, iconTop), outerw);
		}
	}

	paintText(p, paintOver, outerw);

	if (maybeToggleRect().width() > 0) {
		PainterHighQualityEnabler hq(p);
		const auto isChecked = toggled();
		const auto toggledProgress = _toggleAnimation.value(isChecked ? 1.0 : 0.0);
		const auto switchWidth = 44.0;
		const auto switchHeight = 24.0;
		const auto toggleX = pillRect.right() - 14.0 - switchWidth;
		const auto toggleY = (outerh - switchHeight) / 2.0;

		// 1. Draw Track
		if (toggledProgress > 0.0) {
			p.setOpacity(toggledProgress);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowActiveTextFg);
			p.drawRoundedRect(
				QRectF(toggleX, toggleY, switchWidth, switchHeight),
				switchHeight / 2.0,
				switchHeight / 2.0);
		}

		if (toggledProgress < 1.0) {
			p.setOpacity(1.0 - toggledProgress);
			p.setPen(QPen(st::windowSubTextFg->c, 2.0));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(
				QRectF(toggleX + 1.0, toggleY + 1.0, switchWidth - 2.0, switchHeight - 2.0),
				(switchHeight - 2.0) / 2.0,
				(switchHeight - 2.0) / 2.0);
		}
		p.setOpacity(1.0);

		// 2. Draw Thumb
		const auto thumbDiameter = anim::interpolateF(12.0, 18.0, toggledProgress);
		const auto thumbX = anim::interpolateF(toggleX + 4.0, toggleX + switchWidth - 3.0 - 18.0, toggledProgress);
		const auto thumbY = toggleY + (switchHeight - thumbDiameter) / 2.0;
		const auto thumbColor = anim::color(st::windowSubTextFg->c, st::windowBg->c, toggledProgress);

		p.setPen(Qt::NoPen);
		p.setBrush(thumbColor);
		p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));

		// 3. Draw Checkmark inside Thumb when active
		if (toggledProgress > 0.05) {
			p.setOpacity(toggledProgress);
			p.setPen(QPen(st::windowActiveTextFg->c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.setBrush(Qt::NoBrush);

			const auto cx = thumbX + thumbDiameter / 2.0;
			const auto cy = thumbY + thumbDiameter / 2.0;

			auto check = QPainterPath();
			check.moveTo(cx - 3.0, cy - 0.2);
			check.lineTo(cx - 0.8, cy + 2.2);
			check.lineTo(cx + 3.5, cy - 2.2);
			p.drawPath(check);
			p.setOpacity(1.0);
		}
	} else {
		paintToggle(p, outerw);
	}
}

QImage NavDrawerButton::prepareRippleMask() const {
	const auto pillRect = QRectF(
		kPillMargin,
		kPillPaddingY,
		width() - 2.0 * kPillMargin,
		height() - 2.0 * kPillPaddingY);
	const auto radius = pillRect.height() / 2.0;

	auto result = QImage(size(), QImage::Format_ARGB32_Premultiplied);
	result.fill(Qt::transparent);
	{
		QPainter p(&result);
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		p.drawRoundedRect(pillRect, radius, radius);
	}
	return result;
}

QPoint NavDrawerButton::prepareRippleStartPosition() const {
	const auto global = QCursor::pos();
	const auto local = mapFromGlobal(global);
	const auto pillRect = QRect(
		int(kPillMargin),
		int(kPillPaddingY),
		int(width() - 2.0 * kPillMargin),
		int(height() - 2.0 * kPillPaddingY));
	return pillRect.contains(local) ? local : DisabledRippleStartPosition();
}

object_ptr<NavDrawerButton> CreateNavDrawerButton(
		not_null<QWidget*> parent,
		rpl::producer<QString> text,
		const style::SettingsButton &st,
		Settings::IconDescriptor &&descriptor) {
	return object_ptr<NavDrawerButton>(
		parent,
		std::move(text),
		st,
		std::move(descriptor));
}

not_null<NavDrawerButton*> AddNavDrawerButton(
		not_null<::Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		const style::SettingsButton &st,
		Settings::IconDescriptor &&descriptor) {
	return container->add(
		CreateNavDrawerButton(
			container,
			std::move(text),
			st,
			std::move(descriptor)));
}

not_null<::Ui::RpWidget*> AddNavDrawerDivider(
		not_null<::Ui::VerticalLayout*> container) {
	const auto divider = container->add(
		object_ptr<::Ui::RpWidget>(container),
		style::margins(16, 6, 16, 6));
	divider->resize(divider->width(), st::lineWidth);
	divider->paintRequest(
	) | rpl::on_next([divider] {
		Painter p(divider);
		p.fillRect(divider->rect(), st::shadowFg);
	}, divider->lifetime());
	return divider;
}

} // namespace FA::Ui
