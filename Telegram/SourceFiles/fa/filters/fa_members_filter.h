/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/basic_types.h"

class PeerData;
class UserData;
class PeerListDelegate;
class ParticipantsBoxController;
class ParticipantsAdditionalData;

namespace Ui {
class GenericBox;
} // namespace Ui

namespace Fa::MembersFilter {

enum class Type {
	All,
	Administrators,
	Bots,
	DeletedAccount,
};

[[nodiscard]] bool PassesFilter(
	UserData *user,
	Type filter,
	const ParticipantsAdditionalData &additional);

[[nodiscard]] QString FilterLabel(Type filter);

void Setup(
	not_null<ParticipantsBoxController*> controller,
	not_null<PeerListDelegate*> delegate,
	not_null<PeerData*> peer,
	const ParticipantsAdditionalData &additional);

void ShowFilterBox(
	not_null<Ui::GenericBox*> box,
	Type current,
	Fn<void(Type)> onSelected);

} // namespace Fa::MembersFilter
