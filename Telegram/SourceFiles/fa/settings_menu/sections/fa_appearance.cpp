/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/ui/previews.h"

#include "fa/lang/fa_lang.h"

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

#define SettingsMenuJsonSwitch(LangKey, Option) container->add(object_ptr<Button>( \
	container, \
    FAlang::RplTranslate(QString(#LangKey)), \
	st::settingsButtonNoIcon \
))->toggleOn( \
	rpl::single(::FASettings::JsonSettings::GetBool(#Option)) \
)->toggledValue( \
) | rpl::filter([](bool enabled) { \
	return (enabled != ::FASettings::JsonSettings::GetBool(#Option)); \
}) | rpl::on_next([](bool enabled) { \
	::FASettings::JsonSettings::Write(); \
	::FASettings::JsonSettings::Set(#Option, enabled); \
	::FASettings::JsonSettings::Write(); \
}, container->lifetime());

#define RestartSettingsMenuJsonSwitch(LangKey, Option) container->add(object_ptr<Button>( \
    container, \
    FAlang::RplTranslate(QString(#LangKey)), \
    st::settingsButtonNoIcon \
))->toggleOn( \
    rpl::single(::FASettings::JsonSettings::GetBool(#Option)) \
)->toggledValue( \
) | rpl::filter([](bool enabled) { \
    return (enabled != ::FASettings::JsonSettings::GetBool(#Option)); \
}) | rpl::on_next([=](bool enabled) { \
    ::FASettings::JsonSettings::Write(); \
    ::FASettings::JsonSettings::Set(#Option, enabled); \
    ::FASettings::JsonSettings::Write(); \
    controller->show(Ui::MakeConfirmBox({ \
        .text = FAlang::RplTranslate(QString("fa_setting_need_restart")), \
        .confirmed = [=] { \
            ::Core::Restart(); \
        }, \
        .confirmText = FAlang::RplTranslate(QString("fa_restart")) \
    })); \
}, container->lifetime());

namespace Settings {

    rpl::producer<QString> FAAppearance::title() {
        return FAlang::RplTranslate(QString("fa_appearance"));
    }

    FAAppearance::FAAppearance(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent) {
        setupContent(controller);
    }

    void FAAppearance::SetupAppearance(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, FAlang::RplTranslate(("fa_appearance")));

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

		const auto updateUserpicRoundnessLabel = [=](int value) {
    		const auto radius = QString::number(value);
    		userpicRoundnessLabel->setText(FAlang::Translate(QString("fa_rounding")).arg(radius));
    	};
    	const auto updateUserpicRoundness = [=](int value) {
    		updateUserpicRoundnessLabel(value);
			roundnessPreview->repaint();
    		::FASettings::JsonSettings::Set("roundness", value);
    		::FASettings::JsonSettings::Write();
    	};
    	userpicRoundnessSlider->resize(st::settingsAudioVolumeSlider.seekSize);
    	userpicRoundnessSlider->setPseudoDiscrete(
			51,
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("roundness"),
			updateUserpicRoundness);
    	updateUserpicRoundnessLabel(::FASettings::JsonSettings::GetInt("roundness"));
        Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_rounding_desc")));
		RestartSettingsMenuJsonSwitch(fa_use_default_rounding, use_default_rounding);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_use_default_rounding_desc")));
		SettingsMenuJsonSwitch(fa_screenshot_mode, screenshot_mode);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_screenshot_mode_desc")));
		SettingsMenuJsonSwitch(fa_force_snow, force_snow);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_force_snow_desc")));
		SettingsMenuJsonSwitch(fa_hide_phone_number, hide_phone_number);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_hide_phone_number_desc")));
		RestartSettingsMenuJsonSwitch(fa_hide_stories, hide_stories);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_hide_stories_desc")));
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
