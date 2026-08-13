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
#include "fa/ui/components/fa_ui_components.h"

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
		auto &settings = FASettings::FASettings::getInstance();

		FA::Ui::AddModernSectionHeader(container, fatr::fa_context_menu());
		const auto shortcutsCard = FA::Ui::CreateCardContainer(container);

		const auto shortcutsRow = FA::Ui::AddCardToggle(
			shortcutsCard,
			fatr::fa_context_menu_settings(),
			fatr::fa_context_menu_desc(),
			rpl::single(settings.contextMenuUseShortcuts()),
			[&settings](bool enabled) {
				settings.setContextMenuUseShortcuts(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			shortcutsRow, u"fa/context-menu/shortcuts"_q, controller);

		FA::Ui::AddCardDivider(shortcutsCard);

		const auto bottomRow = FA::Ui::AddCardToggle(
			shortcutsCard,
			fatr::fa_context_menu_move_to_bottom(),
			fatr::fa_context_menu_desc(),
			settings.contextMenuShortcutsAtBottomValue(),
			[&settings](bool enabled) {
				settings.setContextMenuShortcutsAtBottom(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			bottomRow, u"fa/context-menu/shortcuts-bottom"_q, controller);

		FA::Ui::AddModernSectionHeader(container, fatr::fa_shortcut_customization());
		const auto slidersCard = FA::Ui::CreateCardContainer(container);

		// Shortcut button size slider
		const auto buttonSizeLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto buttonSizeSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			41,
			[](int val) { return val + 24; },
			FASettings::FASettings::getInstance().contextMenuShortcutButtonSize(),
			updateButtonSize);
		updateButtonSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutButtonSize());
		const auto resetButtonSize = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			buttonSizeLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddCardDivider(slidersCard);

		// Shortcut icon size slider
		const auto iconSizeLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto iconSizeSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			33,
			[](int val) { return val + 16; },
			FASettings::FASettings::getInstance().contextMenuShortcutIconSize(),
			updateIconSize);
		updateIconSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutIconSize());
		const auto resetIconSize = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			iconSizeLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddCardDivider(slidersCard);

		// Shortcut spacing slider
		const auto spacingLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto spacingSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			25,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutSpacing(),
			updateSpacing);
		updateSpacingLabel(FASettings::FASettings::getInstance().contextMenuShortcutSpacing());
		const auto resetSpacing = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			spacingLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddCardDivider(slidersCard);

		// Shortcut horizontal padding slider
		const auto hPaddingLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto hPaddingSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			17,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutHorizontalPadding(),
			updateHPadding);
		updateHPaddingLabel(FASettings::FASettings::getInstance().contextMenuShortcutHorizontalPadding());
		const auto resetHPadding = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			hPaddingLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddCardDivider(slidersCard);

		// Shortcut vertical padding slider
		const auto vPaddingLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto vPaddingSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			17,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutVerticalPadding(),
			updateVPadding);
		updateVPaddingLabel(FASettings::FASettings::getInstance().contextMenuShortcutVerticalPadding());
		const auto resetVPadding = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			vPaddingLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddCardDivider(slidersCard);

		// Shortcut corner radius slider
		const auto cornerRadiusLabel = slidersCard->add(
			object_ptr<Ui::LabelSimple>(
				slidersCard,
				st::settingsAudioVolumeLabel),
			st::settingsAudioVolumeLabelPadding);
		const auto cornerRadiusSlider = slidersCard->add(
			object_ptr<Ui::MediaSlider>(
				slidersCard,
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
			21,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius(),
			updateCornerRadius);
		updateCornerRadiusLabel(FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius());
		const auto resetCornerRadius = Ui::CreateChild<Ui::IconButton>(
			slidersCard,
			st::settingsSliderRestore);
		rpl::combine(
			cornerRadiusLabel->geometryValue(),
			slidersCard->widthValue()
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

		FA::Ui::AddModernSectionHeader(container, fatr::fa_context_menu_actions());
		const auto actionsCard = FA::Ui::CreateCardContainer(container);

		const auto replyPrivateRow = FA::Ui::AddCardToggle(
			actionsCard,
			fatr::fa_context_menu_reply_in_private(),
			fatr::fa_context_menu_reply_in_private_desc(),
			settings.contextMenuReplyInPrivateValue(),
			[&settings](bool enabled) {
				settings.setContextMenuReplyInPrivate(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			replyPrivateRow, u"fa/context-menu/reply-private"_q, controller);

		FA::Ui::AddCardDivider(actionsCard);

		const auto fwdSubmenuRow = FA::Ui::AddCardToggle(
			actionsCard,
			fatr::fa_context_menu_forward_submenu(),
			fatr::fa_context_menu_forward_submenu_desc(),
			settings.contextMenuForwardSubmenuValue(),
			[&settings](bool enabled) {
				settings.setContextMenuForwardSubmenu(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			fwdSubmenuRow, u"fa/context-menu/forward-submenu"_q, controller);
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
