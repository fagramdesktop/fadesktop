/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "ui/color_int_conversion.h"

namespace Ui {

QColor ColorFromSerialized(quint32 serialized) {
	return QColor(
		int((serialized >> 16) & 0xFFU),
		int((serialized >> 8) & 0xFFU),
		int(serialized & 0xFFU));
}

std::optional<QColor> MaybeColorFromSerialized(quint32 serialized) {
	return (serialized == quint32(-1))
		? std::nullopt
		: std::make_optional(ColorFromSerialized(serialized));
}

QColor Color32FromSerialized(quint32 serialized) {
	return QColor(
		int(serialized & 0xFFU),
		int((serialized >> 8) & 0xFFU),
		int((serialized >> 16) & 0xFFU),
		int((serialized >> 24) & 0xFFU));
}

} // namespace Ui
