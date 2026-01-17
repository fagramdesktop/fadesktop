/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "media/view/media_view_playback_controls.h"
#include "media/view/media_view_overlay_widget.h"

namespace TouchBar {

void SetupMediaViewTouchBar(
	WId winId,
	not_null<Media::View::PlaybackControls::Delegate*> controlsDelegate,
	rpl::producer<Media::Player::TrackState> trackState,
	rpl::producer<Media::View::OverlayWidget::TouchBarItemType> display,
	rpl::producer<bool> fullscreenToggled);

} // namespace TouchBar
