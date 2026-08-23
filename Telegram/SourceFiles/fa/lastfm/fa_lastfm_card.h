/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "fa/lastfm/fa_lastfm_config.h"
#include "ui/rp_widget.h"
#include "ui/effects/ripple_animation.h"
#include "base/basic_types.h"

#include <QtGui/QImage>

namespace Window {
class SessionController;
} // namespace Window

namespace Fa::LastFm {

class NowPlayingCard final : public Ui::RpWidget {
public:
	struct Palette {
		QColor color1 = QColor(45, 52, 70);
		QColor color2 = QColor(25, 30, 42);
		QColor accent = QColor(220, 80, 100);
	};

	NowPlayingCard(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~NowPlayingCard() override;

	void setTrack(const std::optional<LastFmTrack> &track);
	void setUsername(const QString &username);

	[[nodiscard]] bool hasTrack() const {
		return _track.has_value();
	}

protected:
	void paintEvent(QPaintEvent *e) override;
	int resizeGetHeight(int newWidth) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEventHook(QEvent *e) override;

private:
	void updateCoverArt();
	[[nodiscard]] QRect playButtonRect() const;
	[[nodiscard]] QRect coverArtRect() const;
	void openTrackUrl();

	not_null<Window::SessionController*> _controller;
	std::optional<LastFmTrack> _track;
	QString _username;

	Palette _palette;
	QImage _coverArt;
	QString _lastLoadedArtUrl;
	bool _playButtonHovered = false;
	bool _playButtonPressed = false;
	bool _cardHovered = false;
};

} // namespace Fa::LastFm
