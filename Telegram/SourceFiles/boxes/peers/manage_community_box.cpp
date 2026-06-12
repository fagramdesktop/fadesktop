/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/peers/manage_community_box.h"

#include "apiwrap.h"
#include "boxes/peers/community_pending_requests_box.h"
#include "boxes/peers/edit_participants_box.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace {

[[nodiscard]] rpl::producer<QString> PositiveNumberString(
		rpl::producer<int> value) {
	return std::move(value) | rpl::map([](int count) {
		return count ? QString::number(count) : QString();
	});
}

void SaveCommunityTitle(
		not_null<ChannelData*> community,
		const QString &title) {
	const auto api = &community->session().api();
	api->request(MTPchannels_EditTitle(
		community->inputChannel(),
		MTP_string(title)
	)).done([=](const MTPUpdates &result) {
		api->applyUpdates(result);
	}).fail([=](const MTP::Error &error) {
		if (error.type() == u"CHAT_NOT_MODIFIED"_q) {
			community->setName(title, QString());
		}
	}).send();
}

void SaveCommunityWhoCanAddChats(
		not_null<ChannelData*> community,
		bool onlyAdmins) {
	const auto rights = onlyAdmins
		? ChatRestrictions(ChatRestriction::ManageLinkedPeers)
		: ChatRestrictions();
	const auto api = &community->session().api();
	api->request(MTPmessages_EditChatDefaultBannedRights(
		community->input(),
		RestrictionsToMTP({ rights, 0 })
	)).done([=](const MTPUpdates &result) {
		api->applyUpdates(result);
	}).fail([=](const MTP::Error &error) {
		if (error.type() == u"CHAT_NOT_MODIFIED"_q) {
			community->setDefaultRestrictions(rights);
		}
	}).send();
}

void DeleteCommunityWithConfirmation(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	const auto show = navigation->uiShow();
	const auto sure = [=](Fn<void()> &&close) {
		close();
		const auto session = &community->session();
		session->api().request(MTPchannels_DeleteChannel(
			community->inputChannel()
		)).done([=](const MTPUpdates &result) {
			session->api().applyUpdates(result);
		}).fail([=](const MTP::Error &error) {
			if (error.type() != u"CHANNEL_NOT_MODIFIED"_q) {
				show->showToast(error.type());
			}
		}).send();
		show->hideLayer();
	};
	show->showBox(Ui::MakeConfirmBox({
		.text = tr::lng_community_delete_sure(),
		.confirmed = sure,
		.confirmText = tr::lng_box_delete(),
		.confirmStyle = &st::attentionBoxButton,
		.title = tr::lng_community_delete(),
	}));
}

void ManageCommunityBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	box->setTitle(tr::lng_community_title());

	const auto canEdit = community->canEditInformation();

	const auto container = box->verticalLayout();

	Ui::AddSubsectionTitle(container, tr::lng_community_manage_name());
	const auto field = container->add(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			tr::lng_community_create_name(),
			community->name()),
		st::boxRowPadding);
	if (!canEdit) {
		field->setEnabled(false);
	}

	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);

	Ui::AddSubsectionTitle(container, tr::lng_community_manage_who_add());
	const auto whoCanAdd = std::make_shared<Ui::RadiobuttonGroup>(
		community->communityAnyoneCanAddPeers() ? 0 : 1);
	const auto addOption = [&](
			int value,
			const QString &label,
			rpl::producer<QString> about) {
		const auto wrap = container->add(
			object_ptr<Ui::VerticalLayout>(container),
			st::boxRowPadding);
		const auto radio = wrap->add(object_ptr<Ui::Radiobutton>(
			wrap,
			whoCanAdd,
			value,
			label,
			st::defaultBoxCheckbox));
		wrap->add(
			object_ptr<Ui::FlatLabel>(
				wrap,
				std::move(about),
				st::editPeerPrivacyLabel),
			st::editPeerPreHistoryLabelMargins);
		if (!canEdit) {
			radio->setDisabled(true);
		}
	};
	addOption(
		0,
		tr::lng_community_manage_all_members(tr::now),
		tr::lng_community_manage_all_members_about());
	addOption(
		1,
		tr::lng_community_manage_only_admins(tr::now),
		tr::lng_community_manage_only_admins_about());

	Ui::AddSkip(container);
	Ui::AddDivider(container);
	Ui::AddSkip(container);

	Settings::AddButtonWithLabel(
		container,
		tr::lng_manage_peer_administrators(),
		PositiveNumberString(Info::Profile::AdminsCountValue(community)),
		st::settingsButton,
		{ &st::menuIconAdmin }
	)->addClickHandler([=] {
		ParticipantsBoxController::Start(
			navigation,
			community,
			ParticipantsBoxController::Role::Admins);
	});
	if (community->canManageLinkedPeers()) {
		const auto wrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		auto count = Info::Profile::PendingRequestsCountValue(
			community
		) | rpl::start_spawning(wrap->lifetime());
		Settings::AddButtonWithLabel(
			wrap->entity(),
			tr::lng_community_requests_title(),
			PositiveNumberString(rpl::duplicate(count)),
			st::settingsButton,
			{ &st::menuIconInvite }
		)->addClickHandler([=] {
			ShowCommunityPendingRequestsBox(navigation, community);
		});
		wrap->toggleOn(
			std::move(count) | rpl::map(rpl::mappers::_1 > 0),
			anim::type::instant);
		wrap->finishAnimating();
	}
	Settings::AddButtonWithLabel(
		container,
		tr::lng_manage_peer_removed_users(),
		PositiveNumberString(Info::Profile::KickedCountValue(community)),
		st::settingsButton,
		{ &st::menuIconRemove }
	)->addClickHandler([=] {
		ParticipantsBoxController::Start(
			navigation,
			community,
			ParticipantsBoxController::Role::Kicked);
	});

	if (community->canDelete()) {
		Ui::AddSkip(container);
		Ui::AddDivider(container);
		Ui::AddSkip(container);
		Settings::AddButtonWithIcon(
			container,
			tr::lng_community_delete(),
			st::settingsAttentionButtonWithIcon,
			{ &st::menuIconDeleteAttention }
		)->addClickHandler([=] {
			DeleteCommunityWithConfirmation(navigation, community);
		});
	}

	if (canEdit) {
		box->addButton(tr::lng_settings_save(), [=] {
			const auto title = field->getLastText().trimmed();
			if (title.isEmpty()) {
				field->showError();
				return;
			}
			if (title != community->name()) {
				SaveCommunityTitle(community, title);
			}
			const auto onlyAdmins = (whoCanAdd->current() == 1);
			const auto wasOnlyAdmins
				= !community->communityAnyoneCanAddPeers();
			if (onlyAdmins != wasOnlyAdmins) {
				SaveCommunityWhoCanAddChats(community, onlyAdmins);
			}
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	} else {
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}

	if (!community->wasFullUpdated()) {
		community->session().api().requestFullPeer(community);
	}
}

} // namespace

void ShowManageCommunityBox(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	navigation->uiShow()->showBox(
		Box(ManageCommunityBox, navigation, community));
}
