/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "dialogs/ui/dialogs_video_userpic.h"

#include "fa/settings/fa_settings.h"

#include "base/unixtime.h"
#include "core/file_location.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "dialogs/dialogs_entry.h"
#include "dialogs/ui/dialogs_layout.h"
#include "ui/painter.h"
#include "styles/style_dialogs.h"

namespace Dialogs::Ui {

VideoUserpic::VideoUserpic(not_null<PeerData*> peer, Fn<void()> repaint)
: _peer(peer)
, _repaint(std::move(repaint)) {
}

VideoUserpic::~VideoUserpic() = default;

int VideoUserpic::frameIndex() const {
	return -1;
}

void VideoUserpic::paintLeft(
		Painter &p,
		Ui::PeerUserpicView &view,
		int x,
		int y,
		int w,
		int size,
		bool paused) {
	_lastSize = size;

	const auto photoId = _peer->userpicPhotoId();
	if (_videoPhotoId != photoId) {
		_videoPhotoId = photoId;
		_video = nullptr;
		_videoPhotoMedia = nullptr;
		const auto photo = _peer->owner().photo(photoId);
		if (photo->isNull()) {
			_peer->updateFullForced();
		} else {
			_videoPhotoMedia = photo->createMediaView();
			_videoPhotoMedia->videoWanted(
				Data::PhotoSize::Small,
				_peer->userpicPhotoOrigin());
		}
	}
	if (!_video) {
		if (!_videoPhotoMedia) {
			const auto photo = _peer->owner().photo(photoId);
			if (!photo->isNull()) {
				_videoPhotoMedia = photo->createMediaView();
				_videoPhotoMedia->videoWanted(
					Data::PhotoSize::Small,
					_peer->userpicPhotoOrigin());
			}
		}
		if (_videoPhotoMedia) {
			auto small = _videoPhotoMedia->videoContent(
				Data::PhotoSize::Small);
			auto bytes = small.isEmpty()
				? _videoPhotoMedia->videoContent(Data::PhotoSize::Large)
				: small;
			if (!bytes.isEmpty()) {
				auto callback = [=](Media::Clip::Notification notification) {
					clipCallback(notification);
				};
				_video = Media::Clip::MakeReader(
					Core::FileLocation(),
					std::move(bytes),
					std::move(callback));
			}
		}
	}
	if (rtl()) {
		x = w - x - size;
	}
if (_video && _video->ready()) {
		startReady();

		const auto now = paused ? crl::time(0) : crl::now();

		bool use_default_rounding = FASettings::JsonSettings::GetBool("use_default_rounding");

		if (use_default_rounding) {
			p.drawImage(x, y, _video->current(request(size), now));
		}
		else {
			p.save();
			QPainterPath clipPath;
			QImage frame = _video->current(request(size), now);
			auto radius = frame.height() * FASettings::JsonSettings::GetInt("roundness") / 100.;
			clipPath.addRoundedRect(
				QRect(x, y, frame.width(), frame.height()),
				radius, radius);
			p.setClipPath(clipPath);
			p.drawImage(x, y, frame);
			p.restore();
		}
	} else {
		_peer->paintUserpicLeft(p, view, x, y, w, size);
	}
}

Media::Clip::FrameRequest VideoUserpic::request(int size) const {
	return {
		.frame = { size, size },
		.outer = { size, size },
		.factor = style::DevicePixelRatio(),
		.radius = ImageRoundRadius::Ellipse,
	};
}

bool VideoUserpic::startReady(int size) {
	if (!_video->ready() || _video->started()) {
		return false;
	} else if (!_lastSize) {
		_lastSize = size ? size : _video->width();
	}
	_video->start(request(_lastSize));
	_repaint();
	return true;
}

void VideoUserpic::clipCallback(Media::Clip::Notification notification) {
	using namespace Media::Clip;

	switch (notification) {
	case Notification::Reinit: {
		if (_video->state() == State::Error) {
			_video.setBad();
		} else if (startReady()) {
			_repaint();
		}
	} break;

	case Notification::Repaint: _repaint(); break;
	}
}

void PaintUserpic(
		Painter &p,
		not_null<Entry*> entry,
		PeerData *peer,
		VideoUserpic *videoUserpic,
		PeerUserpicView &view,
		const Ui::PaintContext &context) {
	if (peer) {
		PaintUserpic(
			p,
			peer,
			videoUserpic,
			view,
			context.st->padding.left(),
			context.st->padding.top(),
			context.width,
			context.st->photoSize,
			context.paused);
	} else {
		entry->paintUserpic(p, view, context);
	}
}

void PaintUserpic(
		Painter &p,
		not_null<PeerData*> peer,
		Ui::VideoUserpic *videoUserpic,
		Ui::PeerUserpicView &view,
		int x,
		int y,
		int outerWidth,
		int size,
		bool paused) {
	if (videoUserpic) {
		videoUserpic->paintLeft(p, view, x, y, outerWidth, size, paused);
	} else {
		peer->paintUserpicLeft(p, view, x, y, outerWidth, size);
	}

	const auto showStatusDot = FASettings::JsonSettings::GetBool("show_status_dot");
	if (showStatusDot) {
		if (const auto user = peer->asUser()) {
			if (!user->isBot() && !user->isServiceUser()) {
				const auto now = base::unixtime::now();

				QColor dotColor;

				if (user->isInaccessible() || user->isBlocked()) {
					dotColor = QColor(0, 0, 0);
				} else if (user->lastseen().isOnline(now)) {
					dotColor = QColor(15, 255, 80);
				} else {
					dotColor = QColor(158, 158, 158);
				}

				const auto dotDiameter = 10.0;
				const auto borderWidth = 2.0;
				const auto totalSize = dotDiameter + borderWidth * 2.0;
				const auto dotX = static_cast<double>(x + size) - totalSize + borderWidth;
				const auto dotY = static_cast<double>(y + size) - totalSize + borderWidth;

				p.save();
				p.setRenderHint(QPainter::Antialiasing, true);
				p.setPen(Qt::NoPen);
				p.setBrush(st::dialogsBg->c);
				p.drawEllipse(QRectF(dotX - borderWidth, dotY - borderWidth, totalSize, totalSize));
				p.setBrush(dotColor);
				p.drawEllipse(QRectF(dotX, dotY, dotDiameter, dotDiameter));
				p.restore();
			}
		}
	}
}

} // namespace Dialogs::Ui
