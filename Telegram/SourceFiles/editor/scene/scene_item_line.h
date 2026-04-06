/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "editor/scene/scene_item_base.h"

namespace Editor {

class ItemLine : public NumberedItem {
public:
	ItemLine(QPixmap &&pixmap);
	QRectF boundingRect() const override;
	void paint(
		QPainter *p,
		const QStyleOptionGraphicsItem *option,
		QWidget *widget) override;
	[[nodiscard]] const QPixmap &pixmap() const;
	void setPixmap(QPixmap pixmap);
	bool applyEraser(const QPixmap &mask, const QPointF &maskPos);

	bool hasState(SaveState state) const override;
	void save(SaveState state) override;
	void restore(SaveState state) override;
protected:
	bool collidesWithItem(
		const QGraphicsItem *,
		Qt::ItemSelectionMode) const override;
	bool collidesWithPath(
		const QPainterPath &,
		Qt::ItemSelectionMode) const override;
private:
	QPixmap _pixmap;
	const QRectF _rect;

	struct {
		bool saved = false;
		NumberedItem::Status status;
	} _saved, _keeped;

};

} // namespace Editor
