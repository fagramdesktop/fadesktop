/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/effects/thanos_effect_renderer.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)

#include "ui/rhi/rhi_shader.h"
#include "ui/rp_widget.h"
#include "ui/painter.h"
#include "styles/style_basic.h"
#include "base/debug_log.h"

#include <rhi/qrhi.h>

namespace Ui {
namespace {

constexpr auto kParticleStride = int(24);
constexpr auto kQuadVertexCount = int(6);
constexpr auto kQuadVertexStride = int(2 * sizeof(float));
constexpr auto kComputeWorkgroupSize = int(64);
constexpr auto kMaxPhaseDuration = 6.0f;
constexpr auto kPhaseSpeed = 1.65f;
constexpr auto kTimeStepMultiplier = 1.65f;
constexpr auto kAccelerationStartPhase = 1.0f;
constexpr auto kAccelerationRampPhase = 2.5f;
constexpr auto kAccelerationMaxMultiplier = 2.2f;
constexpr auto kDisappearStartPhase = kMaxPhaseDuration * 0.15f;
constexpr auto kDisappearDuration
	= kMaxPhaseDuration - kDisappearStartPhase;
constexpr auto kMaxParticleCount = uint32_t(120000);

const float kQuadVertices[kQuadVertexCount * 2] = {
	0.f, 0.f,
	1.f, 0.f,
	0.f, 1.f,
	1.f, 0.f,
	0.f, 1.f,
	1.f, 1.f,
};

struct alignas(16) ComputeInitUniforms {
	uint32_t particleCountX;
	uint32_t particleCountY;
	uint32_t seed;
	uint32_t _pad;
};
static_assert(sizeof(ComputeInitUniforms) % 16 == 0);

struct alignas(16) ComputeUpdateUniforms {
	uint32_t particleCountX;
	uint32_t particleCountY;
	float phase;
	float timeStep;
};
static_assert(sizeof(ComputeUpdateUniforms) % 16 == 0);

struct alignas(16) RenderUniforms {
	float rect[4];
	float size[2];
	uint32_t particleResolution[2];
	float scale[4];
};
static_assert(sizeof(RenderUniforms) % 16 == 0);

[[nodiscard]] float AnimationSpeedMultiplier(float phase) {
	if (phase <= kAccelerationStartPhase) {
		return 1.0f;
	}
	const auto t = std::clamp(
		(phase - kAccelerationStartPhase) / kAccelerationRampPhase,
		0.0f,
		1.0f);
	const auto smooth = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	return 1.0f
		+ ((kAccelerationMaxMultiplier - 1.0f) * smooth);
}

[[nodiscard]] float DisappearProgress(float phase) {
	const auto t = std::clamp(
		(phase - kDisappearStartPhase) / kDisappearDuration,
		0.0f,
		1.0f);
	const auto oneMinus = 1.0f - t;
	return 1.0f - (oneMinus * oneMinus * oneMinus);
}

[[nodiscard]] QShader LoadShader(const QString &name) {
	return Rhi::ShaderFromFile(u":/shaders/"_q + name + u".qsb"_q);
}

} // namespace

ThanosEffectRenderer::ThanosEffectRenderer() {
	_elapsed.start();
}

ThanosEffectRenderer::~ThanosEffectRenderer() {
	releaseResources();
}

void ThanosEffectRenderer::initialize(
		QRhi *rhi,
		QRhiRenderTarget *rt,
		QRhiCommandBuffer *cb) {
	if (_initialized && _rhi == rhi) {
		return;
	}
	releaseResources();
	_rhi = rhi;

	if (!rhi->isFeatureSupported(QRhi::Compute)) {
		LOG(("ThanosEffect: Compute shaders not supported, disabled"));
		return;
	}

	_quadVertexBuffer = rhi->newBuffer(
		QRhiBuffer::Immutable,
		QRhiBuffer::VertexBuffer,
		sizeof(kQuadVertices));
	_quadVertexBuffer->create();

	_computeInitUniformBuffer = rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(ComputeInitUniforms));
	_computeInitUniformBuffer->create();

