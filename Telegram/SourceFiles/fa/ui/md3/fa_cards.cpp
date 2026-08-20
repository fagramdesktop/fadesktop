/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_cards.h"
#include "fa/ui/md3/fa_slider.h"

#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/round_rect.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/effects/ripple_animation.h"
#include "ui/effects/animation_value_f.h"
#include "styles/style_basic.h"
#include "styles/style_widgets.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_fa_styles.h"

#include <QtGui/QPainterPath>

namespace FA::Ui {

QPainterPath MakeSegmentPath(
		const QRectF &r,
		CardSegmentPosition pos,
		float64 largeRadius,
		float64 smallRadius) {
	const auto rtl = (pos == CardSegmentPosition::Top || pos == CardSegmentPosition::Single)
		? largeRadius
		: smallRadius;
	const auto rtr = (pos == CardSegmentPosition::Top || pos == CardSegmentPosition::Single)
		? largeRadius
		: smallRadius;
	const auto rbr = (pos == CardSegmentPosition::Bottom || pos == CardSegmentPosition::Single)
		? largeRadius
		: smallRadius;
	const auto rbl = (pos == CardSegmentPosition::Bottom || pos == CardSegmentPosition::Single)
		? largeRadius
		: smallRadius;

	auto path = QPainterPath();
	path.moveTo(r.left() + rtl, r.top());
	path.lineTo(r.right() - rtr, r.top());
	if (rtr > 0.) {
		path.arcTo(QRectF(r.right() - 2 * rtr, r.top(), 2 * rtr, 2 * rtr), 90, -90);
	}
	path.lineTo(r.right(), r.bottom() - rbr);
	if (rbr > 0.) {
		path.arcTo(QRectF(r.right() - 2 * rbr, r.bottom() - 2 * rbr, 2 * rbr, 2 * rbr), 0, -90);
	}
	path.lineTo(r.left() + rbl, r.bottom());
	if (rbl > 0.) {
		path.arcTo(QRectF(r.left(), r.bottom() - 2 * rbl, 2 * rbl, 2 * rbl), 270, -90);
	}
	path.lineTo(r.left(), r.top() + rtl);
	if (rtl > 0.) {
		path.arcTo(QRectF(r.left(), r.top(), 2 * rtl, 2 * rtl), 180, -90);
	}
	path.closeSubpath();
	return path;
}

QImage MakeSegmentMask(
		const QSize &size,
		CardSegmentPosition pos,
		int largeRadius,
		int smallRadius) {
	return ::Ui::RippleAnimation::MaskByDrawer(size, false, [&](QPainter &p) {
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		const auto r = QRectF(0, 0, size.width(), size.height());
		p.drawPath(MakeSegmentPath(r, pos, largeRadius, smallRadius));
	});
}

bool IsRowVisible(const QWidget *w) {
	if (!w || w->isHidden() || w->height() <= 0) {
		return false;
	}
	if (const auto slide = dynamic_cast<const ::Ui::SlideWrap<::Ui::RpWidget>*>(w)) {
		if (!slide->toggled() && !slide->animating()) {
			return false;
		}
	}
	if (w->inherits("CardDividerWidget")
		|| w->metaObject()->className() == QStringView(u"FA::Ui::(anonymous namespace)::CardDividerWidget")) {
		return false;
	}
	if (w->height() < 20 && w->children().isEmpty()) {
		return false;
	}
	return true;
}

CardSegmentPosition FindSegmentPosition(const QWidget *widget) {
	if (!widget) {
		return CardSegmentPosition::Single;
	}
	const QWidget *card = nullptr;
	for (auto w = widget->parentWidget(); w != nullptr; w = w->parentWidget()) {
		if (w->property("is_fa_card").toBool()) {
			card = w;
			break;
		}
	}
	if (!card) {
		return CardSegmentPosition::Single;
	}

	std::vector<const QWidget*> rows;
	for (const auto child : card->children()) {
		if (const auto layout = dynamic_cast<const ::Ui::VerticalLayout*>(child)) {
			for (const auto rowChild : layout->children()) {
				if (const auto w = qobject_cast<const QWidget*>(rowChild)) {
					if (IsRowVisible(w)) {
						rows.push_back(w);
					}
				}
			}
			break;
		}
	}

	if (rows.size() <= 1) {
		return CardSegmentPosition::Single;
	}

	std::sort(rows.begin(), rows.end(), [](const auto a, const auto b) {
		return a->y() < b->y();
	});

	int targetIndex = -1;
	for (size_t i = 0; i < rows.size(); ++i) {
		const auto r = rows[i];
		if (r == widget) {
			targetIndex = int(i);
			break;
		}
		for (auto w = widget->parentWidget(); w != nullptr && w != card; w = w->parentWidget()) {
			if (w == r) {
				targetIndex = int(i);
				break;
			}
		}
		if (targetIndex != -1) {
			break;
		}
	}

	if (targetIndex == -1) {
		return CardSegmentPosition::Single;
	}
	if (targetIndex == 0) {
		return CardSegmentPosition::Top;
	} else if (targetIndex == int(rows.size()) - 1) {
		return CardSegmentPosition::Bottom;
	}
	return CardSegmentPosition::Middle;
}

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
		if (height() <= 0 || !_layout) {
			return;
		}
		struct VisibleItem {
			const QWidget *widget = nullptr;
			int y = 0;
			int height = 0;
		};
		std::vector<VisibleItem> visibleItems;
		for (const auto child : _layout->children()) {
			if (const auto w = qobject_cast<const QWidget*>(child)) {
				if (IsRowVisible(w)) {
					visibleItems.push_back({
						w,
						w->y(),
						w->height(),
					});
				}
			}
		}
		if (visibleItems.empty()) {
			return;
		}

