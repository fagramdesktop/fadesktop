/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {

class GenericBox;
class DynamicImage;

enum class CollectibleType {
	Phone,
	Username,
};

[[nodiscard]] CollectibleType DetectCollectibleType(const QString &entity);

struct CollectibleInfo {
	QString entity;
	QString copyText;
	std::shared_ptr<DynamicImage> ownerUserpic;
	QString ownerName;
	uint64 cryptoAmount = 0;
	uint64 amount = 0;
	QString cryptoCurrency;
	QString currency;
	QString url;
	TimeId date = 0;
};

void CollectibleInfoBox(not_null<Ui::GenericBox*> box, CollectibleInfo info);

} // namespace Ui
