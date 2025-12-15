/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_chats.h"

#include "fa/lang/fa_lang.h"

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

    rpl::producer<QString> FAChats::title() {
        return FAlang::RplTranslate(QString("fa_chats"));
    }

    FAChats::FAChats(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent) {
        setupContent(controller);
    }

    void FAChats::SetupChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
        Ui::AddSubsectionTitle(container, FAlang::RplTranslate(QString("fa_chats")));

		const auto recentStickersLimitLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto recentStickersLimitSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateRecentStickersLimitLabel = [=](int value) {
			recentStickersLimitLabel->setText(
				(value == 0)
					? FAlang::Translate(QString("fa_recent_stickers_hidden"))
					: FAlang::Translate(QString("fa_recent_stickers")).arg(value) );
		};
        const auto updateRecentStickersLimitHeight = [=](int value) {
			updateRecentStickersLimitLabel(value);
			::FASettings::JsonSettings::Set("recent_stickers_limit", value);
			::FASettings::JsonSettings::Write();
		};
		recentStickersLimitSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		recentStickersLimitSlider->setPseudoDiscrete(
			100+1,
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("recent_stickers_limit"),
			updateRecentStickersLimitHeight);
		updateRecentStickersLimitLabel(::FASettings::JsonSettings::GetInt("recent_stickers_limit"));
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_recent_stickers_desc")));
		SettingsMenuJsonSwitch(fa_show_seconds_message, seconds_message);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_show_seconds_message_desc")));
		SettingsMenuJsonSwitch(fa_disable_custom_chat_background, disable_custom_chat_background);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_disable_custom_chat_background_desc")));
		SettingsMenuJsonSwitch(fa_hide_open_webapp_button_chatlist, hide_open_webapp_button_chatlist);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_hide_open_webapp_button_chatlist_desc")));
		SettingsMenuJsonSwitch(fa_show_discuss_button, show_discuss_button);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_show_discuss_button_desc")));
		SettingsMenuJsonSwitch(fa_show_message_details, show_message_details);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_show_message_details_desc")));
		RestartSettingsMenuJsonSwitch(fa_hide_all_chats_folder, hide_all_chats_folder);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_hide_all_chats_folder_desc")));

		const auto hideBlockedBtn = container->add(object_ptr<Button>(
			container,
			FAlang::RplTranslate(QString("fa_hide_blocked_user_messages")),
			st::settingsButtonNoIcon
		));
		hideBlockedBtn->setColorOverride(QColor(255, 0, 0));
		hideBlockedBtn->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("hide_blocked_user_messages"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("hide_blocked_user_messages"));
		}) | rpl::on_next([=](bool enabled) {
			::FASettings::JsonSettings::Set("hide_blocked_user_messages", enabled);
			::FASettings::JsonSettings::Write();

			controller->showToast(FAlang::Translate(QString("fa_restarting_in_seconds")));
			base::call_delayed(crl::time(3000), container, [] {
				::Core::Restart();
			});
		}, container->lifetime());

		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_hide_blocked_user_messages_desc")));
    }

    void FAChats::SetupContextMenu(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
		Ui::AddSubsectionTitle(container, FAlang::RplTranslate(QString("fa_context_menu")));

		container->add(object_ptr<Button>(
			container,
			FAlang::RplTranslate(QString("fa_context_menu_settings")),
			st::settingsButtonNoIcon
		))->toggleOn(
			rpl::single(::FASettings::JsonSettings::GetBool("context_menu_use_shortcuts"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != ::FASettings::JsonSettings::GetBool("context_menu_use_shortcuts"));
		}) | rpl::on_next([](bool enabled) {
			::FASettings::JsonSettings::Write();
			::FASettings::JsonSettings::Set("context_menu_use_shortcuts", enabled);
			::FASettings::JsonSettings::Write();
		}, container->lifetime());

		SettingsMenuJsonSwitch(fa_context_menu_move_to_bottom, context_menu_shortcuts_at_bottom);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_context_menu_desc")));

		// Shortcut button size slider
		const auto buttonSizeLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto buttonSizeSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateButtonSizeLabel = [=](int value) {
			buttonSizeLabel->setText(FAlang::Translate(QString("fa_shortcut_button_size")).arg(value));
		};
		const auto updateButtonSize = [=](int value) {
			updateButtonSizeLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_button_size", value);
			::FASettings::JsonSettings::Write();
		};
		buttonSizeSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		buttonSizeSlider->setPseudoDiscrete(
			41, // 24 to 64 = 41 values
			[](int val) { return val + 24; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_button_size"),
			updateButtonSize);
		updateButtonSizeLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_button_size"));
		const auto resetButtonSize = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			buttonSizeLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetButtonSize->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetButtonSize->height()) / 2,
				width);
		}, resetButtonSize->lifetime());
		resetButtonSize->setClickedCallback([=] {
			constexpr int defaultValue = 40;
			buttonSizeSlider->setValue(float64(defaultValue - 24) / 40.0);
			updateButtonSize(defaultValue);
		});

		// Shortcut icon size slider
		const auto iconSizeLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto iconSizeSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateIconSizeLabel = [=](int value) {
			iconSizeLabel->setText(FAlang::Translate(QString("fa_shortcut_icon_size")).arg(value));
		};
		const auto updateIconSize = [=](int value) {
			updateIconSizeLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_icon_size", value);
			::FASettings::JsonSettings::Write();
		};
		iconSizeSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		iconSizeSlider->setPseudoDiscrete(
			33, // 16 to 48 = 33 values
			[](int val) { return val + 16; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_icon_size"),
			updateIconSize);
		updateIconSizeLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_icon_size"));
		const auto resetIconSize = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			iconSizeLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetIconSize->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetIconSize->height()) / 2,
				width);
		}, resetIconSize->lifetime());
		resetIconSize->setClickedCallback([=] {
			constexpr int defaultValue = 24;
			iconSizeSlider->setValue(float64(defaultValue - 16) / 32.0);
			updateIconSize(defaultValue);
		});

		// Shortcut spacing slider
		const auto spacingLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto spacingSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateSpacingLabel = [=](int value) {
			spacingLabel->setText(FAlang::Translate(QString("fa_shortcut_spacing")).arg(value));
		};
		const auto updateSpacing = [=](int value) {
			updateSpacingLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_spacing", value);
			::FASettings::JsonSettings::Write();
		};
		spacingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		spacingSlider->setPseudoDiscrete(
			25, // 0 to 24 = 25 values
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_spacing"),
			updateSpacing);
		updateSpacingLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_spacing"));
		const auto resetSpacing = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			spacingLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetSpacing->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetSpacing->height()) / 2,
				width);
		}, resetSpacing->lifetime());
		resetSpacing->setClickedCallback([=] {
			constexpr int defaultValue = 10;
			spacingSlider->setValue(float64(defaultValue) / 24.0);
			updateSpacing(defaultValue);
		});

		// Shortcut horizontal padding slider
		const auto hPaddingLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto hPaddingSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateHPaddingLabel = [=](int value) {
			hPaddingLabel->setText(FAlang::Translate(QString("fa_shortcut_horizontal_padding")).arg(value));
		};
		const auto updateHPadding = [=](int value) {
			updateHPaddingLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_horizontal_padding", value);
			::FASettings::JsonSettings::Write();
		};
		hPaddingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		hPaddingSlider->setPseudoDiscrete(
			17, // 0 to 16 = 17 values
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_horizontal_padding"),
			updateHPadding);
		updateHPaddingLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_horizontal_padding"));
		const auto resetHPadding = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			hPaddingLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetHPadding->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetHPadding->height()) / 2,
				width);
		}, resetHPadding->lifetime());
		resetHPadding->setClickedCallback([=] {
			constexpr int defaultValue = 10;
			hPaddingSlider->setValue(float64(defaultValue) / 16.0);
			updateHPadding(defaultValue);
		});

		// Shortcut vertical padding slider
		const auto vPaddingLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto vPaddingSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateVPaddingLabel = [=](int value) {
			vPaddingLabel->setText(FAlang::Translate(QString("fa_shortcut_vertical_padding")).arg(value));
		};
		const auto updateVPadding = [=](int value) {
			updateVPaddingLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_vertical_padding", value);
			::FASettings::JsonSettings::Write();
		};
		vPaddingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		vPaddingSlider->setPseudoDiscrete(
			17, // 0 to 16 = 17 values
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_vertical_padding"),
			updateVPadding);
		updateVPaddingLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_vertical_padding"));
		const auto resetVPadding = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			vPaddingLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetVPadding->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetVPadding->height()) / 2,
				width);
		}, resetVPadding->lifetime());
		resetVPadding->setClickedCallback([=] {
			constexpr int defaultValue = 2;
			vPaddingSlider->setValue(float64(defaultValue) / 16.0);
			updateVPadding(defaultValue);
		});

		// Shortcut corner radius slider
		const auto cornerRadiusLabel = container->add(
			object_ptr<Ui::LabelSimple>(
				container,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto cornerRadiusSlider = container->add(
			object_ptr<Ui::MediaSlider>(
				container,
				st::settingsAudioVolumeSlider),
			st::settingsAudioVolumeSliderPadding);
		const auto updateCornerRadiusLabel = [=](int value) {
			cornerRadiusLabel->setText(FAlang::Translate(QString("fa_shortcut_corner_radius")).arg(value));
		};
		const auto updateCornerRadius = [=](int value) {
			updateCornerRadiusLabel(value);
			::FASettings::JsonSettings::Set("context_menu_shortcut_corner_radius", value);
			::FASettings::JsonSettings::Write();
		};
		cornerRadiusSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		cornerRadiusSlider->setPseudoDiscrete(
			21, // 0 to 20 = 21 values
			[](int val) { return val; },
			::FASettings::JsonSettings::GetInt("context_menu_shortcut_corner_radius"),
			updateCornerRadius);
		updateCornerRadiusLabel(::FASettings::JsonSettings::GetInt("context_menu_shortcut_corner_radius"));
		const auto resetCornerRadius = Ui::CreateChild<Ui::IconButton>(
			container,
			st::settingsSliderRestore);
		rpl::combine(
			cornerRadiusLabel->geometryValue(),
			container->widthValue()
		) | rpl::on_next([=](QRect labelRect, int width) {
			resetCornerRadius->moveToRight(
				st::settingsAudioVolumeLabelPadding.right(),
				labelRect.y() + (labelRect.height() - resetCornerRadius->height()) / 2,
				width);
		}, resetCornerRadius->lifetime());
		resetCornerRadius->setClickedCallback([=] {
			constexpr int defaultValue = 20;
			cornerRadiusSlider->setValue(float64(defaultValue) / 20.0);
			updateCornerRadius(defaultValue);
		});

		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_shortcut_customization_desc")));

		SettingsMenuJsonSwitch(fa_context_menu_reply_in_private, context_menu_reply_in_private);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_context_menu_reply_in_private_desc")));

		SettingsMenuJsonSwitch(fa_context_menu_forward_submenu, context_menu_forward_submenu);
		Ui::AddDividerText(container, FAlang::RplTranslate(QString("fa_context_menu_forward_submenu_desc")));
    }

    void FAChats::SetupFAChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
		SetupChats(container, controller);
		SetupContextMenu(container, controller);
    }

    void FAChats::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAChats(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram
