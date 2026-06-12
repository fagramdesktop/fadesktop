/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_communities.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"

namespace Api {
namespace {

[[nodiscard]] ChannelData *ExtractCreatedCommunity(
		not_null<Main::Session*> session,
		const MTPUpdates &updates) {
	const auto chats = updates.match([](const MTPDupdates &data) {
		return &data.vchats().v;
	}, [](const MTPDupdatesCombined &data) {
		return &data.vchats().v;
	}, [](const auto &) -> const QVector<MTPChat>* {
		return nullptr;
	});
	if (!chats) {
		LOG(("API Error: unexpected update cons %1 "
			"(Communities::create)").arg(updates.type()));
		return nullptr;
	}
	for (const auto &chat : *chats) {
		if (chat.type() == mtpc_community) {
			const auto channel = session->data().channelLoaded(
				chat.c_community().vid().v);
			if (channel && channel->isCommunity()) {
				return channel;
			}
		}
	}
	return nullptr;
}

} // namespace

Communities::Communities(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
}

void Communities::create(
		const QString &title,
		const QString &about,
		not_null<PeerData*> peer,
		Fn<void(not_null<ChannelData*>)> done,
		Fn<void(const QString &)> fail) {
	using Flag = MTPcommunities_Create::Flag;
	_api.request(MTPcommunities_Create(
		MTP_flags(about.isEmpty() ? Flag() : Flag::f_about),
		MTP_string(title),
		MTP_string(about),
		peer->input()
	)).done([=](const MTPUpdates &result) {
		_session->api().applyUpdates(result);
		if (const auto community = ExtractCreatedCommunity(
				_session,
				result)) {
			_session->api().requestFullPeer(community);
			if (done) {
				done(community);
			}
		} else if (fail) {
			fail(QString());
		}
	}).fail([=](const MTP::Error &error) {
		if (fail) {
			fail(error.type());
		}
	}).send();
}

void Communities::addPeerLink(
		not_null<ChannelData*> community,
		not_null<PeerData*> peer,
		bool visible,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	togglePeerLink(
		community,
		peer,
		visible,
		false,
		std::move(done),
		std::move(fail));
}

void Communities::removePeerLink(
		not_null<ChannelData*> community,
		not_null<PeerData*> peer,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	togglePeerLink(
		community,
		peer,
		std::nullopt,
		true,
		std::move(done),
		std::move(fail));
}

void Communities::togglePeerLink(
		not_null<ChannelData*> community,
		not_null<PeerData*> peer,
		std::optional<bool> visible,
		bool remove,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	using Flag = MTPcommunities_TogglePeerLink::Flag;
	const auto flags = (remove ? Flag::f_deleted : Flag())
		| (visible.value_or(false) ? Flag::f_visible : Flag())
		| ((visible && !*visible) ? Flag::f_hidden : Flag());
	_api.request(MTPcommunities_TogglePeerLink(
		MTP_flags(flags),
		community->inputChannel(),
		peer->input()
	)).done([=] {
		_session->api().requestFullPeer(community);
		if (done) {
			done();
		}
	}).fail([=](const MTP::Error &error) {
		if (error.type() == kCommunityRequestCreated.utf16()) {
			_session->api().requestFullPeer(community);
		}
		if (fail) {
			fail(error.type());
		}
	}).send();
}

void Communities::requestJoinedCommunities(
		Fn<void(const std::vector<not_null<ChannelData*>> &)> done) {
	if (_joinedRequestId) {
		_api.request(_joinedRequestId).cancel();
	}
	_joinedRequestId = _api.request(MTPcommunities_GetJoinedCommunities(
	)).done([=](const MTPmessages_Chats &result) {
		_joinedRequestId = 0;
		auto list = std::vector<not_null<ChannelData*>>();
		const auto &chats = result.match([](const auto &data) {
			return data.vchats().v;
		});
		list.reserve(chats.size());
		for (const auto &chat : chats) {
			const auto peer = _session->data().processChat(chat);
			if (const auto channel = peer->asChannel()) {
				if (channel->isCommunity() && !channel->isForbidden()) {
					list.push_back(channel);
				}
			}
		}
		if (done) {
			done(list);
		}
	}).fail([=] {
		_joinedRequestId = 0;
		if (done) {
			done({});
		}
	}).send();
}

void Communities::toggleCollapsedInDialogs(
		not_null<ChannelData*> community,
		bool collapsed) {
	if (!_collapseRequests.emplace(community).second) {
		return;
	}
	using Flag = MTPcommunities_ToggleCommunityCollapsedInDialogs::Flag;
	_api.request(MTPcommunities_ToggleCommunityCollapsedInDialogs(
		MTP_flags(collapsed ? Flag::f_collapsed : Flag()),
		community->inputChannel()
	)).done([=](const MTPUpdates &result) {
		_collapseRequests.remove(community);
		_session->api().applyUpdates(result);
	}).fail([=] {
		_collapseRequests.remove(community);
	}).send();
}

void Communities::requestPeerLinkRequests(
		not_null<ChannelData*> community,
		const QString &offset,
		int limit,
		Fn<void(CommunityPeerRequestsSlice)> done) {
	_api.request(MTPcommunities_GetPeerLinkRequests(
		community->inputChannel(),
		MTP_string(offset),
		MTP_int(limit)
	)).done([=](const MTPcommunities_PeerLinkRequests &result) {
		const auto &data = result.data();
		auto &owner = _session->data();
		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());
		auto slice = CommunityPeerRequestsSlice();
		slice.totalCount = data.vtotal_count().v;
		slice.nextOffset = qs(data.vnext_offset().value_or_empty());
		slice.list.reserve(data.vrequests().v.size());
		for (const auto &request : data.vrequests().v) {
			const auto &fields = request.data();
			slice.list.push_back({
				.peer = owner.peer(peerFromMTP(fields.vpeer())),
				.requestedBy = owner.userLoaded(
					UserId(fields.vrequested_by())),
				.date = fields.vdate().v,
				.visible = fields.is_visible(),
			});
		}
		if (done) {
			done(std::move(slice));
		}
	}).fail([=] {
		if (done) {
			done({});
		}
	}).send();
}

