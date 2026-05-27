/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QMimeData>
#include <QtGui/QImage>

class PhotoData;

namespace HistoryView {

class DragMimeData final : public QMimeData {
public:
	explicit DragMimeData(QString tempPath = QString());
	~DragMimeData();

private:
	const QString _tempPath;

};

struct PhotoDragData {
	QImage image;
	QString tempPath;
};

[[nodiscard]] PhotoDragData PreparePhotoDragData(
	not_null<PhotoData*> photo,
	TimeId itemDate);

} // namespace HistoryView
