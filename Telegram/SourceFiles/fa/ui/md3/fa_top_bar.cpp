/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/ui/md3/fa_top_bar.h"

#include "ui/painter.h"
#include "ui/wrap/fade_wrap.h"
#include "styles/style_settings.h"

namespace FA::Ui {
namespace {

constexpr auto kPillMarginRight = 12;
constexpr auto kPillPadding = 4;
constexpr auto kButtonWidth = 36;
constexpr auto kPillHeight = 36;

bool IsButtonVisible(const ::Ui::RpWidget *button) {
	if (!button || button->isHidden()) {
		return false;
	}
	if (const auto fade = dynamic_cast<const ::Ui::FadeWrap<::Ui::RpWidget>*>(button)) {
		return fade->toggled() || (fade->animating() && fade->width() > 0);
	}
	return button->width() > 0;
}

} // namespace

int LayoutTopBarPillButtons(
		int newWidth,
		int topBarHeight,
		const std::vector<base::unique_qptr<::Ui::RpWidget>> &buttons) {
	auto visibleButtonsCount = 0;
	for (const auto &button : buttons) {
		if (IsButtonVisible(button.get())) {
			++visibleButtonsCount;
		}
	}
	const auto pillTop = (topBarHeight - kPillHeight) / 2;
	const auto totalPillWidth = (visibleButtonsCount > 0)
		? (visibleButtonsCount * kButtonWidth + kPillPadding * 2)
		: 0;

	auto currentRight = kPillMarginRight + kPillPadding;
	for (const auto &button : buttons) {
		if (!button) {
			continue;
		}
		if (IsButtonVisible(button.get())) {
			button->setGeometry(
				newWidth - currentRight - kButtonWidth,
				pillTop,
				kButtonWidth,
				kPillHeight);
			if (const auto wrap = dynamic_cast<::Ui::Wrap<::Ui::RpWidget>*>(button.get())) {
				if (const auto wrapped = wrap->wrapped()) {
					wrapped->setGeometry(0, 0, kButtonWidth, kPillHeight);
				}
			}
			currentRight += kButtonWidth;
		} else {
			button->setGeometry(
				newWidth - kPillMarginRight,
				pillTop,
				0,
				kPillHeight);
		}
	}
	return (visibleButtonsCount > 0)
		? (kPillMarginRight + totalPillWidth + 12)
		: 0;
}

void PaintTopBarPill(
		QPainter &p,
		int widgetWidth,
		int topBarHeight,
		const std::vector<base::unique_qptr<::Ui::RpWidget>> &buttons,
		bool searchModeEnabled,
		bool selectionMode) {
	if (searchModeEnabled || selectionMode) {
		return;
	}
	auto visibleButtonsCount = 0;
	for (const auto &button : buttons) {
		if (IsButtonVisible(button.get())) {
			++visibleButtonsCount;
		}
	}
	if (visibleButtonsCount <= 0) {
		return;
	}

	const auto pillTop = (topBarHeight - kPillHeight) / 2;
	const auto totalPillWidth = visibleButtonsCount * kButtonWidth + kPillPadding * 2;
	const auto pillRect = QRectF(
		widgetWidth - kPillMarginRight - totalPillWidth,
		pillTop,
		totalPillWidth,
		kPillHeight);

	PainterHighQualityEnabler hq(p);
	p.setPen(Qt::NoPen);
	p.setBrush(st::settingsThemeNotSupportedBg);
	p.drawRoundedRect(pillRect, kPillHeight / 2.0, kPillHeight / 2.0);
}

} // namespace FA::Ui
