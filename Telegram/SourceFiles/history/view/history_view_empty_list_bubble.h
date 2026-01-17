/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Ui {
class ChatStyle;
} // namespace Ui

namespace HistoryView {

class EmptyListBubbleWidget : public Ui::RpWidget {
public:
	EmptyListBubbleWidget(
		not_null<Ui::RpWidget*> parent,
		not_null<const Ui::ChatStyle*> st,
		const style::margins &padding);

	void setText(const TextWithEntities &textWithEntities);
	void setForceWidth(int width);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void updateGeometry(const QSize &size);

	const style::margins &_padding;
	const not_null<const Ui::ChatStyle*> _st;
	Ui::Text::String _text;
	int _innerWidth = 0;
	int _forceWidth = 0;

};

} // namespace HistoryView
