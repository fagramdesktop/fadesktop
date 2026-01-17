/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/basic_click_handlers.h"

namespace Main {
class Session;
} // namespace Main

class PhoneClickHandler : public ClickHandler {
public:
	PhoneClickHandler(not_null<Main::Session*> session, QString text);

	void onClick(ClickContext context) const override;

	TextEntity getTextEntity() const override;

	QString tooltip() const override;

private:
	const not_null<Main::Session*> _session;
	QString _text;

};
