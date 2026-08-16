/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "styles/style_settings.h"

namespace FA::Ui {

[[nodiscard]] QFont DescriptionFont();

void AddModernSectionHeader(
	not_null<::Ui::VerticalLayout*> container,
	rpl::producer<QString> title);

void AddCardDescription(
	not_null<::Ui::VerticalLayout*> container,
	rpl::producer<QString> text);

void AddCardDescription(
	not_null<::Ui::VerticalLayout*> container,
	rpl::producer<TextWithEntities> text);

not_null<::Ui::VerticalLayout*> CreateCardContainer(
	not_null<::Ui::VerticalLayout*> container,
	int topMargin = 0,
	int bottomMargin = 8);

not_null<::Ui::RpWidget*> AddCardToggle(
	not_null<::Ui::VerticalLayout*> card,
	rpl::producer<QString> title,
	rpl::producer<QString> subtitle,
	rpl::producer<bool> value,
	Fn<void(bool)> onToggle);

not_null<::Ui::RpWidget*> AddCardButton(
	not_null<::Ui::VerticalLayout*> card,
	rpl::producer<QString> title,
	Fn<void()> onClick,
	const style::icon *icon = nullptr,
	rpl::producer<QString> rightLabel = nullptr,
	bool showChevron = false);

not_null<::Ui::RpWidget*> AddCardRadio(
	not_null<::Ui::VerticalLayout*> card,
	const std::shared_ptr<::Ui::RadiobuttonGroup> &group,
	int value,
	rpl::producer<QString> title);

void AddCardDivider(not_null<::Ui::VerticalLayout*> card);

[[nodiscard]] not_null<::Ui::RippleButton*> CreateCardRippleButton(
	not_null<QWidget*> parent,
	int radius = 14,
	bool paintHover = false);

} // namespace FA::Ui
