/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

// thx ayugram

#include "fa/url_handlers/fa_url_handlers.h"

#include "window/window_controller.h"

#include "fa/utils/telegram_helpers.h"

#include "fa/settings/fa_settings.h"
#include "fa/lang/fa_lang.h"

#include "base/qthelp_url.h"

#include "lang_auto.h"
#include "mainwindow.h"
#include "ui/boxes/confirm_box.h"
#include "fa/utils/telegram_helpers.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_peer_id.h"

namespace FAUrlHandlers
{

bool HandleSomeText(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}

    std::vector<QString> responds = {
        "Meow ^_^",
        "Use FAgram ;)",
        "Nothing...",
        "just something...",
        "FAgram4ik ><",
        "FAgram4ik :>"
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, responds.size() - 1);

    int randomIndex = dis(gen);
    
    QString respond = responds[randomIndex];

	controller->showToast(respond, 500);
	return true;
}

bool HandleCleanDebugLogs(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}
	controller->showToast(FAlang::Translate(QString("fa_cleaning_debug_logs")), 500);
	cleanDebugLogs();
	controller->showToast(FAlang::Translate(QString("fa_cleaned_debug_logs")), 1000);
	return true;
}

bool HandleNothing(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}
	controller->showToast(FAlang::Translate(QString("fa_not_found")), 500);
	return true;
}

bool HandleSwitchDebugLogs(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}

	bool debug_logs = FASettings::JsonSettings::GetBool("debug_logs");

	FASettings::JsonSettings::Write();
	FASettings::JsonSettings::Set("debug_logs", !debug_logs);
	FASettings::JsonSettings::Write();

	QString message = debug_logs 
		? FAlang::Translate(QString("fa_debug_logs_off")) 
		: FAlang::Translate(QString("fa_debug_logs_on"));
	controller->showToast(message, 1000);

	return true;
}

bool HandleRestart(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}

	Core::Restart();

	return true;
}

bool HandleQuit(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context)
{
	if (!controller) {
		return false;
	}

	Core::Quit();

	return true;
}

// thx ayugram
bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}
	const auto params = url_parse_params(
		match->captured(1),
		qthelp::UrlParamNameTransform::ToLower);
	const auto userId = params.value(qsl("id")).toLongLong();
	if (!userId) {
		return false;
	}
	const auto peer = controller->session().data().peerLoaded(peerFromUser(UserId(userId)));
	if (peer != nullptr) {
		controller->showPeerInfo(peer);
		return true;
	}

	searchById(userId,
			   &controller->session(),
			   [=](const QString &title, UserData *data)
			   {
				   if (data) {
					   controller->showPeerInfo(data);
					   return;
				   }

				   Core::App().hideMediaView();
				   controller->showToast(FAlang::Translate(QString("fa_not_found")), 500);
			   });

	return true;
}

bool ResolveUserChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context) {
	if (!controller) {
		return false;
	}

	const auto userId = match->captured(1).toLongLong();
	if (!userId) {
		return false;
	}

	const auto openChat = [=](not_null<PeerData*> peer) {
		controller->showPeerHistory(peer);
		controller->window().activate();
	};

	const auto peer = controller->session().data().peerLoaded(peerFromUser(UserId(userId)));
	if (peer != nullptr) {
		openChat(peer);
		return true;
	}

	searchById(
		userId,
		&controller->session(),
		[=](const QString &title, UserData *data) {
			if (data) {
				openChat(data);
				return;
			}

			Core::App().hideMediaView();
			controller->showToast(FAlang::Translate(QString("fa_not_found")), 500);
		});

	return true;
}

}