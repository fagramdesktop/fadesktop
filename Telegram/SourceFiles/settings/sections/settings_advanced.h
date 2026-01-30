/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_type.h"

namespace Main {
class Account;
class Session;
} // namespace Main

namespace Ui {
class GenericBox;
class VerticalLayout;
} // namespace Ui

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {

[[nodiscard]] Type AdvancedId();

void SetupConnectionType(
	not_null<Window::Controller*> controller,
	not_null<::Main::Account*> account,
	not_null<Ui::VerticalLayout*> container);
bool HasUpdate();
void SetupUpdate(not_null<Ui::VerticalLayout*> container);
void SetupWindowTitleContent(
	Window::SessionController *controller,
	not_null<Ui::VerticalLayout*> container);
void SetupSystemIntegrationContent(
	Window::SessionController *controller,
	not_null<Ui::VerticalLayout*> container);
void SetupAnimations(
	not_null<Window::Controller*> window,
	not_null<Ui::VerticalLayout*> container);

void ArchiveSettingsBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);
void PreloadArchiveSettings(not_null<::Main::Session*> session);

} // namespace Settings
