/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "base/call_delayed.h"

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"
#include "fa/ui/previews.h"

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

    rpl::producer<QString> FAAppearance::title() {
        return fatr::fa_appearance();
    }

    FAAppearance::FAAppearance(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FAAppearance::SetupAppearance(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, fatr::fa_appearance());

		const auto roundnessPreview = container->add(
			object_ptr<RoundnessPreview>(container),
			st::defaultSubsectionTitlePadding);

    	const auto userpicRoundnessLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
    	const auto userpicRoundnessSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			userpicRoundnessSlider,
			u"fa/appearance/roundness"_q,
			controller);

		const auto savedRoundness = container->lifetime().make_state<int>(
			FASettings::FASettings::getInstance().roundness());
		const auto inSetRoundness = container->lifetime().make_state<bool>(false);

		const auto updateUserpicRoundnessLabel = [=](int value) {
    		const auto radius = QString::number(value);
    		userpicRoundnessLabel->setText(fatr::fa_rounding(fatr::now).arg(radius));
    	};
		const auto valueFromRoundness = [](int roundness) {
			return roundness / 50.0;
		};
    	const auto setRoundness = [=](int value, const auto &repeatSetRoundness) -> void {
			if (*inSetRoundness) {
				return;
			}
			*inSetRoundness = true;
			const auto guard = gsl::finally([=] { *inSetRoundness = false; });

    		updateUserpicRoundnessLabel(value);
			roundnessPreview->repaint();
			userpicRoundnessSlider->setValue(valueFromRoundness(value));

			if (value != *savedRoundness) {
				const auto confirmed = crl::guard(userpicRoundnessSlider, [=] {
					*savedRoundness = value;
					FASettings::FASettings::getInstance().setRoundness(value);
					
					::Core::Restart();
				});
				const auto cancelled = crl::guard(userpicRoundnessSlider, [=](Fn<void()> close) {
					FASettings::FASettings::getInstance().setRoundness(*savedRoundness);
					base::call_delayed(
						st::defaultSettingsSlider.duration,
						userpicRoundnessSlider,
						[=] { repeatSetRoundness(*savedRoundness, repeatSetRoundness); });
					close();
				});
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = confirmed,
					.cancelled = cancelled,
					.confirmText = fatr::fa_restart(),
				}));
			}
    	};
		const auto updateUserpicRoundness = [=](int value) {
			updateUserpicRoundnessLabel(value);
			roundnessPreview->repaint();
			FASettings::FASettings::getInstance().setRoundness(value);
		};
    	userpicRoundnessSlider->resize(st::settingsAudioVolumeSlider.seekSize);
    	userpicRoundnessSlider->setPseudoDiscrete(
			51,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().roundness(),
			updateUserpicRoundness,
			[=](int value) { setRoundness(value, setRoundness); });
    	updateUserpicRoundnessLabel(FASettings::FASettings::getInstance().roundness());
        Ui::AddDividerText(container, fatr::fa_rounding_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_use_default_rounding(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.useDefaultRoundingValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.useDefaultRounding());
			}) | rpl::on_next([=, &settings](bool enabled) {
				settings.setUseDefaultRounding(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/appearance/default-rounding"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_use_default_rounding_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_screenshot_mode(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.screenshotModeValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.screenshotMode());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setScreenshotMode(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/appearance/screenshot-mode"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_screenshot_mode_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_force_snow(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.forceSnowValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.forceSnow());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setForceSnow(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/appearance/force-snow"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_force_snow_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_hide_stories(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.hideStoriesValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.hideStories());
			}) | rpl::on_next([=, &settings](bool enabled) {
				settings.setHideStories(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/appearance/hide-stories"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_hide_stories_desc());
		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_use_tdesktop_themes(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.useTdesktopThemesValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.useTdesktopThemes());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setUseTdesktopThemes(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/appearance/use-tdesktop-themes"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_use_tdesktop_themes_desc());
		const auto iconPackBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_use_material_icon_pack(),
			st::settingsButtonNoIcon
		));
		iconPackBtn->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().useMaterialIconPack())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().useMaterialIconPack());
		}) | rpl::on_next([=](bool enabled) {
			FASettings::FASettings::getInstance().setUseMaterialIconPack(enabled);
			
			controller->show(Ui::MakeConfirmBox({
				.text = fatr::fa_icon_pack_restart_prompt(),
				.confirmed = [=] {
					::Core::Restart();
				},
				.confirmText = fatr::fa_icon_pack_restart_now(),
				.cancelText = fatr::fa_icon_pack_restart_later(),
			}));
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			iconPackBtn, u"fa/appearance/material-icons"_q, controller);
		Ui::AddDividerText(container, fatr::fa_use_material_icon_pack_desc());
    }

    void FAAppearance::SetupFAAppearance(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
    	SetupAppearance(container, controller);
    }

    void FAAppearance::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAAppearance(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram
