/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class StickerPremiumMark;

namespace Overview::Layout {

class ItemBase;

class Delegate {
public:
	virtual void registerHeavyItem(not_null<const ItemBase*> item) = 0;
	virtual void unregisterHeavyItem(not_null<const ItemBase*> item) = 0;
	virtual void repaintItem(not_null<const ItemBase*> item) = 0;
	virtual bool itemVisible(not_null<const ItemBase*> item) = 0;

	[[nodiscard]] virtual not_null<StickerPremiumMark*> hiddenMark() = 0;

	virtual void openPhoto(not_null<PhotoData*> photo, FullMsgId id) = 0;
	virtual void openDocument(
		not_null<DocumentData*> document,
		FullMsgId id,
		bool showInMediaView = false) = 0;

};

} // namespace Overview::Layout
