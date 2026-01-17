/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class PeerData;

using Participants = std::vector<not_null<PeerData*>>;

namespace Ui {

class Checkbox;
class VerticalLayout;

struct ExpandablePeerListController final {
	struct Data final {
		rpl::producer<base::flat_map<PeerId, int>> messagesCounts = nullptr;
		Participants participants;
		std::vector<PeerId> checked;
		bool skipSingle = false;
		bool hideRightButton = false;
		bool checkTopOnAllInner = false;
		bool bold = true;
	};
	ExpandablePeerListController(Data &&data) : data(std::move(data)) {
	}
	const Data data;
	rpl::event_stream<bool> toggleRequestsFromTop;
	rpl::event_stream<bool> toggleRequestsFromInner;
	rpl::event_stream<bool> checkAllRequests;
	Fn<Participants()> collectRequests;
};

void AddExpandablePeerList(
	not_null<Ui::Checkbox*> checkbox,
	not_null<ExpandablePeerListController*> controller,
	not_null<Ui::VerticalLayout*> inner);

} // namespace Ui
