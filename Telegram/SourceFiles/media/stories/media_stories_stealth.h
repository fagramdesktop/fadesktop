/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

namespace style {
struct StealthBoxStyle;
} // namespace style

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Media::Stories {

struct StealthModeDescriptor {
	Fn<void()> onActivated = nullptr;
	const style::StealthBoxStyle *st = nullptr;
};

void SetupStealthMode(
	std::shared_ptr<ChatHelpers::Show> show,
	StealthModeDescriptor descriptor = {});

[[nodiscard]] QString TimeLeftText(int left);

} // namespace Media::Stories