	_computeUpdateUniformBuffer = rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(ComputeUpdateUniforms));
	_computeUpdateUniformBuffer->create();

	_renderUniformBuffer = rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(RenderUniforms));
	_renderUniformBuffer->create();

	_placeholderParticleBuffer = rhi->newBuffer(
		QRhiBuffer::Immutable,
		QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer,
		kParticleStride);
	_placeholderParticleBuffer->create();

	_placeholderTexture = rhi->newTexture(
		QRhiTexture::RGBA8,
		QSize(1, 1));
	_placeholderTexture->create();

	_placeholderSampler = rhi->newSampler(
		QRhiSampler::Linear,
		QRhiSampler::Linear,
		QRhiSampler::None,
		QRhiSampler::ClampToEdge,
		QRhiSampler::ClampToEdge);
	_placeholderSampler->create();

	createPipelines(rt);

	auto *rub = rhi->nextResourceUpdateBatch();
	rub->uploadStaticBuffer(_quadVertexBuffer, kQuadVertices);
	cb->resourceUpdate(rub);

	_initialized = true;
	_lastFrameTime = double(_elapsed.elapsed()) / 1000.0;

	LOG(("ThanosEffect: initialized, backend=%1 device=%2")
		.arg(rhi->backendName())
		.arg(rhi->driverInfo().deviceName));
}

void ThanosEffectRenderer::createPipelines(QRhiRenderTarget *rt) {
	const auto initShader = LoadShader(u"thanos_init.comp"_q);
	const auto updateShader = LoadShader(u"thanos_update.comp"_q);
	const auto vertShader = LoadShader(u"thanos.vert"_q);
	const auto fragShader = LoadShader(u"thanos.frag"_q);

	_computeInitSrbLayout = _rhi->newShaderResourceBindings();
	_computeInitSrbLayout->setBindings({
		QRhiShaderResourceBinding::bufferLoadStore(
			0,
			QRhiShaderResourceBinding::ComputeStage,
			_placeholderParticleBuffer),
		QRhiShaderResourceBinding::uniformBuffer(
			1,
			QRhiShaderResourceBinding::ComputeStage,
			_computeInitUniformBuffer),
	});
	_computeInitSrbLayout->create();

	_computeInitPipeline = _rhi->newComputePipeline();
	_computeInitPipeline->setShaderStage(
		{ QRhiShaderStage::Compute, initShader });
	_computeInitPipeline->setShaderResourceBindings(_computeInitSrbLayout);
	_computeInitPipeline->create();

	_computeUpdateSrbLayout = _rhi->newShaderResourceBindings();
	_computeUpdateSrbLayout->setBindings({
		QRhiShaderResourceBinding::bufferLoadStore(
			0,
			QRhiShaderResourceBinding::ComputeStage,
			_placeholderParticleBuffer),
		QRhiShaderResourceBinding::uniformBuffer(
			1,
			QRhiShaderResourceBinding::ComputeStage,
			_computeUpdateUniformBuffer),
	});
	_computeUpdateSrbLayout->create();

	_computeUpdatePipeline = _rhi->newComputePipeline();
	_computeUpdatePipeline->setShaderStage(
		{ QRhiShaderStage::Compute, updateShader });
	_computeUpdatePipeline->setShaderResourceBindings(
		_computeUpdateSrbLayout);
	_computeUpdatePipeline->create();

	_renderSrbLayout = _rhi->newShaderResourceBindings();
	_renderSrbLayout->setBindings({
		QRhiShaderResourceBinding::uniformBuffer(
			0,
			QRhiShaderResourceBinding::VertexStage,
			_renderUniformBuffer),
		QRhiShaderResourceBinding::sampledTexture(
			1,
			QRhiShaderResourceBinding::FragmentStage,
			_placeholderTexture,
			_placeholderSampler),
	});
	_renderSrbLayout->create();

	_renderPipeline = _rhi->newGraphicsPipeline();
	_renderPipeline->setShaderStages({
		{ QRhiShaderStage::Vertex, vertShader },
		{ QRhiShaderStage::Fragment, fragShader },
	});

	QRhiVertexInputLayout inputLayout;
	inputLayout.setBindings({
		{ quint32(kQuadVertexStride) },
		{ quint32(kParticleStride),
			QRhiVertexInputBinding::PerInstance },
	});
	inputLayout.setAttributes({
		{ 0, 0, QRhiVertexInputAttribute::Float2, 0 },
		{ 1, 1, QRhiVertexInputAttribute::Float2, 0 },
		{ 1, 2, QRhiVertexInputAttribute::Float, 16 },
	});
	_renderPipeline->setVertexInputLayout(inputLayout);

	QRhiGraphicsPipeline::TargetBlend blend;
	blend.enable = true;
	blend.srcColor = QRhiGraphicsPipeline::One;
	blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	blend.srcAlpha = QRhiGraphicsPipeline::One;
	blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
	_renderPipeline->setTargetBlends({ blend });

	_renderPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
	_renderPipeline->setShaderResourceBindings(_renderSrbLayout);
	_renderPipeline->setRenderPassDescriptor(
		rt->renderPassDescriptor());
	_renderPipeline->create();
}

