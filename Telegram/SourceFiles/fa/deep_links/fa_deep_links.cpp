/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/deep_links/fa_deep_links.h"

#include "core/deep_links/deep_links_router.h"
#include "core/deep_links/deep_links_types.h"
#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/settings_menu/sections/fa_general.h"
#include "fa/settings_menu/sections/fa_appearance.h"
#include "fa/settings_menu/sections/fa_chats.h"
#include "fa/settings_menu/sections/fa_context_menu.h"
#include "fa/settings_menu/sections/fa_logs.h"
#include "window/window_session_controller.h"

namespace Core::DeepLinks {
namespace {

struct FASectionMapping {
	QString pathPrefix;
	Settings::Type sectionId;
};

[[nodiscard]] const std::vector<FASectionMapping> &SectionMappings() {
	static const auto result = std::vector<FASectionMapping>{
		{ u"general"_q, Settings::FAGeneral::Id() },
		{ u"appearance"_q, Settings::FAAppearance::Id() },
		{ u"chats"_q, Settings::FAChats::Id() },
		{ u"context-menu"_q, Settings::FAContextMenu::Id() },
		{ u"logs"_q, Settings::FALogs::Id() },
	};
	return result;
}

Result HandleFASettingsWithSuffix(
		const Context &ctx,
		const QString &pathPrefix,
		Settings::Type sectionId) {
	if (!ctx.controller) {
		return Result::NeedsAuth;
	}
	const auto fullPath = ctx.path;
	const auto expectedPrefix = u"settings/"_q + pathPrefix + '/';
	if (fullPath.startsWith(expectedPrefix)) {
		const auto controlSuffix = fullPath.mid(expectedPrefix.size());
		if (!controlSuffix.isEmpty()) {
			const auto controlId = u"fa/"_q
				+ pathPrefix
				+ '/'
				+ controlSuffix;
			ctx.controller->setHighlightControlId(controlId);
		}
	}
	ctx.controller->showSettings(sectionId);
	return Result::Handled;
}

} // namespace

QString FASettingsDeepLink(const QString &controlId) {
	if (!controlId.startsWith(u"fa/"_q)) {
		return QString();
	}
	const auto suffix = controlId.mid(3);
	return u"tg://fa/settings/"_q + suffix;
}

void RegisterFAHandlers(Router &router) {
	router.add(u"fa"_q, {
		.path = QString(),
		.action = SettingsSection{ Settings::FA::Id() },
	});

	router.add(u"fa"_q, {
		.path = u"settings"_q,
		.action = SettingsSection{ Settings::FA::Id() },
	});

	for (const auto &mapping : SectionMappings()) {
		const auto prefix = mapping.pathPrefix;
		const auto sectionId = mapping.sectionId;

		router.add(u"fa"_q, {
			.path = u"settings/"_q + prefix,
			.action = CodeBlock{ [=](const Context &ctx) {
				return HandleFASettingsWithSuffix(ctx, prefix, sectionId);
			}},
		});
	}
}

} // namespace Core::DeepLinks
