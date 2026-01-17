/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/binary_guard.h"

class DocumentData;
class HistoryItem;

namespace Window {
class SessionController;
} // namespace Window

namespace Data {

class DocumentMedia;

extern const char kOptionExternalVideoPlayer[];

base::binary_guard ReadBackgroundImageAsync(
	not_null<Data::DocumentMedia*> media,
	FnMut<QImage(QImage)> postprocess,
	FnMut<void(QImage&&)> done);

void ResolveDocument(
	Window::SessionController *controller,
	not_null<DocumentData*> document,
	HistoryItem *item,
	MsgId topicRootId,
	PeerId monoforumPeerId);

} // namespace Data
