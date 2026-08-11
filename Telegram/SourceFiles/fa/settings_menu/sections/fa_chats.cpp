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

		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_parse_markdown_hyperlink(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.autoFormatMarkdownValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.autoFormatMarkdown());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setAutoFormatMarkdown(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/markdown-hyperlink"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_parse_markdown_hyperlink_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_show_seconds_message(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.secondsMessageValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.secondsMessage());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setSecondsMessage(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/seconds-message"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_show_seconds_message_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_disable_custom_chat_background(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.disableCustomChatBackgroundValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.disableCustomChatBackground());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setDisableCustomChatBackground(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/disable-custom-background"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_disable_custom_chat_background_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_hide_open_webapp_button_chatlist(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.hideOpenWebappButtonChatlistValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.hideOpenWebappButtonChatlist());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setHideOpenWebappButtonChatlist(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/hide-webapp-button"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_hide_open_webapp_button_chatlist_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_show_discuss_button(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.showDiscussButtonValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.showDiscussButton());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setShowDiscussButton(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/show-discuss-button"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_show_discuss_button_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_show_fastshare_in_chats(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.showFastshareInChatsValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.showFastshareInChats());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setShowFastshareInChats(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/show-share-in-chats"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_show_fastshare_in_chats_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_show_message_details(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.showMessageDetailsValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.showMessageDetails());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setShowMessageDetails(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/message-details"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_show_message_details_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_add_comma_after_mention(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.addCommaAfterMentionValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.addCommaAfterMention());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setAddCommaAfterMention(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/add-comma-after-mention"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_add_comma_after_mention_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_unlimited_pinned_chats(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.unlimitedPinnedChatsValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.unlimitedPinnedChats());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setUnlimitedPinnedChats(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/unlimited-pinned-chats"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_unlimited_pinned_chats_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_unlimited_chat_folders(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.unlimitedChatFoldersValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.unlimitedChatFolders());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setUnlimitedChatFolders(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/unlimited-chat-folders"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_unlimited_chat_folders_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_delete_for_everyone(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.deleteForEveryoneValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.deleteForEveryone());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setDeleteForEveryone(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/delete-for-everyone"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_delete_for_everyone_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_last_seen_timestamp(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.lastSeenTimestampValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.lastSeenTimestamp());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setLastSeenTimestamp(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/last-seen-timestamp"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_last_seen_timestamp_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_show_forwarded_date_in_title(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.showForwardedDateInTitleValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.showForwardedDateInTitle());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setShowForwardedDateInTitle(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/show-forwarded-date-in-title"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_show_forwarded_date_in_title_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_disable_greeting_sticker(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.disableGreetingStickerValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.disableGreetingSticker());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setDisableGreetingSticker(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/disable-greeting-sticker"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_disable_greeting_sticker_desc());

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
			rpl::single(FASettings::FASettings::getInstance().showStatusDot())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().showStatusDot());
		}) | rpl::on_next([=](bool enabled) {
			FASettings::FASettings::getInstance().setShowStatusDot(enabled);
			
			onlineOnlyBtn->setEnabled(enabled);
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			statusDotBtn, u"fa/chats/status-dot"_q, controller);

		onlineOnlyBtn->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().statusDotOnlineOnly())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().statusDotOnlineOnly());
		}) | rpl::on_next([=](bool enabled) {
			FASettings::FASettings::getInstance().setStatusDotOnlineOnly(enabled);
			
		}, container->lifetime());
		onlineOnlyBtn->setEnabled(FASettings::FASettings::getInstance().showStatusDot());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			onlineOnlyBtn, u"fa/chats/status-dot-online"_q, controller);
		Ui::AddDividerText(container, fatr::fa_status_dot_desc());

		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_hide_all_chats_folder(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.hideAllChatsFolderValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.hideAllChatsFolder());
			}) | rpl::on_next([=, &settings](bool enabled) {
				settings.setHideAllChatsFolder(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/chats/hide-all-chats-folder"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_hide_all_chats_folder_desc());

		const auto hideBlockedBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_hide_blocked_user_messages(),
			st::settingsButtonNoIcon
		));
		hideBlockedBtn->setColorOverride(QColor(255, 0, 0));
		hideBlockedBtn->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().hideBlockedUserMessages())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().hideBlockedUserMessages());
		}) | rpl::on_next([=](bool enabled) {
			FASettings::FASettings::getInstance().setHideBlockedUserMessages(enabled);
			

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
