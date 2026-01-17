/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "data/data_game.h"

GameData::GameData(not_null<Data::Session*> owner, const GameId &id)
: owner(owner)
, id(id) {
}
