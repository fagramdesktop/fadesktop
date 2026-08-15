/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/wrap/vertical_layout.h"

namespace FA::Ui {

class MaterialSlider : public ::Ui::ContinuousSlider {
public:
	explicit MaterialSlider(QWidget *parent = nullptr);

	void setAlwaysDisplayMarker(bool alwaysDisplayMarker) {
		_alwaysDisplayMarker = alwaysDisplayMarker;
		update();
	}

	template <
		typename Value,
		typename Convert,
		typename Progress,
		typename = std::enable_if_t<
			rpl::details::is_callable_plain_v<Progress, Value>
			&& std::is_same_v<Value, decltype(std::declval<Convert>()(1))>>>
	void setPseudoDiscrete(
			int valuesCount,
			Convert &&convert,
			Value current,
			Progress &&progress,
			int indexMin = 0) {
		Expects(valuesCount > 1);

		setAlwaysDisplayMarker(true);
		setDirection(::Ui::ContinuousSlider::Direction::Horizontal);

		const auto sectionsCount = (valuesCount - 1);
		setValue(1.);
		for (auto index = 0; index != valuesCount; ++index) {
			if (current <= convert(index)) {
				setValue(index / float64(sectionsCount));
				break;
			}
		}
		setAdjustCallback([=](float64 value) {
			return std::max(
				base::SafeRound(value * sectionsCount),
				indexMin * 1.
			) / sectionsCount;
		});
		setChangeProgressCallback([=](float64 value) {
			const auto index = std::max(
				int(base::SafeRound(value * sectionsCount)),
				indexMin);
			progress(convert(index));
		});
	}

	template <
		typename Value,
		typename Convert,
		typename Progress,
		typename Finished,
		typename = std::enable_if_t<
			rpl::details::is_callable_plain_v<Progress, Value>
			&& rpl::details::is_callable_plain_v<Finished, Value>
			&& std::is_same_v<Value, decltype(std::declval<Convert>()(1))>>>
	void setPseudoDiscrete(
			int valuesCount,
			Convert &&convert,
			Value current,
			Progress &&progress,
			Finished &&finished,
			int indexMin = 0) {
		setPseudoDiscrete(
			valuesCount,
			std::forward<Convert>(convert),
			current,
			std::forward<Progress>(progress),
			indexMin);
		setChangeFinishedCallback([=](float64 value) {
			const auto sectionsCount = (valuesCount - 1);
			const auto index = std::max(
				int(base::SafeRound(value * sectionsCount)),
				indexMin);
			finished(convert(index));
		});
	}

	void addDivider(float64 atValue);
	void clearDividers();

protected:
	int resizeGetHeight(int newWidth) override;
	void paintEvent(QPaintEvent *e) override;

private:
	QSize getSeekDecreaseSize() const override;
	float64 getOverDuration() const override;

	bool _alwaysDisplayMarker = true;
	std::vector<float64> _dividers;

};

not_null<MaterialSlider*> AddCardSlider(
	not_null<::Ui::VerticalLayout*> card,
	const style::margins &margin = style::margins(16, 4, 16, 14));

} // namespace FA::Ui
