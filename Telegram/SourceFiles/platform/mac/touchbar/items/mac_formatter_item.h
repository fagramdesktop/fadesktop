/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#import <AppKit/NSPopoverTouchBarItem.h>
#import <AppKit/NSTouchBar.h>

API_AVAILABLE(macos(10.12.2))
@interface TextFormatPopover : NSPopoverTouchBarItem
- (id)init:(NSTouchBarItemIdentifier)identifier;
@end