		std::sort(visibleItems.begin(), visibleItems.end(), [](const auto &a, const auto &b) {
			return a.y < b.y;
		});

		Painter p(this);
		PainterHighQualityEnabler hq(p);
		p.setPen(Qt::NoPen);
		p.setBrush(st::settingsThemeNotSupportedBg);

		const auto N = visibleItems.size();
		for (size_t i = 0; i < N; ++i) {
			const auto pos = (N == 1)
				? CardSegmentPosition::Single
				: (i == 0)
				? CardSegmentPosition::Top
				: (i == N - 1)
				? CardSegmentPosition::Bottom
				: CardSegmentPosition::Middle;

			const auto &item = visibleItems[i];
			const auto topGap = (i == 0) ? 0.0 : 1.0;
			const auto bottomGap = (i == N - 1) ? 0.0 : 1.0;
			const auto r = QRectF(
				0.5,
				item.y + topGap + 0.5,
				width() - 1.0,
				item.height - topGap - bottomGap - 1.0);

			p.drawPath(MakeSegmentPath(r, pos, 24.0, 4.0));
		}
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
		paintRipple(p, 0, 0);

		const auto paddingLeft = 16;
		const auto paddingRight = 16;
		const auto switchWidth = 48.0;
		const auto switchHeight = 28.0;
		const auto toggleX = width() - paddingRight - switchWidth;
		const auto toggleY = (height() - switchHeight) / 2.0;

		const auto toggled = _animation.value(_checked ? 1.0 : 0.0);

		PaintMd3Switch(p, toggleX, toggleY, toggled, switchWidth, switchHeight);

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
			const auto titleFont = st::semiboldFont;
			const auto subtitleFont = DescriptionFont();

			const auto titleH = titleFont->height;
			const auto subH = QFontMetrics(subtitleFont).height();
			const auto spacing = 3;
			const auto totalH = titleH + spacing + subH;
			const auto startY = (height() - totalH) / 2;

