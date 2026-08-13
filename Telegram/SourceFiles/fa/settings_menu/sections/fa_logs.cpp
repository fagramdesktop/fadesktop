/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_logs.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"
#include "fa/utils/telegram_helpers.h"
#include "fa/ui/components/fa_ui_components.h"

#include "fa_lang_auto.h"

#include "lang_auto.h"
#include "mainwindow.h"
#include "settings/settings_common.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/checkbox.h"
#include "ui/vertical_list.h"
#include "boxes/connection_box.h"
#include "platform/platform_specific.h"
#include "window/window_session_controller.h"
#include "lang/lang_instance.h"
#include "core/application.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"
#include "ui/widgets/continuous_sliders.h"

namespace Settings {

    rpl::producer<QString> FALogs::title() {
        return fatr::fa_debug_logs();
    }

    FALogs::FALogs(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FALogs::SetupLogs(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        FA::Ui::AddModernSectionHeader(container, fatr::fa_debug_logs());
		const auto card = FA::Ui::CreateCardContainer(container);
    	
		const auto cleanLogsButton = FA::Ui::AddCardButton(
			card,
			fatr::fa_clean_debug_logs(),
			[=] {
				controller->showToast(fatr::fa_cleaning_debug_logs(fatr::now), 500);
				cleanDebugLogs();
				controller->showToast(fatr::fa_cleaned_debug_logs(fatr::now), 1000);
			},
			&st::menuIconClear);
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			cleanLogsButton,
			u"fa/logs/clean"_q,
			controller);

		FA::Ui::AddCardDivider(card);
		
		auto &settings = FASettings::FASettings::getInstance();
		const auto debugLogsRow = FA::Ui::AddCardToggle(
			card,
			fatr::fa_debug_logs(),
			fatr::fa_logs_dir(),
			settings.debugLogsValue(),
			[&settings](bool enabled) {
				settings.setDebugLogs(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			debugLogsRow, u"fa/logs/debug-logs"_q, controller);
    }

    void FALogs::SetupFALogs(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
    	SetupLogs(container, controller);
    }

    void FALogs::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFALogs(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram