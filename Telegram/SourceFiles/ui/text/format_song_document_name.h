/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/text/format_song_name.h"

class DocumentData;

namespace Ui::Text {

[[nodiscard]] FormatSongName FormatSongNameFor(
	not_null<DocumentData*> document);

[[nodiscard]] TextWithEntities FormatDownloadsName(
	not_null<DocumentData*> document);

[[nodiscard]] FormatSongName FormatVoiceName(
	not_null<DocumentData*> document,
	FullMsgId contextId);

} // namespace Ui::Text
