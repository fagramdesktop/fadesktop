/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/ui/components/fa_ui_components.h"
#include "fa/ui/components/svg_assets.h"
#include "fa/settings/fa_settings.h"
#include "ui/image/image_prepare.h"

#include <QtSvg/QSvgRenderer>

#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/effects/ripple_animation.h"
#include "ui/effects/animation_value_f.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/padding_wrap.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

namespace FA::Ui {
namespace {

class CardContainerWidget final : public ::Ui::RpWidget {
public:
	explicit CardContainerWidget(QWidget *parent) : RpWidget(parent) {
	}

	void setInnerLayout(
			not_null<::Ui::VerticalLayout*> layout,
			not_null<::Ui::VerticalLayout*> container) {
		_layout = layout;
		_container = container;
		_layout->installEventFilter(this);
		watchChildren(_layout);
	}

	int resizeGetHeight(int newWidth) override {
		if (_layout) {
			_layout->resizeToWidth(newWidth);
			return _layout->height();
		}
		return height();
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override {
		const auto type = event->type();
		if ((type == QEvent::Resize || type == QEvent::Show || type == QEvent::Hide) && !_inRelayout) {
			updateGeometryFromInner();
		} else if (type == QEvent::ChildAdded) {
			const auto childAdded = static_cast<QChildEvent*>(event);
			if (const auto widget = qobject_cast<QWidget*>(childAdded->child())) {
				widget->installEventFilter(this);
				watchChildren(widget);
			}
		}
		return RpWidget::eventFilter(watched, event);
	}

	void paintEvent(QPaintEvent *e) override {
		if (height() <= 0) {
			return;
		}
		Painter p(this);
		PainterHighQualityEnabler hq(p);
		constexpr auto kRadius = 14.0;
		const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);

		p.setPen(Qt::NoPen);
		p.setBrush(st::settingsThemeNotSupportedBg);
		p.drawRoundedRect(r, kRadius, kRadius);
	}

private:
	void watchChildren(QObject *parent) {
		for (const auto child : parent->children()) {
			if (const auto widget = qobject_cast<QWidget*>(child)) {
				widget->installEventFilter(this);
				watchChildren(widget);
			}
		}
	}

	void updateGeometryFromInner() {
		if (!_layout || _inRelayout) {
			return;
		}
		_inRelayout = true;
		const auto w = width();
		if (w > 0) {
			_layout->resizeToWidth(w);
			const auto h = _layout->height();
			if (height() != h) {
				resize(w, h);
				if (_container) {
					_container->resizeToWidth(_container->width());
				}
			}
		}
		_inRelayout = false;
	}

	::Ui::VerticalLayout *_layout = nullptr;
	::Ui::VerticalLayout *_container = nullptr;
	bool _inRelayout = false;
};

class CardToggleRow final : public ::Ui::RippleButton {
public:
	CardToggleRow(
		QWidget *parent,
		rpl::producer<QString> &&title,
		rpl::producer<QString> &&subtitle,
		rpl::producer<bool> &&value,
		Fn<void(bool)> onToggle)
	: RippleButton(parent, st::defaultSettingsButton.ripple)
	, _onToggle(std::move(onToggle)) {
		
		std::move(title) | rpl::on_next([this](const QString &text) {
			_titleText = text;
			updateHeight();
		}, lifetime());

		std::move(subtitle) | rpl::on_next([this](const QString &text) {
			_subtitleText = text;
			updateHeight();
		}, lifetime());

		std::move(value) | rpl::on_next([this](bool checked) {
			const auto animated = _hasValue ? anim::type::normal : anim::type::instant;
			_hasValue = true;
			setChecked(checked, animated);
		}, lifetime());

		addClickHandler([this] {
			const auto newChecked = !_checked;
			setChecked(newChecked, anim::type::normal);
			if (_onToggle) {
				_onToggle(newChecked);
			}
		});

		updateHeight();
	}

	void setChecked(bool checked, anim::type animated = anim::type::normal) {
		if (_checked == checked && _hasValue) {
			return;
		}
		_checked = checked;
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

protected:
	int resizeGetHeight(int newWidth) override {
		return computeHeight(newWidth);
	}

	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		PainterHighQualityEnabler hq(p);

		paintRipple(p, 0, 0);

		const auto paddingLeft = 16;
		const auto paddingRight = 16;
		const auto switchWidth = 46.0;
		const auto switchHeight = 26.0;
		const auto toggleX = width() - paddingRight - switchWidth;
		const auto toggleY = (height() - switchHeight) / 2.0;

		const auto toggled = _animation.value(_checked ? 1.0 : 0.0);

		// 1. Draw Track
		// On-state active fill
		if (toggled > 0.0) {
			p.setOpacity(toggled);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowActiveTextFg);
			p.drawRoundedRect(
				QRectF(toggleX, toggleY, switchWidth, switchHeight),
				switchHeight / 2.0,
				switchHeight / 2.0);
		}

		// Off-state border track
		if (toggled < 1.0) {
			p.setOpacity(1.0 - toggled);
			p.setPen(QPen(st::windowSubTextFg->c, 2.0));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(
				QRectF(toggleX + 1.0, toggleY + 1.0, switchWidth - 2.0, switchHeight - 2.0),
				(switchHeight - 2.0) / 2.0,
				(switchHeight - 2.0) / 2.0);
		}
		p.setOpacity(1.0);

		// 2. Draw Thumb (Material 3 size & position interpolation)
		const auto thumbDiameter = anim::interpolateF(14.0, 20.0, toggled);
		const auto thumbX = anim::interpolateF(toggleX + 5.0, toggleX + switchWidth - 3.0 - 20.0, toggled);
		const auto thumbY = toggleY + (switchHeight - thumbDiameter) / 2.0;
		const auto thumbColor = anim::color(st::windowSubTextFg->c, st::windowBg->c, toggled);

		p.setPen(Qt::NoPen);
		p.setBrush(thumbColor);
		p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));

		// 3. Draw Checkmark inside Thumb when active
		if (toggled > 0.05) {
			p.setOpacity(toggled);
			p.setPen(QPen(st::windowActiveTextFg->c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.setBrush(Qt::NoBrush);

			const auto cx = thumbX + thumbDiameter / 2.0;
			const auto cy = thumbY + thumbDiameter / 2.0;

			auto check = QPainterPath();
			check.moveTo(cx - 3.5, cy - 0.2);
			check.lineTo(cx - 1.0, cy + 2.5);
			check.lineTo(cx + 4.0, cy - 2.5);
			p.drawPath(check);
			p.setOpacity(1.0);
		}

		// 4. Draw Title and Subtitle Text
		const auto availableTextWidth = toggleX - paddingLeft - 12.0;
		if (availableTextWidth <= 0) {
			return;
		}

		if (_subtitleText.trimmed().isEmpty()) {
			p.setFont(st::semiboldFont);
			p.setPen(st::windowFg);
			p.drawText(
				QRect(paddingLeft, 0, availableTextWidth, height()),
				Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
				_titleText);
		} else {
			const auto titleFontMetrics = QFontMetrics(st::semiboldFont->f);
			const auto titleH = titleFontMetrics.height();
			const auto topPad = 12;

			p.setFont(st::semiboldFont);
			p.setPen(st::windowFg);
			p.drawText(
				QRect(paddingLeft, topPad, availableTextWidth, titleH),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
				_titleText);

			const auto descFont = DescriptionFont();
			p.setFont(descFont);
			p.setPen(st::windowSubTextFg);
			const auto subtitleY = topPad + titleH + 3;
			p.drawText(
				QRect(paddingLeft, subtitleY, availableTextWidth, height() - subtitleY - 8),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				_subtitleText);
		}
	}

	QImage prepareRippleMask() const override {
		return ::Ui::RippleAnimation::RoundRectMask(size(), 14);
	}

	QPoint prepareRippleStartPosition() const override {
		return mapFromGlobal(QCursor::pos());
	}

private:
	int computeHeight(int w) const {
		if (w <= 0) {
			w = 340;
		}
		if (_subtitleText.trimmed().isEmpty()) {
			return 48;
		}
		const auto switchWidth = 46.0;
		const auto availableTextWidth = w - 16 - switchWidth - 12 - 16;
		if (availableTextWidth <= 0) {
			return 58;
		}
		const auto titleMetrics = QFontMetrics(st::semiboldFont->f);
		const auto titleH = titleMetrics.height();
		const auto descFont = DescriptionFont();
		const auto subMetrics = QFontMetrics(descFont);
		const auto subRect = subMetrics.boundingRect(
			QRect(0, 0, availableTextWidth, 10000),
			Qt::TextWordWrap,
			_subtitleText);
		const auto topPad = 12;
		const auto spacing = 3;
		const auto botPad = 12;
		return std::max(56, topPad + titleH + spacing + subRect.height() + botPad);
	}

	void updateHeight() {
		resize(width(), computeHeight(width()));
		update();
	}

	bool _hasValue = false;
	bool _checked = false;
	::Ui::Animations::Simple _animation;
	Fn<void(bool)> _onToggle;
	QString _titleText;
	QString _subtitleText;
};

class CardButtonRow final : public ::Ui::RippleButton {
public:
	CardButtonRow(
		QWidget *parent,
		rpl::producer<QString> &&title,
		Fn<void()> onClick,
		const style::icon *icon,
		rpl::producer<QString> &&rightLabel,
		bool showChevron)
	: RippleButton(parent, st::defaultSettingsButton.ripple)
	, _icon(icon)
	, _showChevron(showChevron) {

		std::move(title) | rpl::on_next([this](const QString &text) {
			_titleText = text;
			update();
		}, lifetime());

		if (rightLabel) {
			std::move(rightLabel) | rpl::on_next([this](const QString &text) {
				_rightLabelText = text;
				update();
			}, lifetime());
		}

		if (onClick) {
			addClickHandler(std::move(onClick));
		}

		resize(width(), 48);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 48;
	}

	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		PainterHighQualityEnabler hq(p);

		paintRipple(p, 0, 0);

		const auto padding = 16;
		auto textLeft = padding;

		if (_icon) {
			const auto iconY = (height() - _icon->height()) / 2;
			_icon->paint(p, padding, iconY, width(), st::windowFg->c);
			textLeft += _icon->width() + 14;
		}

		auto textRight = width() - padding;

		if (_showChevron) {
			const auto chevronW = 6;
			const auto chevronH = 10;
			const auto chevronX = width() - padding - chevronW;
			const auto chevronY = (height() - chevronH) / 2;

			p.setPen(QPen(st::windowSubTextFg, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.drawLine(chevronX, chevronY, chevronX + chevronW, chevronY + chevronH / 2);
			p.drawLine(chevronX + chevronW, chevronY + chevronH / 2, chevronX, chevronY + chevronH);

			textRight = chevronX - 10;
		}

		if (!_rightLabelText.isEmpty()) {
			p.setFont(st::boxTextFont);
			p.setPen(st::windowActiveTextFg);
			const auto rightLabelW = QFontMetrics(st::boxTextFont->f).horizontalAdvance(_rightLabelText);
			p.drawText(
				QRect(textRight - rightLabelW, 0, rightLabelW, height()),
				Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine,
				_rightLabelText);
			textRight -= rightLabelW + 10;
		}

		p.setFont(st::semiboldFont);
		p.setPen(st::windowFg);
		const auto availableTitleW = std::max(0, textRight - textLeft);
		p.drawText(
			QRect(textLeft, 0, availableTitleW, height()),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			_titleText);
	}

	QImage prepareRippleMask() const override {
		return ::Ui::RippleAnimation::RoundRectMask(size(), 14);
	}

	QPoint prepareRippleStartPosition() const override {
		return mapFromGlobal(QCursor::pos());
	}

private:
	const style::icon *_icon = nullptr;
	bool _showChevron = false;
	QString _titleText;
	QString _rightLabelText;
};

class CardRadioRow final : public ::Ui::RippleButton {
public:
	CardRadioRow(
		QWidget *parent,
		const std::shared_ptr<::Ui::RadiobuttonGroup> &group,
		int value,
		rpl::producer<QString> &&title)
	: RippleButton(parent, st::defaultSettingsButton.ripple)
	, _group(group)
	, _value(value)
	, _checked(group ? (group->current() == value) : false) {

		std::move(title) | rpl::on_next([this](const QString &text) {
			_titleText = text;
			update();
		}, lifetime());

		if (_group) {
			_group->value(
			) | rpl::on_next([this, value](int currentVal) {
				const auto newChecked = (currentVal == value);
				if (_checked != newChecked) {
					_checked = newChecked;
					update();
				}
			}, lifetime());
		}

		addClickHandler([this] {
			if (_group) {
				_group->setValue(_value);
			}
		});

		resize(width(), 48);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 48;
	}

	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		PainterHighQualityEnabler hq(p);

