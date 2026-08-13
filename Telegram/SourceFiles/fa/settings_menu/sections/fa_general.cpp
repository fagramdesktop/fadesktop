/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/



#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"
#include "fa/ui/components/fa_ui_components.h"

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
#include "ui/controls/compose_ai_button_factory.h"
#include "base/options.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"
#include "ui/widgets/continuous_sliders.h"

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
		FA::Ui::AddModernSectionHeader(container, fatr::fa_translation_provider());
		const auto transCard = FA::Ui::CreateCardContainer(container);

		const auto translationGroup = std::make_shared<Ui::RadiobuttonGroup>(
			FASettings::FASettings::getInstance().translationProvider());
		translationGroup->setChangedCallback([=](int value) {
			FASettings::FASettings::getInstance().setTranslationProvider(value);
		});

		const auto addTranslationRadio = [&](int value, rpl::producer<QString> label, bool isLast) {
			const auto radio = FA::Ui::AddCardRadio(
				transCard,
				translationGroup,
				value,
				std::move(label));
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				radio,
				u"fa/general/translation-provider"_q,
				controller);
			if (!isLast) {
				FA::Ui::AddCardDivider(transCard);
			}
		};

		addTranslationRadio(0, fatr::fa_translate_telegram(), false);
		addTranslationRadio(1, fatr::fa_translate_google(), false);
		addTranslationRadio(2, fatr::fa_translate_yandex(), false);
		addTranslationRadio(3, fatr::fa_translate_native(), true);

		FA::Ui::AddModernSectionHeader(container, fatr::fa_general());
		const auto privacyCard = FA::Ui::CreateCardContainer(container);
		auto &settings = FASettings::FASettings::getInstance();

		const auto adsRow = FA::Ui::AddCardToggle(
			privacyCard,
			fatr::fa_disable_ads(),
			fatr::fa_disable_ads_desc(),
			settings.disableAdsValue(),
			[&settings](bool enabled) {
				settings.setDisableAds(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			adsRow, u"fa/general/disable-ads"_q, controller);

		FA::Ui::AddCardDivider(privacyCard);

		const auto hideAiOption = &base::options::lookup<bool>(Ui::kOptionHideAiButton);
		const auto aiRow = FA::Ui::AddCardToggle(
			privacyCard,
			fatr::fa_disable_ai(),
			fatr::fa_disable_ai_desc(),
			rpl::single(settings.disableAi()),
			[=, &settings](bool enabled) {
				hideAiOption->set(enabled);
				settings.setDisableAi(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			aiRow, u"fa/general/disable-ai"_q, controller);

		FA::Ui::AddCardDivider(privacyCard);

		const auto animAvatarsRow = FA::Ui::AddCardToggle(
			privacyCard,
			fatr::fa_disable_animated_avatars(),
			fatr::fa_disable_animated_avatars_desc(),
			settings.disableAnimatedAvatarsValue(),
			[&settings](bool enabled) {
				settings.setDisableAnimatedAvatars(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			animAvatarsRow, u"fa/general/disable-animated-avatars"_q, controller);

		FA::Ui::AddCardDivider(privacyCard);

		const auto premiumAnimRow = FA::Ui::AddCardToggle(
			privacyCard,
			fatr::fa_disable_premium_animation(),
			fatr::fa_disable_premium_animation_desc(),
			settings.disablePremiumAnimationValue(),
			[&settings](bool enabled) {
				settings.setDisablePremiumAnimation(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			premiumAnimRow, u"fa/general/disable-premium-animation"_q, controller);

		FA::Ui::AddCardDivider(privacyCard);

		const auto autoDownloadRow = FA::Ui::AddCardToggle(
			privacyCard,
			fatr::fa_disable_auto_download(),
			fatr::fa_disable_auto_download_desc(),
			rpl::single(settings.disableAutoDownload()),
			[=, &settings](bool enabled) {
				settings.setDisableAutoDownload(enabled);
				auto &session = controller->session();
				session.data().photoLoadSettingsChanged();
				session.data().documentLoadSettingsChanged();
				if (enabled) {
					session.data().checkPlayingAnimations();
				}
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			autoDownloadRow, u"fa/general/disable-auto-download"_q, controller);

		FA::Ui::AddModernSectionHeader(container, fatr::fa_developer_and_profile());
		const auto devCard = FA::Ui::CreateCardContainer(container);

		const auto startTokenRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_show_start_token(),
			fatr::fa_show_start_token_desc(),
			settings.showStartTokenValue(),
			[&settings](bool enabled) {
				settings.setShowStartToken(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			startTokenRow, u"fa/general/start-token"_q, controller);

		FA::Ui::AddCardDivider(devCard);

		const auto peerIdRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_show_peer_ids(),
			fatr::fa_show_peer_ids_desc(),
			settings.showPeerIdValue(),
			[&settings](bool enabled) {
				settings.setShowPeerId(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			peerIdRow, u"fa/general/peer-ids"_q, controller);

		FA::Ui::AddCardDivider(devCard);

		const auto dcIdRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_show_dc_ids(),
			fatr::fa_show_dc_ids_desc(),
			settings.showDcIdValue(),
			[&settings](bool enabled) {
				settings.setShowDcId(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			dcIdRow, u"fa/general/dc-ids"_q, controller);

		FA::Ui::AddCardDivider(devCard);

		const auto botapiRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_id_in_botapi_type(),
			fatr::fa_id_in_botapi_type_desc(),
			settings.showIdBotapiValue(),
			[&settings](bool enabled) {
				settings.setShowIdBotapi(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			botapiRow, u"fa/general/botapi-id"_q, controller);

		FA::Ui::AddCardDivider(devCard);

		const auto premiumRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_local_tg_premium(),
			fatr::fa_local_tg_premium_desc(),
			settings.localPremiumValue(),
			[&settings](bool enabled) {
				settings.setLocalPremium(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			premiumRow, u"fa/general/local-premium"_q, controller);

		FA::Ui::AddCardDivider(devCard);

		const auto regDateRow = FA::Ui::AddCardToggle(
			devCard,
			fatr::fa_show_registration_date(),
			fatr::fa_show_registration_date_desc(),
			settings.showRegistrationDateValue(),
			[&settings](bool enabled) {
				settings.setShowRegistrationDate(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			regDateRow, u"fa/general/registration-date"_q, controller);
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