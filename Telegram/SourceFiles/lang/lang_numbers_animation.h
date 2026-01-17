/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/effects/numbers_animation.h"

namespace Lang {

template <typename ResultString>
struct StartReplacements;

template <>
struct StartReplacements<Ui::StringWithNumbers> {
	static inline Ui::StringWithNumbers Call(QString &&langString) {
		return { std::move(langString) };
	}
};

template <typename ResultString>
struct ReplaceTag;

template <>
struct ReplaceTag<Ui::StringWithNumbers> {
	static Ui::StringWithNumbers Call(
		Ui::StringWithNumbers &&original,
		ushort tag,
		const Ui::StringWithNumbers &replacement);
};

} // namespace Lang
