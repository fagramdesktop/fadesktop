/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_section_stack.h"

#include "ui/widgets/box_content_divider.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"

#include "styles/style_info.h"
#include "styles/style_layers.h"

namespace Info::Profile {

SectionSeparator SectionSeparator::None() {
	return SectionSeparator{ .kind = Kind::None };
}

SectionSeparator SectionSeparator::Plain() {
	return SectionSeparator{ .kind = Kind::Plain };
}

SectionSeparator SectionSeparator::Text(
		rpl::producer<TextWithEntities> text,
		Fn<void(not_null<Ui::FlatLabel*>)> setup) {
	return SectionSeparator{
		.kind = Kind::Text,
		.text = std::move(text),
		.textSetup = std::move(setup),
	};
}

SectionStack::SectionStack(not_null<Ui::VerticalLayout*> layout)
: _layout(layout) {
}

void SectionStack::add(Section section) {
	Expects(!_finalized);

	_sections.push_back(std::move(section));
}

void SectionStack::addPlainSeparator() {
	Expects(!_finalized);

	if (_sections.empty()) {
		return;
	}
	const auto position = int(_sections.size() - 1);
	if (!_plainMarkerAfter.empty()
		&& _plainMarkerAfter.back() == position) {
		return;
	}
	_plainMarkerAfter.push_back(position);
}

int SectionStack::count() const {
	return int(_sections.size());
}

not_null<Ui::VerticalLayout*> SectionStack::layout() const {
	return _layout;
}

rpl::producer<bool> SectionStack::anyShownAtOrBefore(int index) const {
	if (index < 0) {
		return rpl::single(false);
	}
	auto producers = std::vector<rpl::producer<bool>>();
	producers.reserve(index + 1);
	for (auto k = 0; k <= index; ++k) {
		producers.push_back(_sections[k].shown
			? rpl::duplicate(_sections[k].shown)
			: rpl::single(true));
	}
	return rpl::combine(
		std::move(producers),
		[](const std::vector<bool> &v) {
			return ranges::find(v, true) != v.end();
		}) | rpl::distinct_until_changed();
}

rpl::producer<bool> SectionStack::anyShownAfter(int index) const {
	if (index + 1 >= int(_sections.size())) {
		return rpl::single(false);
	}
	auto producers = std::vector<rpl::producer<bool>>();
	producers.reserve(_sections.size() - index - 1);
	for (auto k = index + 1; k != int(_sections.size()); ++k) {
		producers.push_back(_sections[k].shown
			? rpl::duplicate(_sections[k].shown)
			: rpl::single(true));
	}
	return rpl::combine(
		std::move(producers),
		[](const std::vector<bool> &v) {
			return ranges::find(v, true) != v.end();
		}) | rpl::distinct_until_changed();
}

rpl::producer<bool> SectionStack::anyShownInRange(
		int from,
		int toInclusive) const {
	const auto sectionCount = int(_sections.size());
	if (from > toInclusive || from >= sectionCount || toInclusive < 0) {
		return rpl::single(false);
	}
	const auto begin = std::max(from, 0);
	const auto end = std::min(toInclusive, sectionCount - 1);
	auto producers = std::vector<rpl::producer<bool>>();
	producers.reserve(end - begin + 1);
	for (auto k = begin; k <= end; ++k) {
		producers.push_back(_sections[k].shown
			? rpl::duplicate(_sections[k].shown)
			: rpl::single(true));
	}
	return rpl::combine(
		std::move(producers),
		[](const std::vector<bool> &v) {
			return ranges::find(v, true) != v.end();
		}) | rpl::distinct_until_changed();
}

rpl::producer<bool> SectionStack::nextVisibleIsNonEmbedding(
		int afterIndex) const {
	if (afterIndex + 1 >= int(_sections.size())) {
		return rpl::single(false);
	}
	auto producers = std::vector<rpl::producer<bool>>();
	auto embeds = std::vector<bool>();
	producers.reserve(_sections.size() - afterIndex - 1);
	embeds.reserve(_sections.size() - afterIndex - 1);
	for (auto j = afterIndex + 1; j != int(_sections.size()); ++j) {
		producers.push_back(_sections[j].shown
			? rpl::duplicate(_sections[j].shown)
			: rpl::single(true));
		embeds.push_back(_sections[j].embedsLeadingSeparator);
	}
	return rpl::combine(
		std::move(producers),
		[embeds = std::move(embeds)](const std::vector<bool> &values) {
			for (auto k = 0; k != int(values.size()); ++k) {
				if (values[k]) {
					return !embeds[k];
				}
			}
			return false;
		}) | rpl::distinct_until_changed();
}

rpl::producer<bool> SectionStack::computePlainMarkerCandidate(
		int position) const {
	auto upper = anyShownAtOrBefore(position);
	auto lower = nextVisibleIsNonEmbedding(position);
	return rpl::combine(
		std::move(upper),
		std::move(lower)
	) | rpl::map([](bool u, bool l) {
		return u && l;
	}) | rpl::distinct_until_changed();
}

void SectionStack::addPlainMarkerSlot(
		int markerIndex,
		not_null<std::vector<rpl::variable<bool>>*> candidates,
		bool suppressedByText) {
	auto inner = object_ptr<Ui::VerticalLayout>(_layout);
	Ui::AddSkip(inner.data(), st::infoProfileSkip);
	inner->add(object_ptr<Ui::BoxContentDivider>(
		inner.data(),
		st::boxDividerHeight,
		st::defaultDividerBar));
	Ui::AddSkip(inner.data(), st::infoProfileSkip);
	const auto wrap = _layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_layout,
			std::move(inner)));

	auto shown = rpl::producer<bool>();
	if (suppressedByText) {
		shown = rpl::single(false);
	} else if (markerIndex == 0) {
		shown = (*candidates)[0].value();
	} else {
		const auto position = _plainMarkerAfter[markerIndex];
		const auto prevPosition = _plainMarkerAfter[markerIndex - 1];
		auto sameGap = anyShownInRange(prevPosition + 1, position);
		shown = rpl::combine(
			(*candidates)[markerIndex].value(),
			(*candidates)[markerIndex - 1].value(),
			std::move(sameGap)
		) | rpl::map([](bool here, bool prev, bool anyBetween) {
			const auto sameGapAsPrev = !anyBetween;
			return here && !(sameGapAsPrev && prev);
		}) | rpl::distinct_until_changed();
	}
	wrap->setDuration(st::infoSlideDuration)->toggleOn(std::move(shown));
}

