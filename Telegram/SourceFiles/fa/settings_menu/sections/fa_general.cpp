/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"

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
#include "ui/boxes/confirm_box.h"
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

#define SettingsMenuJsonSwitch(LangKey, Option, ControlId) do { \
	const auto _btn = container->add(object_ptr<Button>( \
		container, \
		fatr::LangKey(), \
		st::settingsButtonNoIcon \
	)); \
	_btn->toggleOn( \
		rpl::single(::FASettings::JsonSettings::GetBool(#Option)) \
	)->toggledValue( \
	) | rpl::filter([](bool enabled) { \
		return (enabled != ::FASettings::JsonSettings::GetBool(#Option)); \
	}) | rpl::on_next([](bool enabled) { \
		::FASettings::JsonSettings::Write(); \
		::FASettings::JsonSettings::Set(#Option, enabled); \
		::FASettings::JsonSettings::Write(); \
	}, container->lifetime()); \
	Settings::FADeepLinkMenu::AttachSettingsContextMenu( \
		_btn, ControlId, controller); \
} while (false)

#define RestartSettingsMenuJsonSwitch(LangKey, Option, ControlId) do { \
	const auto _btn = container->add(object_ptr<Button>( \
		container, \
		fatr::LangKey(), \
		st::settingsButtonNoIcon \
	)); \
	_btn->toggleOn( \
		rpl::single(::FASettings::JsonSettings::GetBool(#Option)) \
	)->toggledValue( \
	) | rpl::filter([](bool enabled) { \
		return (enabled != ::FASettings::JsonSettings::GetBool(#Option)); \
	}) | rpl::on_next([=](bool enabled) { \
		::FASettings::JsonSettings::Write(); \
		::FASettings::JsonSettings::Set(#Option, enabled); \
		::FASettings::JsonSettings::Write(); \
		controller->show(Ui::MakeConfirmBox({ \
			.text = fatr::fa_setting_need_restart(), \
			.confirmed = [=] { \
				::Core::Restart(); \
			}, \
			.confirmText = fatr::fa_restart() \
		})); \
	}, container->lifetime()); \
	Settings::FADeepLinkMenu::AttachSettingsContextMenu( \
		_btn, ControlId, controller); \
} while (false)

namespace Settings {

    rpl::producer<QString> FAGeneral::title() {
        return fatr::fa_general();
    }

    FAGeneral::FAGeneral(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FAGeneral::SetupGeneral(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, fatr::fa_general());

        RestartSettingsMenuJsonSwitch(fa_enable_whats_new_chat, enable_whats_new_chat, u"fa/general/whats-new-chat"_q);
        Ui::AddDividerText(container, fatr::fa_enable_whats_new_chat_desc());
        SettingsMenuJsonSwitch(fa_disable_ads, disable_ads, u"fa/general/disable-ads"_q);
        Ui::AddDividerText(container, fatr::fa_disable_ads_desc());
        SettingsMenuJsonSwitch(fa_show_start_token, show_start_token, u"fa/general/start-token"_q);
        Ui::AddDividerText(container, fatr::fa_show_start_token_desc());
        SettingsMenuJsonSwitch(fa_show_peer_ids, show_peer_id, u"fa/general/peer-ids"_q);
        Ui::AddDividerText(container, fatr::fa_show_peer_ids_desc());
        SettingsMenuJsonSwitch(fa_show_dc_ids, show_dc_id, u"fa/general/dc-ids"_q);
        Ui::AddDividerText(container, fatr::fa_show_dc_ids_desc());
        SettingsMenuJsonSwitch(fa_id_in_botapi_type, show_id_botapi, u"fa/general/botapi-id"_q);
        Ui::AddDividerText(container, fatr::fa_id_in_botapi_type_desc());
        SettingsMenuJsonSwitch(fa_local_tg_premium, local_premium, u"fa/general/local-premium"_q);
        Ui::AddDividerText(container, fatr::fa_local_tg_premium_desc());
        SettingsMenuJsonSwitch(fa_show_registration_date, show_registration_date, u"fa/general/registration-date"_q);
        Ui::AddDividerText(container, fatr::fa_show_registration_date_desc());
    }

    void FAGeneral::SetupFAGeneral(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
    	SetupGeneral(container, controller);
    }

    void FAGeneral::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAGeneral(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram