/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/ui/components/fa_ui_components.h"

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

protected:
	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		PainterHighQualityEnabler hq(p);
		constexpr auto kRadius = 14.0;
		const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);

		p.setPen(Qt::NoPen);
		p.setBrush(st::settingsThemeNotSupportedBg);
		p.drawRoundedRect(r, kRadius, kRadius);
	}
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
			setChecked(checked, anim::type::normal);
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
		if (_checked == checked) {
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
	
	cardSurface->widthValue(
	) | rpl::on_next([layout](int width) {
		layout->resizeToWidth(width);
	}, layout->lifetime());

	layout->heightValue(
	) | rpl::on_next([cardSurface](int height) {
		cardSurface->resize(cardSurface->width(), height);
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
	resize(width(), 24);
}

int MaterialSlider::resizeGetHeight(int newWidth) {
	return 24;
}

QSize MaterialSlider::getSeekDecreaseSize() const {
	return QSize();
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

	const auto value = horizontal
		? getCurrentValue()
		: (1. - getCurrentValue());

	const auto from = 0.0;
	const auto length = float64(horizontal ? width() : height());
	const auto mid = from + value * length;

	const auto activeFg = disabled
		? st::windowSubTextFg->c
		: st::windowActiveTextFg->c;
	const auto inactiveFg = disabled
		? st::windowBg->c
		: st::sliderBgInactive->c;

	const auto gap = 4.0;
	const auto halfGap = gap / 2.0;

	if (horizontal) {
		const auto trackY = (height() - trackThickness) / 2.0;
		const auto activeEnd = mid - halfGap;
		const auto inactiveStart = mid + halfGap;

		if (activeEnd > from) {
			const auto x = from;
			const auto y = trackY;
			const auto w = activeEnd - from;
			const auto h = trackThickness;
			const auto rOuter = h / 2.0;
			const auto rInner = 2.0;

			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			if (w <= rOuter) {
				p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
			} else {
				auto path = QPainterPath();
				path.moveTo(x + rOuter, y);
				path.lineTo(x + w - rInner, y);
				path.quadTo(x + w, y, x + w, y + rInner);
				path.lineTo(x + w, y + h - rInner);
				path.quadTo(x + w, y + h, x + w - rInner, y + h);
				path.lineTo(x + rOuter, y + h);
				path.quadTo(x, y + h, x, y + h - rOuter);
				path.lineTo(x, y + rOuter);
				path.quadTo(x, y, x + rOuter, y);
				p.drawPath(path);
			}
		}

		if (inactiveStart < length) {
			const auto x = inactiveStart;
			const auto y = trackY;
			const auto w = length - inactiveStart;
			const auto h = trackThickness;
			const auto rOuter = h / 2.0;
			const auto rInner = 2.0;

			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			if (w <= rOuter) {
				p.drawRoundedRect(QRectF(x, y, w, h), w / 2.0, w / 2.0);
			} else {
				auto path = QPainterPath();
				path.moveTo(x + rInner, y);
				path.lineTo(x + w - rOuter, y);
				path.quadTo(x + w, y, x + w, y + rOuter);
				path.lineTo(x + w, y + h - rOuter);
				path.quadTo(x + w, y + h, x + w - rOuter, y + h);
				path.lineTo(x + rInner, y + h);
				path.quadTo(x, y + h, x, y + h - rInner);
				path.lineTo(x, y + rInner);
				path.quadTo(x, y, x + rInner, y);
				p.drawPath(path);
			}
		}

		const auto dotCenterX = length - radius;
		const auto dotCenterY = trackY + radius;
		const auto dotRadius = 2.0;
		if (inactiveStart + 4.0 < dotCenterX - dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}
	} else {
		const auto trackX = (width() - trackThickness) / 2.0;
		const auto inactiveEnd = mid - halfGap;
		const auto activeStart = mid + halfGap;

		if (inactiveEnd > from) {
			const auto x = trackX;
			const auto y = from;
			const auto w = trackThickness;
			const auto h = inactiveEnd - from;
			const auto rOuter = w / 2.0;
			const auto rInner = 2.0;

			p.setPen(Qt::NoPen);
			p.setBrush(inactiveFg);
			if (h <= rOuter) {
				p.drawRoundedRect(QRectF(x, y, w, h), h / 2.0, h / 2.0);
			} else {
				auto path = QPainterPath();
				path.moveTo(x + rOuter, y);
				path.quadTo(x + w, y, x + w, y + rOuter);
				path.lineTo(x + w, y + h - rInner);
				path.quadTo(x + w, y + h, x + w - rInner, y + h);
				path.lineTo(x + rInner, y + h);
				path.quadTo(x, y + h, x, y + h - rInner);
				path.lineTo(x, y + rOuter);
				path.quadTo(x, y, x + rOuter, y);
				p.drawPath(path);
			}
		}

		if (activeStart < length) {
			const auto x = trackX;
			const auto y = activeStart;
			const auto w = trackThickness;
			const auto h = length - activeStart;
			const auto rOuter = w / 2.0;
			const auto rInner = 2.0;

			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			if (h <= rOuter) {
				p.drawRoundedRect(QRectF(x, y, w, h), h / 2.0, h / 2.0);
			} else {
				auto path = QPainterPath();
				path.moveTo(x + rInner, y);
				path.lineTo(x + w - rInner, y);
				path.quadTo(x + w, y, x + w, y + rInner);
				path.lineTo(x + w, y + h - rOuter);
				path.quadTo(x + w, y + h, x + w - rOuter, y + h);
				path.lineTo(x + rOuter, y + h);
				path.quadTo(x, y + h, x, y + h - rOuter);
				path.lineTo(x, y + rInner);
				path.quadTo(x, y, x + rInner, y);
				p.drawPath(path);
			}
		}

		const auto dotCenterX = trackX + radius;
		const auto dotCenterY = from + radius;
		const auto dotRadius = 2.0;
		if (inactiveEnd - 4.0 > dotCenterY + dotRadius) {
			p.setPen(Qt::NoPen);
			p.setBrush(activeFg);
			p.drawEllipse(QPointF(dotCenterX, dotCenterY), dotRadius, dotRadius);
		}
	}

	if (!_dividers.empty()) {
		for (const auto &divValue : _dividers) {
			const auto dividerValue = horizontal
				? divValue
				: (1.0 - divValue);
			const auto dividerMid = from + dividerValue * length;
			const auto dist = std::abs(dividerMid - mid);
			if (dist > halfGap + 2.0) {
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

} // namespace FA::Ui
