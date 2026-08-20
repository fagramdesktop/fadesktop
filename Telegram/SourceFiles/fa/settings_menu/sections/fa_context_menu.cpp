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
#include "fa/ui/md3/fa_cards.h"
#include "fa/ui/md3/fa_slider.h"

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
		const auto buttonSize = FA::Ui::AddCardSliderRow(slidersCard);
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			buttonSize.slider,
			u"fa/context-menu/button-size"_q,
			controller);
		const auto updateButtonSizeLabel = [=](int value) {
			buttonSize.label->setText(fatr::fa_shortcut_button_size(fatr::now).arg(value));
		};
		const auto updateButtonSize = [=](int value) {
			updateButtonSizeLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutButtonSize(value);
		};
		buttonSize.slider->setPseudoDiscrete(
			41,
			[](int val) { return val + 24; },
			FASettings::FASettings::getInstance().contextMenuShortcutButtonSize(),
			updateButtonSize);
		updateButtonSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutButtonSize());
		buttonSize.reset->setClickedCallback([=] {
			constexpr int defaultValue = 40;
			buttonSize.slider->setValue(float64(defaultValue - 24) / 40.0);
			updateButtonSize(defaultValue);
		});

		// Shortcut icon size slider
		const auto iconSize = FA::Ui::AddCardSliderRow(slidersCard);
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			iconSize.slider,
			u"fa/context-menu/icon-size"_q,
			controller);
		const auto updateIconSizeLabel = [=](int value) {
			iconSize.label->setText(fatr::fa_shortcut_icon_size(fatr::now).arg(value));
		};
		const auto updateIconSize = [=](int value) {
			updateIconSizeLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutIconSize(value);
		};
		iconSize.slider->setPseudoDiscrete(
			33,
			[](int val) { return val + 16; },
			FASettings::FASettings::getInstance().contextMenuShortcutIconSize(),
			updateIconSize);
		updateIconSizeLabel(FASettings::FASettings::getInstance().contextMenuShortcutIconSize());
		iconSize.reset->setClickedCallback([=] {
			constexpr int defaultValue = 24;
			iconSize.slider->setValue(float64(defaultValue - 16) / 32.0);
			updateIconSize(defaultValue);
		});

		// Shortcut corner radius slider
		const auto cornerRadius = FA::Ui::AddCardSliderRow(slidersCard);
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			cornerRadius.slider,
			u"fa/context-menu/corner-radius"_q,
			controller);
		const auto updateCornerRadiusLabel = [=](int value) {
			cornerRadius.label->setText(fatr::fa_shortcut_corner_radius(fatr::now).arg(value));
		};
		const auto updateCornerRadius = [=](int value) {
			updateCornerRadiusLabel(value);
			FASettings::FASettings::getInstance().setContextMenuShortcutCornerRadius(value);
		};
		cornerRadius.slider->setPseudoDiscrete(
			21,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius(),
			updateCornerRadius);
		updateCornerRadiusLabel(FASettings::FASettings::getInstance().contextMenuShortcutCornerRadius());
		cornerRadius.reset->setClickedCallback([=] {
			constexpr int defaultValue = 20;
			cornerRadius.slider->setValue(float64(defaultValue) / 20.0);
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
