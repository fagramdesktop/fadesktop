/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_drag.h"

#include "base/call_delayed.h"
#include "core/file_utilities.h"
#include "data/data_photo.h"
#include "data/data_photo_media.h"
#include "main/main_session.h"
#include "storage/storage_account.h"

namespace HistoryView {
namespace {

constexpr auto kTempFileLifetime = crl::time(60 * 1000);

[[nodiscard]] QString WritePhotoTempFile(
		not_null<PhotoData*> photo,
		not_null<Data::PhotoMedia*> media,
		TimeId itemDate) {
	constexpr auto kSize = Data::PhotoSize::Large;
	const auto base = photo->session().local().tempDirectory();
	if (base.isEmpty()) {
		return QString();
	}
	const auto dir = base.endsWith('/')
		? (base + u"drag"_q)
		: (base + u"/drag"_q);

	static auto sweptDirs = base::flat_set<QString>();
	if (sweptDirs.emplace(dir).second) {
		QDir(dir).removeRecursively();
	}

	if (!QDir().mkpath(dir)) {
		return QString();
	}
	const auto path = filedialogDefaultName(
		u"photo"_q,
		u".jpg"_q,
		dir,
		false,
		itemDate);
	if (const auto bytes = media->imageBytes(kSize); !bytes.isEmpty()) {
		auto file = QFile(path);
		if (!file.open(QIODevice::WriteOnly)
			|| file.write(bytes) != bytes.size()) {
			file.remove();
			return QString();
		}
		return path;
	}
	const auto image = media->image(kSize);
	if (!image) {
		return QString();
	}
	const auto original = image->original();
	return (!original.isNull() && original.save(path, "JPG"))
		? path
		: QString();
}

} // namespace

DragMimeData::DragMimeData(QString tempPath)
: _tempPath(std::move(tempPath)) {
}

DragMimeData::~DragMimeData() {
	if (_tempPath.isEmpty()) {
		return;
	}
	base::call_delayed(kTempFileLifetime, [path = _tempPath] {
		QFile::remove(path);
	});
}

PhotoDragData PreparePhotoDragData(
		not_null<PhotoData*> photo,
		TimeId itemDate) {
	if (photo->isNull()) {
		return {};
	}
	const auto media = photo->activeMediaView();
	constexpr auto kSize = Data::PhotoSize::Large;
	const auto animated = media
		&& !media->videoContent(kSize).isEmpty();
	if (!media || !media->loaded() || animated) {
		return {};
	}
	auto result = PhotoDragData();
	if (const auto image = media->image(kSize)) {
		result.image = image->original();
	}
	result.tempPath = WritePhotoTempFile(photo, media.get(), itemDate);
	return result;
}

} // namespace HistoryView
