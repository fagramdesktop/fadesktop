/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "spellcheck/spellcheck_types.h"

class History;

class HistoryTranslation final {
public:
	HistoryTranslation(
		not_null<History*> history,
		const LanguageId &offerFrom);

	void offerFrom(LanguageId id);
	[[nodiscard]] LanguageId offeredFrom() const;

	void translateTo(LanguageId id);
	[[nodiscard]] LanguageId translatedTo() const;

private:
	const not_null<History*> _history;

	LanguageId _offerFrom;
	LanguageId _translatedTo;

};