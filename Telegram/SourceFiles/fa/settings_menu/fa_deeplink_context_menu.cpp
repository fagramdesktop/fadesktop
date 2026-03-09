/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/settings_menu/fa_deeplink_context_menu.h"

#include "base/event_filter.h"
#include "boxes/share_box.h"
#include "lang/lang_keys.h"
#include "fa/deep_links/fa_deep_links.h"
#include "fa/lang/fa_lang.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

#include <QGuiApplication>
#include <QClipboard>

namespace Settings::FADeepLinkMenu {
namespace {

base::unique_qptr<Ui::PopupMenu> ContextMenu;

} // namespace

void AttachSettingsContextMenu(
		not_null<QWidget*> widget,
		const QString &controlId,
		not_null<Window::SessionController*> controller) {
	const auto deepLink = Core::DeepLinks::FASettingsDeepLink(controlId);
	if (deepLink.isEmpty()) {
		return;
	}

	base::install_event_filter(widget, [=](not_null<QEvent*> e) {
		if (e->type() != QEvent::ContextMenu) {
			return base::EventFilterResult::Continue;
		}

		ContextMenu = base::make_unique_q<Ui::PopupMenu>(widget);

		ContextMenu->addAction(
			tr::lng_context_copy_link(tr::now),
			[=] {
				QGuiApplication::clipboard()->setText(deepLink);
				controller->showToast(
					tr::lng_background_link_copied(tr::now));
			});

		ContextMenu->addAction(
			FAlang::Translate(u"fa_share"_q),
			[=] {
				FastShareLink(controller, deepLink);
			});

		ContextMenu->popup(QCursor::pos());
		return base::EventFilterResult::Cancel;
	});
}

} // namespace Settings::FADeepLinkMenu
