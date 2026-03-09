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

		const auto savedRoundness = container->lifetime().make_state<int>(
			::FASettings::JsonSettings::GetInt("roundness"));
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
					::FASettings::JsonSettings::Set("roundness", value);
					::FASettings::JsonSettings::Write();
					::Core::Restart();
				});
				const auto cancelled = crl::guard(userpicRoundnessSlider, [=](Fn<void()> close) {
					::FASettings::JsonSettings::Set("roundness", *savedRoundness);
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
			::FASettings::JsonSettings::Set("roundness", value);
		};
    	userpicRoundnessSlider->resize(st::settingsAudioVolumeSlider.seekSize);
    	userpicRoundnessSlider->setPseudoDiscrete(
			51,
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("roundness"),
			updateUserpicRoundness,
			[=](int value) { setRoundness(value, setRoundness); });
    	updateUserpicRoundnessLabel(::FASettings::JsonSettings::GetInt("roundness"));
        Ui::AddDividerText(container, fatr::fa_rounding_desc());
		RestartSettingsMenuJsonSwitch(fa_use_default_rounding, use_default_rounding, u"fa/appearance/default-rounding"_q);
		Ui::AddDividerText(container, fatr::fa_use_default_rounding_desc());
		SettingsMenuJsonSwitch(fa_screenshot_mode, screenshot_mode, u"fa/appearance/screenshot-mode"_q);
		Ui::AddDividerText(container, fatr::fa_screenshot_mode_desc());
		SettingsMenuJsonSwitch(fa_force_snow, force_snow, u"fa/appearance/force-snow"_q);
		Ui::AddDividerText(container, fatr::fa_force_snow_desc());
		SettingsMenuJsonSwitch(fa_hide_phone_number, hide_phone_number, u"fa/appearance/hide-phone"_q);
		Ui::AddDividerText(container, fatr::fa_hide_phone_number_desc());
		RestartSettingsMenuJsonSwitch(fa_hide_stories, hide_stories, u"fa/appearance/hide-stories"_q);
		Ui::AddDividerText(container, fatr::fa_hide_stories_desc());
		RestartSettingsMenuJsonSwitch(fa_hide_folder_tabs_titles, hide_folder_tabs_titles, u"fa/appearance/hide-folder-titles"_q);
		Ui::AddDividerText(container, fatr::fa_hide_folder_tabs_titles_desc());
		SettingsMenuJsonSwitch(fa_use_tdesktop_themes, use_tdesktop_themes, u"fa/appearance/use-tdesktop-themes"_q);
		Ui::AddDividerText(container, fatr::fa_use_tdesktop_themes_desc());
		const auto iconPackBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_use_custom_icon_pack(),
			st::settingsButtonNoIcon
		));
		iconPackBtn->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("use_custom_icon_pack"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("use_custom_icon_pack"));
		}) | rpl::on_next([=](bool enabled) {
			::FASettings::JsonSettings::Set("use_custom_icon_pack", enabled);
			::FASettings::JsonSettings::Write();
			controller->show(Ui::MakeConfirmBox({
				.text = fatr::fa_icon_pack_restart_prompt(),
				.confirmed = [=] {
					::Core::Restart();
				},
				.confirmText = fatr::fa_icon_pack_restart_now(),
				.cancelText = fatr::fa_icon_pack_restart_later(),
			}));
		}, container->lifetime());
		Ui::AddDividerText(container, fatr::fa_use_custom_icon_pack_desc());
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
