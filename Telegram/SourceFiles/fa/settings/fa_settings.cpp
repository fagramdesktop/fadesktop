/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/settings/fa_settings.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <algorithm>
#include "core/application.h"
#include "core/file_utilities.h"

namespace FASettings {

namespace {

QString getSettingsPath() {
	return cWorkingDir() + qsl("tdata/fa_settings.json");
}

QString getOldCustomSettingsPath() {
	return cWorkingDir() + qsl("tdata/FA-settings-custom.json");
}

} // namespace

FASettings::FASettings() = default;

FASettings &FASettings::getInstance() {
	static FASettings instance;
	return instance;
}

void FASettings::load() {
	auto &settings = getInstance();
	QFile file(getSettingsPath());
	if (file.open(QIODevice::ReadOnly)) {
		const auto data = file.readAll();
		const auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			settings.loadFromJson(doc.object());
		}
		file.close();
	} else {
		// Try to migrate from old FA-settings-custom.json
		QFile oldFile(getOldCustomSettingsPath());
		if (oldFile.open(QIODevice::ReadOnly)) {
			const auto data = oldFile.readAll();
			const auto doc = QJsonDocument::fromJson(data);
			if (doc.isObject()) {
				// Old format was direct key-value at the root
				settings.loadFromJson(doc.object());
				settings.save(); // Save to new file
			}
			oldFile.close();
		}
	}
	settings.validate();
}

void FASettings::save() {
	auto &settings = getInstance();
	QFile file(getSettingsPath());
	if (file.open(QIODevice::WriteOnly)) {
		const auto doc = QJsonDocument(settings.saveToJson());
		file.write(doc.toJson(QJsonDocument::Indented));
		file.close();
	}
}

void FASettings::validate() {
	bool modified = false;

	auto validateRange = [&](auto &var, int min, int max, int defaultVar) {
		if (var.current() < min || var.current() > max) {
			var = defaultVar;
			modified = true;
		}
	};

	validateRange(_roundness, 0, 50, 50);
	validateRange(_contextMenuShortcutButtonSize, 24, 64, 40);
	validateRange(_contextMenuShortcutIconSize, 16, 48, 24);
	validateRange(_contextMenuShortcutSpacing, 0, 24, 10);
	validateRange(_contextMenuShortcutVerticalPadding, 0, 16, 2);
	validateRange(_contextMenuShortcutHorizontalPadding, 0, 16, 10);
	validateRange(_contextMenuShortcutCornerRadius, 0, 20, 20);
	validateRange(_translationProvider, 0, 3, static_cast<int>(TranslationProvider::Telegram));

	if (modified) {
		save();
	}
}

void FASettings::loadFromJson(const QJsonObject &obj) {
	_debugLogs = obj.contains("debug_logs") ? obj["debug_logs"].toBool() : _debugLogs.current();
	_secondsMessage = obj.contains("seconds_message") ? obj["seconds_message"].toBool() : _secondsMessage.current();
	_disableAds = obj.contains("disable_ads") ? obj["disable_ads"].toBool() : _disableAds.current();
	_disableAi = obj.contains("disable_ai") ? obj["disable_ai"].toBool() : _disableAi.current();
	_disableAnimatedAvatars = obj.contains("disable_animated_avatars") ? obj["disable_animated_avatars"].toBool() : _disableAnimatedAvatars.current();
	_disableAutoDownload = obj.contains("disable_auto_download") ? obj["disable_auto_download"].toBool() : _disableAutoDownload.current();
	_showStartToken = obj.contains("show_start_token") ? obj["show_start_token"].toBool() : _showStartToken.current();
	_showPeerId = obj.contains("show_peer_id") ? obj["show_peer_id"].toBool() : _showPeerId.current();
	_showDcId = obj.contains("show_dc_id") ? obj["show_dc_id"].toBool() : _showDcId.current();
	_showIdBotapi = obj.contains("show_id_botapi") ? obj["show_id_botapi"].toBool() : _showIdBotapi.current();
	_showRegistrationDate = obj.contains("show_registration_date") ? obj["show_registration_date"].toBool() : _showRegistrationDate.current();
	_disableCustomChatBackground = obj.contains("disable_custom_chat_background") ? obj["disable_custom_chat_background"].toBool() : _disableCustomChatBackground.current();
	_hideAllChatsFolder = obj.contains("hide_all_chats_folder") ? obj["hide_all_chats_folder"].toBool() : _hideAllChatsFolder.current();
	_hideArchiveChats = obj.contains("hide_archive_chats") ? obj["hide_archive_chats"].toBool() : _hideArchiveChats.current();
	_hideStories = obj.contains("hide_stories") ? obj["hide_stories"].toBool() : _hideStories.current();
	_hideOpenWebappButtonChatlist = obj.contains("hide_open_webapp_button_chatlist") ? obj["hide_open_webapp_button_chatlist"].toBool() : _hideOpenWebappButtonChatlist.current();
	_localPremium = obj.contains("local_premium") ? obj["local_premium"].toBool() : _localPremium.current();
	_deleteForEveryone = obj.contains("delete_for_everyone") ? obj["delete_for_everyone"].toBool() : _deleteForEveryone.current();
	_lastSeenTimestamp = obj.contains("last_seen_timestamp") ? obj["last_seen_timestamp"].toBool() : _lastSeenTimestamp.current();
	_showForwardedDateInTitle = obj.contains("show_forwarded_date_in_title") ? obj["show_forwarded_date_in_title"].toBool() : _showForwardedDateInTitle.current();
	_showForwardsCount = obj.contains("show_forwards_count") ? obj["show_forwards_count"].toBool() : _showForwardsCount.current();
	_disableGreetingSticker = obj.contains("disable_greeting_sticker") ? obj["disable_greeting_sticker"].toBool() : _disableGreetingSticker.current();
	_useDefaultRounding = obj.contains("use_default_rounding") ? obj["use_default_rounding"].toBool() : _useDefaultRounding.current();
	_showDiscussButton = obj.contains("show_discuss_button") ? obj["show_discuss_button"].toBool() : _showDiscussButton.current();
	_showFastshareInChats = obj.contains("show_fastshare_in_chats") ? obj["show_fastshare_in_chats"].toBool() : _showFastshareInChats.current();
	_roundness = obj.contains("roundness") ? obj["roundness"].toInt() : _roundness.current();
	_forceSnow = obj.contains("force_snow") ? obj["force_snow"].toBool() : _forceSnow.current();
	_showMessageDetails = obj.contains("show_message_details") ? obj["show_message_details"].toBool() : _showMessageDetails.current();
	_hideBlockedUserMessages = obj.contains("hide_blocked_user_messages") ? obj["hide_blocked_user_messages"].toBool() : _hideBlockedUserMessages.current();
	_showStatusDot = obj.contains("show_status_dot") ? obj["show_status_dot"].toBool() : _showStatusDot.current();
	_statusDotOnlineOnly = obj.contains("status_dot_online_only") ? obj["status_dot_online_only"].toBool() : _statusDotOnlineOnly.current();
	_contextMenuUseShortcuts = obj.contains("context_menu_use_shortcuts") ? obj["context_menu_use_shortcuts"].toBool() : _contextMenuUseShortcuts.current();
	_contextMenuShortcutsAtBottom = obj.contains("context_menu_shortcuts_at_bottom") ? obj["context_menu_shortcuts_at_bottom"].toBool() : _contextMenuShortcutsAtBottom.current();
	_contextMenuReplyInPrivate = obj.contains("context_menu_reply_in_private") ? obj["context_menu_reply_in_private"].toBool() : _contextMenuReplyInPrivate.current();
	_contextMenuForwardSubmenu = obj.contains("context_menu_forward_submenu") ? obj["context_menu_forward_submenu"].toBool() : _contextMenuForwardSubmenu.current();
	_useTdesktopThemes = obj.contains("use_tdesktop_themes") ? obj["use_tdesktop_themes"].toBool() : _useTdesktopThemes.current();
	_useMaterialIconPack = obj.contains("use_material_icon_pack") ? obj["use_material_icon_pack"].toBool() : _useMaterialIconPack.current();
	_shareMenuFolderIcons = obj.contains("share_menu_folder_icons") ? obj["share_menu_folder_icons"].toBool() : _shareMenuFolderIcons.current();
	_avatarShape = obj.contains("avatar_shape") ? obj["avatar_shape"].toInt() : _avatarShape.current();
	_disablePremiumAnimation = obj.contains("disable_premium_animation") ? obj["disable_premium_animation"].toBool() : _disablePremiumAnimation.current();
	_screenshotMode = obj.contains("screenshot_mode") ? obj["screenshot_mode"].toBool() : _screenshotMode.current();
	_autoFormatMarkdown = obj.contains("auto_format_markdown") ? obj["auto_format_markdown"].toBool() : _autoFormatMarkdown.current();
	_addCommaAfterMention = obj.contains("add_comma_after_mention") ? obj["add_comma_after_mention"].toBool() : _addCommaAfterMention.current();
	_disableLinkPreview = obj.contains("disable_link_preview") ? obj["disable_link_preview"].toBool() : _disableLinkPreview.current();
	_contextMenuShortcutButtonSize = obj.contains("context_menu_shortcut_button_size") ? obj["context_menu_shortcut_button_size"].toInt() : _contextMenuShortcutButtonSize.current();
	_contextMenuShortcutIconSize = obj.contains("context_menu_shortcut_icon_size") ? obj["context_menu_shortcut_icon_size"].toInt() : _contextMenuShortcutIconSize.current();
	_contextMenuShortcutSpacing = obj.contains("context_menu_shortcut_spacing") ? obj["context_menu_shortcut_spacing"].toInt() : _contextMenuShortcutSpacing.current();
	_contextMenuShortcutVerticalPadding = obj.contains("context_menu_shortcut_vertical_padding") ? obj["context_menu_shortcut_vertical_padding"].toInt() : _contextMenuShortcutVerticalPadding.current();
	_contextMenuShortcutHorizontalPadding = obj.contains("context_menu_shortcut_horizontal_padding") ? obj["context_menu_shortcut_horizontal_padding"].toInt() : _contextMenuShortcutHorizontalPadding.current();
	_contextMenuShortcutCornerRadius = obj.contains("context_menu_shortcut_corner_radius") ? obj["context_menu_shortcut_corner_radius"].toInt() : _contextMenuShortcutCornerRadius.current();
	_translationProvider = obj.contains("translation_provider") ? obj["translation_provider"].toInt() : _translationProvider.current();
}

