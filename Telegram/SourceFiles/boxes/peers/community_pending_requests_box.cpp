/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/peers/community_pending_requests_box.h"

#include "api/api_communities.h"
#include "apiwrap.h"
#include "boxes/peer_list_box.h"
#include "boxes/peers/manage_community_box.h"
#include "data/data_channel.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/effects/ripple_animation.h"
#include "ui/painter.h"
#include "ui/round_rect.h"
#include "ui/text/text_utilities.h"
#include "ui/vertical_list.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace {

constexpr auto kPerPage = 100;
constexpr auto kAcceptButton = 1;
constexpr auto kRejectButton = 2;

class RowDelegate {
public:
	[[nodiscard]] virtual QSize rowAcceptButtonSize() = 0;
	[[nodiscard]] virtual QSize rowRejectButtonSize() = 0;
	virtual void rowPaintAccept(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) = 0;
	virtual void rowPaintReject(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) = 0;
};

class Row final : public PeerListRow {
public:
	Row(
		not_null<RowDelegate*> delegate,
		const Api::CommunityPeerRequest &request);

	int elementsCount() const override;
	QRect elementGeometry(int element, int outerWidth) const override;
	bool elementDisabled(int element) const override;
	bool elementOnlySelect(int element) const override;
	void elementAddRipple(
		int element,
		QPoint point,
		Fn<void()> updateCallback) override;
	void elementsStopLastRipple() override;
	void elementsPaint(
		Painter &p,
		int outerWidth,
		bool selected,
		int selectedElement) override;

private:
	const not_null<RowDelegate*> _delegate;
	std::unique_ptr<Ui::RippleAnimation> _acceptRipple;
	std::unique_ptr<Ui::RippleAnimation> _rejectRipple;

};

Row::Row(
	not_null<RowDelegate*> delegate,
	const Api::CommunityPeerRequest &request)
: PeerListRow(request.peer)
, _delegate(delegate) {
	const auto suggestedBy = request.requestedBy
		? request.requestedBy->shortName()
		: QString();
	auto status = request.peer->isBroadcast()
		? tr::lng_community_request_suggested_channel(
			tr::now,
			lt_user,
			suggestedBy)
		: tr::lng_community_request_suggested_group(
			tr::now,
			lt_user,
			suggestedBy);
	if (!request.visible) {
		status += QString::fromUtf8(" \xE2\x80\xA2 ")
			+ tr::lng_community_request_only_members(tr::now);
	}
	setCustomStatus(status);
}

int Row::elementsCount() const {
	return 2;
}

QRect Row::elementGeometry(int element, int outerWidth) const {
	switch (element) {
	case kAcceptButton: {
		const auto size = _delegate->rowAcceptButtonSize();
		return QRect(st::requestAcceptPosition, size);
	} break;
	case kRejectButton: {
		const auto accept = _delegate->rowAcceptButtonSize();
		const auto size = _delegate->rowRejectButtonSize();
		return QRect(
			(st::requestAcceptPosition
				+ QPoint(accept.width() + st::requestButtonsSkip, 0)),
			size);
	} break;
	}
	return QRect();
}

bool Row::elementDisabled(int element) const {
	return false;
}

bool Row::elementOnlySelect(int element) const {
	return true;
}

void Row::elementAddRipple(
		int element,
		QPoint point,
		Fn<void()> updateCallback) {
	const auto pointer = (element == kAcceptButton)
		? &_acceptRipple
		: (element == kRejectButton)
		? &_rejectRipple
		: nullptr;
	if (!pointer) {
		return;
	}
	auto &ripple = *pointer;
	if (!ripple) {
		auto mask = Ui::RippleAnimation::RoundRectMask(
			(element == kAcceptButton
				? _delegate->rowAcceptButtonSize()
				: _delegate->rowRejectButtonSize()),
			st::buttonRadius);
		ripple = std::make_unique<Ui::RippleAnimation>(
			(element == kAcceptButton
				? st::requestsAcceptButton.ripple
				: st::requestsRejectButton.ripple),
			std::move(mask),
			std::move(updateCallback));
	}
	ripple->add(point);
}

void Row::elementsStopLastRipple() {
	if (_acceptRipple) {
		_acceptRipple->lastStop();
	}
	if (_rejectRipple) {
		_rejectRipple->lastStop();
	}
}

void Row::elementsPaint(
		Painter &p,
		int outerWidth,
		bool selected,
		int selectedElement) {
	const auto accept = elementGeometry(kAcceptButton, outerWidth);
	const auto reject = elementGeometry(kRejectButton, outerWidth);

	const auto over = [&](int element) {
		return (selectedElement == element);
	};
	_delegate->rowPaintAccept(
		p,
		accept,
		_acceptRipple,
		outerWidth,
		over(kAcceptButton));
	_delegate->rowPaintReject(
		p,
		reject,
		_rejectRipple,
		outerWidth,
		over(kRejectButton));
}

class Controller final
	: public PeerListController
	, public RowDelegate
	, public base::has_weak_ptr {
public:
	Controller(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community);

	Main::Session &session() const override;
	void prepare() override;
	void loadMoreRows() override;
	void rowClicked(not_null<PeerListRow*> row) override;
	void rowElementClicked(
		not_null<PeerListRow*> row,
		int element) override;

	void setCloseBox(Fn<void()> closeBox) {
		_closeBox = std::move(closeBox);
	}

	QSize rowAcceptButtonSize() override;
	QSize rowRejectButtonSize() override;
	void rowPaintAccept(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) override;
	void rowPaintReject(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) override;

private:
	void paintButton(
		Painter &p,
		QRect geometry,
		const style::RoundButton &st,
		const Ui::RoundRect &rect,
		const Ui::RoundRect &rectOver,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		const QString &text,
		int textWidth,
		int outerWidth,
		bool over);
	void process(not_null<PeerListRow*> row, bool reject);

	const not_null<Window::SessionNavigation*> _navigation;
	const not_null<ChannelData*> _community;
	Fn<void()> _closeBox;

	QString _offset;
	bool _allLoaded = false;
	bool _loading = false;

	Ui::RoundRect _acceptRect;
	Ui::RoundRect _acceptRectOver;
	Ui::RoundRect _rejectRect;
	Ui::RoundRect _rejectRectOver;
	QString _acceptText;
	QString _rejectText;
	int _acceptTextWidth = 0;
	int _rejectTextWidth = 0;

};

Controller::Controller(
	not_null<Window::SessionNavigation*> navigation,
	not_null<ChannelData*> community)
: _navigation(navigation)
, _community(community)
, _acceptRect(st::buttonRadius, st::requestsAcceptButton.textBg)
, _acceptRectOver(st::buttonRadius, st::requestsAcceptButton.textBgOver)
, _rejectRect(st::buttonRadius, st::requestsRejectButton.textBg)
, _rejectRectOver(st::buttonRadius, st::requestsRejectButton.textBgOver)
, _acceptText(tr::lng_community_request_add(tr::now))
, _rejectText(tr::lng_community_request_decline(tr::now))
, _acceptTextWidth(st::requestsAcceptButton.style.font->width(_acceptText))
, _rejectTextWidth(st::requestsRejectButton.style.font->width(_rejectText)) {
	setStyleOverrides(&st::requestsBoxList);
}

Main::Session &Controller::session() const {
	return _community->session();
}

void Controller::prepare() {
	delegate()->peerListSetTitle(tr::lng_community_requests_title());
	loadMoreRows();
}

void Controller::loadMoreRows() {
	if (_allLoaded || _loading) {
		return;
	}
	_loading = true;
	session().api().communities().requestPeerLinkRequests(
		_community,
		_offset,
		kPerPage,
		crl::guard(this, [=](Api::CommunityPeerRequestsSlice slice) {
			_loading = false;
			for (const auto &request : slice.list) {
				if (!delegate()->peerListFindRow(request.peer->id.value)) {
					delegate()->peerListAppendRow(
						std::make_unique<Row>(this, request));
				}
			}
			delegate()->peerListRefreshRows();
			_offset = slice.nextOffset;
			if (_offset.isEmpty()) {
				_allLoaded = true;
			}
		}));
}

void Controller::rowClicked(not_null<PeerListRow*> row) {
	const auto peer = row->peer();
	if (const auto window = _navigation->parentController()) {
		window->showPeer(peer);
	}
}

void Controller::rowElementClicked(
		not_null<PeerListRow*> row,
		int element) {
	if (element == kAcceptButton) {
		process(row, false);
	} else if (element == kRejectButton) {
		process(row, true);
	}
}

void Controller::process(not_null<PeerListRow*> row, bool reject) {
	const auto peer = row->peer();
	const auto id = peer->id.value;
	session().api().communities().togglePeerLinkRequestApproval(
		_community,
		peer,
		reject,
		crl::guard(this, [=] {
			if (const auto row = delegate()->peerListFindRow(id)) {
				delegate()->peerListRemoveRow(row);
				delegate()->peerListRefreshRows();
			}
			const auto show = _navigation->uiShow();
			const auto closeBox = (_allLoaded
				&& !delegate()->peerListFullRowsCount())
				? _closeBox
				: nullptr;
			if (closeBox) {
				// Destroys the box and this controller.
				closeBox();
			}
			show->showToast(reject
				? tr::lng_community_request_declined_toast(
					tr::now,
					lt_count,
					1)
				: tr::lng_community_request_added_toast(
					tr::now,
					lt_count,
					1));
		}),
		crl::guard(this, [=](const QString &error) {
			delegate()->peerListUiShow()->showToast(error);
		}));
}

