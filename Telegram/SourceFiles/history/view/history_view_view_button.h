/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/chat/chat_style.h"

namespace Data {
class Media;
} // namespace Data

struct WebPageData;

namespace HistoryView {

struct TextState;

class ViewButton {
public:
	ViewButton(
		not_null<Data::Media*> media,
		uint8 colorIndex,
		Fn<void()> updateCallback);
	~ViewButton();

	[[nodiscard]] static bool MediaHasViewButton(
		not_null<Data::Media*> media);

	[[nodiscard]] int height() const;
	[[nodiscard]] bool belowMessageInfo() const;

	void draw(
		Painter &p,
		const QRect &r,
		const Ui::ChatPaintContext &context);

	[[nodiscard]] const ClickHandlerPtr &link() const;
	bool checkLink(const ClickHandlerPtr &other, bool pressed);

	[[nodiscard]] QRect countRect(const QRect &r) const;

	[[nodiscard]] bool getState(
		QPoint point,
		const QRect &g,
		not_null<TextState*> outResult) const;

private:
	void resized() const;

	struct Inner;
	const std::unique_ptr<Inner> _inner;
};

} // namespace HistoryView
