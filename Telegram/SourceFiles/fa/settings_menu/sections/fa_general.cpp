/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/



#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"

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
        Ui::AddSubsectionTitle(container, fatr::fa_general());

		Ui::AddSubsectionTitle(container, fatr::fa_translation_provider());

		const auto translationGroup = std::make_shared<Ui::RadiobuttonGroup>(
			FASettings::FASettings::getInstance().translationProvider());
		translationGroup->setChangedCallback([=](int value) {
			FASettings::FASettings::getInstance().setTranslationProvider(value);
			
		});

		const auto addTranslationRadio = [&](int value, const QString &text) {
			const auto radio = container->add(
				object_ptr<Ui::Radiobutton>(
					container,
					translationGroup,
					value,
					text,
					st::settingsSendType),
				st::settingsSendTypePadding);
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				radio,
				u"fa/general/translation-provider"_q,
				controller);
		};

		addTranslationRadio(0, fatr::fa_translate_telegram(fatr::now));
		addTranslationRadio(1, fatr::fa_translate_google(fatr::now));
		addTranslationRadio(2, fatr::fa_translate_yandex(fatr::now));
		addTranslationRadio(3, fatr::fa_translate_native(fatr::now));

		Ui::AddSkip(container);
		Ui::AddDivider(container);

        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_disable_ads(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.disableAdsValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.disableAds());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setDisableAds(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/disable-ads"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_disable_ads_desc());
		const auto disableAi = container->add(object_ptr<Button>(
			container,
			fatr::fa_disable_ai(),
			st::settingsButtonNoIcon
		));
		const auto hideAiOption = &base::options::lookup<bool>(Ui::kOptionHideAiButton);
		disableAi->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().disableAi())
		)->toggledValue(
		) | rpl::filter([=](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().disableAi());
		}) | rpl::on_next([=](bool enabled) {
			hideAiOption->set(enabled);
			FASettings::FASettings::getInstance().setDisableAi(enabled);
			
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			disableAi,
			u"fa/general/disable-ai"_q,
			controller);
		Ui::AddDividerText(container, fatr::fa_disable_ai_desc());

        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_disable_animated_avatars(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.disableAnimatedAvatarsValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.disableAnimatedAvatars());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setDisableAnimatedAvatars(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/disable-animated-avatars"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_disable_animated_avatars_desc());

        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_disable_premium_animation(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.disablePremiumAnimationValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.disablePremiumAnimation());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setDisablePremiumAnimation(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/disable-premium-animation"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_disable_premium_animation_desc());

		const auto disableAutoDownload = container->add(object_ptr<Button>(
			container,
			fatr::fa_disable_auto_download(),
			st::settingsButtonNoIcon
		));
		disableAutoDownload->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().disableAutoDownload())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().disableAutoDownload());
		}) | rpl::on_next([=](bool enabled) {
			FASettings::FASettings::getInstance().setDisableAutoDownload(enabled);
			
			auto &session = controller->session();
			session.data().photoLoadSettingsChanged();
			session.data().documentLoadSettingsChanged();
			if (enabled) {
				session.data().checkPlayingAnimations();
			}
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			disableAutoDownload,
			u"fa/general/disable-auto-download"_q,
			controller);
		Ui::AddDividerText(container, fatr::fa_disable_auto_download_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_show_start_token(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.showStartTokenValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.showStartToken());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setShowStartToken(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/start-token"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_show_start_token_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_show_peer_ids(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.showPeerIdValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.showPeerId());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setShowPeerId(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/peer-ids"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_show_peer_ids_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_show_dc_ids(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.showDcIdValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.showDcId());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setShowDcId(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/dc-ids"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_show_dc_ids_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_id_in_botapi_type(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.showIdBotapiValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.showIdBotapi());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setShowIdBotapi(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/botapi-id"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_id_in_botapi_type_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_local_tg_premium(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.localPremiumValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.localPremium());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setLocalPremium(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/local-premium"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_local_tg_premium_desc());
        {
        	auto &settings = FASettings::FASettings::getInstance();
        	const auto btn = container->add(object_ptr<Button>(
        		container,
        		fatr::fa_show_registration_date(),
        		st::settingsButtonNoIcon
        	));
        	btn->toggleOn(
        		settings.showRegistrationDateValue()
        	)->toggledValue(
        	) | rpl::filter([&settings](bool enabled) {
        		return (enabled != settings.showRegistrationDate());
        	}) | rpl::on_next([&settings](bool enabled) {
        		settings.setShowRegistrationDate(enabled);
        	}, container->lifetime());
        	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
        		btn, u"fa/general/registration-date"_q, controller);
        }
        Ui::AddDividerText(container, fatr::fa_show_registration_date_desc());
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