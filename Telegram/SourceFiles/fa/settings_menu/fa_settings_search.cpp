/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/settings_menu/fa_settings_search.h"

#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/sections/fa_chats.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/sections/fa_logs.h"

#include "fa/lang/fa_lang.h"

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
	.customTitle = [] { return FAlang::Translate(QString("fa_client_preferences")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/preferences"_q,
			.title = FAlang::Translate(QString("fa_client_preferences")),
			.keywords = { u"fagram"_q, u"fa"_q, u"preferences"_q, u"settings"_q, u"custom"_q, u"mod"_q },
			.icon = { &st::menuIconSettings },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general"_q,
			.title = FAlang::Translate(QString("fa_general")),
			.keywords = { u"fagram"_q, u"general"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats"_q,
			.title = FAlang::Translate(QString("fa_chats")),
			.keywords = { u"fagram"_q, u"chats"_q, u"messages"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu"_q,
			.title = FAlang::Translate(QString("fa_context_menu")),
			.keywords = { u"fagram"_q, u"context"_q, u"menu"_q, u"shortcuts"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance"_q,
			.title = FAlang::Translate(QString("fa_appearance")),
			.keywords = { u"fagram"_q, u"appearance"_q, u"theme"_q, u"look"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs"_q,
			.title = FAlang::Translate(QString("fa_debug_logs")),
			.keywords = { u"fagram"_q, u"debug"_q, u"logs"_q },
			.icon = { &st::menuIconFile },
		};
	});
});

// FA General
const auto kFAGeneralMeta = BuildHelper({
	.id = FAGeneral::Id(),
	.parentId = FA::Id(),
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconShowAll,
	.customTitle = [] { return FAlang::Translate(QString("fa_general")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/whats-new-chat"_q,
			.title = FAlang::Translate(QString("fa_enable_whats_new_chat")),
			.keywords = { u"whats new"_q, u"chat"_q, u"updates"_q, u"changelog"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/disable-ads"_q,
			.title = FAlang::Translate(QString("fa_disable_ads")),
			.keywords = { u"ads"_q, u"advertising"_q, u"sponsored"_q, u"disable"_q, u"block"_q, u"remove"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/start-token"_q,
			.title = FAlang::Translate(QString("fa_show_start_token")),
			.keywords = { u"start"_q, u"token"_q, u"bot"_q, u"parameter"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/peer-ids"_q,
			.title = FAlang::Translate(QString("fa_show_peer_ids")),
			.keywords = { u"peer"_q, u"id"_q, u"user"_q, u"chat"_q, u"identifier"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/dc-ids"_q,
			.title = FAlang::Translate(QString("fa_show_dc_ids")),
			.keywords = { u"dc"_q, u"datacenter"_q, u"data center"_q, u"server"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/botapi-id"_q,
			.title = FAlang::Translate(QString("fa_id_in_botapi_type")),
			.keywords = { u"bot"_q, u"api"_q, u"id"_q, u"type"_q, u"format"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/local-premium"_q,
			.title = FAlang::Translate(QString("fa_local_tg_premium")),
			.keywords = { u"premium"_q, u"local"_q, u"fake"_q, u"free"_q },
			.icon = { &st::menuIconShowAll },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/general/registration-date"_q,
			.title = FAlang::Translate(QString("fa_show_registration_date")),
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
	.customTitle = [] { return FAlang::Translate(QString("fa_chats")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/recent-stickers"_q,
			.title = FAlang::Translate(QString("fa_recent_stickers")),
			.keywords = { u"stickers"_q, u"recent"_q, u"limit"_q, u"hide"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/markdown-hyperlink"_q,
			.title = FAlang::Translate(QString("fa_parse_markdown_hyperlink")),
			.keywords = { u"markdown"_q, u"hyperlink"_q, u"link"_q, u"format"_q, u"parse"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/seconds-message"_q,
			.title = FAlang::Translate(QString("fa_show_seconds_message")),
			.keywords = { u"seconds"_q, u"time"_q, u"message"_q, u"timestamp"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/disable-custom-background"_q,
			.title = FAlang::Translate(QString("fa_disable_custom_chat_background")),
			.keywords = { u"background"_q, u"wallpaper"_q, u"custom"_q, u"disable"_q, u"chat"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-webapp-button"_q,
			.title = FAlang::Translate(QString("fa_hide_open_webapp_button_chatlist")),
			.keywords = { u"webapp"_q, u"button"_q, u"hide"_q, u"chatlist"_q, u"mini app"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/show-discuss-button"_q,
			.title = FAlang::Translate(QString("fa_show_discuss_button")),
			.keywords = { u"discuss"_q, u"button"_q, u"comments"_q, u"channel"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/message-details"_q,
			.title = FAlang::Translate(QString("fa_show_message_details")),
			.keywords = { u"message"_q, u"details"_q, u"info"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/status-dot"_q,
			.title = FAlang::Translate(QString("fa_show_status_dot")),
			.keywords = { u"status"_q, u"dot"_q, u"online"_q, u"indicator"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/status-dot-online"_q,
			.title = FAlang::Translate(QString("fa_status_dot_online_only")),
			.keywords = { u"status"_q, u"dot"_q, u"online"_q, u"only"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-all-chats-folder"_q,
			.title = FAlang::Translate(QString("fa_hide_all_chats_folder")),
			.keywords = { u"all chats"_q, u"folder"_q, u"hide"_q, u"filter"_q },
			.icon = { &st::menuIconChatBubble },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/chats/hide-blocked-messages"_q,
			.title = FAlang::Translate(QString("fa_hide_blocked_user_messages")),
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
	.customTitle = [] { return FAlang::Translate(QString("fa_appearance")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/roundness"_q,
			.title = FAlang::Translate(QString("fa_rounding")),
			.keywords = { u"roundness"_q, u"rounding"_q, u"corners"_q, u"radius"_q, u"avatar"_q, u"userpic"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/default-rounding"_q,
			.title = FAlang::Translate(QString("fa_use_default_rounding")),
			.keywords = { u"default"_q, u"rounding"_q, u"corners"_q, u"radius"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/screenshot-mode"_q,
			.title = FAlang::Translate(QString("fa_screenshot_mode")),
			.keywords = { u"screenshot"_q, u"mode"_q, u"privacy"_q, u"blur"_q, u"hide"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/force-snow"_q,
			.title = FAlang::Translate(QString("fa_force_snow")),
			.keywords = { u"snow"_q, u"winter"_q, u"effect"_q, u"christmas"_q, u"new year"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/hide-phone"_q,
			.title = FAlang::Translate(QString("fa_hide_phone_number")),
			.keywords = { u"phone"_q, u"number"_q, u"hide"_q, u"privacy"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/hide-stories"_q,
			.title = FAlang::Translate(QString("fa_hide_stories")),
			.keywords = { u"stories"_q, u"hide"_q, u"disable"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/hide-folder-titles"_q,
			.title = FAlang::Translate(QString("fa_hide_folder_tabs_titles")),
			.keywords = { u"folder"_q, u"tabs"_q, u"titles"_q, u"hide"_q, u"filters"_q },
			.icon = { &st::menuIconPalette },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/appearance/use-tdesktop-themes"_q,
			.title = FAlang::Translate(QString("fa_use_tdesktop_themes")),
			.keywords = { u"tdesktop"_q, u"themes"_q, u"theme"_q, u"default"_q, },
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
	.customTitle = [] { return FAlang::Translate(QString("fa_context_menu")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/shortcuts"_q,
			.title = FAlang::Translate(QString("fa_context_menu_settings")),
			.keywords = { u"context"_q, u"menu"_q, u"shortcuts"_q, u"buttons"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/shortcuts-bottom"_q,
			.title = FAlang::Translate(QString("fa_context_menu_move_to_bottom")),
			.keywords = { u"shortcuts"_q, u"bottom"_q, u"move"_q, u"position"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/button-size"_q,
			.title = FAlang::Translate(QString("fa_shortcut_button_size")),
			.keywords = { u"shortcut"_q, u"button"_q, u"size"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/icon-size"_q,
			.title = FAlang::Translate(QString("fa_shortcut_icon_size")),
			.keywords = { u"shortcut"_q, u"icon"_q, u"size"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/spacing"_q,
			.title = FAlang::Translate(QString("fa_shortcut_spacing")),
			.keywords = { u"shortcut"_q, u"spacing"_q, u"gap"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/h-padding"_q,
			.title = FAlang::Translate(QString("fa_shortcut_horizontal_padding")),
			.keywords = { u"shortcut"_q, u"padding"_q, u"horizontal"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/v-padding"_q,
			.title = FAlang::Translate(QString("fa_shortcut_vertical_padding")),
			.keywords = { u"shortcut"_q, u"padding"_q, u"vertical"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/corner-radius"_q,
			.title = FAlang::Translate(QString("fa_shortcut_corner_radius")),
			.keywords = { u"shortcut"_q, u"corner"_q, u"radius"_q, u"rounded"_q, u"customize"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/reply-private"_q,
			.title = FAlang::Translate(QString("fa_context_menu_reply_in_private")),
			.keywords = { u"reply"_q, u"private"_q, u"dm"_q, u"direct"_q },
			.icon = { &st::menuIconSigned },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/context-menu/forward-submenu"_q,
			.title = FAlang::Translate(QString("fa_context_menu_forward_submenu")),
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
	.customTitle = [] { return FAlang::Translate(QString("fa_debug_logs")); },
}, [](SectionBuilder &builder) {
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs/clean"_q,
			.title = FAlang::Translate(QString("fa_clean_debug_logs")),
			.keywords = { u"clean"_q, u"clear"_q, u"debug"_q, u"logs"_q, u"delete"_q },
			.icon = { &st::menuIconFile },
		};
	});

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"fa/logs/enable"_q,
			.title = FAlang::Translate(QString("fa_debug_logs")),
			.keywords = { u"debug"_q, u"logs"_q, u"enable"_q, u"logging"_q },
			.icon = { &st::menuIconFile },
		};
	});
});

} // namespace
} // namespace Settings
