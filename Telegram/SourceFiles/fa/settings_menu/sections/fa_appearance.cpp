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
#include "fa/ui/components/fa_ui_components.h"
#include "fa/ui/components/previews.h"

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
		Ui::AddSubsectionTitle(container, fatr::fa_icons());

		const auto block = container->add(object_ptr<Ui::FixedHeightWidget>(
			container));
		
		const auto group = std::make_shared<Ui::RadioenumGroup<bool>>(
			FASettings::FASettings::getInstance().useMaterialIconPack());
		
		std::vector<Ui::Radioenum<bool>*> buttons;
		const auto makeButton = [&](bool value, rpl::producer<QString> &&textProducer, bool isMaterial) {
			auto check = std::make_unique<IconPackCheck>(isMaterial, false);
			const auto weak = check.get();
			
			static const auto customStyle = [] {
				auto style = st::settingsTheme;
				style.textPosition = QPoint(0, 76 + 8);
				style.style = st::semiboldTextStyle;
				return style;
			}();
			
			const auto result = Ui::CreateChild<Ui::Radioenum<bool>>(
				block,
				group,
				value,
				QString(),
				customStyle,
				std::move(check));
			rpl::duplicate(
				textProducer
			) | rpl::on_next([=](const QString &text) {
				result->setText(text);
			}, result->lifetime());
			weak->setUpdateCallback([=] { result->update(); });
			buttons.push_back(result);
		};
		makeButton(false, fatr::fa_default_icon(), false);
		makeButton(true, fatr::fa_material_icon(), true);
		
		for (const auto button : buttons) {
			button->setCheckAlignment(style::al_top);
			button->resizeToWidth(button->width());
		}
		block->resize(block->width(), buttons[0]->height());
		block->widthValue(
		) | rpl::on_next([buttons = std::move(buttons), block](int width) {
			Expects(!buttons.empty());

			const auto padding = 16;
			const auto gap = 16;
			const auto count = int(buttons.size());
			const auto availableWidth = width - 2 * padding;
			const auto single = (availableWidth - (count - 1) * gap) / count;
			if (single <= 0) {
				return;
			}
			auto left = padding;

			for (const auto button : buttons) {
				button->resizeToWidth(single);
				button->moveToLeft(left, 0);
				left += single + gap;
			}
			block->resize(width, buttons[0]->height());
		}, block->lifetime());

		group->setChangedCallback([=](bool enabled) {
			if (enabled == FASettings::FASettings::getInstance().useMaterialIconPack()) {
				return;
			}
			FASettings::FASettings::getInstance().setUseMaterialIconPack(enabled);
			controller->show(Ui::MakeConfirmBox({
				.text = fatr::fa_icon_pack_restart_prompt(),
				.confirmed = [=] {
					::Core::Restart();
				},
				.confirmText = fatr::fa_icon_pack_restart_now(),
				.cancelText = fatr::fa_icon_pack_restart_later(),
			}));
		});

		FA::Ui::AddModernSectionHeader(container, fatr::fa_avatar());

		const auto shapeBlock = container->add(object_ptr<Ui::FixedHeightWidget>(
			container));
		
		const auto shapeGroup = std::make_shared<Ui::RadioenumGroup<int>>(
			FASettings::FASettings::getInstance().avatarShape());
		
		static const auto kAvatarShapeCheckboxStyle = [] {
			auto st = st::settingsTheme;
			st.textPosition = QPoint(0, 0);
			return st;
		}();

		std::vector<Ui::Radioenum<int>*> shapeButtons;
		const auto makeShapeButton = [&](int shapeIndex) {
			auto check = std::make_unique<AvatarShapeCheck>(shapeIndex, false);
			const auto weak = check.get();
			
			const auto result = Ui::CreateChild<Ui::Radioenum<int>>(
				shapeBlock,
				shapeGroup,
				shapeIndex,
				QString(),
				kAvatarShapeCheckboxStyle,
				std::move(check));
			weak->setUpdateCallback([=] { result->update(); });
			shapeButtons.push_back(result);
		};

		for (auto i = 0; i < 8; ++i) {
			makeShapeButton(i);
		}

		for (const auto button : shapeButtons) {
			button->setCheckAlignment(style::al_top);
			button->resizeToWidth(button->width());
		}

		const auto buttonHeight = 72;
		const auto rowGap = 8;
		shapeBlock->resize(shapeBlock->width(), buttonHeight * 2 + rowGap);
		shapeBlock->widthValue(
		) | rpl::on_next([buttons = std::move(shapeButtons), shapeBlock, buttonHeight, rowGap](int width) {
			Expects(buttons.size() == 8);

			const auto padding = 16;
			const auto gap = 8;
			const auto cols = 4;
			const auto availableWidth = width - 2 * padding;
			const auto single = (availableWidth - (cols - 1) * gap) / cols;
			if (single <= 0) {
				return;
			}

			for (auto i = 0; i < 8; ++i) {
				const auto row = i / cols;
				const auto col = i % cols;
				const auto left = padding + col * (single + gap);
				const auto top = row * (buttonHeight + rowGap);
				buttons[i]->resizeToWidth(single);
				buttons[i]->moveToLeft(left, top);
			}
			shapeBlock->resize(width, buttonHeight * 2 + rowGap);
		}, shapeBlock->lifetime());

		const auto roundnessCardWrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		const auto roundnessInner = roundnessCardWrap->entity();
		const auto roundnessCard = FA::Ui::CreateCardContainer(roundnessInner, 12, 4);
		const auto roundnessPreview = roundnessCard->add(
			object_ptr<RoundnessPreview>(roundnessCard),
			style::margins(0, 4, 0, 4));

		FA::Ui::AddCardDivider(roundnessCard);

		const auto userpicRoundnessLabel = roundnessCard->add(
			object_ptr<Ui::LabelSimple>(
				roundnessCard,
				st::settingsAudioVolumeLabel),
			style::margins(16, 12, 16, 4));
		const auto userpicRoundnessSlider = FA::Ui::AddCardSlider(
			roundnessCard,
			style::margins(16, 4, 16, 16));
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			userpicRoundnessSlider,
			u"fa/appearance/roundness"_q,
			controller);

		roundnessCardWrap->toggle(
			FASettings::FASettings::getInstance().avatarShape() == 0,
			anim::type::instant);

		shapeGroup->setChangedCallback([=](int shape) {
			if (shape == FASettings::FASettings::getInstance().avatarShape()) {
				return;
			}
			FASettings::FASettings::getInstance().setAvatarShape(shape);
			roundnessCardWrap->toggle(shape == 0, anim::type::normal);
		});

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
		userpicRoundnessSlider->setPseudoDiscrete(
			51,
			[](int val) { return val; },
			FASettings::FASettings::getInstance().roundness(),
			updateUserpicRoundness,
			[=](int value) { setRoundness(value, setRoundness); });
		updateUserpicRoundnessLabel(FASettings::FASettings::getInstance().roundness());

		FA::Ui::AddModernSectionHeader(container, fatr::fa_appearance());
		const auto optionsCard = FA::Ui::CreateCardContainer(container);
		auto &settings = FASettings::FASettings::getInstance();

		const auto defRoundRow = FA::Ui::AddCardToggle(
			optionsCard,
			fatr::fa_use_default_rounding(),
			fatr::fa_use_default_rounding_desc(),
			settings.useDefaultRoundingValue(),
			[=, &settings](bool enabled) {
				settings.setUseDefaultRounding(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			defRoundRow, u"fa/appearance/default-rounding"_q, controller);

		FA::Ui::AddCardDivider(optionsCard);

		const auto ssModeRow = FA::Ui::AddCardToggle(
			optionsCard,
			fatr::fa_screenshot_mode(),
			fatr::fa_screenshot_mode_desc(),
			settings.screenshotModeValue(),
			[&settings](bool enabled) {
				settings.setScreenshotMode(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			ssModeRow, u"fa/appearance/screenshot-mode"_q, controller);

		FA::Ui::AddCardDivider(optionsCard);

		const auto snowRow = FA::Ui::AddCardToggle(
			optionsCard,
			fatr::fa_force_snow(),
			fatr::fa_force_snow_desc(),
			settings.forceSnowValue(),
			[&settings](bool enabled) {
				settings.setForceSnow(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			snowRow, u"fa/appearance/force-snow"_q, controller);

		FA::Ui::AddCardDivider(optionsCard);

		const auto storiesRow = FA::Ui::AddCardToggle(
			optionsCard,
			fatr::fa_hide_stories(),
			fatr::fa_hide_stories_desc(),
			settings.hideStoriesValue(),
			[=, &settings](bool enabled) {
				settings.setHideStories(enabled);
				controller->show(Ui::MakeConfirmBox({
					.text = fatr::fa_setting_need_restart(),
					.confirmed = [=] {
						::Core::Restart();
					},
					.confirmText = fatr::fa_restart()
				}));
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			storiesRow, u"fa/appearance/hide-stories"_q, controller);

		FA::Ui::AddCardDivider(optionsCard);

		const auto themesRow = FA::Ui::AddCardToggle(
			optionsCard,
			fatr::fa_use_tdesktop_themes(),
			fatr::fa_use_tdesktop_themes_desc(),
			settings.useTdesktopThemesValue(),
			[&settings](bool enabled) {
				settings.setUseTdesktopThemes(enabled);
			});
		Settings::FADeepLinkMenu::AttachSettingsContextMenu(
			themesRow, u"fa/appearance/use-tdesktop-themes"_q, controller);
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
