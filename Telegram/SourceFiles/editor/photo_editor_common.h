/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Editor {

class Scene;

enum class RoundedCornersLevel {
	Large,
	Medium,
	Small,
	None,
};

[[nodiscard]] float64 RoundedCornersMultiplier(RoundedCornersLevel level);

struct EditorData {
	enum class CropType {
		Rect,
		Ellipse,
		RoundedRect,
	};

	TextWithEntities about;
	QString confirm;
	QSize exactSize;
	CropType cropType = CropType::Rect;
	bool keepAspectRatio = false;
	bool fixedCrop = false;
};

struct PhotoModifications {
	int angle = 0;
	bool flipped = false;
	QRect crop;
	EditorData::CropType cropType = EditorData::CropType::Rect;
	RoundedCornersLevel cornersLevel = RoundedCornersLevel::Large;
	std::shared_ptr<Scene> paint = nullptr;

	[[nodiscard]] bool empty() const;
	[[nodiscard]] explicit operator bool() const;
	~PhotoModifications();

};

[[nodiscard]] QImage ImageModified(
	QImage image,
	const PhotoModifications &mods);

} // namespace Editor
