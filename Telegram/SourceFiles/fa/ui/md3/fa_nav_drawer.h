/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"

namespace FA::Ui {

class NavDrawerButton : public ::Ui::SettingsButton {
public:
	NavDrawerButton(
		QWidget *parent,
		rpl::producer<QString> text,
		const style::SettingsButton &st,
		Settings::IconDescriptor &&descriptor = {});
	NavDrawerButton(
		QWidget *parent,
		rpl::producer<TextWithEntities> text,
		const style::SettingsButton &st,
		Settings::IconDescriptor &&descriptor = {});

	void setIcon(const style::icon *icon);
	void setDescriptor(Settings::IconDescriptor &&descriptor);

	NavDrawerButton *toggleOn(
		rpl::producer<bool> &&toggled,
		bool ignoreClick = false);

	int resizeGetHeight(int newWidth) override;

protected:
	void paintEvent(QPaintEvent *e) override;
	QImage prepareRippleMask() const override;
	QPoint prepareRippleStartPosition() const override;

private:
	void initToggleAnimation();

	Settings::IconDescriptor _descriptor;
	const style::icon *_icon = nullptr;
	::Ui::Animations::Simple _toggleAnimation;
	bool _hasToggled = false;

};

[[nodiscard]] object_ptr<NavDrawerButton> CreateNavDrawerButton(
	not_null<QWidget*> parent,
	rpl::producer<QString> text,
	const style::SettingsButton &st,
	Settings::IconDescriptor &&descriptor = {});

not_null<NavDrawerButton*> AddNavDrawerButton(
	not_null<::Ui::VerticalLayout*> container,
	rpl::producer<QString> text,
	const style::SettingsButton &st,
	Settings::IconDescriptor &&descriptor = {});

not_null<::Ui::RpWidget*> AddNavDrawerDivider(
	not_null<::Ui::VerticalLayout*> container);

} // namespace FA::Ui
