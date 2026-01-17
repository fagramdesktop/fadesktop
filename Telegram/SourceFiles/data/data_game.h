/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "data/data_photo.h"
#include "data/data_document.h"

struct GameData {
	GameData(not_null<Data::Session*> owner, const GameId &id);

	const not_null<Data::Session*> owner;
	GameId id = 0;
	uint64 accessHash = 0;
	QString shortName;
	QString title;
	QString description;
	PhotoData *photo = nullptr;
	DocumentData *document = nullptr;
};
