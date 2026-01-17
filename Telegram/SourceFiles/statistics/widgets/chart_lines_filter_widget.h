/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Statistic {

class ChartLinesFilterWidget final : public Ui::RpWidget {
public:
	ChartLinesFilterWidget(not_null<Ui::RpWidget*> parent);

	struct ButtonData final {
		QString text;
		QColor color;
		int id = 0;
		bool disabled = false;
	};

	void fillButtons(const std::vector<ButtonData> &buttonsData);

	void resizeToWidth(int outerWidth);

	struct Entry final {
		int id = 0;
		bool enabled = 0;
	};
	[[nodiscard]] rpl::producer<Entry> buttonEnabledChanges() const;

private:
	class FlatCheckbox;

	std::vector<base::unique_qptr<FlatCheckbox>> _buttons;

	rpl::event_stream<Entry> _buttonEnabledChanges;

};

} // namespace Statistic
