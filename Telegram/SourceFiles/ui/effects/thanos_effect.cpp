/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/effects/thanos_effect.h"

#include "ui/effects/thanos_effect_renderer.h"
#include "ui/gl/gl_detection.h"
#include "ui/gl/gl_surface.h"
#include "ui/power_saving.h"
#include "ui/rp_widget.h"
#include "ui/ui_utility.h"
#include "base/debug_log.h"
#include "base/platform/base_platform_info.h"

#include <QTimer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <rhi/qrhi.h>
#ifdef Q_OS_UNIX
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#endif
#endif

namespace Ui {
namespace {

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
[[nodiscard]] bool ProbeComputeSupport() {
	// Create a throw-away QRhi with the same backend SurfaceRhi will use
	// in production, ask whether GPU compute is available, then destroy.
	// This is real hardware/driver capability detection — no OS version
	// guards. On older Metal-capable Macs the answer is "no", which
	// is exactly what makes the surface render uninitialized (Y-flipped)
	// garbage and triggers the mirror bug elsewhere.
	auto rhi = std::unique_ptr<QRhi>(nullptr);
#ifdef Q_OS_MAC
	if (::Platform::MetalSupported()) {
		auto params = QRhiMetalInitParams();
		rhi.reset(QRhi::create(QRhi::Metal, &params));
	}
	if (!rhi) {
		return false;
	}
#elif defined(Q_OS_WIN)
	auto params = QRhiD3D11InitParams();
	rhi.reset(QRhi::create(QRhi::D3D11, &params));
	if (!rhi) {
		return false;
	}
#else
	// Linux/Unix: probe the OpenGL backend with an offscreen surface.
	// Matches what SurfaceRhi uses in production so the result is
	// representative of what the real widget would get. If anything
	// here fails (no GL context, software fallback without compute,
	// driver bug), treat compute as unsupported and refuse the effect
	// rather than show an uninitialized swap-chain.
	auto format = QSurfaceFormat::defaultFormat();
	auto offscreen = std::unique_ptr<QOffscreenSurface>(
		QRhiGles2InitParams::newFallbackSurface(format));
	if (!offscreen) {
		LOG(("ThanosEffect: probe failed — no offscreen surface"));
		return false;
	}
	auto params = QRhiGles2InitParams();
	params.format = format;
	params.fallbackSurface = offscreen.get();
	rhi.reset(QRhi::create(QRhi::OpenGLES2, &params));
	if (!rhi) {
		LOG(("ThanosEffect: probe failed — no GL RHI"));
		return false;
	}
#endif
	const auto supported = rhi->isFeatureSupported(QRhi::Compute);
	LOG(("ThanosEffect: probe backend=%1 device=%2 compute=%3"
		).arg(rhi->backendName()
		).arg(rhi->driverInfo().deviceName
		).arg(supported ? "yes" : "no"));
	rhi.reset();
	return supported;
}

[[nodiscard]] bool RhiComputeSupportedCached() {
	static const auto cached = ProbeComputeSupport();
	return cached;
}
#endif // Qt >= 6.7

} // namespace

bool ThanosEffect::Supported() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	if (PowerSaving::On(PowerSaving::kChatEffects)) {
		return false;
	}
	if (!GL::WidgetsRhiEnabled()) {
		return false;
	}
	return RhiComputeSupportedCached();
#else
	return false;
#endif
}

void ThanosEffect::WarmUp() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	if (!GL::WidgetsRhiEnabled()) {
		return;
	}
	(void)RhiComputeSupportedCached();
#endif
}

ThanosEffect::ThanosEffect(not_null<QWidget*> parent)
: _parent(parent) {
}

ThanosEffect::~ThanosEffect() {
	stopUpdateTimer();
}

void ThanosEffect::ensureSurface() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	if (_surface) {
		return;
	}

	auto renderer = std::make_unique<ThanosEffectRenderer>();
	_renderer = renderer.get();

	_renderer->allDone() | rpl::on_next([=] {
		crl::on_main(_parent, [=] {
			hideSurface();
			_allDone.fire({});
		});
	}, _lifetime);

	_surface = GL::CreateSurface(
		_parent,
		GL::ChosenRenderer{
			.renderer = std::move(renderer),
			.backend = GL::Backend::QRhi,
		});

	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		w->setAttribute(Qt::WA_TransparentForMouseEvents);
		w->setAttribute(Qt::WA_AlwaysStackOnTop);
		w->setGeometry(_parent->rect());
		w->hide();
	}
#endif
}

void ThanosEffect::showSurface() {
	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		w->setGeometry(_parent->rect());
		// Defer show until the current call stack returns to the event
		// loop, so that all items from a batch deletion are added
		// before the first render. Without this, w->show() triggers
		// an immediate platform compositing pass with only the first
		// item visible.
		Ui::PostponeCall(w, [w] {
			w->show();
			w->raise();
		});
		startUpdateTimer();
	}
}

void ThanosEffect::hideSurface() {
	stopUpdateTimer();
	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		w->hide();
	}
}

void ThanosEffect::addItem(QImage snapshot, QRect rect) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	ensureSurface();
	if (!_renderer) {
		return;
	}

	const auto wasAnimating = _renderer->hasActiveItems();

	_renderer->addItem({
		.snapshot = std::move(snapshot),
		.rect = QRectF(rect),
	});

	if (!wasAnimating) {
		showSurface();
	}
#endif
}

bool ThanosEffect::animating() const {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	return _renderer && _renderer->hasActiveItems();
#else
	return false;
#endif
}

rpl::producer<> ThanosEffect::allDone() const {
	return _allDone.events();
}

void ThanosEffect::setGeometry(QRect rect) {
	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		if (w->isVisible()) {
			w->setGeometry(rect);
		}
	}
}

void ThanosEffect::raise() {
	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		w->raise();
	}
}

void ThanosEffect::startUpdateTimer() {
	if (_updateTimer) {
		return;
	}
	if (const auto w = _surface ? _surface->rpWidget() : nullptr) {
		_updateTimer = new QTimer(w);
		_updateTimer->setInterval(16);
		QObject::connect(_updateTimer, &QTimer::timeout, w, [w] {
			w->update();
		});
		_updateTimer->start();
	}
}

void ThanosEffect::stopUpdateTimer() {
	if (_updateTimer) {
		_updateTimer->stop();
		delete _updateTimer;
		_updateTimer = nullptr;
	}
}

} // namespace Ui
