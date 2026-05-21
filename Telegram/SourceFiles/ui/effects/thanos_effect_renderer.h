/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rhi/rhi_renderer.h"
#include "ui/gl/gl_surface.h"

#include <QElapsedTimer>
#include <QImage>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)

class QRhi;
class QRhiBuffer;
class QRhiTexture;
class QRhiSampler;
class QRhiGraphicsPipeline;
class QRhiComputePipeline;
class QRhiShaderResourceBindings;
class QRhiRenderTarget;
class QRhiCommandBuffer;

namespace Ui {

struct ThanosItem {
	QImage snapshot;
	QRectF rect;
};

class ThanosEffectRenderer final
	: public GL::Renderer
	, public Rhi::Renderer {
public:
	ThanosEffectRenderer();
	~ThanosEffectRenderer();

	void initialize(
		QRhi *rhi,
		QRhiRenderTarget *rt,
		QRhiCommandBuffer *cb) override;
	void render(
		QRhi *rhi,
		QRhiRenderTarget *rt,
		QRhiCommandBuffer *cb) override;
	void releaseResources() override;

	QColor rhiClearColor() override {
		return QColor(0, 0, 0, 0);
	}

	std::optional<QColor> clearColor() override {
		return QColor(0, 0, 0, 0);
	}

	void addItem(ThanosItem item);
	[[nodiscard]] bool hasActiveItems() const;

	rpl::producer<> allDone() const;

private:
	struct AnimatingItem {
		QRhiTexture *texture = nullptr;
		QRhiSampler *sampler = nullptr;
		QRhiBuffer *particleBuffer = nullptr;
		QRhiBuffer *computeInitUniformBuffer = nullptr;
		QRhiBuffer *computeUpdateUniformBuffer = nullptr;
		QRhiBuffer *renderUniformBuffer = nullptr;
		QRhiShaderResourceBindings *computeInitSrb = nullptr;
		QRhiShaderResourceBindings *computeUpdateSrb = nullptr;
		QRhiShaderResourceBindings *renderSrb = nullptr;
		QImage uploadImage;
		QRectF rect;
		uint32_t particleCountX = 0;
		uint32_t particleCountY = 0;
		float phase = 0.f;
		bool particlesInitialized = false;
	};

	void createPipelines(QRhiRenderTarget *rt);
	void addPendingItems(QRhiCommandBuffer *cb);
	AnimatingItem createAnimatingItem(ThanosItem &&item);
	void destroyAnimatingItem(AnimatingItem &item);

	QRhi *_rhi = nullptr;

	QRhiBuffer *_quadVertexBuffer = nullptr;
	QRhiBuffer *_computeInitUniformBuffer = nullptr;
	QRhiBuffer *_computeUpdateUniformBuffer = nullptr;
	QRhiBuffer *_renderUniformBuffer = nullptr;

	QRhiBuffer *_placeholderParticleBuffer = nullptr;
	QRhiTexture *_placeholderTexture = nullptr;
	QRhiSampler *_placeholderSampler = nullptr;

	QRhiShaderResourceBindings *_computeInitSrbLayout = nullptr;
	QRhiShaderResourceBindings *_computeUpdateSrbLayout = nullptr;
	QRhiShaderResourceBindings *_renderSrbLayout = nullptr;

	QRhiComputePipeline *_computeInitPipeline = nullptr;
	QRhiComputePipeline *_computeUpdatePipeline = nullptr;
	QRhiGraphicsPipeline *_renderPipeline = nullptr;

	std::vector<AnimatingItem> _items;
	std::vector<ThanosItem> _pendingItems;

	QElapsedTimer _elapsed;
	double _lastFrameTime = 0.;
	bool _initialized = false;
	uint32_t _seedCounter = 0;

	rpl::event_stream<> _allDone;

};

} // namespace Ui

#endif // Qt >= 6.7
