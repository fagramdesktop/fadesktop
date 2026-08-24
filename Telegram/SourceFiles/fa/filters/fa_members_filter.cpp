/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/filters/fa_members_filter.h"

#include "boxes/peers/edit_participants_box.h"
#include "boxes/peer_list_box.h"
#include "main/session/session_show.h"
#include "data/data_user.h"
#include "data/data_peer.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/layers/generic_box.h"
#include "ui/wrap/vertical_layout.h"
#include "settings/settings_common.h"
#include "lang/lang_keys.h"
#include "fa_lang_auto.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_settings.h"

namespace Fa::MembersFilter {

bool PassesFilter(
		UserData *user,
		Type filter,
		const ParticipantsAdditionalData &additional) {
	if (filter == Type::All) {
		return true;
	} else if (!user) {
		return false;
	}
	switch (filter) {
	case Type::All:
		return true;
	case Type::Administrators:
		return additional.isCreator(user)
			|| additional.adminRights(user).has_value();
	case Type::Bots:
		return user->isBot();
	case Type::DeletedAccount:
		return user->isInaccessible()
			|| (user->flags() & UserDataFlag::Deleted)
			|| (user->name().trimmed().isEmpty() && user->username().isEmpty() && !user->isBot());
	}
	return true;
}

QString FilterLabel(Type filter) {
	switch (filter) {
	case Type::All:
		return fatr::fa_members_filter_all(fatr::now);
	case Type::Administrators:
		return fatr::fa_members_filter_admins(fatr::now);
	case Type::Bots:
		return fatr::fa_members_filter_bots(fatr::now);
	case Type::DeletedAccount:
		return fatr::fa_members_filter_deleted(fatr::now);
	}
	return QString();
}

void ShowFilterBox(
		not_null<Ui::GenericBox*> box,
		Type current,
		Fn<void(Type)> onSelected) {
	box->setTitle(rpl::single(fatr::fa_members_filter(fatr::now)));

	const auto group = std::make_shared<Ui::RadioenumGroup<Type>>(current);

	const auto addOption = [&](Type type) {
		box->addRow(
			object_ptr<Ui::Radioenum<Type>>(
				box->verticalLayout(),
				group,
				type,
				FilterLabel(type),
				st::settingsSendType),
			st::settingsSendTypePadding);
	};

	addOption(Type::All);
	addOption(Type::Administrators);
	addOption(Type::Bots);
	addOption(Type::DeletedAccount);

	box->addButton(tr::lng_box_ok(), [=] {
		const auto selected = group->current();
		box->closeBox();
		if (onSelected) {
			onSelected(selected);
		}
	});
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

void Setup(
		not_null<ParticipantsBoxController*> controller,
		not_null<PeerListDelegate*> delegate,
		not_null<PeerData*> peer,
		const ParticipantsAdditionalData &additional) {
	if (!peer->isChat() && !peer->isMegagroup()) {
		return;
	}

	auto wrap = object_ptr<Ui::VerticalLayout>((QWidget*)nullptr);
	const auto currentFilter = std::make_shared<Type>(controller->membersFilter());
	const auto labelStream = wrap->lifetime().make_state<rpl::event_stream<QString>>();

	const auto button = Settings::AddButtonWithLabel(
		wrap.data(),
		rpl::single(fatr::fa_members_filter(fatr::now)),
		labelStream->events_starting_with(FilterLabel(*currentFilter)),
		st::infoSharedMediaButton);

	button->setClickedCallback([=, &additional] {
		const auto show = delegate->peerListUiShow();
		if (!show) {
			return;
		}
		show->showBox(Box(
			ShowFilterBox,
			*currentFilter,
			[=, &additional](Type selected) {
				*currentFilter = selected;
				labelStream->fire(FilterLabel(selected));
				controller->setMembersFilter(selected);
			}));
	});

	delegate->peerListSetAboveWidget(std::move(wrap));
}

} // namespace Fa::MembersFilter
