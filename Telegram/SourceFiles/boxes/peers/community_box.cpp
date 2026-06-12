/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/peers/community_box.h"

#include "api/api_communities.h"
#include "apiwrap.h"
#include "boxes/peer_list_box.h"
#include "boxes/peers/add_to_community_box.h"
#include "boxes/peers/community_pending_requests_box.h"
#include "boxes/peers/manage_community_box.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_community.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_hardcoded.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
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

struct PartitionedChats {
	std::vector<not_null<PeerData*>> joined;
	std::vector<not_null<PeerData*>> viewable;
	std::vector<not_null<PeerData*>> requestable;
};

[[nodiscard]] PartitionedChats PartitionChats(
		not_null<Data::CommunityInfo*> info) {
	auto result = PartitionedChats();
	for (const auto &linked : info->linkedPeers()) {
		const auto channel = linked.peer->asChannel();
		if (channel && channel->amIn()) {
			result.joined.push_back(linked.peer);
		} else if (channel && channel->isPublic()) {
			result.viewable.push_back(linked.peer);
		} else {
			result.requestable.push_back(linked.peer);
		}
	}
	return result;
}

class ChatsController final : public PeerListController {
public:
	ChatsController(
		not_null<Main::Session*> session,
		rpl::producer<std::vector<not_null<PeerData*>>> chats,
		Fn<void(not_null<PeerData*>)> callback);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;

	[[nodiscard]] rpl::producer<int> countValue() const {
		return _count.value();
	}

private:
	const not_null<Main::Session*> _session;
	rpl::producer<std::vector<not_null<PeerData*>>> _chats;
	Fn<void(not_null<PeerData*>)> _callback;
	rpl::variable<int> _count = 0;

};

ChatsController::ChatsController(
	not_null<Main::Session*> session,
	rpl::producer<std::vector<not_null<PeerData*>>> chats,
	Fn<void(not_null<PeerData*>)> callback)
: _session(session)
, _chats(std::move(chats))
, _callback(std::move(callback)) {
}

Main::Session &ChatsController::session() const {
	return *_session;
}

void ChatsController::prepare() {
	std::move(
		_chats
	) | rpl::on_next([=](const std::vector<not_null<PeerData*>> &list) {
		while (delegate()->peerListFullRowsCount() > 0) {
			delegate()->peerListRemoveRow(
				delegate()->peerListRowAt(
					delegate()->peerListFullRowsCount() - 1));
		}
		for (const auto &peer : list) {
			auto row = std::make_unique<PeerListRow>(peer);
			const auto channel = peer->asChannel();
			if (channel && channel->membersCountKnown()) {
				row->setCustomStatus(tr::lng_chat_status_members(
					tr::now,
					lt_count_decimal,
					channel->membersCount()));
			}
			delegate()->peerListAppendRow(std::move(row));
		}
		delegate()->peerListRefreshRows();
		_count = int(list.size());
	}, lifetime());
}

void ChatsController::rowClicked(not_null<PeerListRow*> row) {
	_callback(row->peer());
}

void CommunityBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	box->setTitle(Info::Profile::NameValue(community));
	box->setWidth(st::boxWideWidth);

	const auto info = community->communityInfo();
	if (!info) {
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	const auto container = box->verticalLayout();

	if (community->canEditInformation()) {
		box->addTopButton(st::infoTopBarMenu, [=] {
			ShowManageCommunityBox(navigation, community);
		});
	}

	const auto toggle = Settings::AddButtonWithIcon(
		container,
		tr::lng_community_show_as_one(),
		st::settingsButtonNoIcon);
	toggle->toggleOn(Data::PeerFlagValue(
		community.get(),
		ChannelDataFlag::CommunityCollapsed));
	toggle->toggledChanges(
	) | rpl::filter([=](bool toggled) {
		const auto flags = community->flags();
		return toggled != ((flags & ChannelDataFlag::CommunityCollapsed)
			!= 0);
	}) | rpl::on_next([=](bool toggled) {
		community->session().api().communities().toggleCollapsedInDialogs(
			community,
			toggled);
	}, toggle->lifetime());
	Ui::AddSkip(container);
	Ui::AddDividerText(container, tr::lng_community_show_as_one_about());

	if (community->canManageLinkedPeers()) {
		const auto wrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();
		Ui::AddSkip(inner);
		auto count = Info::Profile::PendingRequestsCountValue(
			community
		) | rpl::start_spawning(wrap->lifetime());
		Settings::AddButtonWithIcon(
			inner,
			tr::lng_community_requests_button(
				lt_count,
				rpl::duplicate(count) | tr::to_count()),
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

	auto chats = info->linkedPeersValue(
	) | rpl::map([=] {
		return PartitionChats(info);
	}) | rpl::start_spawning(box->lifetime());

	const auto openChat = [=](not_null<PeerData*> peer) {
		navigation->showPeerHistory(
			peer,
			Window::SectionShow::Way::ClearStack,
			ShowAtUnreadMsgId);
	};

	const auto addSection = [&](
			rpl::producer<QString> title,
			rpl::producer<std::vector<not_null<PeerData*>>> list) {
		const auto wrap = container->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				container,
				object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();
		Ui::AddSkip(inner);
		Ui::AddSubsectionTitle(inner, std::move(title));

		class Delegate final : public PeerListContentDelegateSimple {
		public:
			explicit Delegate(std::shared_ptr<Main::SessionShow> show)
			: _show(std::move(show)) {
			}

			std::shared_ptr<Main::SessionShow> peerListUiShow() override {
				return _show;
			}

		private:
			const std::shared_ptr<Main::SessionShow> _show;

		};
		const auto delegate = inner->lifetime().make_state<Delegate>(
			Main::MakeSessionShow(box->uiShow(), &community->session()));
		const auto controller = inner->lifetime().make_state<
			ChatsController
		>(&community->session(), std::move(list), openChat);
		const auto content = inner->add(object_ptr<PeerListContent>(
			inner,
			controller));
		delegate->setContent(content);
		controller->setDelegate(delegate);

		wrap->toggleOn(
			controller->countValue() | rpl::map(rpl::mappers::_1 > 0),
			anim::type::instant);
		wrap->finishAnimating();
	};

	addSection(
		tr::lng_community_chats_joined(),
		rpl::duplicate(chats) | rpl::map([](const PartitionedChats &c) {
			return c.joined;
		}));
	addSection(
		tr::lng_community_chats_viewable(),
		rpl::duplicate(chats) | rpl::map([](const PartitionedChats &c) {
			return c.viewable;
		}));
	addSection(
		tr::lng_community_chats_requestable(),
		rpl::duplicate(chats) | rpl::map([](const PartitionedChats &c) {
			return c.requestable;
		}));

	const auto canAdd = community->canManageLinkedPeers()
		|| community->communityAnyoneCanAddPeers();
	if (canAdd) {
		box->addButton(tr::lng_community_add_chat(), [=] {
			ShowChooseChatToAddBox(navigation, community);
		});
	} else {
		box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
	}

	if (!community->wasFullUpdated()) {
		community->session().api().requestFullPeer(community);
	}
}

class ChooseChatController final : public PeerListController {
public:
	ChooseChatController(
		not_null<Main::Session*> session,
		not_null<ChannelData*> community,
		Fn<void(not_null<ChannelData*>)> callback);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;

private:
	const not_null<Main::Session*> _session;
	const not_null<ChannelData*> _community;
	Fn<void(not_null<ChannelData*>)> _callback;

};

ChooseChatController::ChooseChatController(
	not_null<Main::Session*> session,
	not_null<ChannelData*> community,
	Fn<void(not_null<ChannelData*>)> callback)
: _session(session)
, _community(community)
, _callback(std::move(callback)) {
}

Main::Session &ChooseChatController::session() const {
	return *_session;
}

void ChooseChatController::prepare() {
	delegate()->peerListSetTitle(tr::lng_community_add_chat());
	auto candidates = std::vector<not_null<ChannelData*>>();
	_session->data().enumerateGroups([&](not_null<PeerData*> peer) {
		const auto channel = peer->asChannel();
		if (channel
			&& channel->isMegagroup()
			&& channel->amCreator()
			&& !channel->isForbidden()
			&& !channel->isMonoforum()
			&& !channel->linkedCommunityId()) {
			candidates.push_back(channel);
		}
	});
	ranges::sort(candidates, [](
			not_null<ChannelData*> a,
			not_null<ChannelData*> b) {
		return a->name().compare(b->name(), Qt::CaseInsensitive) < 0;
	});
	for (const auto &channel : candidates) {
		if (delegate()->peerListFindRow(channel->id.value)) {
			continue;
		}
		auto row = std::make_unique<PeerListRow>(channel);
		if (channel->membersCountKnown()) {
			row->setCustomStatus(tr::lng_chat_status_members(
				tr::now,
				lt_count_decimal,
				channel->membersCount()));
		}
		delegate()->peerListAppendRow(std::move(row));
	}
	delegate()->peerListRefreshRows();
}

void ChooseChatController::rowClicked(not_null<PeerListRow*> row) {
	if (const auto channel = row->peer()->asChannel()) {
		_callback(channel);
	}
}

} // namespace

void ShowCommunityBox(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	navigation->uiShow()->showBox(
		Box(CommunityBox, navigation, community));
}

void BanFromCommunityWithWarning(
		std::shared_ptr<Ui::Show> show,
		not_null<ChannelData*> community,
		not_null<PeerData*> participant) {
	const auto session = &community->session();
	const auto ban = [=] {
		session->api().communities().toggleParticipantBanned(
			community,
			participant,
			false,
			[=] {
				show->showToast(tr::lng_community_ban_done(
					tr::now,
					lt_user,
					participant->shortName()));
			},
			[=](const QString &error) {
				show->showToast(error.isEmpty()
					? Lang::Hard::ServerError()
					: error);
			});
	};
	session->api().communities().requestParticipantJoinedChats(
		community,
		participant,
		[=](Api::CommunityParticipantJoinedChats chats) {
			if (chats.creatorChats.empty()) {
				ban();
				return;
			}
			const auto creatorChats = chats.creatorChats;
			show->showBox(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_community_ban_warning_title());
				box->addRow(object_ptr<Ui::FlatLabel>(
					box,
					rpl::single(tr::lng_community_ban_warning(
						tr::now,
						lt_count,
						int(creatorChats.size()),
						lt_user,
						tr::bold(participant->shortName()),
						tr::marked)),
					st::boxLabel));

				const auto container = box->verticalLayout();
				Ui::AddSkip(container);

				class Delegate final : public PeerListContentDelegateSimple {
				public:
					explicit Delegate(std::shared_ptr<Main::SessionShow> show)
					: _show(std::move(show)) {
					}

					std::shared_ptr<Main::SessionShow> peerListUiShow(
							) override {
						return _show;
					}

				private:
					const std::shared_ptr<Main::SessionShow> _show;

				};
				const auto delegate
					= container->lifetime().make_state<Delegate>(
						Main::MakeSessionShow(show, session));
				const auto controller = container->lifetime().make_state<
					ChatsController
				>(
					session,
					rpl::single(creatorChats),
					[](not_null<PeerData*>) {});
				const auto content = container->add(
					object_ptr<PeerListContent>(container, controller));
				delegate->setContent(content);
				controller->setDelegate(delegate);

				box->addButton(tr::lng_community_ban_button(), [=] {
					box->closeBox();
					ban();
				}, st::attentionBoxButton);
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
			}));
		});
}

void ShowChooseChatToAddBox(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	const auto choose = [=](not_null<ChannelData*> group) {
		ShowAddPeerToCommunity(navigation, community, group);
	};
	auto controller = std::make_unique<ChooseChatController>(
		&community->session(),
		community,
		choose);
	const auto init = [=](not_null<PeerListBox*> box) {
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	navigation->uiShow()->showBox(Box<PeerListBox>(
		std::move(controller),
		init));
}
