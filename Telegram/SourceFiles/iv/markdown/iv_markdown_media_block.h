/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_common.h"

#include "ui/click_handler.h"
#include "ui/painter.h"
#include "ui/text/text_entity.h"

#include <memory>

#include <QtCore/QPoint>
#include <QtCore/QRect>

namespace Iv::Markdown {

struct MarkdownArticlePaintCaches;
class MediaRuntime;
struct PreparedAudioBlockData;
struct PreparedChannelBlockData;
struct PreparedGroupedMediaBlockData;
struct PreparedMapBlockData;
struct PreparedPhotoBlockData;
struct PreparedVideoBlockData;

class MediaBlockHost {
public:
	virtual ~MediaBlockHost() = default;

	virtual void requestRepaint(QRect articleRect) = 0;
	virtual void requestRelayout(QRect articleRect) = 0;
};

struct MediaBlockSelectionData {
	QString copyText;
	TextWithEntities caption;
};

class MediaBlock : public std::enable_shared_from_this<MediaBlock> {
public:
	virtual ~MediaBlock();

	void setHost(MediaBlockHost *host);
	[[nodiscard]] MediaBlockHost *host() const;

	[[nodiscard]] virtual uint64 stableId() const = 0;
	[[nodiscard]] virtual int resizeGetHeight(int width) = 0;
	virtual void setGeometry(QRect geometry) = 0;
	[[nodiscard]] virtual QRect geometry() const = 0;
	[[nodiscard]] virtual int firstLineBaseline() const = 0;
	virtual void paint(
		Painter &p,
		QRect clip,
		const MarkdownArticlePaintCaches &caches) const = 0;
	[[nodiscard]] virtual ClickHandlerPtr linkAt(QPoint point) const = 0;
	[[nodiscard]] virtual MediaActivation activationAt(QPoint point) const = 0;
	[[nodiscard]] virtual MediaBlockSelectionData selectionData() const = 0;

protected:
	void requestRepaint(QRect articleRect) const;
	void requestRelayout(QRect articleRect) const;

private:
	MediaBlockHost *_host = nullptr;
};

[[nodiscard]] std::shared_ptr<MediaBlock> CreatePhotoMediaBlock(
	const PreparedPhotoBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] std::shared_ptr<MediaBlock> CreateVideoMediaBlock(
	const PreparedVideoBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] std::shared_ptr<MediaBlock> CreateAudioMediaBlock(
	const PreparedAudioBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] std::shared_ptr<MediaBlock> CreateMapMediaBlock(
	const PreparedMapBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] std::shared_ptr<MediaBlock> CreateChannelMediaBlock(
	const PreparedChannelBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);
[[nodiscard]] std::shared_ptr<MediaBlock> CreateGroupedMediaBlock(
	const PreparedGroupedMediaBlockData &prepared,
	const std::shared_ptr<MediaRuntime> &mediaRuntime);

} // namespace Iv::Markdown