void SectionStack::addTextSeparatorSlot(
		int sectionIndex,
		SectionSeparator &trailing) {
	auto inner = object_ptr<Ui::VerticalLayout>(_layout);
	Ui::AddSkip(inner.data(), st::infoProfileSkip);
	auto label = object_ptr<Ui::FlatLabel>(
		inner.data(),
		std::move(trailing.text),
		st::defaultDividerLabel.label);
	const auto rawLabel = label.data();
	inner->add(object_ptr<Ui::DividerLabel>(
		inner.data(),
		std::move(label),
		st::defaultBoxDividerLabelPadding,
		st::defaultDividerLabel.bar,
		RectPart::Top | RectPart::Bottom));
	Ui::AddSkip(inner.data(), st::infoProfileSkip);
	const auto wrap = _layout->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_layout,
			std::move(inner)));
	if (trailing.textSetup) {
		trailing.textSetup(rawLabel);
	}
	auto upper = anyShownAtOrBefore(sectionIndex);
	auto lower = anyShownAfter(sectionIndex);
	auto shown = rpl::combine(
		std::move(upper),
		std::move(lower)
	) | rpl::map([](bool u, bool l) {
		return u && l;
	}) | rpl::distinct_until_changed();
	wrap->setDuration(st::infoSlideDuration)->toggleOn(std::move(shown));
}

void SectionStack::finalize() {
	Expects(!_finalized);

	_finalized = true;
	for (auto &s : _sections) {
		if (!s.shown) {
			s.shown = rpl::single(true);
		}
	}

	Ui::AddSkip(_layout, st::infoProfileSkip);

	const auto sectionCount = int(_sections.size());
	const auto markerCandidates = _layout->lifetime().make_state<
		std::vector<rpl::variable<bool>>>();
	markerCandidates->reserve(_plainMarkerAfter.size());
	for (auto m = 0; m != int(_plainMarkerAfter.size()); ++m) {
		const auto position = _plainMarkerAfter[m];
		markerCandidates->emplace_back(
			computePlainMarkerCandidate(position));
	}

	auto nextMarker = 0;
	for (auto i = 0; i != sectionCount; ++i) {
		auto &section = _sections[i];
		Expects(section.widget != nullptr);

		_layout->add(std::move(section.widget));

		const auto hasText = (section.trailing.kind
			== SectionSeparator::Kind::Text);
		if (hasText && i + 1 < sectionCount) {
			addTextSeparatorSlot(i, section.trailing);
		}

		while (nextMarker != int(_plainMarkerAfter.size())
			&& _plainMarkerAfter[nextMarker] == i) {
			addPlainMarkerSlot(
				nextMarker,
				markerCandidates,
				hasText);
			++nextMarker;
		}
	}

	Ui::AddSkip(_layout, st::infoProfileSkip);
}

} // namespace Info::Profile
