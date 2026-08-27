/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/features/hide_archive_chats/hide_archive_chats.h"

#include "fa/settings/fa_settings.h"
#include "fa/ui/md3/fa_cards.h"

#include "fa_lang_auto.h"

#include "lang_auto.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace FA::Features::HideArchiveChats {

bool ShouldHide() {
	return FASettings::FASettings::getInstance().hideArchiveChats();
}

not_null<::Ui::RpWidget*> AddToggle(
		not_null<::Ui::VerticalLayout*> card,
		not_null<Window::SessionController*> controller) {
	auto &settings = FASettings::FASettings::getInstance();

	return FA::Ui::AddCardToggle(
		card,
		fatr::fa_hide_archived_chats(),
		fatr::fa_hide_archived_chats_desc(),
		settings.hideArchiveChatsValue(),
		[](bool enabled) {
			FASettings::FASettings::getInstance().setHideArchiveChats(enabled);
		});
}

} // namespace FA::Features::HideArchiveChats
