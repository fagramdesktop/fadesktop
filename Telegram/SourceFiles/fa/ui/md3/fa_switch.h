/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/effects/animations.h"

#include <QtGui/QPainter>

namespace FA::Ui {

void PaintMd3Switch(
	QPainter &p,
	float64 x,
	float64 y,
	float64 toggled,
	float64 switchWidth = 48.0,
	float64 switchHeight = 28.0);

class MaterialSwitch : public ::Ui::RippleButton {
public:
	explicit MaterialSwitch(
		QWidget *parent = nullptr,
		bool checked = false);

	[[nodiscard]] bool checked() const {
		return _checked;
	}
	void setChecked(bool checked, anim::type animated = anim::type::normal);

	[[nodiscard]] rpl::producer<bool> checkedChanges() const {
		return _checkedChanges.events();
	}
	[[nodiscard]] rpl::producer<bool> checkedValue() const {
		return _checkedChanges.events_starting_with_copy(_checked);
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	QSize sizeHint() const override;

private:
	bool _checked = false;
	::Ui::Animations::Simple _animation;
	rpl::event_stream<bool> _checkedChanges;
};

} // namespace FA::Ui
