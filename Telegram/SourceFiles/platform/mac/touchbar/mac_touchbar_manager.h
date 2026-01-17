/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#import <AppKit/NSTouchBar.h>

namespace Main {
class Domain;
} // namespace Main

namespace Window {
class Controller;
} // namespace Window

namespace Ui {
struct MarkdownEnabledState;
} // namespace Ui

API_AVAILABLE(macos(10.12.2))
@interface RootTouchBar : NSTouchBar<NSTouchBarDelegate>
- (id)init:(rpl::producer<Ui::MarkdownEnabledState>)markdownState
	controller:(not_null<Window::Controller*>)controller
	domain:(not_null<Main::Domain*>)domain;
@end
