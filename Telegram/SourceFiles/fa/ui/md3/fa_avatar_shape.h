/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtGui/QImage>
#include <QtGui/QColor>
#include <QtCore/QSize>

namespace FA::Ui {

[[nodiscard]] QImage MaterialShapeMask(QSize size, int shapeIndex = -1);

[[nodiscard]] QImage MaterialShapeOutline(
	QSize size,
	int shapeIndex = -1,
	QColor color = QColor(),
	float strokeWidth = 0.0f);

[[nodiscard]] QImage ApplyMaterialShape(QImage image, int shapeIndex = -1);

} // namespace FA::Ui
