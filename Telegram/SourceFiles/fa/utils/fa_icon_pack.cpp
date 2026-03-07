/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/utils/fa_icon_pack.h"

#include "fa/settings/fa_settings.h"

#include <QtCore/QFile>

namespace FAIcons {

bool UseCustomIconPack() {
	return FASettings::JsonSettings::GetBool("use_custom_icon_pack");
}

QString MiconPath(const QString &name) {
	if (UseCustomIconPack()) {
		const auto custom = u":/fa/micons/"_q + name;
		if (QFile::exists(custom)) {
			return custom;
		}
	}
	return u":/icons/"_q + name;
}

} // namespace FAIcons
