/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/utils/fa_icon_pack.h"

#include "fa/settings/fa_settings.h"
#include "ui/style/style_core_icon.h"

namespace FAIcons {

void InitIconPack() {
	if (FASettings::JsonSettings::GetBool("use_material_icon_pack")) {
		style::internal::SetUseIconOverride(true);
	}
}

} // namespace FAIcons
