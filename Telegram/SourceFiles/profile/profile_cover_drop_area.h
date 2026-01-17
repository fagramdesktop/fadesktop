/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"
#include "ui/rp_widget.h"

namespace Profile {

class CoverDropArea : public Ui::RpWidget {
public:
	CoverDropArea(QWidget *parent, const QString &title, const QString &subtitle);

	void showAnimated();

	using HideFinishCallback = Fn<void(CoverDropArea*)>;
	void hideAnimated(HideFinishCallback &&callback);

	bool hiding() const {
		return _hiding;
	}

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void setupAnimation();

	QString _title, _subtitle;
	int _titleWidth, _subtitleWidth;

	QPixmap _cache;
	Ui::Animations::Simple _a_appearance;
	bool _hiding = false;
	HideFinishCallback _hideFinishCallback;

};

} // namespace Profile
