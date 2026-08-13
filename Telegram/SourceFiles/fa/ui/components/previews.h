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
	IconPackCheck(bool isMaterial, bool checked);

	QSize getSize() const override;

	void paint(QPainter &p, int left, int top, int outerWidth) override;
	
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;
	
private:
	bool _isMaterial;

};

class AvatarShapeCheck final : public Ui::AbstractCheckView {
public:
	AvatarShapeCheck(int shapeIndex, bool checked);

	QSize getSize() const override;

	void paint(QPainter &p, int left, int top, int outerWidth) override;
	
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;
	
private:
	int _shapeIndex = 0;

};

class RoundnessPreview : public Ui::RpWidget {
public:
	RoundnessPreview(QWidget *parent);

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;

};