void Communities::togglePeerLinkRequestApproval(
		not_null<ChannelData*> community,
		not_null<PeerData*> peer,
		bool reject,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	using Flag = MTPcommunities_TogglePeerLinkRequestApproval::Flag;
	_api.request(MTPcommunities_TogglePeerLinkRequestApproval(
		MTP_flags(reject ? Flag::f_reject : Flag()),
		community->inputChannel(),
		peer->input()
	)).done([=] {
		_session->api().requestFullPeer(community);
		if (done) {
			done();
		}
	}).fail([=](const MTP::Error &error) {
		if (fail) {
			fail(error.type());
		}
	}).send();
}

void Communities::toggleAllPeerLinkRequestApproval(
		not_null<ChannelData*> community,
		bool reject,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	using Flag = MTPcommunities_ToggleAllPeerLinkRequestApproval::Flag;
	_api.request(MTPcommunities_ToggleAllPeerLinkRequestApproval(
		MTP_flags(reject ? Flag::f_reject : Flag()),
		community->inputChannel()
	)).done([=] {
		_session->api().requestFullPeer(community);
		if (done) {
			done();
		}
	}).fail([=](const MTP::Error &error) {
		if (fail) {
			fail(error.type());
		}
	}).send();
}

void Communities::toggleParticipantBanned(
		not_null<ChannelData*> community,
		not_null<PeerData*> participant,
		bool unban,
		Fn<void()> done,
		Fn<void(const QString &)> fail) {
	using Flag = MTPcommunities_ToggleParticipantBanned::Flag;
	_api.request(MTPcommunities_ToggleParticipantBanned(
		MTP_flags(unban ? Flag::f_unban : Flag()),
		community->inputChannel(),
		participant->input()
	)).done([=] {
		_session->api().requestFullPeer(community);
		if (done) {
			done();
		}
	}).fail([=](const MTP::Error &error) {
		if (fail) {
			fail(error.type());
		}
	}).send();
}

void Communities::requestParticipantJoinedChats(
		not_null<ChannelData*> community,
		not_null<PeerData*> participant,
		Fn<void(CommunityParticipantJoinedChats)> done) {
	_api.request(MTPcommunities_GetParticipantJoinedChats(
		community->inputChannel(),
		participant->input()
	)).done([=](const MTPcommunities_ParticipantJoinedChats &result) {
		const auto &data = result.data();
		auto &owner = _session->data();
		owner.processUsers(data.vusers());
		owner.processChats(data.vchats());
		auto parsed = CommunityParticipantJoinedChats();
		const auto append = [&](
				std::vector<not_null<PeerData*>> &to,
				const QVector<MTPlong> &ids) {
			to.reserve(ids.size());
			for (const auto &id : ids) {
				if (const auto chat = owner.channelLoaded(id.v)) {
					to.push_back(chat);
				}
			}
		};
		append(parsed.creatorChats, data.vcreator_chat_ids().v);
		append(parsed.joinedChats, data.vjoined_chat_ids().v);
		if (done) {
			done(std::move(parsed));
		}
	}).fail([=] {
		if (done) {
			done({});
		}
	}).send();
}

} // namespace Api
