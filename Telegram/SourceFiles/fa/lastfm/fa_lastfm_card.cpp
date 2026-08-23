/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/lastfm/fa_lastfm_card.h"
#include "fa/lastfm/fa_lastfm_client.h"
#include "fa/ui/md3/svg_assets.h"
#include "fa/settings/fa_settings.h"
#include "fa_lang_auto.h"

#include "window/window_session_controller.h"
#include "ui/basic_click_handlers.h"
#include "ui/painter.h"
#include "styles/style_basic.h"
#include "styles/style_widgets.h"
#include "styles/style_settings.h"
#include "styles/style_boxes.h"

#include <QtSvg/QSvgRenderer>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainterPath>

namespace Fa::LastFm {
namespace {

constexpr auto kCardHeight = 92;
constexpr auto kArtSize = 60;
constexpr auto kPlayBtnSize = 44;
constexpr auto kMargin = 16;

QImage RenderSvg(std::string_view svgData, QSize size, std::optional<QColor> color = std::nullopt) {
	auto result = QImage(size * style::DevicePixelRatio(), QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(style::DevicePixelRatio());
	result.fill(Qt::transparent);

	auto svg = QSvgRenderer(QByteArray::fromRawData(svgData.data(), svgData.size()));
	auto p = QPainter(&result);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, QRectF(0, 0, size.width(), size.height()));

	if (color && color->isValid()) {
		p.setCompositionMode(QPainter::CompositionMode_SourceIn);
		p.fillRect(QRect(0, 0, size.width(), size.height()), *color);
	}
	p.end();

	return result;
}

QImage MaskImageWithShape(const QImage &source, QSize size, std::string_view shapeSvg) {
	const auto ratio = style::DevicePixelRatio();
	auto mask = RenderSvg(shapeSvg, size);

	auto scaled = source.scaled(
		size * ratio,
		Qt::KeepAspectRatioByExpanding,
		Qt::SmoothTransformation);

	auto result = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);

	auto p = QPainter(&result);
	p.setRenderHint(QPainter::Antialiasing);

	const auto sx = (scaled.width() / ratio - size.width()) / 2;
	const auto sy = (scaled.height() / ratio - size.height()) / 2;
	p.drawImage(QPoint(-sx, -sy), scaled);

	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawImage(0, 0, mask);
	p.end();

	return result;
}

QImage CreatePlaceholderArt(QSize size, std::string_view shapeSvg) {
	const auto ratio = style::DevicePixelRatio();
	auto result = QImage(size * ratio, QImage::Format_ARGB32_Premultiplied);
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);

	auto mask = RenderSvg(shapeSvg, size);

	auto p = QPainter(&result);
	p.setRenderHint(QPainter::Antialiasing);

	p.drawImage(0, 0, mask);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(QRect(0, 0, size.width(), size.height()), QColor(45, 52, 70));

	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.setPen(Qt::white);
	p.setFont(st::semiboldFont);
	p.drawText(
		QRect(0, 0, size.width(), size.height()),
		Qt::AlignCenter,
		QString::fromUtf8("\xE2\x99\xAB")); // ♫

	p.end();
	return result;
}

NowPlayingCard::Palette ExtractPaletteFromImage(const QImage &image) {
	if (image.isNull()) {
		return NowPlayingCard::Palette();
	}

	const auto thumb = image.scaled(
		32,
		32,
		Qt::IgnoreAspectRatio,
		Qt::SmoothTransformation
	).convertToFormat(QImage::Format_ARGB32_Premultiplied);

	int rSum1 = 0, gSum1 = 0, bSum1 = 0, count1 = 0;
	int rSum2 = 0, gSum2 = 0, bSum2 = 0, count2 = 0;

	QColor maxSatColor;
	float maxSat = -1.0f;

	const auto width = thumb.width();
	const auto height = thumb.height();

	for (int y = 0; y < height; ++y) {
		const auto scan = reinterpret_cast<const QRgb*>(thumb.constScanLine(y));
		for (int x = 0; x < width; ++x) {
			const auto rgb = scan[x];
			const auto r = qRed(rgb);
			const auto g = qGreen(rgb);
			const auto b = qBlue(rgb);

			QColor c(r, g, b);
			const auto hsvS = c.hsvSaturationF();
			const auto hsvV = c.valueF();

			if (hsvS > maxSat && hsvV > 0.25f && hsvV < 0.95f) {
				maxSat = hsvS;
				maxSatColor = c;
			}

			if (x + y < (width + height) / 2) {
				rSum1 += r;
				gSum1 += g;
				bSum1 += b;
				count1++;
			} else {
				rSum2 += r;
				gSum2 += g;
				bSum2 += b;
				count2++;
			}
		}
	}

	NowPlayingCard::Palette palette;

	if (count1 > 0) {
		auto c1 = QColor(rSum1 / count1, gSum1 / count1, bSum1 / count1);
		float h = 0.f, s = 0.f, v = 0.f, a = 0.f;
		c1.getHsvF(&h, &s, &v, &a);
		s = std::clamp(s * 1.25f, 0.45f, 0.95f);
		v = std::clamp(v * 0.65f, 0.22f, 0.50f);
		c1.setHsvF(h >= 0.f ? h : 0.6f, s, v);
		palette.color1 = c1;
	}

	if (count2 > 0) {
		auto c2 = QColor(rSum2 / count2, gSum2 / count2, bSum2 / count2);
		float h = 0.f, s = 0.f, v = 0.f, a = 0.f;
		c2.getHsvF(&h, &s, &v, &a);
		s = std::clamp(s * 1.35f, 0.50f, 1.0f);
		v = std::clamp(v * 0.45f, 0.14f, 0.38f);
		c2.setHsvF(h >= 0.f ? h : 0.7f, s, v);
		palette.color2 = c2;
	}

	if (maxSatColor.isValid() && maxSat > 0.15f) {
		float h = 0.f, s = 0.f, v = 0.f, a = 0.f;
		maxSatColor.getHsvF(&h, &s, &v, &a);
		s = std::clamp(s * 1.3f, 0.65f, 1.0f);
		v = std::clamp(v * 1.2f, 0.75f, 0.95f);
		maxSatColor.setHsvF(h >= 0.f ? h : 0.0f, s, v);
		palette.accent = maxSatColor;
	} else {
		auto acc = palette.color1;
		float h = 0.f, s = 0.f, v = 0.f, a = 0.f;
		acc.getHsvF(&h, &s, &v, &a);
		acc.setHsvF(h >= 0.f ? h : 0.0f, std::max(0.7f, s), 0.85f);
		palette.accent = acc;
	}

	return palette;
}

} // namespace

NowPlayingCard::NowPlayingCard(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: RpWidget(parent)
, _controller(controller) {
	setMouseTracking(true);
}

NowPlayingCard::~NowPlayingCard() = default;

void NowPlayingCard::setUsername(const QString &username) {
	_username = username;
}

void NowPlayingCard::setTrack(const std::optional<LastFmTrack> &track) {
	_track = track;

	if (_track && _track->imageUrl != _lastLoadedArtUrl) {
		updateCoverArt();
	} else if (!_track) {
		_coverArt = QImage();
		_lastLoadedArtUrl.clear();
		_palette = Palette();
	}

	update();
}

void NowPlayingCard::updateCoverArt() {
	if (!_track || _track->imageUrl.trimmed().isEmpty()) {
		_coverArt = QImage();
		_lastLoadedArtUrl.clear();
		_palette = Palette();
		update();
		return;
	}

	_lastLoadedArtUrl = _track->imageUrl;
	Client::Instance().fetchCoverArt(
		_track->imageUrl,
		crl::guard(this, [=, expectedUrl = _track->imageUrl](QImage image) {
			if (_track && _track->imageUrl == expectedUrl) {
				_coverArt = image;
				_palette = ExtractPaletteFromImage(image);
				update();
			}
		}));
}

int NowPlayingCard::resizeGetHeight(int newWidth) {
	return kCardHeight;
}

QRect NowPlayingCard::coverArtRect() const {
	const auto y = (height() - kArtSize) / 2;
	return QRect(kMargin, y, kArtSize, kArtSize);
}

QRect NowPlayingCard::playButtonRect() const {
	const auto x = width() - kMargin - kPlayBtnSize;
	const auto y = (height() - kPlayBtnSize) / 2;
	return QRect(x, y, kPlayBtnSize, kPlayBtnSize);
}

void NowPlayingCard::openTrackUrl() {
	if (_track && !_track->url.trimmed().isEmpty()) {
		UrlClickHandler::Open(_track->url.trimmed());
	} else if (!_username.trimmed().isEmpty()) {
		UrlClickHandler::Open(
			u"https://www.last.fm/user/"_q + _username.trimmed());
	}
}

void NowPlayingCard::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		if (playButtonRect().contains(e->pos())) {
			_playButtonPressed = true;
			update();
		}
	}
}

void NowPlayingCard::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		const auto onPlayBtn = playButtonRect().contains(e->pos());
		if (_playButtonPressed && onPlayBtn) {
			_playButtonPressed = false;
			openTrackUrl();
			update();
		} else if (rect().contains(e->pos())) {
			_playButtonPressed = false;
			openTrackUrl();
			update();
		} else {
			_playButtonPressed = false;
			update();
		}
	}
}

void NowPlayingCard::mouseMoveEvent(QMouseEvent *e) {
	const auto overPlay = playButtonRect().contains(e->pos());
	const auto overCard = rect().contains(e->pos());

	if (overPlay != _playButtonHovered || overCard != _cardHovered) {
		_playButtonHovered = overPlay;
		_cardHovered = overCard;
		setCursor(overCard ? Qt::PointingHandCursor : Qt::ArrowCursor);
		update();
	}
}

void NowPlayingCard::leaveEventHook(QEvent *e) {
	_playButtonHovered = false;
	_playButtonPressed = false;
	_cardHovered = false;
	setCursor(Qt::ArrowCursor);
	update();
	RpWidget::leaveEventHook(e);
}

void NowPlayingCard::paintEvent(QPaintEvent *e) {
	if (!_track) {
		return;
	}

	Painter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const auto cardRect = QRectF(0, 0, width(), height());
	const auto roundness = FASettings::FASettings::getInstance().roundness();
	const auto radius = 24.0 * (roundness / 50.0);

	auto bgGrad = QLinearGradient(cardRect.topLeft(), cardRect.bottomRight());
	bgGrad.setColorAt(0.0, _palette.color1);
	bgGrad.setColorAt(1.0, _palette.color2);

	p.setPen(Qt::NoPen);
	p.setBrush(bgGrad);
	p.drawRoundedRect(cardRect, radius, radius);

	if (_cardHovered) {
		p.setBrush(QColor(255, 255, 255, 18));
		p.drawRoundedRect(cardRect, radius, radius);
	}

	const auto artR = coverArtRect();
	QImage maskedArt;
	if (!_coverArt.isNull()) {
		maskedArt = MaskImageWithShape(
			_coverArt,
			artR.size(),
			fa::svg::material_shape1);
	} else {
		maskedArt = CreatePlaceholderArt(
			artR.size(),
			fa::svg::material_shape1);
	}
	p.drawImage(artR.topLeft(), maskedArt);

	const auto textLeft = artR.right() + 14;
	const auto textRight = width() - kMargin - kPlayBtnSize - 12;
	const auto availableTextW = std::max(10, textRight - textLeft);

	const auto title = _track->title.isEmpty() ? u"Unknown Track"_q : _track->title;
	const auto artist = _track->artist.isEmpty() ? u"Unknown Artist"_q : _track->artist;
	const auto status = _track->isNowPlaying
		? fatr::fa_lastfm_now_playing(fatr::now)
		: fatr::fa_lastfm_scrobbling(fatr::now);

	const auto titleFont = st::semiboldFont->f;
	const auto titleMetrics = QFontMetrics(titleFont);

	auto artistFont = titleFont;
	if (artistFont.pixelSize() > 0) {
		artistFont.setPixelSize(std::max(10, artistFont.pixelSize() - 2));
	} else if (artistFont.pointSizeF() > 0) {
		artistFont.setPointSizeF(std::max(8.0, artistFont.pointSizeF() - 2.0));
	} else {
		artistFont.setPointSize(std::max(8, artistFont.pointSize() - 2));
	}
	artistFont.setWeight(QFont::Normal);
	const auto artistMetrics = QFontMetrics(artistFont);

	auto statusFont = artistFont;
	const auto statusMetrics = QFontMetrics(statusFont);

	const auto titleH = titleMetrics.height();
	const auto artistH = artistMetrics.height();
	const auto statusH = statusMetrics.height();
	const auto totalTextH = titleH + 3 + artistH + 3 + statusH;
	int curY = (height() - totalTextH) / 2;

	{
		p.setFont(titleFont);
		p.setPen(Qt::white);
		p.drawText(
			QRect(textLeft, curY, availableTextW, titleH),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			titleMetrics.elidedText(title, Qt::ElideRight, availableTextW));

		curY += titleH + 3;
	}

	{
		p.setFont(artistFont);
		p.setPen(QColor(255, 255, 255, 220));
		p.drawText(
			QRect(textLeft, curY, availableTextW, artistH),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			artistMetrics.elidedText(artist, Qt::ElideRight, availableTextW));

		curY += artistH + 3;
	}

	{
		p.setFont(statusFont);
		p.setPen(QColor(255, 255, 255, 170));
		p.drawText(
			QRect(textLeft, curY, availableTextW, statusH),
			Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
			statusMetrics.elidedText(status, Qt::ElideRight, availableTextW));
	}

	const auto btnR = playButtonRect();

	auto btnBgColor = _palette.accent;
	if (_playButtonPressed) {
		btnBgColor = btnBgColor.darker(120);
	} else if (_playButtonHovered) {
		btnBgColor = btnBgColor.lighter(120);
	}

	auto shape6Bg = RenderSvg(fa::svg::material_shape6, btnR.size(), btnBgColor);
	p.drawImage(btnR.topLeft(), shape6Bg);

	auto shape6Outline = RenderSvg(fa::svg::material_shape6, btnR.size(), QColor(255, 255, 255, 70));
	p.drawImage(btnR.topLeft(), shape6Outline);

	const auto isPlaying = _track->isNowPlaying;
	const auto iconSvg = isPlaying ? fa::svg::pause : fa::svg::play;
	const auto iconSize = QSize(20, 20);
	auto iconImg = RenderSvg(iconSvg, iconSize, Qt::white);

	const auto iconX = btnR.x() + (btnR.width() - iconSize.width()) / 2;
	const auto iconY = btnR.y() + (btnR.height() - iconSize.height()) / 2;
	p.drawImage(QPoint(iconX, iconY), iconImg);
}

} // namespace Fa::LastFm
