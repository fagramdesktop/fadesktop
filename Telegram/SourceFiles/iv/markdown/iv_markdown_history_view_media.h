/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_common.h"
#include "base/weak_ptr.h"

#include <functional>
#include <memory>
#include <vector>

namespace Window {
class SessionController;
} // namespace Window

class DocumentData;
class History;
class HistoryItem;
class PhotoData;

namespace HistoryView {
class Element;
class Media;
class Message;
} // namespace HistoryView

namespace Data {
class Session;
} // namespace Data

namespace Iv::Markdown {

class IvHistoryViewMediaHost final {
public:
	IvHistoryViewMediaHost(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		QString pageUrl);
	~IvHistoryViewMediaHost();

	[[nodiscard]] not_null<::Data::Session*> session() const;
	[[nodiscard]] not_null<HistoryItem*> item() const;
	[[nodiscard]] not_null<HistoryView::Message*> view() const;
	[[nodiscard]] const QString &pageUrl() const;

	void registerPhoto(not_null<PhotoData*> photo) const;
	void registerDocument(not_null<DocumentData*> document) const;

private:
	struct State;
	std::unique_ptr<State> _state;
};

enum class IvHistoryViewMediaKind {
	Photo,
	Document,
	Map,
	Audio,
};

struct IvHistoryViewMediaDescriptor {
	using MediaFactory = std::function<std::unique_ptr<HistoryView::Media>(
		not_null<HistoryView::Element*> view)>;

	uint64 stableId = 0;
	IvHistoryViewMediaKind kind = IvHistoryViewMediaKind::Map;
	QString copyText;
	QSize layoutHint;
	std::shared_ptr<IvHistoryViewMediaHost> host;
	MediaFactory mediaFactory;
	std::vector<std::shared_ptr<void>> keepAlive;
	std::shared_ptr<PhotoRuntime> photo;
	std::shared_ptr<DocumentRuntime> document;
};

class IvHistoryViewMediaBlockFactory final : public HostedMediaBlockFactory {
public:
	using PhotoFactory = std::function<std::shared_ptr<MediaBlock>(
		Window::SessionController *controller,
		const PreparedPhotoBlockData &prepared)>;
	using VideoFactory = std::function<std::shared_ptr<MediaBlock>(
		Window::SessionController *controller,
		const PreparedVideoBlockData &prepared)>;
	using AudioFactory = std::function<std::shared_ptr<MediaBlock>(
		Window::SessionController *controller,
		const PreparedAudioBlockData &prepared)>;
	using MapFactory = std::function<std::shared_ptr<MediaBlock>(
		Window::SessionController *controller,
		const PreparedMapBlockData &prepared)>;

	IvHistoryViewMediaBlockFactory(
		base::weak_ptr<Window::SessionController> controller,
		PhotoFactory createPhoto = {},
		VideoFactory createVideo = {},
		AudioFactory createAudio = {},
		MapFactory createMap = {});

	[[nodiscard]] std::shared_ptr<MediaBlock> createPhoto(
		const PreparedPhotoBlockData &prepared) const override;
	[[nodiscard]] std::shared_ptr<MediaBlock> createVideo(
		const PreparedVideoBlockData &prepared) const override;
	[[nodiscard]] std::shared_ptr<MediaBlock> createAudio(
		const PreparedAudioBlockData &prepared) const override;
	[[nodiscard]] std::shared_ptr<MediaBlock> createMap(
		const PreparedMapBlockData &prepared) const override;

private:
	template <typename Prepared, typename Factory>
	[[nodiscard]] std::shared_ptr<MediaBlock> create(
			const Prepared &prepared,
			const Factory &factory) const;

	const base::weak_ptr<Window::SessionController> _controller;
	const PhotoFactory _createPhoto;
	const VideoFactory _createVideo;
	const AudioFactory _createAudio;
	const MapFactory _createMap;
};

template <typename Prepared, typename Factory>
std::shared_ptr<MediaBlock> IvHistoryViewMediaBlockFactory::create(
		const Prepared &prepared,
		const Factory &factory) const {
	if (!factory) {
		return nullptr;
	}
	const auto controller = _controller.get();
	return controller ? factory(controller, prepared) : nullptr;
}

[[nodiscard]] std::shared_ptr<MediaBlock> CreateIvHistoryViewMediaBlock(
	Window::SessionController *controller,
	IvHistoryViewMediaDescriptor descriptor);

} // namespace Iv::Markdown
