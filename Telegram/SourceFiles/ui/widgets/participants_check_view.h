/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/widgets/checkbox.h"

namespace Ui {

class ParticipantsCheckView : public Ui::AbstractCheckView {
public:
	ParticipantsCheckView(
		int count,
		int duration,
		bool checked,
		Fn<void()> updateCallback);

	[[nodiscard]] static QSize ComputeSize(int count);

	QSize getSize() const override;

	void paint(QPainter &p, int left, int top, int outerWidth) override;
	QImage prepareRippleMask() const override;
	bool checkRippleStartPosition(QPoint position) const override;

	~ParticipantsCheckView();

private:
	const QString _text;
	const int _count;
	void checkedChangedHook(anim::type animated) override;

};

} // namespace Ui