			p.setFont(titleFont);
			p.setPen(st::windowFg);
			p.drawText(
				QRect(paddingLeft, startY, availableTextWidth, titleH),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
				_titleText);

			p.setFont(subtitleFont);
			p.setPen(st::windowSubTextFg);
			p.drawText(
				QRect(paddingLeft, startY + titleH + spacing, availableTextWidth, subH),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
				_subtitleText);
		}
	}

	QImage prepareRippleMask() const override {
		const auto pos = FindSegmentPosition(this);
		return MakeSegmentMask(size(), pos, 24, 4);
	}

	QPoint prepareRippleStartPosition() const override {
		return mapFromGlobal(QCursor::pos());
	}

private:
	int computeHeight(int w) const {
		if (_subtitleText.trimmed().isEmpty()) {
			return 56;
		}
		return 68;
	}

	void updateHeight() {
		resize(width(), computeHeight(width()));
		update();
	}

	Fn<void(bool)> _onToggle;
	QString _titleText;
	QString _subtitleText;
	bool _checked = false;
	bool _hasValue = false;
	::Ui::Animations::Simple _animation;
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

		resize(width(), 56);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 56;
	}

	void paintEvent(QPaintEvent *e) override {
		Painter p(this);
		PainterHighQualityEnabler hq(p);

		paintRipple(p, 0, 0);

		auto textLeft = 16;
		if (_icon) {
			const auto iconY = (height() - _icon->height()) / 2;
			_icon->paint(p, 16, iconY, width());
			textLeft = 16 + _icon->width() + 14;
		}

		auto textRight = width() - 16;

		if (_showChevron) {
			const auto chevronW = 6;
			const auto chevronH = 10;
			const auto cx = width() - 16 - chevronW;
			const auto cy = (height() - chevronH) / 2;

			QPainterPath chevron;
			chevron.moveTo(cx, cy);
			chevron.lineTo(cx + chevronW, cy + chevronH / 2.0);
			chevron.lineTo(cx, cy + chevronH);

			p.setPen(QPen(st::windowSubTextFg->c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.setBrush(Qt::NoBrush);
			p.drawPath(chevron);

			textRight = cx - 10;
		}

		if (!_rightLabelText.isEmpty()) {
			p.setFont(DescriptionFont());
			p.setPen(st::windowSubTextFg);
			const auto rightLabelW = QFontMetrics(DescriptionFont()).horizontalAdvance(_rightLabelText);
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
		const auto pos = FindSegmentPosition(this);
		return MakeSegmentMask(size(), pos, 24, 4);
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

		resize(width(), 52);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 52;
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
		const auto pos = FindSegmentPosition(this);
		return MakeSegmentMask(size(), pos, 24, 4);
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

class CardSliderRowWidget final : public ::Ui::RpWidget {
public:
	CardSliderRowWidget(QWidget *parent)
	: RpWidget(parent)
	, _label(::Ui::CreateChild<::Ui::LabelSimple>(this, st::settingsAudioVolumeLabel))
	, _slider(::Ui::CreateChild<MaterialSlider>(this))
	, _reset(::Ui::CreateChild<::Ui::IconButton>(this, st::settingsSliderRestore)) {
		_label->setAttribute(Qt::WA_TransparentForMouseEvents);
		resize(width(), 74);
	}

	[[nodiscard]] not_null<::Ui::LabelSimple*> label() const { return _label; }
	[[nodiscard]] not_null<MaterialSlider*> slider() const { return _slider; }
	[[nodiscard]] not_null<::Ui::IconButton*> reset() const { return _reset; }

protected:
	int resizeGetHeight(int newWidth) override {
		return 74;
	}

	void resizeEvent(QResizeEvent *e) override {
		const auto left = 16;
		const auto right = 16;
		const auto top = 8;
		const auto labelH = _label->height();
		_label->moveToLeft(left, top);
		_reset->moveToRight(right, top - (_reset->height() - labelH) / 2, width());
		const auto sliderTop = top + labelH + 4;
		const auto sliderW = width() - left - right;
		_slider->setGeometry(left, sliderTop, sliderW, 32);
	}

private:
	not_null<::Ui::LabelSimple*> _label;
	not_null<MaterialSlider*> _slider;
	not_null<::Ui::IconButton*> _reset;
};

class CardDividerWidget final : public ::Ui::RpWidget {
public:
	explicit CardDividerWidget(QWidget *parent) : RpWidget(parent) {
		resize(width(), 0);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		return 0;
	}

	void paintEvent(QPaintEvent *e) override {
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
		int bottomMargin,
		int sideMargin) {
	const auto cardSurface = container->add(
		object_ptr<CardContainerWidget>(container),
		style::margins(sideMargin, topMargin, sideMargin, bottomMargin));
	
	const auto layout = ::Ui::CreateChild<::Ui::VerticalLayout>(cardSurface);
	cardSurface->setInnerLayout(layout, container);
	
	cardSurface->widthValue(
	) | rpl::on_next([layout](int w) {
		if (w > 0) {
			layout->move(0, 0);
			layout->resizeToWidth(w);
		}
	}, layout->lifetime());

	layout->heightValue(
	) | rpl::on_next([cardSurface](int height) {
		if (height > 0) {
			cardSurface->updateGeometryFromInner();
		}
	}, layout->lifetime());

	return layout;
}

not_null<::Ui::RpWidget*> CreateCardToggle(
		not_null<QWidget*> parent,
		rpl::producer<QString> title,
		rpl::producer<QString> subtitle,
		rpl::producer<bool> value,
		Fn<void(bool)> onToggle) {
	return ::Ui::CreateChild<CardToggleRow>(
		parent.get(),
		std::move(title),
		std::move(subtitle),
		std::move(value),
		std::move(onToggle));
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

CardSliderRowControls AddCardSliderRow(
		not_null<::Ui::VerticalLayout*> card,
		const style::margins &margin) {
	const auto row = card->add(
		object_ptr<CardSliderRowWidget>(card),
		margin);
	return {
		.label = row->label(),
		.slider = row->slider(),
		.reset = row->reset(),
		.row = row,
	};
}

void AddCardDivider(not_null<::Ui::VerticalLayout*> card) {
	card->add(object_ptr<CardDividerWidget>(card));
}

not_null<::Ui::RippleButton*> CreateCardRippleButton(
		not_null<QWidget*> parent,
		int radius,
		bool paintHover) {
	class CardRippleButton final : public ::Ui::RippleButton {
	public:
		CardRippleButton(QWidget *parent, int radius, bool paintHover)
		: RippleButton(parent, st::defaultSettingsButton.ripple)
		, _radius(radius)
		, _paintHover(paintHover) {
		}

	protected:
		void paintEvent(QPaintEvent *e) override {
			Painter p(this);
			const auto paintOver = (isOver() || isDown()) && !isDisabled();
			if (_paintHover && paintOver) {
				PainterHighQualityEnabler hq(p);
				p.setPen(Qt::NoPen);
				p.setBrush(st::windowBgOver);
				const auto pos = FindSegmentPosition(this);
				const auto r = QRectF(0.5, 0.5, width() - 1.0, height() - 1.0);
				p.drawPath(MakeSegmentPath(r, pos, 24.0, 4.0));
			}
			paintRipple(p, 0, 0);
		}

		QImage prepareRippleMask() const override {
			const auto pos = FindSegmentPosition(this);
			return MakeSegmentMask(size(), pos, 24, 4);
		}

		QPoint prepareRippleStartPosition() const override {
			return mapFromGlobal(QCursor::pos());
		}

	private:
		int _radius = 24;
		bool _paintHover = false;
	};

	return ::Ui::CreateChild<CardRippleButton>(parent.get(), radius, paintHover);
}

} // namespace FA::Ui
