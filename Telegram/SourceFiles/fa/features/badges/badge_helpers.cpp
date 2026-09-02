/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/features/badges/badge_helpers.h"

#include "data/data_peer.h"
#include "fa_lang_auto.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/rp_widget.h"
#include "styles/style_info.h"
#include "styles/style_fa_styles.h"

#include <rpl/rpl.h>

namespace FA::Badges {
namespace {

class BadgeToastIcon final : public Ui::RpWidget {
public:
	BadgeToastIcon(
		QWidget *parent,
		not_null<PeerData*> peer,
		Info::Profile::Badge::Content content);

private:
	void updateInnerGeometry();

	Info::Profile::Badge _badge;

};

BadgeToastIcon::BadgeToastIcon(
	QWidget *parent,
	not_null<PeerData*> peer,
	Info::Profile::Badge::Content content)
: Ui::RpWidget(parent)
, _badge(
	this,
	st::infoPeerBadge,
	&peer->session(),
	rpl::single(content),
	nullptr,
	[] { return false; },
	0,
	BadgeTypes()) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	_badge.setOverrideStyle(&st::faBadgeToastBadge);
	_badge.updated() | rpl::on_next([=] {
		updateInnerGeometry();
	}, lifetime());
	updateInnerGeometry();
}

void BadgeToastIcon::updateInnerGeometry() {
	const auto widget = _badge.widget();
	const auto size = widget ? widget->size() : QSize();
	resize(size.width(), size.height());
	if (widget) {
		widget->moveToLeft(0, 0);
	}
}

[[nodiscard]] object_ptr<Ui::RpWidget> MakeBadgeToastIcon(
		not_null<PeerData*> peer,
		Info::Profile::Badge::Content content) {
	return (content.badge == Info::Profile::BadgeType::None)
		? object_ptr<Ui::RpWidget>(nullptr)
		: object_ptr<BadgeToastIcon>(nullptr, peer, content);
}

} // namespace

FaID getBareID(not_null<PeerData*> peer) {
	return peer->id.value & PeerId::kChatTypeMask;
}

bool isFAgramPeer(FaID peerId) {
	const auto &manager = BadgeRegistry::getInstance();
	return manager.fagramDevelopers().contains(peerId)
		|| manager.fagramOfficialChannels().contains(peerId);
}

bool isFAgramSupporterPeer(FaID peerId) {
	const auto &manager = BadgeRegistry::getInstance();
	return manager.fagramSupporters().contains(peerId)
		|| manager.fagramSupporterChannels().contains(peerId);
}

base::flags<Info::Profile::BadgeType> BadgeTypes() {
	using Type = Info::Profile::BadgeType;
	return Type::FAgramOfficial
		| Type::FAgramSupporter;
}

Info::Profile::Badge::Content ComputeBadgeContent(
		not_null<PeerData*> peer) {
	const auto peerId = getBareID(peer);
	if (isFAgramPeer(peerId)) {
		return Info::Profile::Badge::Content{
			.badge = Info::Profile::BadgeType::FAgramOfficial,
		};
	} else if (isFAgramSupporterPeer(peerId)) {
		return Info::Profile::Badge::Content{
			.badge = Info::Profile::BadgeType::FAgramSupporter,
		};
	}
	return {};
}

rpl::producer<Info::Profile::Badge::Content> BadgeContentForPeer(
		not_null<PeerData*> peer) {
	return rpl::single(ComputeBadgeContent(peer));
}

Fn<void()> badgeClickHandler(not_null<PeerData*> peer) {
	return [=] {
		const auto badge = ComputeBadgeContent(peer);

		const auto formatPopup = [&](const QString &templateStr) {
			auto replaced = templateStr;
			replaced.replace(u"{item}"_q, peer->name());
			return Ui::Text::RichLangValue(replaced);
		};

		TextWithEntities text;
		switch (badge.badge) {
		case Info::Profile::BadgeType::FAgramOfficial:
			text = peer->isUser()
				? formatPopup(fatr::fa_fagram_developer_popup(fatr::now))
				: formatPopup(fatr::fa_fagram_official_resource_popup(fatr::now));
			break;
		case Info::Profile::BadgeType::FAgramSupporter:
			text = formatPopup(fatr::fa_fagram_supporter_popup(fatr::now));
			break;
		default:
			return;
		}

		auto config = Ui::Toast::Config{
			.text = text,
			.iconContent = MakeBadgeToastIcon(peer, badge),
			.st = &st::faBadgeToast,
			.adaptive = true,
			.duration = 3 * crl::time(1000),
		};
		Ui::Toast::Show(std::move(config));
	};
}

} // namespace FA::Badges