		paintRipple(p, 0, 0);

		const auto padding = 16;
		const auto radioSize = 18;
		const auto radioX = padding;
		const auto radioY = (height() - radioSize) / 2;

		if (_checked) {
			p.setPen(QPen(st::windowActiveTextFg, 2.0));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(QRectF(radioX + 1, radioY + 1, radioSize - 2, radioSize - 2));

			p.setPen(Qt::NoPen);
			p.setBrush(st::windowActiveTextFg);
			p.drawEllipse(QRectF(radioX + 5, radioY + 5, radioSize - 10, radioSize - 10));
		} else {
			p.setPen(QPen(st::windowSubTextFg, 1.8));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(QRectF(radioX + 1, radioY + 1, radioSize - 2, radioSize - 2));
		}

		const auto textLeft = radioX + radioSize + 14;
		const auto textW = width() - textLeft - padding;

		p.setFont(st::semiboldFont);
		p.setPen(st::windowFg);
		p.drawText(
			QRect(textLeft, 0, textW, height()),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			_titleText);
	}

	QImage prepareRippleMask() const override {
		return ::Ui::RippleAnimation::RoundRectMask(size(), 14);
	}

	QPoint prepareRippleStartPosition() const override {
		return mapFromGlobal(QCursor::pos());
	}

private:
	std::shared_ptr<::Ui::RadiobuttonGroup> _group;
	int _value = 0;
	bool _checked = false;
	QString _titleText;
};

class CardDividerWidget final : public ::Ui::RpWidget {
public:
	explicit CardDividerWidget(QWidget *parent) : RpWidget(parent) {
		resize(width(), 1);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 1;
	}

	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		p.fillRect(QRect(16, 0, width() - 32, 1), st::shadowFg);
	}
};

} // namespace

QFont DescriptionFont() {
	auto font = st::boxTextFont->f;
	if (font.pixelSize() > 0) {
		font.setPixelSize(std::max(11, font.pixelSize() - 2));
	} else if (font.pointSizeF() > 0) {
		font.setPointSizeF(std::max(8.5, font.pointSizeF() - 1.5));
	} else {
		font.setPointSize(std::max(9, font.pointSize() - 1));
	}
	return font;
}

void AddModernSectionHeader(
		not_null<::Ui::VerticalLayout*> container,
		rpl::producer<QString> title) {
	const auto label = container->add(
		object_ptr<::Ui::FlatLabel>(
			container,
			std::move(title),
			st::defaultSubsectionTitle),
		style::margins(22, 12, 22, 6));
	label->setTextColorOverride(st::windowActiveTextFg->c);
}

