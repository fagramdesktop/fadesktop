/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/layers/box_content.h"

class PeerData;

namespace Window {
class SessionController;
} // namespace Window

namespace Data {
class WallPaper;
} // namespace Data

class BackgroundBox : public Ui::BoxContent {
public:
	BackgroundBox(
		QWidget*,
		not_null<Window::SessionController*> controller,
		PeerData *forPeer = nullptr);

protected:
	void prepare() override;

private:
	class Inner;

	void chosen(const Data::WallPaper &paper);
	[[nodiscard]] bool hasDefaultForPeer() const;
	[[nodiscard]] bool chosenDefaultForPeer(
		const Data::WallPaper &paper) const;
	void removePaper(const Data::WallPaper &paper);
	void resetForPeer();
	[[nodiscard]] bool forChannel() const;

	void chooseFromFile();

	const not_null<Window::SessionController*> _controller;

	QPointer<Inner> _inner;
	PeerData *_forPeer = nullptr;

};
