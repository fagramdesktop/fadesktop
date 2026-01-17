/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <crl/crl_object_on_queue.h>
#include "ui/emoji_config.h"

namespace Stickers {

class EmojiImageLoader {
public:
	using UniversalImages = Ui::Emoji::UniversalImages;

	explicit EmojiImageLoader(crl::weak_on_queue<EmojiImageLoader> weak);

	void init(
		std::shared_ptr<UniversalImages> images,
		bool largeEnabled);

	[[nodiscard]] QImage prepare(EmojiPtr emoji) const;
	void switchTo(std::shared_ptr<UniversalImages> images);
	std::shared_ptr<UniversalImages> releaseImages();

private:
	crl::weak_on_queue<EmojiImageLoader> _weak;
	std::shared_ptr<UniversalImages> _images;

};

} // namespace Stickers
