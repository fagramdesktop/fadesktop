/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

// thanks rabbitGram

#include <ui/boxes/single_choice_box.h>

#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/sections/fa_chats.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/sections/fa_logs.h"
#include "fa/ui/components/fa_ui_components.h"

#include "fa_lang_auto.h"
#include "fa/settings/fa_settings.h"

#include "core/application.h"
#include "core/file_utilities.h"
#include "lang_auto.h"
#include "mainwindow.h"
#include "settings/settings_common.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/checkbox.h"
#include "ui/vertical_list.h"
#include "boxes/connection_box.h"
#include "platform/platform_specific.h"
#include "window/window_session_controller.h"
#include "lang/lang_instance.h"
#include "core/application.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "ui/toast/toast.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_payments.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"
#include "api/api_common.h"
#include "data/data_user.h"
#include "storage/localimageloader.h"
#include "storage/storage_media_prepare.h"
#include "ui/chat/attach/attach_prepare.h"
#include "window/window_peer_menu.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/basic_click_handlers.h"
#include "styles/style_chat.h"
#include "styles/style_boxes.h"


namespace Settings {

    rpl::producer<QString> FA::title() {
        return fatr::fa_client_preferences();
    }

    FA::FA(
            QWidget *parent,
            not_null<Window::SessionController *> controller)
            : Section(parent, controller) {
        setupContent(controller);
    }

    void FA::fillTopBarMenu(
            const Ui::Menu::MenuCallback &addAction) {
        addAction(
            fatr::fa_share_settings_to_chat(fatr::now),
            [=] {
                const auto data = FASettings::FASettings::getInstance().exportSettingsJson();
                const auto tempPath = QDir::temp().filePath(u"settings.faconfig"_q);
                auto f = QFile(tempPath);
                if (!f.open(QIODevice::WriteOnly)) {
                    return;
                }
                f.write(data);
                f.close();

                const auto ctrl = controller();
                auto chosen = [=](not_null<Data::Thread*> thread) mutable -> bool {
                    auto list = Storage::PrepareMediaList(
                        QStringList{ tempPath },
                        st::sendMediaPreviewSize,
                        ctrl->session().user()->isPremium());
                    if (list.error != Ui::PreparedList::Error::None) {
                        return false;
                    }
                    auto action = Api::SendAction(thread);
                    action.clearDraft = false;
                    ctrl->session().api().sendFiles(
                        std::move(list),
                        SendMediaType::File,
                        nullptr,
                        action);
                    return true;
                };
                Window::ShowChooseRecipientBox(ctrl, std::move(chosen));
            },
            &st::menuIconShare);
        addAction(
            fatr::fa_share_settings(fatr::now),
            [=] {
                const auto data = FASettings::FASettings::getInstance().exportSettingsJson();
                FileDialog::GetWritePath(
                    Core::App().getFileDialogParent(),
                    u"Export FAgram Settings"_q,
                    u"FA Config (*.faconfig)"_q,
                    u"settings.faconfig"_q,
                    crl::guard(this, [=](const QString &path) {
                        if (path.isEmpty()) {
                            return;
                        }
                        auto f = QFile(path);
                        if (!f.open(QIODevice::WriteOnly)) {
                            return;
                        }
                        f.write(data);
                    }));
            },
            &st::menuIconExport);
        addAction(
            fatr::fa_restart(fatr::now),
            [] { Core::Restart(); },
            &st::menuIconRestore);
    }

    void FA::SetupFASettings(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
    	::FA::Ui::AddModernSectionHeader(container, fatr::fa_categories());
		const auto card = ::FA::Ui::CreateCardContainer(container);

		const auto addSection = [&](
				rpl::producer<QString> label,
				Type type,
				const style::icon *icon,
				bool isLast) {
			::FA::Ui::AddCardButton(
				card,
				std::move(label),
				[=] { showOther(type); },
				icon,
				nullptr,
				true);
			if (!isLast) {
				::FA::Ui::AddCardDivider(card);
			}
		};
    	addSection(
			fatr::fa_general(),
			FAGeneral::Id(),
			&st::menuIconShowAll,
			false);
    	addSection(
			fatr::fa_chats(),
			FAChats::Id(),
			&st::menuIconChatBubble,
			false);
    	addSection(
			fatr::fa_context_menu(),
			FAContextMenu::Id(),
			&st::menuIconSigned,
			false);
    	addSection(
			fatr::fa_appearance(),
			FAAppearance::Id(),
			&st::menuIconPalette,
			false);
    	addSection(
			fatr::fa_debug_logs(),
			FALogs::Id(),
			&st::menuIconFile,
			true);
    }

	void FA::SetupLinks(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller)
    {
    	::FA::Ui::AddModernSectionHeader(container, fatr::fa_links());
		const auto card = ::FA::Ui::CreateCardContainer(container);

	    ::FA::Ui::AddCardButton(
			card,
			fatr::fa_channel(),
			[=] { Core::App().openLocalUrl("tg://resolve?domain=FAgramDesktop", {}); },
			&st::menuIconChannel,
			rpl::single(u"@FAgramDesktop"_q),
			false);

		::FA::Ui::AddCardDivider(card);

    	::FA::Ui::AddCardButton(
			card,
			fatr::fa_group(),
			[=] { Core::App().openLocalUrl("tg://resolve?domain=FAgramChat", {}); },
			&st::menuIconGroups,
			rpl::single(u"@FAgramChat"_q),
			false);

		::FA::Ui::AddCardDivider(card);

    	::FA::Ui::AddCardButton(
			card,
			fatr::fa_translation(),
			[=] { UrlClickHandler::Open("https://hosted.weblate.org/projects/fagramdesktop/"); },
			&st::menuIconTranslate,
			rpl::single(u"Weblate"_q),
			false);

		::FA::Ui::AddCardDivider(card);

    	::FA::Ui::AddCardButton(
			card,
			fatr::fa_source_code(),
			[=] { UrlClickHandler::Open("https://github.com/fagramdesktop/fadesktop"); },
			&st::menuIconLink,
			rpl::single(u"GitHub"_q),
			false);
    }

    void FA::SetupDown(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller) {
		::FA::Ui::AddModernSectionHeader(container, fatr::fa_fagram());
		const auto card = ::FA::Ui::CreateCardContainer(container);

    	::FA::Ui::AddCardButton(
			card,
			fatr::fa_restart_client(),
			[=] { Core::Restart(); },
			&st::menuIconRestore);

		::FA::Ui::AddCardDivider(card);

    	::FA::Ui::AddCardButton(
			card,
			fatr::fa_quit_client(),
			[=] { Core::Quit(); },
			&st::menuIconCancel);
    }

    void FA::setupContent(not_null<Window::SessionController *> controller) {
        const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

        SetupFASettings(content, controller);
    	SetupLinks(content, controller);
    	SetupDown(content, controller);
        Ui::ResizeFitChild(this, content);
    }
} // namespace Settings
