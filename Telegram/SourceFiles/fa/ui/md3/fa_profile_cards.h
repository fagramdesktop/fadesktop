/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "info/profile/info_profile_section_stack.h"

class PeerData;
class UserData;

namespace Data {
class ForumTopic;
} // namespace Data

namespace Info {
class Controller;
} // namespace Info

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace FA::Ui {

[[nodiscard]] ::Info::Profile::Section MakeLastFmCard(
	not_null<::Info::Controller*> controller,
	not_null<UserData*> user,
	not_null<::Ui::VerticalLayout*> parent);

[[nodiscard]] ::Info::Profile::Section MakeProfileInfo(
	not_null<::Info::Controller*> controller,
	not_null<PeerData*> peer,
	const Data::ForumTopic *topic,
	not_null<::Ui::VerticalLayout*> parent);

} // namespace FA::Ui
