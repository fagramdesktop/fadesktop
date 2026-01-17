/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/text/text_entity.h"

namespace Main {
class Session;
} // namespace Main

namespace Api {

enum class ConvertOption {
	WithLocal,
	SkipLocal,
};

[[nodiscard]] EntitiesInText EntitiesFromMTP(
	Main::Session *session,
	const QVector<MTPMessageEntity> &entities);

[[nodiscard]] MTPVector<MTPMessageEntity> EntitiesToMTP(
	Main::Session *session,
	const EntitiesInText &entities,
	ConvertOption option = ConvertOption::WithLocal);

[[nodiscard]] TextWithEntities ParseTextWithEntities(
	Main::Session *session,
	const MTPTextWithEntities &text);

} // namespace Api
