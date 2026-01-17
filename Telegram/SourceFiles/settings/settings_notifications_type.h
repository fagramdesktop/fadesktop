/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common_session.h"
#include "data/notify/data_notify_settings.h"

namespace Settings {

class NotificationsType : public AbstractSection {
public:
	NotificationsType(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Data::DefaultNotify type);

	[[nodiscard]] rpl::producer<QString> title() override;

	[[nodiscard]] static Type Id(Data::DefaultNotify type);

	[[nodiscard]] Type id() const final override {
		return Id(_type);
	}

private:
	void setupContent(not_null<Window::SessionController*> controller);

	const Data::DefaultNotify _type;

};

[[nodiscard]] bool NotificationsEnabledForType(
	not_null<::Main::Session*> session,
	Data::DefaultNotify type);

[[nodiscard]] rpl::producer<bool> NotificationsEnabledForTypeValue(
	not_null<::Main::Session*> session,
	Data::DefaultNotify type);

} // namespace Settings
