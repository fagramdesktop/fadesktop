/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Layout {

struct DocumentGenericPreview final {
	static DocumentGenericPreview Create(DocumentData *document);
	const style::icon *icon() const;
	const int index;
	const style::color &color;
	const style::color &dark;
	const style::color &over;
	const style::color &selected;
	const QString ext;
};

// Ui::CachedRoundCorners DocumentCorners(int colorIndex);

} // namespace Layout
