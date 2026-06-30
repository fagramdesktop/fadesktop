/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_community_requestable_list.h"

#include "apiwrap.h"
#include "boxes/peer_list_box.h"
#include "data/data_channel.h"
#include "data/data_community.h"
#include "lang/lang_keys.h"
#include "main/session/session_show.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/qt_object_factory.h"
#include "window/window_session_controller.h"

#include "styles/style_dialogs.h"

namespace Dialogs {
namespace {

[[nodiscard]] std::unique_ptr<PeerListRow> MakeCommunityChatRow(
		not_null<PeerData*> peer) {
	auto row = std::make_unique<PeerListRow>(peer);
	const auto channel = peer->asChannel();
	if (channel && channel->membersCountKnown()) {
		row->setCustomStatus(tr::lng_chat_status_members(
			tr::now,
			lt_count_decimal,
			channel->membersCount()));
	}
	return row;
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
	setStyleOverrides(&st::communityRequestableList);
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
			delegate()->peerListAppendRow(MakeCommunityChatRow(peer));
		}
		delegate()->peerListRefreshRows();
		_count = int(list.size());
	}, lifetime());
}

void ChatsController::rowClicked(not_null<PeerListRow*> row) {
	_callback(row->peer());
}

} // namespace

CommunityRequestableList::CommunityRequestableList(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<Data::CommunityInfo*> community)
: RpWidget(parent)
, _controller(controller) {
	auto requestable = community->linkedPeersValue(
	) | rpl::map([=] {
		auto result = std::vector<not_null<PeerData*>>();
		for (const auto &linked : community->linkedPeers()) {
			const auto channel = linked.peer->asChannel();
			if (channel && channel->amIn()) {
				continue;
			} else if (Data::IsCommunityChatViewable(linked)) {
				continue;
			}
			result.push_back(linked.peer);
		}
		return result;
	}) | rpl::start_spawning(lifetime());

	const auto openChat = [=](not_null<PeerData*> peer) {
		const auto channel = peer->asChannel();
		// A hidden chat can only be joined by its creator; a visible chat
		// can be joined by anyone. Everything else is inaccessible.
		const auto joinable = channel
			&& (!community->isHidden(peer) || channel->amCreator());
		if (!joinable) {
			_controller->showToast(
				tr::lng_community_hidden_not_accessible(tr::now));
			return;
		}
		const auto join = [=](Fn<void()> close) {
			_controller->session().api().joinChannel(channel);
			close();
		};
		_controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_community_join_sure(
				tr::now,
				lt_group,
				tr::bold(peer->name()),
				tr::marked),
			.confirmed = join,
			.confirmText = (channel->isMegagroup()
				? tr::lng_profile_join_group(tr::now)
				: tr::lng_profile_join_channel(tr::now)),
		}));
	};

	const auto delegate = lifetime().make_state<
		PeerListContentDelegateShow
	>(controller->uiShow());
	const auto chatsController = lifetime().make_state<ChatsController>(
		&controller->session(),
		rpl::duplicate(requestable),
		openChat);
	_content = Ui::CreateChild<PeerListContent>(this, chatsController);
	delegate->setContent(_content);
	chatsController->setDelegate(delegate);

	_count = chatsController->countValue();
	_count.value(
	) | rpl::on_next([=](int count) {
		setVisible(count > 0);
		resizeToWidth(width());
	}, lifetime());

	_content->heightValue(
	) | rpl::on_next([=] {
		resizeToWidth(width());
	}, lifetime());
}

CommunityRequestableList::~CommunityRequestableList() = default;

int CommunityRequestableList::resizeGetHeight(int newWidth) {
	_content->resizeToWidth(newWidth);
	_content->moveToLeft(0, 0);
	return _content->height();
}

rpl::producer<int> CommunityRequestableList::countValue() const {
	return _count.value();
}

} // namespace Dialogs
