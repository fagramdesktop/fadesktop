/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include "ui/rp_widget.h"
#include "ui/widgets/checkbox.h"

#include "ui/style/style_core_types.h"

class IconPackCheck final : public Ui::AbstractCheckView {
public:
	IconPackCheck(const style::icon *icon, bool checked);

	QSize getSize() const override;

	void paint(QPainter &p, int left, int top, int outerWidth) override;
	
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;
	
private:
	void checkedChangedHook(anim::type animated) override;
	
	const style::icon *_icon = nullptr;
	Ui::RadioView _radio;
};

class RoundnessPreview : public Ui::RpWidget
{
public:
  RoundnessPreview(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *e) override;
};

// thx rabbitGram