QJsonObject FASettings::saveToJson() const {
	QJsonObject obj;
	obj["debug_logs"] = _debugLogs.current();
	obj["seconds_message"] = _secondsMessage.current();
	obj["disable_ads"] = _disableAds.current();
	obj["disable_ai"] = _disableAi.current();
	obj["disable_animated_avatars"] = _disableAnimatedAvatars.current();
	obj["disable_auto_download"] = _disableAutoDownload.current();
	obj["show_start_token"] = _showStartToken.current();
	obj["show_peer_id"] = _showPeerId.current();
	obj["show_dc_id"] = _showDcId.current();
	obj["show_id_botapi"] = _showIdBotapi.current();
	obj["show_registration_date"] = _showRegistrationDate.current();
	obj["disable_custom_chat_background"] = _disableCustomChatBackground.current();
	obj["hide_all_chats_folder"] = _hideAllChatsFolder.current();
	obj["hide_archive_chats"] = _hideArchiveChats.current();
	obj["hide_stories"] = _hideStories.current();
	obj["hide_open_webapp_button_chatlist"] = _hideOpenWebappButtonChatlist.current();
	obj["local_premium"] = _localPremium.current();
	obj["delete_for_everyone"] = _deleteForEveryone.current();
	obj["last_seen_timestamp"] = _lastSeenTimestamp.current();
	obj["show_forwarded_date_in_title"] = _showForwardedDateInTitle.current();
	obj["show_forwards_count"] = _showForwardsCount.current();
	obj["disable_greeting_sticker"] = _disableGreetingSticker.current();
	obj["use_default_rounding"] = _useDefaultRounding.current();
	obj["show_discuss_button"] = _showDiscussButton.current();
	obj["show_fastshare_in_chats"] = _showFastshareInChats.current();
	obj["roundness"] = _roundness.current();
	obj["force_snow"] = _forceSnow.current();
	obj["show_message_details"] = _showMessageDetails.current();
	obj["hide_blocked_user_messages"] = _hideBlockedUserMessages.current();
	obj["show_status_dot"] = _showStatusDot.current();
	obj["status_dot_online_only"] = _statusDotOnlineOnly.current();
	obj["context_menu_use_shortcuts"] = _contextMenuUseShortcuts.current();
	obj["context_menu_shortcuts_at_bottom"] = _contextMenuShortcutsAtBottom.current();
	obj["context_menu_reply_in_private"] = _contextMenuReplyInPrivate.current();
	obj["context_menu_forward_submenu"] = _contextMenuForwardSubmenu.current();
	obj["use_tdesktop_themes"] = _useTdesktopThemes.current();
	obj["use_material_icon_pack"] = _useMaterialIconPack.current();
	obj["share_menu_folder_icons"] = _shareMenuFolderIcons.current();
	obj["avatar_shape"] = _avatarShape.current();
	obj["disable_premium_animation"] = _disablePremiumAnimation.current();
	obj["screenshot_mode"] = _screenshotMode.current();
	obj["auto_format_markdown"] = _autoFormatMarkdown.current();
	obj["add_comma_after_mention"] = _addCommaAfterMention.current();
	obj["disable_link_preview"] = _disableLinkPreview.current();
	obj["context_menu_shortcut_button_size"] = _contextMenuShortcutButtonSize.current();
	obj["context_menu_shortcut_icon_size"] = _contextMenuShortcutIconSize.current();
	obj["context_menu_shortcut_spacing"] = _contextMenuShortcutSpacing.current();
	obj["context_menu_shortcut_vertical_padding"] = _contextMenuShortcutVerticalPadding.current();
	obj["context_menu_shortcut_horizontal_padding"] = _contextMenuShortcutHorizontalPadding.current();
	obj["context_menu_shortcut_corner_radius"] = _contextMenuShortcutCornerRadius.current();
	obj["translation_provider"] = _translationProvider.current();

	return obj;
}

QByteArray FASettings::exportSettingsJson() const {
	const auto doc = QJsonDocument(saveToJson());
	return doc.toJson(QJsonDocument::Indented);
}

bool FASettings::importSettingsFromJson(const QByteArray &json) {
	const auto doc = QJsonDocument::fromJson(json);
	if (doc.isObject()) {
		loadFromJson(doc.object());
		validate();
		save();
		return true;
	}
	return false;
}

void FASettings::setDebugLogs(bool val) {
	if (_debugLogs.current() == val) return;
	_debugLogs = val;
	save();
}

void FASettings::setSecondsMessage(bool val) {
	if (_secondsMessage.current() == val) return;
	_secondsMessage = val;
	save();
}

void FASettings::setDisableAds(bool val) {
	if (_disableAds.current() == val) return;
	_disableAds = val;
	save();
}

void FASettings::setDisableAi(bool val) {
	if (_disableAi.current() == val) return;
	_disableAi = val;
	save();
}

void FASettings::setDisableAnimatedAvatars(bool val) {
	if (_disableAnimatedAvatars.current() == val) return;
	_disableAnimatedAvatars = val;
	save();
}

void FASettings::setDisableAutoDownload(bool val) {
	if (_disableAutoDownload.current() == val) return;
	_disableAutoDownload = val;
	save();
}

void FASettings::setShowStartToken(bool val) {
	if (_showStartToken.current() == val) return;
	_showStartToken = val;
	save();
}

void FASettings::setShowPeerId(bool val) {
	if (_showPeerId.current() == val) return;
	_showPeerId = val;
	save();
}

void FASettings::setShowDcId(bool val) {
	if (_showDcId.current() == val) return;
	_showDcId = val;
	save();
}

void FASettings::setShowIdBotapi(bool val) {
	if (_showIdBotapi.current() == val) return;
	_showIdBotapi = val;
	save();
}

void FASettings::setShowRegistrationDate(bool val) {
	if (_showRegistrationDate.current() == val) return;
	_showRegistrationDate = val;
	save();
}

void FASettings::setDisableCustomChatBackground(bool val) {
	if (_disableCustomChatBackground.current() == val) return;
	_disableCustomChatBackground = val;
	save();
}

void FASettings::setHideAllChatsFolder(bool val) {
	if (_hideAllChatsFolder.current() == val) return;
	_hideAllChatsFolder = val;
	save();
}

void FASettings::setHideArchiveChats(bool val) {
	if (_hideArchiveChats.current() == val) return;
	_hideArchiveChats = val;
	save();
}

void FASettings::setHideStories(bool val) {
	if (_hideStories.current() == val) return;
	_hideStories = val;
	save();
}

void FASettings::setHideOpenWebappButtonChatlist(bool val) {
	if (_hideOpenWebappButtonChatlist.current() == val) return;
	_hideOpenWebappButtonChatlist = val;
	save();
}

void FASettings::setLocalPremium(bool val) {
	if (_localPremium.current() == val) return;
	_localPremium = val;
	save();
}

void FASettings::setDeleteForEveryone(bool val) {
	if (_deleteForEveryone.current() == val) return;
	_deleteForEveryone = val;
	save();
}

void FASettings::setLastSeenTimestamp(bool val) {
	if (_lastSeenTimestamp.current() == val) return;
	_lastSeenTimestamp = val;
	save();
}

void FASettings::setShowForwardedDateInTitle(bool val) {
	if (_showForwardedDateInTitle.current() == val) return;
	_showForwardedDateInTitle = val;
	save();
}

void FASettings::setShowForwardsCount(bool val) {
	if (_showForwardsCount.current() == val) return;
	_showForwardsCount = val;
	save();
}

void FASettings::setDisableGreetingSticker(bool val) {
	if (_disableGreetingSticker.current() == val) return;
	_disableGreetingSticker = val;
	save();
}

void FASettings::setUseDefaultRounding(bool val) {
	if (_useDefaultRounding.current() == val) return;
	_useDefaultRounding = val;
	save();
}

void FASettings::setShowDiscussButton(bool val) {
	if (_showDiscussButton.current() == val) return;
	_showDiscussButton = val;
	save();
}

void FASettings::setShowFastshareInChats(bool val) {
	if (_showFastshareInChats.current() == val) return;
	_showFastshareInChats = val;
	save();
}

void FASettings::setRoundness(int val) {
	if (_roundness.current() == val) return;
	_roundness = val;
	save();
}

void FASettings::setForceSnow(bool val) {
	if (_forceSnow.current() == val) return;
	_forceSnow = val;
	save();
}

void FASettings::setShowMessageDetails(bool val) {
	if (_showMessageDetails.current() == val) return;
	_showMessageDetails = val;
	save();
}

void FASettings::setHideBlockedUserMessages(bool val) {
	if (_hideBlockedUserMessages.current() == val) return;
	_hideBlockedUserMessages = val;
	save();
}

void FASettings::setShowStatusDot(bool val) {
	if (_showStatusDot.current() == val) return;
	_showStatusDot = val;
	save();
}

void FASettings::setStatusDotOnlineOnly(bool val) {
	if (_statusDotOnlineOnly.current() == val) return;
	_statusDotOnlineOnly = val;
	save();
}

void FASettings::setContextMenuUseShortcuts(bool val) {
	if (_contextMenuUseShortcuts.current() == val) return;
	_contextMenuUseShortcuts = val;
	save();
}

void FASettings::setContextMenuShortcutsAtBottom(bool val) {
	if (_contextMenuShortcutsAtBottom.current() == val) return;
	_contextMenuShortcutsAtBottom = val;
	save();
}

void FASettings::setContextMenuReplyInPrivate(bool val) {
	if (_contextMenuReplyInPrivate.current() == val) return;
	_contextMenuReplyInPrivate = val;
	save();
}

void FASettings::setContextMenuForwardSubmenu(bool val) {
	if (_contextMenuForwardSubmenu.current() == val) return;
	_contextMenuForwardSubmenu = val;
	save();
}

void FASettings::setUseTdesktopThemes(bool val) {
	if (_useTdesktopThemes.current() == val) return;
	_useTdesktopThemes = val;
	save();
}

void FASettings::setUseMaterialIconPack(bool val) {
	if (_useMaterialIconPack.current() == val) return;
	_useMaterialIconPack = val;
	save();
}

void FASettings::setShareMenuFolderIcons(bool val) {
	if (_shareMenuFolderIcons.current() == val) return;
	_shareMenuFolderIcons = val;
	save();
}

void FASettings::setAvatarShape(int val) {
	if (_avatarShape.current() == val) return;
	_avatarShape = val;
	save();
}

void FASettings::setDisablePremiumAnimation(bool val) {
	if (_disablePremiumAnimation.current() == val) return;
	_disablePremiumAnimation = val;
	save();
}

void FASettings::setScreenshotMode(bool val) {
	if (_screenshotMode.current() == val) return;
	_screenshotMode = val;
	save();
}

void FASettings::setAutoFormatMarkdown(bool val) {
	if (_autoFormatMarkdown.current() == val) return;
	_autoFormatMarkdown = val;
	save();
}

void FASettings::setAddCommaAfterMention(bool val) {
	if (_addCommaAfterMention.current() == val) return;
	_addCommaAfterMention = val;
	save();
}

void FASettings::setDisableLinkPreview(bool val) {
	if (_disableLinkPreview.current() == val) return;
	_disableLinkPreview = val;
	save();
}

void FASettings::setContextMenuShortcutButtonSize(int val) {
	if (_contextMenuShortcutButtonSize.current() == val) return;
	_contextMenuShortcutButtonSize = val;
	save();
}

void FASettings::setContextMenuShortcutIconSize(int val) {
	if (_contextMenuShortcutIconSize.current() == val) return;
	_contextMenuShortcutIconSize = val;
	save();
}

void FASettings::setContextMenuShortcutSpacing(int val) {
	if (_contextMenuShortcutSpacing.current() == val) return;
	_contextMenuShortcutSpacing = val;
	save();
}

void FASettings::setContextMenuShortcutVerticalPadding(int val) {
	if (_contextMenuShortcutVerticalPadding.current() == val) return;
	_contextMenuShortcutVerticalPadding = val;
	save();
}

void FASettings::setContextMenuShortcutHorizontalPadding(int val) {
	if (_contextMenuShortcutHorizontalPadding.current() == val) return;
	_contextMenuShortcutHorizontalPadding = val;
	save();
}

void FASettings::setContextMenuShortcutCornerRadius(int val) {
	if (_contextMenuShortcutCornerRadius.current() == val) return;
	_contextMenuShortcutCornerRadius = val;
	save();
}

void FASettings::setTranslationProvider(int val) {
	if (_translationProvider.current() == val) return;
	_translationProvider = val;
	save();
}

} // namespace FASettings
