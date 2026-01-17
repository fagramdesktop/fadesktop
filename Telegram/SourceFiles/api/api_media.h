/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class HistoryItem;

namespace Api {

struct RemoteFileInfo;

MTPInputMedia PrepareUploadedPhoto(
	not_null<HistoryItem*> item,
	RemoteFileInfo info);

MTPInputMedia PrepareUploadedDocument(
	not_null<HistoryItem*> item,
	RemoteFileInfo info);

bool HasAttachedStickers(MTPInputMedia media);

} // namespace Api
