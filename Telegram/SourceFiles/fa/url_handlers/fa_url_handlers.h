/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

// thx ayugram

#pragma once

#include "window/window_session_controller.h"
#include "base/qthelp_regex.h"
#include <random>
#include <vector>

namespace FAUrlHandlers
{

using Match = qthelp::RegularExpressionMatch;

bool HandleSomeText(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleCleanDebugLogs(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleNothing(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleSwitchDebugLogs(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleRestart(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool HandleQuit(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool ResolveUser(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool ResolveUserChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

bool ResolveChat(
	Window::SessionController *controller,
	const Match &match,
	const QVariant &context);

}