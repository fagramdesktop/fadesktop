/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_chats.h"
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
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/buttons.h"
#include "base/call_delayed.h"

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

    rpl::producer<QString> FAChats::title() {
        return fatr::fa_chats();
    }

    FAChats::FAChats(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FAChats::SetupChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, fatr::fa_chats());

		SettingsMenuJsonSwitch(fa_parse_markdown_hyperlink, auto_format_markdown, u"fa/chats/markdown-hyperlink"_q);
		Ui::AddDividerText(container, fatr::fa_parse_markdown_hyperlink_desc());
		SettingsMenuJsonSwitch(fa_show_seconds_message, seconds_message, u"fa/chats/seconds-message"_q);
		Ui::AddDividerText(container, fatr::fa_show_seconds_message_desc());
		SettingsMenuJsonSwitch(fa_disable_custom_chat_background, disable_custom_chat_background, u"fa/chats/disable-custom-background"_q);
		Ui::AddDividerText(container, fatr::fa_disable_custom_chat_background_desc());
		SettingsMenuJsonSwitch(fa_hide_open_webapp_button_chatlist, hide_open_webapp_button_chatlist, u"fa/chats/hide-webapp-button"_q);
		Ui::AddDividerText(container, fatr::fa_hide_open_webapp_button_chatlist_desc());
		SettingsMenuJsonSwitch(fa_show_discuss_button, show_discuss_button, u"fa/chats/show-discuss-button"_q);
		Ui::AddDividerText(container, fatr::fa_show_discuss_button_desc());
		SettingsMenuJsonSwitch(fa_show_fastshare_in_chats, show_fastshare_in_chats, u"fa/chats/show-share-in-chats"_q);
		Ui::AddDividerText(container, fatr::fa_show_fastshare_in_chats_desc());
		SettingsMenuJsonSwitch(fa_show_message_details, show_message_details, u"fa/chats/message-details"_q);
		Ui::AddDividerText(container, fatr::fa_show_message_details_desc());
		SettingsMenuJsonSwitch(fa_add_comma_after_mention, add_comma_after_mention, u"fa/chats/add-comma-after-mention"_q);
		Ui::AddDividerText(container, fatr::fa_add_comma_after_mention_desc());
		SettingsMenuJsonSwitch(fa_unlimited_pinned_chats, unlimited_pinned_chats, u"fa/chats/unlimited-pinned-chats"_q);
		Ui::AddDividerText(container, fatr::fa_unlimited_pinned_chats_desc());

		const auto statusDotBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_show_status_dot(),
			st::settingsButtonNoIcon
		));
		const auto onlineOnlyBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_status_dot_online_only(),
			st::settingsButtonNoIcon
		));

		statusDotBtn->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("show_status_dot"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("show_status_dot"));
		}) | rpl::on_next([=](bool enabled) {
			::FASettings::JsonSettings::Set("show_status_dot", enabled);
			::FASettings::JsonSettings::Write();
			onlineOnlyBtn->setEnabled(enabled);
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			statusDotBtn, u"fa/chats/status-dot"_q, controller);

		onlineOnlyBtn->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("status_dot_online_only"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("status_dot_online_only"));
		}) | rpl::on_next([=](bool enabled) {
			::FASettings::JsonSettings::Set("status_dot_online_only", enabled);
			::FASettings::JsonSettings::Write();
		}, container->lifetime());
		onlineOnlyBtn->setEnabled(::FASettings::JsonSettings::GetBool("show_status_dot"));
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			onlineOnlyBtn, u"fa/chats/status-dot-online"_q, controller);
		Ui::AddDividerText(container, fatr::fa_status_dot_desc());

		RestartSettingsMenuJsonSwitch(fa_hide_all_chats_folder, hide_all_chats_folder, u"fa/chats/hide-all-chats-folder"_q);
		Ui::AddDividerText(container, fatr::fa_hide_all_chats_folder_desc());

		const auto hideBlockedBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_hide_blocked_user_messages(),
			st::settingsButtonNoIcon
		));
		hideBlockedBtn->setColorOverride(QColor(255, 0, 0));
		hideBlockedBtn->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("hide_blocked_user_messages"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("hide_blocked_user_messages"));
		}) | rpl::on_next([=](bool enabled) {
			::FASettings::JsonSettings::Set("hide_blocked_user_messages", enabled);
			::FASettings::JsonSettings::Write();

			controller->showToast(fatr::fa_restarting_in_seconds(fatr::now));
			base::call_delayed(crl::time(3000), container, [] {
				::Core::Restart();
			});
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			hideBlockedBtn, u"fa/chats/hide-blocked-messages"_q, controller);

		Ui::AddDividerText(container, fatr::fa_hide_blocked_user_messages_desc());
    }

    void FAChats::SetupFAChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
		SetupChats(container, controller);
    }

    void FAChats::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAChats(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram
