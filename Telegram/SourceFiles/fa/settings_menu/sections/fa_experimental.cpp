/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_experimental.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"

#include "fa_lang_auto.h"

#include "lang_auto.h"
#include "mainwindow.h"
#include "settings/settings_common.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/wrap.h"
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
#include "styles/style_fa_styles.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"

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

namespace Settings {

    rpl::producer<QString> FAExperimental::title() {
        return fatr::fa_experimental();
    }

    FAExperimental::FAExperimental(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FAExperimental::SetupExperimental(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, fatr::fa_experimental());

		SettingsMenuJsonSwitch(fa_anti_delete_messages, anti_delete_messages, u"fa/experimental/anti-delete-messages"_q);
		Ui::AddDividerText(container, fatr::fa_anti_delete_messages_desc());
		SettingsMenuJsonSwitch(fa_remove_restrictions, remove_restrictions, u"fa/experimental/remove-restrictions"_q);
		Ui::AddDividerText(container, fatr::fa_remove_restrictions_desc());
		SettingsMenuJsonSwitch(fa_show_view_once_media, show_view_once_media, u"fa/experimental/show-view-once-media"_q);
		Ui::AddDividerText(container, fatr::fa_show_view_once_media_desc());
		SettingsMenuJsonSwitch(fa_suppress_auto_delete, suppress_auto_delete, u"fa/experimental/suppress-auto-delete"_q);
		Ui::AddDividerText(container, fatr::fa_suppress_auto_delete_desc());
    }

    void FAExperimental::SetupFAExperimental(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
		SetupExperimental(container, controller);
    }

    void FAExperimental::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAExperimental(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings
