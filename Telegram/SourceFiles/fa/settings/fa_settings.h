/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include <rpl/variable.h>
#include <rpl/producer.h>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QByteArray>

enum class TranslationProvider {
	Telegram = 0,
	Google = 1,
	Yandex = 2,
	Native = 3,
};

namespace FASettings {

class FASettings {
public:
	FASettings(const FASettings &) = delete;
	FASettings &operator=(const FASettings &) = delete;

	static FASettings &getInstance();
	static void load();
	static void save();
	void validate();

	[[nodiscard]] QByteArray exportSettingsJson() const;
	bool importSettingsFromJson(const QByteArray &json);

	// Type-safe getters
	[[nodiscard]] bool debugLogs() const { return _debugLogs.current(); }
	[[nodiscard]] bool secondsMessage() const { return _secondsMessage.current(); }
	[[nodiscard]] bool disableAds() const { return _disableAds.current(); }
	[[nodiscard]] bool disableAi() const { return _disableAi.current(); }
	[[nodiscard]] bool disableAnimatedAvatars() const { return _disableAnimatedAvatars.current(); }
	[[nodiscard]] bool disableAutoDownload() const { return _disableAutoDownload.current(); }
	[[nodiscard]] bool showStartToken() const { return _showStartToken.current(); }
	[[nodiscard]] bool showPeerId() const { return _showPeerId.current(); }
	[[nodiscard]] bool showDcId() const { return _showDcId.current(); }
	[[nodiscard]] bool showIdBotapi() const { return _showIdBotapi.current(); }
	[[nodiscard]] bool showRegistrationDate() const { return _showRegistrationDate.current(); }
	[[nodiscard]] bool disableCustomChatBackground() const { return _disableCustomChatBackground.current(); }
	[[nodiscard]] bool hideAllChatsFolder() const { return _hideAllChatsFolder.current(); }
	[[nodiscard]] bool hideStories() const { return _hideStories.current(); }
	[[nodiscard]] bool hideArchivedStories() const { return _hideArchivedStories.current(); }
	[[nodiscard]] bool hideOpenWebappButtonChatlist() const { return _hideOpenWebappButtonChatlist.current(); }
	[[nodiscard]] bool localPremium() const { return _localPremium.current(); }
	[[nodiscard]] bool unlimitedPinnedChats() const { return _unlimitedPinnedChats.current(); }
	[[nodiscard]] bool unlimitedChatFolders() const { return _unlimitedChatFolders.current(); }
	[[nodiscard]] bool deleteForEveryone() const { return _deleteForEveryone.current(); }
	[[nodiscard]] bool lastSeenTimestamp() const { return _lastSeenTimestamp.current(); }
	[[nodiscard]] bool showForwardedDateInTitle() const { return _showForwardedDateInTitle.current(); }
	[[nodiscard]] bool disableGreetingSticker() const { return _disableGreetingSticker.current(); }
	[[nodiscard]] bool useDefaultRounding() const { return _useDefaultRounding.current(); }
	[[nodiscard]] bool showDiscussButton() const { return _showDiscussButton.current(); }
	[[nodiscard]] bool showFastshareInChats() const { return _showFastshareInChats.current(); }
	[[nodiscard]] int roundness() const { return _roundness.current(); }
	[[nodiscard]] bool forceSnow() const { return _forceSnow.current(); }
	[[nodiscard]] bool showMessageDetails() const { return _showMessageDetails.current(); }
	[[nodiscard]] bool hideBlockedUserMessages() const { return _hideBlockedUserMessages.current(); }
	[[nodiscard]] bool showStatusDot() const { return _showStatusDot.current(); }
	[[nodiscard]] bool statusDotOnlineOnly() const { return _statusDotOnlineOnly.current(); }
	[[nodiscard]] bool contextMenuUseShortcuts() const { return _contextMenuUseShortcuts.current(); }
	[[nodiscard]] bool contextMenuShortcutsAtBottom() const { return _contextMenuShortcutsAtBottom.current(); }
	[[nodiscard]] bool contextMenuReplyInPrivate() const { return _contextMenuReplyInPrivate.current(); }
	[[nodiscard]] bool contextMenuForwardSubmenu() const { return _contextMenuForwardSubmenu.current(); }
	[[nodiscard]] bool useTdesktopThemes() const { return _useTdesktopThemes.current(); }
	[[nodiscard]] bool useMaterialIconPack() const { return _useMaterialIconPack.current(); }
	[[nodiscard]] bool disablePremiumAnimation() const { return _disablePremiumAnimation.current(); }
	[[nodiscard]] bool screenshotMode() const { return _screenshotMode.current(); }
	[[nodiscard]] bool autoFormatMarkdown() const { return _autoFormatMarkdown.current(); }
	[[nodiscard]] bool addCommaAfterMention() const { return _addCommaAfterMention.current(); }
	[[nodiscard]] int contextMenuShortcutButtonSize() const { return _contextMenuShortcutButtonSize.current(); }
	[[nodiscard]] int contextMenuShortcutIconSize() const { return _contextMenuShortcutIconSize.current(); }
	[[nodiscard]] int contextMenuShortcutSpacing() const { return _contextMenuShortcutSpacing.current(); }
	[[nodiscard]] int contextMenuShortcutVerticalPadding() const { return _contextMenuShortcutVerticalPadding.current(); }
	[[nodiscard]] int contextMenuShortcutHorizontalPadding() const { return _contextMenuShortcutHorizontalPadding.current(); }
	[[nodiscard]] int contextMenuShortcutCornerRadius() const { return _contextMenuShortcutCornerRadius.current(); }
	[[nodiscard]] int translationProvider() const { return _translationProvider.current(); }

	// Type-safe setters
	void setDebugLogs(bool val);
	void setSecondsMessage(bool val);
	void setDisableAds(bool val);
	void setDisableAi(bool val);
	void setDisableAnimatedAvatars(bool val);
	void setDisableAutoDownload(bool val);
	void setShowStartToken(bool val);
	void setShowPeerId(bool val);
	void setShowDcId(bool val);
	void setShowIdBotapi(bool val);
	void setShowRegistrationDate(bool val);
	void setDisableCustomChatBackground(bool val);
	void setHideAllChatsFolder(bool val);
	void setHideStories(bool val);
	void setHideArchivedStories(bool val);
	void setHideOpenWebappButtonChatlist(bool val);
	void setLocalPremium(bool val);
	void setUnlimitedPinnedChats(bool val);
	void setUnlimitedChatFolders(bool val);
	void setDeleteForEveryone(bool val);
	void setLastSeenTimestamp(bool val);
	void setShowForwardedDateInTitle(bool val);
	void setDisableGreetingSticker(bool val);
	void setUseDefaultRounding(bool val);
	void setShowDiscussButton(bool val);
	void setShowFastshareInChats(bool val);
	void setRoundness(int val);
	void setForceSnow(bool val);
	void setShowMessageDetails(bool val);
	void setHideBlockedUserMessages(bool val);
	void setShowStatusDot(bool val);
	void setStatusDotOnlineOnly(bool val);
	void setContextMenuUseShortcuts(bool val);
	void setContextMenuShortcutsAtBottom(bool val);
	void setContextMenuReplyInPrivate(bool val);
	void setContextMenuForwardSubmenu(bool val);
	void setUseTdesktopThemes(bool val);
	void setUseMaterialIconPack(bool val);
	void setDisablePremiumAnimation(bool val);
	void setScreenshotMode(bool val);
	void setAutoFormatMarkdown(bool val);
	void setAddCommaAfterMention(bool val);
	void setContextMenuShortcutButtonSize(int val);
	void setContextMenuShortcutIconSize(int val);
	void setContextMenuShortcutSpacing(int val);
	void setContextMenuShortcutVerticalPadding(int val);
	void setContextMenuShortcutHorizontalPadding(int val);
	void setContextMenuShortcutCornerRadius(int val);
	void setTranslationProvider(int val);

	// Reactive producers
	[[nodiscard]] rpl::producer<bool> debugLogsValue() const { return _debugLogs.value(); }
	[[nodiscard]] rpl::producer<bool> secondsMessageValue() const { return _secondsMessage.value(); }
	[[nodiscard]] rpl::producer<bool> disableAdsValue() const { return _disableAds.value(); }
	[[nodiscard]] rpl::producer<bool> disableAiValue() const { return _disableAi.value(); }
	[[nodiscard]] rpl::producer<bool> disableAnimatedAvatarsValue() const { return _disableAnimatedAvatars.value(); }
	[[nodiscard]] rpl::producer<bool> disableAutoDownloadValue() const { return _disableAutoDownload.value(); }
	[[nodiscard]] rpl::producer<bool> showStartTokenValue() const { return _showStartToken.value(); }
	[[nodiscard]] rpl::producer<bool> showPeerIdValue() const { return _showPeerId.value(); }
	[[nodiscard]] rpl::producer<bool> showDcIdValue() const { return _showDcId.value(); }
	[[nodiscard]] rpl::producer<bool> showIdBotapiValue() const { return _showIdBotapi.value(); }
	[[nodiscard]] rpl::producer<bool> showRegistrationDateValue() const { return _showRegistrationDate.value(); }
	[[nodiscard]] rpl::producer<bool> disableCustomChatBackgroundValue() const { return _disableCustomChatBackground.value(); }
	[[nodiscard]] rpl::producer<bool> hideAllChatsFolderValue() const { return _hideAllChatsFolder.value(); }
	[[nodiscard]] rpl::producer<bool> hideStoriesValue() const { return _hideStories.value(); }
	[[nodiscard]] rpl::producer<bool> hideArchivedStoriesValue() const { return _hideArchivedStories.value(); }
	[[nodiscard]] rpl::producer<bool> hideOpenWebappButtonChatlistValue() const { return _hideOpenWebappButtonChatlist.value(); }
	[[nodiscard]] rpl::producer<bool> localPremiumValue() const { return _localPremium.value(); }
	[[nodiscard]] rpl::producer<bool> unlimitedPinnedChatsValue() const { return _unlimitedPinnedChats.value(); }
	[[nodiscard]] rpl::producer<bool> unlimitedChatFoldersValue() const { return _unlimitedChatFolders.value(); }
	[[nodiscard]] rpl::producer<bool> deleteForEveryoneValue() const { return _deleteForEveryone.value(); }
	[[nodiscard]] rpl::producer<bool> lastSeenTimestampValue() const { return _lastSeenTimestamp.value(); }
	[[nodiscard]] rpl::producer<bool> showForwardedDateInTitleValue() const { return _showForwardedDateInTitle.value(); }
	[[nodiscard]] rpl::producer<bool> disableGreetingStickerValue() const { return _disableGreetingSticker.value(); }
	[[nodiscard]] rpl::producer<bool> useDefaultRoundingValue() const { return _useDefaultRounding.value(); }
	[[nodiscard]] rpl::producer<bool> showDiscussButtonValue() const { return _showDiscussButton.value(); }
	[[nodiscard]] rpl::producer<bool> showFastshareInChatsValue() const { return _showFastshareInChats.value(); }
	[[nodiscard]] rpl::producer<int> roundnessValue() const { return _roundness.value(); }
	[[nodiscard]] rpl::producer<bool> forceSnowValue() const { return _forceSnow.value(); }
	[[nodiscard]] rpl::producer<bool> showMessageDetailsValue() const { return _showMessageDetails.value(); }
	[[nodiscard]] rpl::producer<bool> hideBlockedUserMessagesValue() const { return _hideBlockedUserMessages.value(); }
	[[nodiscard]] rpl::producer<bool> showStatusDotValue() const { return _showStatusDot.value(); }
	[[nodiscard]] rpl::producer<bool> statusDotOnlineOnlyValue() const { return _statusDotOnlineOnly.value(); }
	[[nodiscard]] rpl::producer<bool> contextMenuUseShortcutsValue() const { return _contextMenuUseShortcuts.value(); }
	[[nodiscard]] rpl::producer<bool> contextMenuShortcutsAtBottomValue() const { return _contextMenuShortcutsAtBottom.value(); }
	[[nodiscard]] rpl::producer<bool> contextMenuReplyInPrivateValue() const { return _contextMenuReplyInPrivate.value(); }
	[[nodiscard]] rpl::producer<bool> contextMenuForwardSubmenuValue() const { return _contextMenuForwardSubmenu.value(); }
	[[nodiscard]] rpl::producer<bool> useTdesktopThemesValue() const { return _useTdesktopThemes.value(); }
	[[nodiscard]] rpl::producer<bool> useMaterialIconPackValue() const { return _useMaterialIconPack.value(); }
	[[nodiscard]] rpl::producer<bool> disablePremiumAnimationValue() const { return _disablePremiumAnimation.value(); }
	[[nodiscard]] rpl::producer<bool> screenshotModeValue() const { return _screenshotMode.value(); }
	[[nodiscard]] rpl::producer<bool> autoFormatMarkdownValue() const { return _autoFormatMarkdown.value(); }
	[[nodiscard]] rpl::producer<bool> addCommaAfterMentionValue() const { return _addCommaAfterMention.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutButtonSizeValue() const { return _contextMenuShortcutButtonSize.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutIconSizeValue() const { return _contextMenuShortcutIconSize.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutSpacingValue() const { return _contextMenuShortcutSpacing.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutVerticalPaddingValue() const { return _contextMenuShortcutVerticalPadding.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutHorizontalPaddingValue() const { return _contextMenuShortcutHorizontalPadding.value(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutCornerRadiusValue() const { return _contextMenuShortcutCornerRadius.value(); }
	[[nodiscard]] rpl::producer<int> translationProviderValue() const { return _translationProvider.value(); }

	[[nodiscard]] rpl::producer<bool> debugLogsChanges() const { return _debugLogs.changes(); }
	[[nodiscard]] rpl::producer<bool> secondsMessageChanges() const { return _secondsMessage.changes(); }
	[[nodiscard]] rpl::producer<bool> disableAdsChanges() const { return _disableAds.changes(); }
	[[nodiscard]] rpl::producer<bool> disableAiChanges() const { return _disableAi.changes(); }
	[[nodiscard]] rpl::producer<bool> disableAnimatedAvatarsChanges() const { return _disableAnimatedAvatars.changes(); }
	[[nodiscard]] rpl::producer<bool> disableAutoDownloadChanges() const { return _disableAutoDownload.changes(); }
	[[nodiscard]] rpl::producer<bool> showStartTokenChanges() const { return _showStartToken.changes(); }
	[[nodiscard]] rpl::producer<bool> showPeerIdChanges() const { return _showPeerId.changes(); }
	[[nodiscard]] rpl::producer<bool> showDcIdChanges() const { return _showDcId.changes(); }
	[[nodiscard]] rpl::producer<bool> showIdBotapiChanges() const { return _showIdBotapi.changes(); }
	[[nodiscard]] rpl::producer<bool> showRegistrationDateChanges() const { return _showRegistrationDate.changes(); }
	[[nodiscard]] rpl::producer<bool> disableCustomChatBackgroundChanges() const { return _disableCustomChatBackground.changes(); }
	[[nodiscard]] rpl::producer<bool> hideAllChatsFolderChanges() const { return _hideAllChatsFolder.changes(); }
	[[nodiscard]] rpl::producer<bool> hideStoriesChanges() const { return _hideStories.changes(); }
	[[nodiscard]] rpl::producer<bool> hideArchivedStoriesChanges() const { return _hideArchivedStories.changes(); }
	[[nodiscard]] rpl::producer<bool> hideOpenWebappButtonChatlistChanges() const { return _hideOpenWebappButtonChatlist.changes(); }
	[[nodiscard]] rpl::producer<bool> localPremiumChanges() const { return _localPremium.changes(); }
	[[nodiscard]] rpl::producer<bool> unlimitedPinnedChatsChanges() const { return _unlimitedPinnedChats.changes(); }
	[[nodiscard]] rpl::producer<bool> unlimitedChatFoldersChanges() const { return _unlimitedChatFolders.changes(); }
	[[nodiscard]] rpl::producer<bool> deleteForEveryoneChanges() const { return _deleteForEveryone.changes(); }
	[[nodiscard]] rpl::producer<bool> lastSeenTimestampChanges() const { return _lastSeenTimestamp.changes(); }
	[[nodiscard]] rpl::producer<bool> showForwardedDateInTitleChanges() const { return _showForwardedDateInTitle.changes(); }
	[[nodiscard]] rpl::producer<bool> disableGreetingStickerChanges() const { return _disableGreetingSticker.changes(); }
	[[nodiscard]] rpl::producer<bool> useDefaultRoundingChanges() const { return _useDefaultRounding.changes(); }
	[[nodiscard]] rpl::producer<bool> showDiscussButtonChanges() const { return _showDiscussButton.changes(); }
	[[nodiscard]] rpl::producer<bool> showFastshareInChatsChanges() const { return _showFastshareInChats.changes(); }
	[[nodiscard]] rpl::producer<int> roundnessChanges() const { return _roundness.changes(); }
	[[nodiscard]] rpl::producer<bool> forceSnowChanges() const { return _forceSnow.changes(); }
	[[nodiscard]] rpl::producer<bool> showMessageDetailsChanges() const { return _showMessageDetails.changes(); }
	[[nodiscard]] rpl::producer<bool> hideBlockedUserMessagesChanges() const { return _hideBlockedUserMessages.changes(); }
	[[nodiscard]] rpl::producer<bool> showStatusDotChanges() const { return _showStatusDot.changes(); }
	[[nodiscard]] rpl::producer<bool> statusDotOnlineOnlyChanges() const { return _statusDotOnlineOnly.changes(); }
	[[nodiscard]] rpl::producer<bool> contextMenuUseShortcutsChanges() const { return _contextMenuUseShortcuts.changes(); }
	[[nodiscard]] rpl::producer<bool> contextMenuShortcutsAtBottomChanges() const { return _contextMenuShortcutsAtBottom.changes(); }
	[[nodiscard]] rpl::producer<bool> contextMenuReplyInPrivateChanges() const { return _contextMenuReplyInPrivate.changes(); }
	[[nodiscard]] rpl::producer<bool> contextMenuForwardSubmenuChanges() const { return _contextMenuForwardSubmenu.changes(); }
	[[nodiscard]] rpl::producer<bool> useTdesktopThemesChanges() const { return _useTdesktopThemes.changes(); }
	[[nodiscard]] rpl::producer<bool> useMaterialIconPackChanges() const { return _useMaterialIconPack.changes(); }
	[[nodiscard]] rpl::producer<bool> disablePremiumAnimationChanges() const { return _disablePremiumAnimation.changes(); }
	[[nodiscard]] rpl::producer<bool> screenshotModeChanges() const { return _screenshotMode.changes(); }
	[[nodiscard]] rpl::producer<bool> autoFormatMarkdownChanges() const { return _autoFormatMarkdown.changes(); }
	[[nodiscard]] rpl::producer<bool> addCommaAfterMentionChanges() const { return _addCommaAfterMention.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutButtonSizeChanges() const { return _contextMenuShortcutButtonSize.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutIconSizeChanges() const { return _contextMenuShortcutIconSize.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutSpacingChanges() const { return _contextMenuShortcutSpacing.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutVerticalPaddingChanges() const { return _contextMenuShortcutVerticalPadding.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutHorizontalPaddingChanges() const { return _contextMenuShortcutHorizontalPadding.changes(); }
	[[nodiscard]] rpl::producer<int> contextMenuShortcutCornerRadiusChanges() const { return _contextMenuShortcutCornerRadius.changes(); }
	[[nodiscard]] rpl::producer<int> translationProviderChanges() const { return _translationProvider.changes(); }

	// Account-scoped settings
	[[nodiscard]] QJsonArray pinnedChatOrder(uint64 accountId) const;
	void setPinnedChatOrder(const QJsonArray &val, uint64 accountId);

	[[nodiscard]] QJsonArray localChatFolders(uint64 accountId) const;
	void setLocalChatFolders(const QJsonArray &val, uint64 accountId);

	[[nodiscard]] QJsonArray localChatFoldersOrder(uint64 accountId) const;
	void setLocalChatFoldersOrder(const QJsonArray &val, uint64 accountId);

private:
	FASettings();

	void loadFromJson(const QJsonObject &obj);
	QJsonObject saveToJson() const;

	rpl::variable<bool> _debugLogs = false;
	rpl::variable<bool> _secondsMessage = true;
	rpl::variable<bool> _disableAds = false;
	rpl::variable<bool> _disableAi = false;
	rpl::variable<bool> _disableAnimatedAvatars = false;
	rpl::variable<bool> _disableAutoDownload = false;
	rpl::variable<bool> _showStartToken = true;
	rpl::variable<bool> _showPeerId = true;
	rpl::variable<bool> _showDcId = true;
	rpl::variable<bool> _showIdBotapi = true;
	rpl::variable<bool> _showRegistrationDate = true;
	rpl::variable<bool> _disableCustomChatBackground = false;
	rpl::variable<bool> _hideAllChatsFolder = false;
	rpl::variable<bool> _hideStories = false;
	rpl::variable<bool> _hideArchivedStories = false;
	rpl::variable<bool> _hideOpenWebappButtonChatlist = true;
	rpl::variable<bool> _localPremium = false;
	rpl::variable<bool> _unlimitedPinnedChats = false;
	rpl::variable<bool> _unlimitedChatFolders = false;
	rpl::variable<bool> _deleteForEveryone = true;
	rpl::variable<bool> _lastSeenTimestamp = true;
	rpl::variable<bool> _showForwardedDateInTitle = false;
	rpl::variable<bool> _disableGreetingSticker = false;
	rpl::variable<bool> _useDefaultRounding = true;
	rpl::variable<bool> _showDiscussButton = true;
	rpl::variable<bool> _showFastshareInChats = false;
	rpl::variable<int> _roundness = 50;
	rpl::variable<bool> _forceSnow = false;
	rpl::variable<bool> _showMessageDetails = true;
	rpl::variable<bool> _hideBlockedUserMessages = false;
	rpl::variable<bool> _showStatusDot = true;
	rpl::variable<bool> _statusDotOnlineOnly = false;
	rpl::variable<bool> _contextMenuUseShortcuts = false;
	rpl::variable<bool> _contextMenuShortcutsAtBottom = false;
	rpl::variable<bool> _contextMenuReplyInPrivate = true;
	rpl::variable<bool> _contextMenuForwardSubmenu = false;
	rpl::variable<bool> _useTdesktopThemes = false;
	rpl::variable<bool> _useMaterialIconPack = false;
	rpl::variable<bool> _disablePremiumAnimation = false;
	rpl::variable<bool> _screenshotMode = false;
	rpl::variable<bool> _autoFormatMarkdown = false;
	rpl::variable<bool> _addCommaAfterMention = true;
	rpl::variable<int> _contextMenuShortcutButtonSize = 40;
	rpl::variable<int> _contextMenuShortcutIconSize = 24;
	rpl::variable<int> _contextMenuShortcutSpacing = 10;
	rpl::variable<int> _contextMenuShortcutVerticalPadding = 2;
	rpl::variable<int> _contextMenuShortcutHorizontalPadding = 10;
	rpl::variable<int> _contextMenuShortcutCornerRadius = 20;
	rpl::variable<int> _translationProvider = static_cast<int>(TranslationProvider::Telegram);

	QJsonObject _accountSettings;
};

// Ensure backward compatibility during transition if needed
// Or just let `getInstance()` be called globally.

} // namespace FASettings