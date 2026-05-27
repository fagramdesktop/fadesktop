/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "ui/rect_part.h"

namespace Ui {
class VerticalLayout;
class FlatLabel;
class RpWidget;
} // namespace Ui

namespace Info::Profile {

struct SectionSeparator {
	enum class Kind { None, Plain, Text };
	Kind kind = Kind::None;
	rpl::producer<TextWithEntities> text;
	Fn<void(not_null<Ui::FlatLabel*>)> textSetup;

	[[nodiscard]] static SectionSeparator None();
	[[nodiscard]] static SectionSeparator Plain();
	[[nodiscard]] static SectionSeparator Text(
		rpl::producer<TextWithEntities> text,
		Fn<void(not_null<Ui::FlatLabel*>)> setup = nullptr);
};

struct Section {
	object_ptr<Ui::RpWidget> widget = { nullptr };
	rpl::producer<bool> shown;
	SectionSeparator trailing;
	bool embedsLeadingSeparator = false;
};

class SectionStack final {
public:
	explicit SectionStack(not_null<Ui::VerticalLayout*> layout);

	void add(Section section);
	void addPlainSeparator();
	void finalize();

	[[nodiscard]] int count() const;
	[[nodiscard]] not_null<Ui::VerticalLayout*> layout() const;

private:
	[[nodiscard]] rpl::producer<bool> nextVisibleIsNonEmbedding(
		int afterIndex) const;
	[[nodiscard]] rpl::producer<bool> anyShownAtOrBefore(int index) const;
	[[nodiscard]] rpl::producer<bool> anyShownAfter(int index) const;
	[[nodiscard]] rpl::producer<bool> anyShownInRange(
		int from,
		int toInclusive) const;
	[[nodiscard]] rpl::producer<bool> computePlainMarkerCandidate(
		int position) const;
	void addPlainMarkerSlot(
		int markerIndex,
		not_null<std::vector<rpl::variable<bool>>*> candidates,
		bool suppressedByText);
	void addTextSeparatorSlot(int sectionIndex, SectionSeparator &trailing);

	const not_null<Ui::VerticalLayout*> _layout;
	std::vector<Section> _sections;
	std::vector<int> _plainMarkerAfter;
	bool _finalized = false;

};

} // namespace Info::Profile
