/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

// thanks rabbitGram

#include <ui/boxes/single_choice_box.h>

#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/sections/fa_chats.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/sections/fa_logs.h"

#include "fa/lang/fa_lang.h"
#include "fa/settings/fa_settings.h"

#include "core/application.h"
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
#include "styles/style_payments.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/basic_click_handlers.h"

#define SettingsMenuJsonSwitch(LangKey, Option) container->add(object_ptr<Button>( \
	container, \
    FAlang::RplTranslate(QString(#LangKey)), \
	st::settingsButtonNoIcon \
))->toggleOn( \
	rpl::single(::FASettings::JsonSettings::GetBool(#Option)) \
)->toggledValue( \
) | rpl::filter([](bool enabled) { \
	return (enabled != ::FASettings::JsonSettings::GetBool(#Option)); \
}) | rpl::on_next([](bool enabled) { \
	::FASettings::JsonSettings::Write(); \
	::FASettings::JsonSettings::Set(#Option, enabled); \
	::FASettings::JsonSettings::Write(); \
}, container->lifetime());

namespace Settings {

    rpl::producer<QString> FA::title() {
        return FAlang::RplTranslate(QString("fa_client_preferences"));
    }

    FA::FA(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FA::SetupFASettings(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
    	AddSubsectionTitle(container, FAlang::RplTranslate(QString("fa_categories")));

		const auto addSection = [&](
				rpl::producer<QString> label,
				Type type,
				IconDescriptor &&descriptor) {
			AddButtonWithIcon(
				container,
				std::move(label),
				st::settingsButton,
				std::move(descriptor)
			)->addClickHandler([=] {
				showOther(type);
			});
		};
    	addSection(
			FAlang::RplTranslate(QString("fa_general")),
			FAGeneral::Id(),
			{ &st::menuIconShowAll });
    	addSection(
			FAlang::RplTranslate(QString("fa_chats")),
			FAChats::Id(),
			{ &st::menuIconChatBubble });
    	addSection(
			FAlang::RplTranslate(QString("fa_context_menu")),
			FAContextMenu::Id(),
			{ &st::menuIconSigned });
    	addSection(
			FAlang::RplTranslate(QString("fa_appearance")),
			FAAppearance::Id(),
			{ &st::menuIconPalette });
    	addSection(
			FAlang::RplTranslate(QString("fa_debug_logs")),
			FALogs::Id(),
			{ &st::menuIconFile });
    }

	void FA::SetupLinks(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller)
    {
    	AddSubsectionTitle(container, FAlang::RplTranslate(QString("fa_links")));

	    AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_channel")),
			rpl::single(QString("@FAgramDesktop")),
			st::settingsButton,
			{ &st::menuIconChannel }
		)->setClickedCallback([=] {
			Core::App().openLocalUrl("tg://resolve?domain=FAgramDesktop", {});
		});

    	AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_group")),
			rpl::single(QString("@FAgramChat")),
			st::settingsButton,
			{ &st::menuIconGroups }
		)->setClickedCallback([=] {
			Core::App().openLocalUrl("tg://resolve?domain=FAgramChat", {});
		});

    	AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_translation")),
			rpl::single(QString("Crowdin")),
			st::settingsButton,
			{ &st::menuIconTranslate }
		)->setClickedCallback([=] {
			UrlClickHandler::Open("https://crowdin.com/project/fagramdesktop");
		});

    	AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_source_code")),
			rpl::single(QString("Github")),
			st::settingsButton,
			{ &st::menuIconLink }
		)->setClickedCallback([=] {
			UrlClickHandler::Open("https://github.com/fagramdesktop/fadesktop");
		});
    }

    void FA::SetupDown(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {

		AddSubsectionTitle(container, rpl::single(QString("FAgram")));

		const auto addSection = [&](
				rpl::producer<QString> label,
				Type type,
				IconDescriptor &&descriptor) {
			AddButtonWithIcon(
				container,
				std::move(label),
				st::settingsButton,
				std::move(descriptor)
			)->addClickHandler([=] {
				showOther(type);
			});
		};

    	AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_restart_client")),
			rpl::single(QString("")),
			st::settingsButton,
			{ &st::menuIconRemove }
		)->setClickedCallback([=] {
			Core::App().openLocalUrl("tg://fa/restart", {});
		});

    	AddButtonWithLabel(
			container,
			FAlang::RplTranslate(QString("fa_quit_client")),
			rpl::single(QString("")),
			st::settingsButton,
			{ &st::menuIconCancel }
		)->setClickedCallback([=] {
			Core::App().openLocalUrl("tg://fa/quit", {});
		});
    }

    void FA::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

    	Ui::AddSkip(content);
        SetupFASettings(content, controller);

		Ui::AddSkip(content);
    	Ui::AddDivider(content);
    	Ui::AddSkip(content);
    	SetupLinks(content, controller);

		Ui::AddSkip(content);
    	Ui::AddDivider(content);
    	Ui::AddSkip(content);
    	SetupDown(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings
