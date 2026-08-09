// This is the source code of FAgram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "fa/settings/fa_settings.h"
#include "translate_provider.h"

#include <memory>

namespace Main {
class Session;
} // namespace Main

namespace Ui {

[[nodiscard]] std::unique_ptr<TranslateProvider> CreateFaTranslateProvider(
	not_null<Main::Session*> session,
	TranslationProvider provider);

} // namespace Ui
