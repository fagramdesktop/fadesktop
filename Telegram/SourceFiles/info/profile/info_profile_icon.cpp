/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_icon.h"

namespace Info {
namespace Profile {

FloatingIcon::FloatingIcon(
	RpWidget *parent,
	const style::icon &icon,
	QPoint position)
: FloatingIcon(parent, icon, position, Tag{}) {
}

FloatingIcon::FloatingIcon(
	RpWidget *parent,
	const style::icon &icon,
	QPoint position,
	const Tag &)
: RpWidget(parent)
, _icon(&icon)
, _point(position) {
	setGeometry(QRect(
		QPoint(0, 0),
		QSize(_point.x() + _icon->width(), _point.y() + _icon->height())));
	setAttribute(Qt::WA_TransparentForMouseEvents);
}

void FloatingIcon::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);
	_icon->paint(p, _point, width());
}

} // namespace Profile
} // namespace Info