not_null<::Ui::VerticalLayout*> CreateCardContainer(
		not_null<::Ui::VerticalLayout*> container,
		int topMargin,
		int bottomMargin) {
	const auto cardSurface = container->add(
		object_ptr<CardContainerWidget>(container),
		style::margins(16, topMargin, 16, bottomMargin));
	
	const auto layout = ::Ui::CreateChild<::Ui::VerticalLayout>(cardSurface);
	cardSurface->setInnerLayout(layout, container);
	
	cardSurface->widthValue(
	) | rpl::on_next([layout](int width) {
		layout->resizeToWidth(width);
	}, layout->lifetime());

	layout->heightValue(
	) | rpl::on_next([cardSurface, container](int height) {
		cardSurface->resize(cardSurface->width(), height);
		container->resizeToWidth(container->width());
	}, layout->lifetime());

	return layout;
}

not_null<::Ui::RpWidget*> AddCardToggle(
		not_null<::Ui::VerticalLayout*> card,
		rpl::producer<QString> title,
		rpl::producer<QString> subtitle,
		rpl::producer<bool> value,
		Fn<void(bool)> onToggle) {
	return card->add(object_ptr<CardToggleRow>(
		card,
		std::move(title),
		std::move(subtitle),
		std::move(value),
		std::move(onToggle)));
}

not_null<::Ui::RpWidget*> AddCardButton(
		not_null<::Ui::VerticalLayout*> card,
		rpl::producer<QString> title,
		Fn<void()> onClick,
		const style::icon *icon,
		rpl::producer<QString> rightLabel,
		bool showChevron) {
	return card->add(object_ptr<CardButtonRow>(
		card,
		std::move(title),
		std::move(onClick),
		icon,
		std::move(rightLabel),
		showChevron));
}

not_null<::Ui::RpWidget*> AddCardRadio(
		not_null<::Ui::VerticalLayout*> card,
		const std::shared_ptr<::Ui::RadiobuttonGroup> &group,
		int value,
		rpl::producer<QString> title) {
	return card->add(object_ptr<CardRadioRow>(
		card,
		group,
		value,
		std::move(title)));
}

void AddCardDivider(not_null<::Ui::VerticalLayout*> card) {
	card->add(object_ptr<CardDividerWidget>(card));
}

MaterialSlider::MaterialSlider(QWidget *parent)
: ContinuousSlider(parent) {
	resize(width(), 32);
}

int MaterialSlider::resizeGetHeight(int newWidth) {
	return 32;
}

QSize MaterialSlider::getSeekDecreaseSize() const {
	if (!_alwaysDisplayMarker) {
		return QSize();
	}
	const auto trackThickness = 16;
	return QSize(trackThickness, trackThickness);
}

float64 MaterialSlider::getOverDuration() const {
	return 150.0;
}

void MaterialSlider::addDivider(float64 atValue) {
	_dividers.push_back(atValue);
	update();
}

void MaterialSlider::clearDividers() {
	_dividers.clear();
	update();
}

