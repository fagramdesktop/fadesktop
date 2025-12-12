/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#pragma once

#include "settings/settings_common.h"
#include "settings/settings_common_session.h"

class BoxContent;

namespace Window {
    class Controller;

    class SessionController;
} // namespace Window

namespace Settings {
    class FAChats : public Section<FAChats> {
    public:
        FAChats(QWidget *parent, not_null<Window::SessionController *> controller);

        [[nodiscard]] rpl::producer<QString> title() override;

    private:
        void SetupChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller);
        void SetupContextMenu(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> controller);
        
        void SetupFAChats(not_null<Ui::VerticalLayout *> container, not_null<Window::SessionController *> null);
        void setupContent(not_null<Window::SessionController *> controller);
    };

} // namespace Settings