void ThanosEffectRenderer::render(
		QRhi *rhi,
		QRhiRenderTarget *rt,
		QRhiCommandBuffer *cb) {
	if (!_initialized || !rhi->isFeatureSupported(QRhi::Compute)) {
		return;
	}
	_rhi = rhi;

	const auto now = double(_elapsed.elapsed()) / 1000.0;
	const auto dt = float(std::clamp(now - _lastFrameTime, 0.001, 0.1));
	_lastFrameTime = now;

	addPendingItems(cb);

	if (_items.empty()) {
		return;
	}

	const auto pixelSize = rt->pixelSize();
	const auto factor = style::DevicePixelRatio();
	const auto viewW = float(pixelSize.width()) / factor;
	const auto viewH = float(pixelSize.height()) / factor;

	{
		auto *rub = rhi->nextResourceUpdateBatch();
		auto needsInit = false;

		for (auto &item : _items) {
			if (item.phase >= kMaxPhaseDuration) {
				continue;
			}
			const auto animationTimeStep
				= dt * AnimationSpeedMultiplier(item.phase);
			item.phase += animationTimeStep * kPhaseSpeed;
			if (!item.particlesInitialized) {
				needsInit = true;
				item.particlesInitialized = true;

				ComputeInitUniforms uni;
				uni.particleCountX = item.particleCountX;
				uni.particleCountY = item.particleCountY;
				uni.seed = _seedCounter++;
				uni._pad = 0;
				rub->updateDynamicBuffer(
					item.computeInitUniformBuffer,
					0,
					sizeof(uni),
					&uni);
			}

			ComputeUpdateUniforms updateUni;
			updateUni.particleCountX = item.particleCountX;
			updateUni.particleCountY = item.particleCountY;
			updateUni.phase = item.phase;
			updateUni.timeStep = animationTimeStep * kTimeStepMultiplier;
			rub->updateDynamicBuffer(
				item.computeUpdateUniformBuffer,
				0,
				sizeof(updateUni),
				&updateUni);
		}

		cb->beginComputePass(rub);

		if (needsInit) {
			for (auto &item : _items) {
				if (item.phase <= dt * kPhaseSpeed * 1.1f) {
					cb->setComputePipeline(_computeInitPipeline);
					cb->setShaderResources(item.computeInitSrb);
					const auto count =
						item.particleCountX * item.particleCountY;
					const auto groups =
						(count + kComputeWorkgroupSize - 1)
						/ kComputeWorkgroupSize;
					cb->dispatch(int(groups), 1, 1);
				}
			}
		}

		for (auto &item : _items) {
			if (item.phase >= kMaxPhaseDuration) {
				continue;
			}
			cb->setComputePipeline(_computeUpdatePipeline);
			cb->setShaderResources(item.computeUpdateSrb);
			const auto count = item.particleCountX * item.particleCountY;
			const auto groups = (count + kComputeWorkgroupSize - 1)
				/ kComputeWorkgroupSize;
			cb->dispatch(int(groups), 1, 1);
		}

		cb->endComputePass();
	}

	{
		auto *renderRub = rhi->nextResourceUpdateBatch();
		for (auto &item : _items) {
			if (item.phase >= kMaxPhaseDuration) {
				continue;
			}
			RenderUniforms uni;
			uni.rect[0] = float(item.rect.x()) / viewW;
			uni.rect[1] = (viewH - float(item.rect.y())
				- float(item.rect.height())) / viewH;
			uni.rect[2] = float(item.rect.width()) / viewW;
			uni.rect[3] = float(item.rect.height()) / viewH;
			uni.size[0] = float(item.rect.width());
			uni.size[1] = float(item.rect.height());
			uni.particleResolution[0] = item.particleCountX;
			uni.particleResolution[1] = item.particleCountY;
			const auto disappearProgress = DisappearProgress(item.phase);
			const auto inverseDisappear = 1.0f - disappearProgress;
			uni.scale[0] = inverseDisappear;
			uni.scale[1] = 0.0f;
			uni.scale[2] = inverseDisappear;
			uni.scale[3] = 0.0f;

			renderRub->updateDynamicBuffer(
				item.renderUniformBuffer,
				0,
				sizeof(uni),
				&uni);
		}

		const auto bg = QColor(0, 0, 0, 0);
		cb->beginPass(rt, bg, { 1.0f, 0 }, renderRub);

		for (auto &item : _items) {
			if (item.phase >= kMaxPhaseDuration) {
				continue;
			}
			cb->setGraphicsPipeline(_renderPipeline);
			cb->setShaderResources(item.renderSrb);
			cb->setViewport({
				0, 0,
				float(pixelSize.width()),
				float(pixelSize.height()) });

			const QRhiCommandBuffer::VertexInput vbufs[] = {
				{ _quadVertexBuffer, 0 },
				{ item.particleBuffer, 0 },
			};
			cb->setVertexInput(0, 2, vbufs);

			const auto instanceCount =
				item.particleCountX * item.particleCountY;
			cb->draw(kQuadVertexCount, instanceCount);
		}

		cb->endPass();
	}

	// Remove finished items using deleteLater() so QRhi resources
	// survive until the command buffer is fully submitted.
	auto hadItems = !_items.empty();
	_items.erase(
		std::remove_if(_items.begin(), _items.end(), [&](auto &item) {
			if (item.phase >= kMaxPhaseDuration) {
				if (item.renderSrb) item.renderSrb->deleteLater();
				if (item.computeUpdateSrb) item.computeUpdateSrb->deleteLater();
				if (item.computeInitSrb) item.computeInitSrb->deleteLater();
				if (item.renderUniformBuffer) item.renderUniformBuffer->deleteLater();
				if (item.computeUpdateUniformBuffer) item.computeUpdateUniformBuffer->deleteLater();
				if (item.computeInitUniformBuffer) item.computeInitUniformBuffer->deleteLater();
				if (item.particleBuffer) item.particleBuffer->deleteLater();
				if (item.sampler) item.sampler->deleteLater();
				if (item.texture) item.texture->deleteLater();
				item = {};
				return true;
			}
			return false;
		}),
		_items.end());

	if (hadItems && _items.empty()) {
		_allDone.fire({});
	}
}