QSize Controller::rowAcceptButtonSize() {
	const auto &st = st::requestsAcceptButton;
	return {
		(st.width <= 0) ? (_acceptTextWidth - st.width) : st.width,
		st.height,
	};
}

QSize Controller::rowRejectButtonSize() {
	const auto &st = st::requestsRejectButton;
	return {
		(st.width <= 0) ? (_rejectTextWidth - st.width) : st.width,
		st.height,
	};
}

void Controller::rowPaintAccept(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) {
	paintButton(
		p,
		geometry,
		st::requestsAcceptButton,
		_acceptRect,
		_acceptRectOver,
		ripple,
		_acceptText,
		_acceptTextWidth,
		outerWidth,
		over);
}

void Controller::rowPaintReject(
		Painter &p,
		QRect geometry,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		int outerWidth,
		bool over) {
	paintButton(
		p,
		geometry,
		st::requestsRejectButton,
		_rejectRect,
		_rejectRectOver,
		ripple,
		_rejectText,
		_rejectTextWidth,
		outerWidth,
		over);
}

void Controller::paintButton(
		Painter &p,
		QRect geometry,
		const style::RoundButton &st,
		const Ui::RoundRect &rect,
		const Ui::RoundRect &rectOver,
		std::unique_ptr<Ui::RippleAnimation> &ripple,
		const QString &text,
		int textWidth,
		int outerWidth,
		bool over) {
	rect.paint(p, geometry);
	if (over) {
		rectOver.paint(p, geometry);
	}
	if (ripple) {
		ripple->paint(p, geometry.x(), geometry.y(), outerWidth);
		if (ripple->empty()) {
			ripple = nullptr;
		}
	}
	p.setFont(st.style.font);
	p.setPen(over ? st.textFgOver : st.textFg);
	p.drawTextLeft(
		geometry.x() + (geometry.width() - textWidth) / 2,
		geometry.y() + st.textTop,
		outerWidth,
		text);
}

} // namespace

void ShowCommunityPendingRequestsBox(
		not_null<Window::SessionNavigation*> navigation,
		not_null<ChannelData*> community) {
	auto controller = std::make_unique<Controller>(navigation, community);
	const auto raw = controller.get();
	const auto init = [=](not_null<PeerListBox*> box) {
		raw->setCloseBox(crl::guard(box, [=] { box->closeBox(); }));
		const auto processAll = [=](bool reject) {
			const auto count = std::max(
				community->pendingRequestsCount(),
				1);
			const auto sure = [=](Fn<void()> &&close) {
				close();
				community->session().api().communities(
				).toggleAllPeerLinkRequestApproval(
					community,
					reject,
					crl::guard(box, [=] {
						const auto show = navigation->uiShow();
						box->closeBox();
						show->showToast(reject
							? tr::lng_community_request_declined_toast(
								tr::now,
								lt_count,
								count)
							: tr::lng_community_request_added_toast(
								tr::now,
								lt_count,
								count));
					}),
					crl::guard(box, [=](const QString &error) {
						box->uiShow()->showToast(error);
					}));
			};
			box->uiShow()->showBox(Ui::MakeConfirmBox({
				.text = (reject
					? tr::lng_community_requests_decline_all_sure(
						tr::now,
						lt_count,
						count)
					: tr::lng_community_requests_add_all_sure(
						tr::now,
						lt_count,
						count)),
				.confirmed = sure,
				.confirmText = (reject
					? tr::lng_community_request_decline()
					: tr::lng_community_request_add()),
				.title = (reject
					? tr::lng_community_requests_decline_all_title()
					: tr::lng_community_requests_add_all_title()),
			}));
		};
		box->addTopButton(st::boxTitleClose, [=] { box->closeBox(); });
		box->addButton(
			tr::lng_community_requests_add_all(),
			[=] { processAll(false); });
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		box->addLeftButton(
			tr::lng_community_requests_decline_all(),
			[=] { processAll(true); });

		auto above = object_ptr<Ui::VerticalLayout>(box);
		Ui::AddDividerText(above, tr::lng_community_requests_about());
		Ui::AddSkip(above);
		Ui::AddSubsectionTitle(
			above,
			tr::lng_community_requests_count(
				lt_count,
				Info::Profile::PendingRequestsCountValue(
					community
				) | rpl::map([](int count) {
					return float64(std::max(count, 1));
				})));
		box->peerListSetAboveWidget(std::move(above));
	};
	navigation->uiShow()->showBox(Box<PeerListBox>(
		std::move(controller),
		init));
}
