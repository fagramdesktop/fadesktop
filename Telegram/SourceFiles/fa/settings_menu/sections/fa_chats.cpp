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
#include "fa/ui/md3/fa_cards.h"

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
		auto &settings = FASettings::FASettings::getInstance();

		FA::Ui::AddModernSectionHeader(container, fatr::fa_chats());
		const auto msgCard = FA::Ui::CreateCardContainer(container);

		const auto mdRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_parse_markdown_hyperlink(),
			fatr::fa_parse_markdown_hyperlink_desc(),
			settings.autoFormatMarkdownValue(),
			[&settings](bool enabled) {
				settings.setAutoFormatMarkdown(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			mdRow, u"fa/chats/markdown-hyperlink"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto secondsRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_show_seconds_message(),
			fatr::fa_show_seconds_message_desc(),
			settings.secondsMessageValue(),
			[&settings](bool enabled) {
				settings.setSecondsMessage(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			secondsRow, u"fa/chats/seconds-message"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto fwdDateRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_show_forwarded_date_in_title(),
			fatr::fa_show_forwarded_date_in_title_desc(),
			settings.showForwardedDateInTitleValue(),
			[&settings](bool enabled) {
				settings.setShowForwardedDateInTitle(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			fwdDateRow, u"fa/chats/show-forwarded-date-in-title"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto fwdCountRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_show_forwards_count(),
			fatr::fa_show_forwards_count_desc(),
			settings.showForwardsCountValue(),
			[&settings](bool enabled) {
				settings.setShowForwardsCount(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			fwdCountRow, u"fa/chats/show-forwards-count"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto commaRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_add_comma_after_mention(),
			fatr::fa_add_comma_after_mention_desc(),
			settings.addCommaAfterMentionValue(),
			[&settings](bool enabled) {
				settings.setAddCommaAfterMention(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			commaRow, u"fa/chats/add-comma-after-mention"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto linkPreviewRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_disable_link_preview(),
			fatr::fa_disable_link_preview_desc(),
			settings.disableLinkPreviewValue(),
			[&settings](bool enabled) {
				settings.setDisableLinkPreview(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			linkPreviewRow, u"fa/chats/disable-link-preview"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto delEveryoneRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_delete_for_everyone(),
			fatr::fa_delete_for_everyone_desc(),
			settings.deleteForEveryoneValue(),
			[&settings](bool enabled) {
				settings.setDeleteForEveryone(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			delEveryoneRow, u"fa/chats/delete-for-everyone"_q, controller);

		FA::Ui::AddCardDivider(msgCard);

		const auto lastSeenRow = FA::Ui::AddCardToggle(
			msgCard,
			fatr::fa_last_seen_timestamp(),
			fatr::fa_last_seen_timestamp_desc(),
			settings.lastSeenTimestampValue(),
			[&settings](bool enabled) {
				settings.setLastSeenTimestamp(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			lastSeenRow, u"fa/chats/last-seen-timestamp"_q, controller);

		FA::Ui::AddModernSectionHeader(container, fatr::fa_chat_list_and_folders());
		const auto chatListCard = FA::Ui::CreateCardContainer(container);

		const auto hideAllFolderRow = FA::Ui::AddCardToggle(
			chatListCard,
			fatr::fa_hide_all_chats_folder(),
			fatr::fa_hide_all_chats_folder_desc(),
			settings.hideAllChatsFolderValue(),
			[=, &settings](bool enabled) {
				settings.setHideAllChatsFolder(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			hideAllFolderRow, u"fa/chats/hide-all-chats-folder"_q, controller);

		FA::Ui::AddCardDivider(chatListCard);

		const auto webappBtnRow = FA::Ui::AddCardToggle(
			chatListCard,
			fatr::fa_hide_open_webapp_button_chatlist(),
			fatr::fa_hide_open_webapp_button_chatlist_desc(),
			settings.hideOpenWebappButtonChatlistValue(),
			[&settings](bool enabled) {
				settings.setHideOpenWebappButtonChatlist(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			webappBtnRow, u"fa/chats/hide-webapp-button"_q, controller);

		FA::Ui::AddCardDivider(chatListCard);

		const auto discussBtnRow = FA::Ui::AddCardToggle(
			chatListCard,
			fatr::fa_show_discuss_button(),
			fatr::fa_show_discuss_button_desc(),
			settings.showDiscussButtonValue(),
			[&settings](bool enabled) {
				settings.setShowDiscussButton(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			discussBtnRow, u"fa/chats/show-discuss-button"_q, controller);

		FA::Ui::AddCardDivider(chatListCard);

		const auto fastshareRow = FA::Ui::AddCardToggle(
			chatListCard,
			fatr::fa_show_fastshare_in_chats(),
			fatr::fa_show_fastshare_in_chats_desc(),
			settings.showFastshareInChatsValue(),
			[&settings](bool enabled) {
				settings.setShowFastshareInChats(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			fastshareRow, u"fa/chats/show-share-in-chats"_q, controller);

		FA::Ui::AddCardDivider(chatListCard);

		const auto msgDetailsRow = FA::Ui::AddCardToggle(
			chatListCard,
			fatr::fa_show_message_details(),
			fatr::fa_show_message_details_desc(),
			settings.showMessageDetailsValue(),
			[&settings](bool enabled) {
				settings.setShowMessageDetails(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			msgDetailsRow, u"fa/chats/message-details"_q, controller);

		FA::Ui::AddModernSectionHeader(container, fatr::fa_media_and_appearance());
		const auto mediaCard = FA::Ui::CreateCardContainer(container);

		const auto customBgRow = FA::Ui::AddCardToggle(
			mediaCard,
			fatr::fa_disable_custom_chat_background(),
			fatr::fa_disable_custom_chat_background_desc(),
			settings.disableCustomChatBackgroundValue(),
			[&settings](bool enabled) {
				settings.setDisableCustomChatBackground(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			customBgRow, u"fa/chats/disable-custom-background"_q, controller);

		FA::Ui::AddCardDivider(mediaCard);

		const auto greetingStickerRow = FA::Ui::AddCardToggle(
			mediaCard,
			fatr::fa_disable_greeting_sticker(),
			fatr::fa_disable_greeting_sticker_desc(),
			settings.disableGreetingStickerValue(),
			[&settings](bool enabled) {
				settings.setDisableGreetingSticker(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			greetingStickerRow, u"fa/chats/disable-greeting-sticker"_q, controller);

		FA::Ui::AddCardDivider(mediaCard);

		const auto statusDotRow = FA::Ui::AddCardToggle(
			mediaCard,
			fatr::fa_show_status_dot(),
			fatr::fa_status_dot_desc(),
			rpl::single(settings.showStatusDot()),
			[&settings](bool enabled) {
				settings.setShowStatusDot(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			statusDotRow, u"fa/chats/status-dot"_q, controller);

		FA::Ui::AddCardDivider(mediaCard);

		const auto statusDotOnlineRow = FA::Ui::AddCardToggle(
			mediaCard,
			fatr::fa_status_dot_online_only(),
			nullptr,
			rpl::single(settings.statusDotOnlineOnly()),
			[&settings](bool enabled) {
				settings.setStatusDotOnlineOnly(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			statusDotOnlineRow, u"fa/chats/status-dot-online"_q, controller);

		FA::Ui::AddCardDivider(mediaCard);

		const auto blockedMsgRow = FA::Ui::AddCardToggle(
			mediaCard,
			fatr::fa_hide_blocked_user_messages(),
			fatr::fa_hide_blocked_user_messages_desc(),
			rpl::single(settings.hideBlockedUserMessages()),
			[=, &settings](bool enabled) {
				settings.setHideBlockedUserMessages(enabled);
				controller->showToast(fatr::fa_restarting_in_seconds(fatr::now));
				base::call_delayed(crl::time(3000), container, [] {
					::Core::Restart();
				});
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			blockedMsgRow, u"fa/chats/hide-blocked-messages"_q, controller);
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
