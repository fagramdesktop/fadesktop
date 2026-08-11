/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include <ui/boxes/single_choice_box.h>

#include "fa/settings/fa_settings.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"

#include "fa_lang_auto.h"

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

namespace Settings {

    rpl::producer<QString> FAContextMenu::title() {
        return fatr::fa_context_menu();
    }

    FAContextMenu::FAContextMenu(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FAContextMenu::SetupContextMenu(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSubsectionTitle(container, fatr::fa_context_menu());

		const auto shortcutsBtn = container->add(object_ptr<Button>(
			container,
			fatr::fa_context_menu_settings(),
			st::settingsButtonNoIcon
		));
		shortcutsBtn->toggleOn(
			rpl::single(FASettings::FASettings::getInstance().contextMenuUseShortcuts())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != FASettings::FASettings::getInstance().contextMenuUseShortcuts());
		}) | rpl::on_next([](bool enabled) {
			
			FASettings::FASettings::getInstance().setContextMenuUseShortcuts(enabled);
			
		}, container->lifetime());
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			shortcutsBtn, u"fa/context-menu/shortcuts"_q, controller);

		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_context_menu_move_to_bottom(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.contextMenuShortcutsAtBottomValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.contextMenuShortcutsAtBottom());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setContextMenuShortcutsAtBottom(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/context-menu/shortcuts-bottom"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_context_menu_desc());

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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			buttonSizeSlider,
			u"fa/context-menu/button-size"_q,
			controller);
		const auto updateButtonSizeLabel = [=](int value) {
			buttonSizeLabel->setText(fatr::fa_shortcut_button_size(fatr::now).arg(value));
		};
		const auto updateButtonSize = [=](int value) {
			updateButtonSizeLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutButtonSize(value);
			
		};
		buttonSizeSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		buttonSizeSlider->setPseudoDiscrete(
			41, // 24 to 64 = 41 values
			[](int val) { return val + 24; },
			FASettings::FASettings::getInstance().contextMenuShortcutButtonSize(),
			updateButtonSize);
		updateButtonSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutButtonSize());
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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			iconSizeSlider,
			u"fa/context-menu/icon-size"_q,
			controller);
		const auto updateIconSizeLabel = [=](int value) {
			iconSizeLabel->setText(fatr::fa_shortcut_icon_size(fatr::now).arg(value));
		};
		const auto updateIconSize = [=](int value) {
			updateIconSizeLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutIconSize(value);
			
		};
		iconSizeSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		iconSizeSlider->setPseudoDiscrete(
			33, // 16 to 48 = 33 values
			[](int val) { return val + 16; },
			FASettings::FASettings::getInstance().contextMenuShortcutIconSize(),
			updateIconSize);
		updateIconSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutIconSize());
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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			spacingSlider,
			u"fa/context-menu/spacing"_q,
			controller);
		const auto updateSpacingLabel = [=](int value) {
			spacingLabel->setText(fatr::fa_shortcut_spacing(fatr::now).arg(value));
		};
		const auto updateSpacing = [=](int value) {
			updateSpacingLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutSpacing(value);
			
		};
		spacingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		spacingSlider->setPseudoDiscrete(
			25, // 0 to 24 = 25 values
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutSpacing(),
			updateSpacing);
		updateSpacingLabel(FASettings::FASettings::getInstance().contextMenuShortcutSpacing());
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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			hPaddingSlider,
			u"fa/context-menu/h-padding"_q,
			controller);
		const auto updateHPaddingLabel = [=](int value) {
			hPaddingLabel->setText(fatr::fa_shortcut_horizontal_padding(fatr::now).arg(value));
		};
		const auto updateHPadding = [=](int value) {
			updateHPaddingLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutHorizontalPadding(value);
			
		};
		hPaddingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		hPaddingSlider->setPseudoDiscrete(
			17, // 0 to 16 = 17 values
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutHorizontalPadding(),
			updateHPadding);
		updateHPaddingLabel(FASettings::FASettings::getInstance().contextMenuShortcutHorizontalPadding());
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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			vPaddingSlider,
			u"fa/context-menu/v-padding"_q,
			controller);
		const auto updateVPaddingLabel = [=](int value) {
			vPaddingLabel->setText(fatr::fa_shortcut_vertical_padding(fatr::now).arg(value));
		};
		const auto updateVPadding = [=](int value) {
			updateVPaddingLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutVerticalPadding(value);
			
		};
		vPaddingSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		vPaddingSlider->setPseudoDiscrete(
			17, // 0 to 16 = 17 values
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutVerticalPadding(),
			updateVPadding);
		updateVPaddingLabel(FASettings::FASettings::getInstance().contextMenuShortcutVerticalPadding());
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
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			cornerRadiusSlider,
			u"fa/context-menu/corner-radius"_q,
			controller);
		const auto updateCornerRadiusLabel = [=](int value) {
			cornerRadiusLabel->setText(fatr::fa_shortcut_corner_radius(fatr::now).arg(value));
		};
		const auto updateCornerRadius = [=](int value) {
			updateCornerRadiusLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutCornerRadius(value);
			
		};
		cornerRadiusSlider->resize(st::settingsAudioVolumeSlider.seekSize);
		cornerRadiusSlider->setPseudoDiscrete(
			21, // 0 to 20 = 21 values
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius(),
			updateCornerRadius);
		updateCornerRadiusLabel(FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius());
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

		Ui::AddDividerText(container, fatr::fa_shortcut_customization_desc());

		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_context_menu_reply_in_private(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.contextMenuReplyInPrivateValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.contextMenuReplyInPrivate());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setContextMenuReplyInPrivate(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/context-menu/reply-private"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_context_menu_reply_in_private_desc());

		{
			auto &settings = FASettings::FASettings::getInstance();
			const auto btn = container->add(object_ptr<Button>(
				container,
				fatr::fa_context_menu_forward_submenu(),
				st::settingsButtonNoIcon
			));
			btn->toggleOn(
				settings.contextMenuForwardSubmenuValue()
			)->toggledValue(
			) | rpl::filter([&settings](bool enabled) {
				return (enabled != settings.contextMenuForwardSubmenu());
			}) | rpl::on_next([&settings](bool enabled) {
				settings.setContextMenuForwardSubmenu(enabled);
			}, container->lifetime());
			Settings::FADeepLinkMenu::AttachSettingsContextMenu(
				btn, u"fa/context-menu/forward-submenu"_q, controller);
		}
		Ui::AddDividerText(container, fatr::fa_context_menu_forward_submenu_desc());
    }

    void FAContextMenu::SetupFAContextMenu(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		Ui::AddSkip(container);
		SetupContextMenu(container, controller);
    }

    void FAContextMenu::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFAContextMenu(content, controller);

        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings

// thanks rabbitGram
