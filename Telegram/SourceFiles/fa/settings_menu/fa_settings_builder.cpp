/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/settings_menu/fa_settings_builder.h"

#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/sections/fa_chats.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/sections/fa_logs.h"
#include "fa/deep_links/fa_deep_links.h"

#include "fa_lang_auto.h"

#include "lang/lang_keys.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"

#include "styles/style_menu_icons.h"

namespace Settings {
namespace {

using namespace Builder;

const auto kFAMeta = BuildHelper({
	.id = FA::Id(),
	.parentId = MainId(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconSettings,
	.customTitle = [] { return fatr::fa_client_preferences(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/preferences"_q,
			.title = fatr::fa_client_preferences(fatr::now),
			.keywords = { u"fagram"_q, u"fa"_q, u"preferences"_q, u"settings"_q, u"custom"_q, u"mod"_q },
			.icon = { &st::menuIconSettings },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general"_q,
			.title = fatr::fa_general(fatr::now),
			.keywords = { u"fagram"_q, u"general"_q },
			.icon = { &st::menuIconShowAll },
			.deeplink = Core::DeepLinks::FASettingsDeepLink(u"fa/general"_q),
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats"_q,
			.title = fatr::fa_chats(fatr::now),
			.keywords = { u"fagram"_q, u"chats"_q, u"messages"_q },
			.icon = { &st::menuIconChatBubble },
			.deeplink = Core::DeepLinks::FASettingsDeepLink(u"fa/chats"_q),
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu"_q,
			.title = fatr::fa_context_menu(fatr::now),
			.keywords = { u"fagram"_q, u"context"_q, u"menu"_q, u"shortcuts"_q },
			.icon = { &st::menuIconSigned },
			.deeplink = Core::DeepLinks::FASettingsDeepLink(u"fa/context-menu"_q),
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance"_q,
			.title = fatr::fa_appearance(fatr::now),
			.keywords = { u"fagram"_q, u"appearance"_q, u"theme"_q, u"look"_q },
			.icon = { &st::menuIconPalette },
			.deeplink = Core::DeepLinks::FASettingsDeepLink(u"fa/appearance"_q),
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs"_q,
			.title = fatr::fa_debug_logs(fatr::now),
			.keywords = { u"fagram"_q, u"debug"_q, u"logs"_q },
			.icon = { &st::menuIconFile },
			.deeplink = Core::DeepLinks::FASettingsDeepLink(u"fa/logs"_q),
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/share-settings"_q,
			.title = fatr::fa_share_settings_to_chat(fatr::now),
			.keywords = { u"fagram"_q, u"share"_q, u"settings"_q, u"export"_q, u"send"_q, u"chat"_q },
			.icon = { &st::menuIconShare },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/export-settings"_q,
			.title = fatr::fa_share_settings(fatr::now),
			.keywords = { u"fagram"_q, u"export"_q, u"settings"_q, u"save"_q, u"file"_q, u"faconfig"_q },
			.icon = { &st::menuIconExport },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/restart"_q,
			.title = fatr::fa_restart(fatr::now),
			.keywords = { u"fagram"_q, u"restart"_q, u"reboot"_q, u"reload"_q },
			.icon = { &st::menuIconRestore },
		};
	});
});

// FA General
const auto kFAGeneralMeta = BuildHelper({
	.id = FAGeneral::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconShowAll,
	.customTitle = [] { return fatr::fa_general(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/whats-new-chat"_q,
			.title = fatr::fa_enable_whats_new_chat(fatr::now),
			.keywords = { u"whats new"_q, u"chat"_q, u"updates"_q, u"changelog"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/disable-ads"_q,
			.title = fatr::fa_disable_ads(fatr::now),
			.keywords = { u"ads"_q, u"advertising"_q, u"sponsored"_q, u"disable"_q, u"block"_q, u"remove"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/disable-ai-text-editor"_q,
			.title = fatr::fa_disable_ai_text_editor(fatr::now),
			.keywords = { u"ai"_q, u"compose"_q, u"text"_q, u"editor"_q, u"disable"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/start-token"_q,
			.title = fatr::fa_show_start_token(fatr::now),
			.keywords = { u"start"_q, u"token"_q, u"bot"_q, u"parameter"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/peer-ids"_q,
			.title = fatr::fa_show_peer_ids(fatr::now),
			.keywords = { u"peer"_q, u"id"_q, u"user"_q, u"chat"_q, u"identifier"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/dc-ids"_q,
			.title = fatr::fa_show_dc_ids(fatr::now),
			.keywords = { u"dc"_q, u"datacenter"_q, u"data center"_q, u"server"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/botapi-id"_q,
			.title = fatr::fa_id_in_botapi_type(fatr::now),
			.keywords = { u"bot"_q, u"api"_q, u"id"_q, u"type"_q, u"format"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/local-premium"_q,
			.title = fatr::fa_local_tg_premium(fatr::now),
			.keywords = { u"premium"_q, u"local"_q, u"fake"_q, u"free"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/registration-date"_q,
			.title = fatr::fa_show_registration_date(fatr::now),
			.keywords = { u"registration"_q, u"date"_q, u"account"_q, u"created"_q, u"joined"_q },
			.icon = { &st::menuIconShowAll },
		};
	});
});
// FA Chats
const auto kFAChatsMeta = BuildHelper({
	.id = FAChats::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconChatBubble,
	.customTitle = [] { return fatr::fa_chats(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/markdown-hyperlink"_q,
			.title = fatr::fa_parse_markdown_hyperlink(fatr::now),
			.keywords = { u"markdown"_q, u"hyperlink"_q, u"link"_q, u"format"_q, u"parse"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/seconds-message"_q,
			.title = fatr::fa_show_seconds_message(fatr::now),
			.keywords = { u"seconds"_q, u"time"_q, u"message"_q, u"timestamp"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/disable-custom-background"_q,
			.title = fatr::fa_disable_custom_chat_background(fatr::now),
			.keywords = { u"background"_q, u"wallpaper"_q, u"custom"_q, u"disable"_q, u"chat"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-webapp-button"_q,
			.title = fatr::fa_hide_open_webapp_button_chatlist(fatr::now),
			.keywords = { u"webapp"_q, u"button"_q, u"hide"_q, u"chatlist"_q, u"mini app"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/show-discuss-button"_q,
			.title = fatr::fa_show_discuss_button(fatr::now),
			.keywords = { u"discuss"_q, u"button"_q, u"comments"_q, u"channel"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/show-share-in-chats"_q,
			.title = fatr::fa_show_fastshare_in_chats(fatr::now),
			.keywords = { u"share"_q, u"forward"_q, u"button"_q, u"chats"_q, u"groups"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/message-details"_q,
			.title = fatr::fa_show_message_details(fatr::now),
			.keywords = { u"message"_q, u"details"_q, u"info"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/add-comma-after-mention"_q,
			.title = fatr::fa_add_comma_after_mention(fatr::now),
			.keywords = { u"mention"_q, u"comma"_q, u"username"_q, u"typing"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/unlimited-pinned-chats"_q,
			.title = fatr::fa_unlimited_pinned_chats(fatr::now),
			.keywords = { u"pinned"_q, u"pin"_q, u"chats"_q, u"limit"_q, u"unlimited"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/status-dot"_q,
			.title = fatr::fa_show_status_dot(fatr::now),
			.keywords = { u"status"_q, u"dot"_q, u"online"_q, u"indicator"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/status-dot-online"_q,
			.title = fatr::fa_status_dot_online_only(fatr::now),
			.keywords = { u"status"_q, u"dot"_q, u"online"_q, u"only"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-all-chats-folder"_q,
			.title = fatr::fa_hide_all_chats_folder(fatr::now),
			.keywords = { u"all chats"_q, u"folder"_q, u"hide"_q, u"filter"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-blocked-messages"_q,
			.title = fatr::fa_hide_blocked_user_messages(fatr::now),
			.keywords = { u"blocked"_q, u"user"_q, u"messages"_q, u"hide"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});
});

// FA Appearance
const auto kFAAppearanceMeta = BuildHelper({
	.id = FAAppearance::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconPalette,
	.customTitle = [] { return fatr::fa_appearance(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/roundness"_q,
			.title = fatr::fa_rounding(fatr::now),
			.keywords = { u"roundness"_q, u"rounding"_q, u"corners"_q, u"radius"_q, u"avatar"_q, u"userpic"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/default-rounding"_q,
			.title = fatr::fa_use_default_rounding(fatr::now),
			.keywords = { u"default"_q, u"rounding"_q, u"corners"_q, u"radius"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/screenshot-mode"_q,
			.title = fatr::fa_screenshot_mode(fatr::now),
			.keywords = { u"screenshot"_q, u"mode"_q, u"privacy"_q, u"blur"_q, u"hide"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/force-snow"_q,
			.title = fatr::fa_force_snow(fatr::now),
			.keywords = { u"snow"_q, u"winter"_q, u"effect"_q, u"christmas"_q, u"new year"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/hide-stories"_q,
			.title = fatr::fa_hide_stories(fatr::now),
			.keywords = { u"stories"_q, u"hide"_q, u"disable"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/hide-folder-titles"_q,
			.title = fatr::fa_hide_folder_tabs_titles(fatr::now),
			.keywords = { u"folder"_q, u"tabs"_q, u"titles"_q, u"hide"_q, u"filters"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/use-tdesktop-themes"_q,
			.title = fatr::fa_use_tdesktop_themes(fatr::now),
			.keywords = { u"tdesktop"_q, u"themes"_q, u"theme"_q, u"default"_q, },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/material-icons"_q,
			.title = fatr::fa_use_material_icon_pack(fatr::now),
			.keywords = { u"icon"_q, u"pack"_q, u"material"_q, u"icons"_q },
			.icon = { &st::menuIconPalette },
		};
	});
});
// FA Context Menu
const auto kFAContextMenuMeta = BuildHelper({
	.id = FAContextMenu::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconSigned,
	.customTitle = [] { return fatr::fa_context_menu(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/shortcuts"_q,
			.title = fatr::fa_context_menu_settings(fatr::now),
			.keywords = { u"context"_q, u"menu"_q, u"shortcuts"_q, u"buttons"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/shortcuts-bottom"_q,
			.title = fatr::fa_context_menu_move_to_bottom(fatr::now),
			.keywords = { u"shortcuts"_q, u"bottom"_q, u"move"_q, u"position"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/button-size"_q,
			.title = fatr::fa_shortcut_button_size(fatr::now),
			.keywords = { u"shortcut"_q, u"button"_q, u"size"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/icon-size"_q,
			.title = fatr::fa_shortcut_icon_size(fatr::now),
			.keywords = { u"shortcut"_q, u"icon"_q, u"size"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/spacing"_q,
			.title = fatr::fa_shortcut_spacing(fatr::now),
			.keywords = { u"shortcut"_q, u"spacing"_q, u"gap"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/h-padding"_q,
			.title = fatr::fa_shortcut_horizontal_padding(fatr::now),
			.keywords = { u"shortcut"_q, u"padding"_q, u"horizontal"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/v-padding"_q,
			.title = fatr::fa_shortcut_vertical_padding(fatr::now),
			.keywords = { u"shortcut"_q, u"padding"_q, u"vertical"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/corner-radius"_q,
			.title = fatr::fa_shortcut_corner_radius(fatr::now),
			.keywords = { u"shortcut"_q, u"corner"_q, u"radius"_q, u"rounded"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/reply-private"_q,
			.title = fatr::fa_context_menu_reply_in_private(fatr::now),
			.keywords = { u"reply"_q, u"private"_q, u"dm"_q, u"direct"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/forward-submenu"_q,
			.title = fatr::fa_context_menu_forward_submenu(fatr::now),
			.keywords = { u"forward"_q, u"submenu"_q, u"menu"_q },
			.icon = { &st::menuIconSigned },
		};
	});
});

// FA Logs
const auto kFALogsMeta = BuildHelper({
	.id = FALogs::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconFile,
	.customTitle = [] { return fatr::fa_debug_logs(fatr::now); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs/clean"_q,
			.title = fatr::fa_clean_debug_logs(fatr::now),
			.keywords = { u"clean"_q, u"clear"_q, u"debug"_q, u"logs"_q, u"delete"_q },
			.icon = { &st::menuIconFile },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs/debug-logs"_q,
			.title = fatr::fa_debug_logs(fatr::now),
			.keywords = { u"debug"_q, u"logs"_q, u"enable"_q, u"logging"_q },
			.icon = { &st::menuIconFile },
		};
	});
});

} // namespace
} // namespace Settings
