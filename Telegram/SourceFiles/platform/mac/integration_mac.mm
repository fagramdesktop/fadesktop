/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "platform/mac/integration_mac.h"

#include "platform/platform_integration.h"

namespace Platform {
namespace {

class MacIntegration final : public Integration {
};

} // namespace

std::unique_ptr<Integration> CreateIntegration() {
	return std::make_unique<MacIntegration>();
}

} // namespace Platform