void ThanosEffectRenderer::addItem(ThanosItem item) {
	_pendingItems.push_back(std::move(item));
}

bool ThanosEffectRenderer::hasActiveItems() const {
	return !_items.empty() || !_pendingItems.empty();
}

rpl::producer<> ThanosEffectRenderer::allDone() const {
	return _allDone.events();
}

void ThanosEffectRenderer::addPendingItems(QRhiCommandBuffer *cb) {
	if (_pendingItems.empty() || !_rhi) {
		return;
	}

	auto *rub = _rhi->nextResourceUpdateBatch();

	for (auto &pending : _pendingItems) {
		auto animating = createAnimatingItem(std::move(pending));
		if (animating.texture) {
			auto image = animating.uploadImage;
			if (!image.isNull()) {
				rub->uploadTexture(
					animating.texture,
					QRhiTextureUploadDescription(
						QRhiTextureUploadEntry(
							0, 0,
							QRhiTextureSubresourceUploadDescription(
								image))));
			}
			animating.uploadImage = QImage();
			_items.push_back(std::move(animating));
		}
	}
	_pendingItems.clear();

	cb->resourceUpdate(rub);
}

ThanosEffectRenderer::AnimatingItem ThanosEffectRenderer::createAnimatingItem(
		ThanosItem &&item) {
	AnimatingItem result;
	result.rect = item.rect;

	const auto w = int(item.rect.width());
	const auto h = int(item.rect.height());
	if (w <= 0 || h <= 0 || item.snapshot.isNull()) {
		return result;
	}

	const auto totalPixels = uint32_t(w) * uint32_t(h);
	if (totalPixels <= kMaxParticleCount) {
		result.particleCountX = uint32_t(w);
		result.particleCountY = uint32_t(h);
	} else {
		const auto aspectRatio = float(w) / float(h);
		result.particleCountY = uint32_t(
			std::sqrt(float(kMaxParticleCount) / aspectRatio));
		result.particleCountX = uint32_t(
			float(kMaxParticleCount) / float(result.particleCountY));
		if (result.particleCountX < 1) result.particleCountX = 1;
		if (result.particleCountY < 1) result.particleCountY = 1;
	}
	const auto particleCount =
		result.particleCountX * result.particleCountY;

	auto *tex = _rhi->newTexture(
		QRhiTexture::RGBA8,
		QSize(item.snapshot.width(), item.snapshot.height()));
	tex->create();
	result.texture = tex;

	result.uploadImage = item.snapshot.convertToFormat(
		QImage::Format_RGBA8888_Premultiplied);

	auto *sampler = _rhi->newSampler(
		QRhiSampler::Linear,
		QRhiSampler::Linear,
		QRhiSampler::None,
		QRhiSampler::ClampToEdge,
		QRhiSampler::ClampToEdge);
	sampler->create();
	result.sampler = sampler;

	auto *particleBuf = _rhi->newBuffer(
		QRhiBuffer::Static,
		QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer,
		particleCount * kParticleStride);
	particleBuf->create();
	result.particleBuffer = particleBuf;

	auto *initUbo = _rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(ComputeInitUniforms));
	initUbo->create();
	result.computeInitUniformBuffer = initUbo;

	auto *updateUbo = _rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(ComputeUpdateUniforms));
	updateUbo->create();
	result.computeUpdateUniformBuffer = updateUbo;

	result.computeInitSrb = _rhi->newShaderResourceBindings();
	result.computeInitSrb->setBindings({
		QRhiShaderResourceBinding::bufferLoadStore(
			0,
			QRhiShaderResourceBinding::ComputeStage,
			particleBuf),
		QRhiShaderResourceBinding::uniformBuffer(
			1,
			QRhiShaderResourceBinding::ComputeStage,
			initUbo),
	});
	result.computeInitSrb->create();

	result.computeUpdateSrb = _rhi->newShaderResourceBindings();
	result.computeUpdateSrb->setBindings({
		QRhiShaderResourceBinding::bufferLoadStore(
			0,
			QRhiShaderResourceBinding::ComputeStage,
			particleBuf),
		QRhiShaderResourceBinding::uniformBuffer(
			1,
			QRhiShaderResourceBinding::ComputeStage,
			updateUbo),
	});
	result.computeUpdateSrb->create();

	auto *renderUbo = _rhi->newBuffer(
		QRhiBuffer::Dynamic,
		QRhiBuffer::UniformBuffer,
		sizeof(RenderUniforms));
	renderUbo->create();
	result.renderUniformBuffer = renderUbo;

	result.renderSrb = _rhi->newShaderResourceBindings();
	result.renderSrb->setBindings({
		QRhiShaderResourceBinding::uniformBuffer(
			0,
			QRhiShaderResourceBinding::VertexStage,
			renderUbo),
		QRhiShaderResourceBinding::sampledTexture(
			1,
			QRhiShaderResourceBinding::FragmentStage,
			tex,
			sampler),
	});
	result.renderSrb->create();

	return result;
}

