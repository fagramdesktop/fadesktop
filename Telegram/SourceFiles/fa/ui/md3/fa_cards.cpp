/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_cards.h"

#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/round_rect.h"
#include "ui/effects/ripple_animation.h"
#include "ui/effects/animation_value_f.h"
#include "styles/style_basic.h"
#include "styles/style_widgets.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

namespace FA::Ui {
namespace {

class CardContainerWidget final : public ::Ui::RpWidget {
public:
	explicit CardContainerWidget(QWidget *parent) : RpWidget(parent) {
		setProperty("is_fa_card", true);
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
			_layout->move(0, 0);
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
			_layout->move(0, 0);
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

		const auto paintOver = (isOver() || isDown()) && !isDisabled();
		if (paintOver) {
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgOver);
			const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
			p.drawRoundedRect(r, 14.0, 14.0);
		}

		paintRipple(p, 0, 0);

		const auto paddingLeft = 16;
		const auto paddingRight = 16;
		const auto switchWidth = 46.0;
		const auto switchHeight = 26.0;
		const auto toggleX = width() - paddingRight - switchWidth;
		const auto toggleY = (height() - switchHeight) / 2.0;

		const auto toggled = _animation.value(_checked ? 1.0 : 0.0);

		if (toggled > 0.0) {
			p.setOpacity(toggled);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowActiveTextFg);
			p.drawRoundedRect(
				QRectF(toggleX, toggleY, switchWidth, switchHeight),
				switchHeight / 2.0,
				switchHeight / 2.0);
		}

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

		const auto thumbDiameter = anim::interpolateF(14.0, 20.0, toggled);
		const auto thumbX = anim::interpolateF(toggleX + 5.0, toggleX + switchWidth - 3.0 - 20.0, toggled);
		const auto thumbY = toggleY + (switchHeight - thumbDiameter) / 2.0;
		const auto thumbColor = anim::color(st::windowSubTextFg->c, st::windowBg->c, toggled);

		p.setPen(Qt::NoPen);
		p.setBrush(thumbColor);
		p.drawEllipse(QRectF(thumbX, thumbY, thumbDiameter, thumbDiameter));

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

		const auto paintOver = (isOver() || isDown()) && !isDisabled();
		if (paintOver) {
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgOver);
			const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
			p.drawRoundedRect(r, 14.0, 14.0);
		}

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

		const auto paintOver = (isOver() || isDown()) && !isDisabled();
		if (paintOver) {
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgOver);
			const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
			p.drawRoundedRect(r, 14.0, 14.0);
		}

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

void AddCardDescription(
		not_null<::Ui::VerticalLayout*> container,
		rpl::producer<QString> text) {
	AddCardDescription(
		container,
		std::move(text) | rpl::map([](const QString &s) {
			return TextWithEntities{ s };
		}));
}

void AddCardDescription(
		not_null<::Ui::VerticalLayout*> container,
		rpl::producer<TextWithEntities> text) {
	const auto label = container->add(
		object_ptr<::Ui::FlatLabel>(
			container,
			std::move(text),
			st::boxDividerLabel),
		style::margins(22, 4, 22, 8));
	label->setTextColorOverride(st::windowSubTextFg->c);
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
	
	cardSurface->sizeValue(
	) | rpl::on_next([layout](const QSize &s) {
		layout->setGeometry(0, 0, s.width(), s.height());
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

not_null<::Ui::RippleButton*> CreateCardRippleButton(
		not_null<QWidget*> parent,
		int radius) {
	class CardRippleButton final : public ::Ui::RippleButton {
	public:
		CardRippleButton(QWidget *parent, int radius)
		: RippleButton(parent, st::defaultSettingsButton.ripple)
		, _radius(radius) {
		}

	protected:
		void paintEvent(QPaintEvent *e) override {
			Painter p(this);
			const auto paintOver = (isOver() || isDown()) && !isDisabled();
			if (paintOver) {
				PainterHighQualityEnabler hq(p);
				p.setPen(Qt::NoPen);
				p.setBrush(st::windowBgOver);
				const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
				p.drawRoundedRect(r, _radius, _radius);
			}
			paintRipple(p, 0, 0);
		}

		QImage prepareRippleMask() const override {
			return ::Ui::RippleAnimation::RoundRectMask(size(), _radius);
		}

		QPoint prepareRippleStartPosition() const override {
			return mapFromGlobal(QCursor::pos());
		}

	private:
		int _radius = 14;
	};

	return ::Ui::CreateChild<CardRippleButton>(parent.get(), radius);
}

} // namespace FA::Ui