void MaterialSlider::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	PainterHighQualityEnabler hq(p);

	p.setPen(Qt::NoPen);
	p.setOpacity(fadeOpacity());

	const auto horizontal = isHorizontal();
	const auto trackThickness = 16.0;
	const auto radius = trackThickness / 2.0;
	const auto disabled = isDisabled();
	const auto over = getCurrentOverFactor();

	const auto value = horizontal
		? getCurrentValue()
		: (1. - getCurrentValue());

	const auto from = 0.0;
	const auto length = float64(horizontal ? width() : height());

	const auto handleThickness = 4.0;
	const auto handleLength = 28.0;
	const auto seekPadding = _alwaysDisplayMarker ? radius : (radius * over);
	const auto mid = from + seekPadding + value * (length - 2.0 * seekPadding);

	const auto activeFg = disabled
		? st::windowSubTextFg->c
		: st::windowActiveTextFg->c;
	const auto inactiveFg = disabled
		? st::windowBg->c
		: st::sliderBgInactive->c;
	const auto seekFg = activeFg;

	const auto markerSizeRatio = disabled
		? 0.0
		: (_alwaysDisplayMarker ? 1.0 : over);

	const auto gap = 4.0;
	const auto effectiveGap = gap * markerSizeRatio;
	const auto effectiveHandleThickness = handleThickness * markerSizeRatio;

	if (horizontal) {
		const auto trackY = (height() - trackThickness) / 2.0;
		const auto activeEnd = mid - (effectiveHandleThickness / 2.0) - effectiveGap;
		const auto inactiveStart = mid + (effectiveHandleThickness / 2.0) + effectiveGap;

		if (value <= 0.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawRoundedRect(QRectF(from, trackY, length, trackThickness), radius, radius);
		} else if (value >= 1.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawRoundedRect(QRectF(from, trackY, length, trackThickness), radius, radius);
		} else {
			if (activeEnd > from) {
				const auto x = from;
				const auto y = trackY;
				const auto w = activeEnd - from;
				const auto h = trackThickness;
				const auto rLeft = h / 2.0;
				const auto rRight = (inactiveStart >= length) ? (h / 2.0) : 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(activeFg);
				if (w <= rLeft || (rLeft == rRight && w <= h)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rLeft, y);
					path.lineTo(x + w - rRight, y);
					path.quadTo(x + w, y, x + w, y + rRight);
					path.lineTo(x + w, y + h - rRight);
					path.quadTo(x + w, y + h, x + w - rRight, y + h);
					path.lineTo(x + rLeft, y + h);
					path.quadTo(x, y + h, x, y + h - rLeft);
					path.lineTo(x, y + rLeft);
					path.quadTo(x, y, x + rLeft, y);
					p.drawPath(path);
				}
			}

			if (inactiveStart < length) {
				const auto x = inactiveStart;
				const auto y = trackY;
				const auto w = length - inactiveStart;
				const auto h = trackThickness;
				const auto rLeft = (activeEnd <= from) ? (h / 2.0) : 2.0;
				const auto rRight = h / 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(inactiveFg);
				if (w <= rRight || (rLeft == rRight && w <= h)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rLeft, y);
					path.lineTo(x + w - rRight, y);
					path.quadTo(x + w, y, x + w, y + rRight);
					path.lineTo(x + w, y + h - rRight);
					path.quadTo(x + w, y + h, x + w - rRight, y + h);
					path.lineTo(x + rLeft, y + h);
					path.quadTo(x, y + h, x, y + h - rLeft);
					path.lineTo(x, y + rLeft);
					path.quadTo(x, y, x + rLeft, y);
					p.drawPath(path);
				}
			}
		}

		const auto dotCenterX = length - radius;
		const auto dotCenterY = trackY + radius;
		const auto dotRadius = 2.0;
		if (value < 0.95 && inactiveStart + 4.0 < dotCenterX - dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}

		if (value > 0.05 && from + radius + dotRadius < activeEnd - 4.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawEllipse(QPointF(from + radius, dotCenterY), dotRadius, dotRadius);
		}

		if (markerSizeRatio > 0.0) {
			const auto hW = std::max(effectiveHandleThickness, 1.0);
			const auto hH = handleLength * markerSizeRatio;
			const auto hX = mid - hW / 2.0;
			const auto hY = (height() - hH) / 2.0;
			const auto hRadius = hW / 2.0;
			p.setPen(Qt::NoPen);
			p.setBrush(seekFg);
			p.drawRoundedRect(QRectF(hX, hY, hW, hH), hRadius, hRadius);
		}
	} else {
		const auto trackX = (width() - trackThickness) / 2.0;
		const auto inactiveEnd = mid - (effectiveHandleThickness / 2.0) - effectiveGap;
		const auto activeStart = mid + (effectiveHandleThickness / 2.0) + effectiveGap;

		if (value <= 0.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawRoundedRect(QRectF(trackX, from, trackThickness, length), radius, radius);
		} else if (value >= 1.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawRoundedRect(QRectF(trackX, from, trackThickness, length), radius, radius);
		} else {
			if (inactiveEnd > from) {
				const auto x = trackX;
				const auto y = from;
				const auto w = trackThickness;
				const auto h = inactiveEnd - from;
				const auto rTop = w / 2.0;
				const auto rBottom = (activeStart >= length) ? (w / 2.0) : 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(inactiveFg);
				if (h <= rTop || (rTop == rBottom && h <= w)) {
					p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rTop, y);
					path.quadTo(x + w, y, x + w, y + rTop);
					path.lineTo(x + w, y + h - rBottom);
					path.quadTo(x + w, y + h, x + w - rBottom, y + h);
					path.lineTo(x + rBottom, y + h);
					path.quadTo(x, y + h, x, y + h - rBottom);
					path.lineTo(x, y + rTop);
					path.quadTo(x, y, x + rTop, y);
					p.drawPath(path);
				}
			}

			if (activeStart < length) {
				const auto x = trackX;
				const auto y = activeStart;
				const auto w = trackThickness;
				const auto h = length - activeStart;
				const auto rTop = (inactiveEnd <= from) ? (w / 2.0) : 2.0;
				const auto rBottom = w / 2.0;

				p.setPen(Qt::NoPen);
				p.setBrush(activeFg);
				if (h <= rBottom || (rTop == rBottom && h <= w)) {
					p.drawRoundedRect(QRectF(x, y, w, h), h / 2.0, h / 2.0);
				} else {
					auto path = QPainterPath();
					path.moveTo(x + rTop, y);
					path.lineTo(x + w - rTop, y);
					path.quadTo(x + w, y, x + w, y + rTop);
					path.lineTo(x + w, y + h - rBottom);
					path.quadTo(x + w, y + h, x + w - rBottom, y + h);
					path.lineTo(x + rBottom, y + h);
					path.quadTo(x, y + h, x, y + h - rBottom);
					path.lineTo(x, y + rTop);
					path.quadTo(x, y, x + rTop, y);
					p.drawPath(path);
				}
			}
		}

		const auto dotCenterX = trackX + radius;
		const auto dotCenterY = from + radius;
		const auto dotRadius = 2.0;
		if (value < 0.95 && inactiveEnd - 4.0 > dotCenterY + dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}

		if (value > 0.05 && length - radius - dotRadius > activeStart + 4.0) {
			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			p.drawEllipse(QPointF(dotCenterX, length - radius), dotRadius, dotRadius);
		}

		if (markerSizeRatio > 0.0) {
			const auto hW = handleLength * markerSizeRatio;
			const auto hH = std::max(effectiveHandleThickness, 1.0);
			const auto hX = (width() - hW) / 2.0;
			const auto hY = mid - hH / 2.0;
			const auto hRadius = hH / 2.0;
			p.setPen(Qt::NoPen);
			p.setBrush(seekFg);
			p.drawRoundedRect(QRectF(hX, hY, hW, hH), hRadius, hRadius);
		}
	}

	if (!_dividers.empty()) {
		for (const auto &divValue : _dividers) {
			const auto dividerValue = horizontal
				? divValue
				: (1.0 - divValue);
			const auto dividerMid = from + dividerValue * length;
			const auto dist = std::abs(dividerMid - mid);
			if (dist > (effectiveHandleThickness / 2.0) + effectiveGap + 2.0) {
				const auto isPassed = (horizontal ? (dividerValue <= value) : (dividerValue >= value));
				p.setBrush(isPassed ? inactiveFg : activeFg);
				const auto dRadius = 2.0;
				if (horizontal) {
					p.drawEllipse(QPointF(dividerMid, (height() / 2.0)), dRadius, dRadius);
				} else {
					p.drawEllipse(QPointF((width() / 2.0), dividerMid), dRadius, dRadius);
				}
			}
		}
	}
}

not_null<MaterialSlider*> AddCardSlider(
		not_null<::Ui::VerticalLayout*> card,
		const style::margins &margin) {
	return card->add(
		object_ptr<MaterialSlider>(card),
		margin);
}

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