void ThanosEffectRenderer::destroyAnimatingItem(AnimatingItem &item) {
	delete item.renderSrb;
	delete item.computeUpdateSrb;
	delete item.computeInitSrb;
	delete item.renderUniformBuffer;
	delete item.computeUpdateUniformBuffer;
	delete item.computeInitUniformBuffer;
	delete item.particleBuffer;
	delete item.sampler;
	delete item.texture;
	item = {};
}

void ThanosEffectRenderer::releaseResources() {
	for (auto &item : _items) {
		destroyAnimatingItem(item);
	}
	_items.clear();

	delete _renderPipeline;
	_renderPipeline = nullptr;
	delete _renderSrbLayout;
	_renderSrbLayout = nullptr;
	delete _computeUpdatePipeline;
	_computeUpdatePipeline = nullptr;
	delete _computeUpdateSrbLayout;
	_computeUpdateSrbLayout = nullptr;
	delete _computeInitPipeline;
	_computeInitPipeline = nullptr;
	delete _computeInitSrbLayout;
	_computeInitSrbLayout = nullptr;

	delete _placeholderSampler;
	_placeholderSampler = nullptr;
	delete _placeholderTexture;
	_placeholderTexture = nullptr;
	delete _placeholderParticleBuffer;
	_placeholderParticleBuffer = nullptr;

	delete _renderUniformBuffer;
	_renderUniformBuffer = nullptr;
	delete _computeUpdateUniformBuffer;
	_computeUpdateUniformBuffer = nullptr;
	delete _computeInitUniformBuffer;
	_computeInitUniformBuffer = nullptr;
	delete _quadVertexBuffer;
	_quadVertexBuffer = nullptr;

	_initialized = false;
}

} // namespace Ui

#endif // Qt >= 6.7
