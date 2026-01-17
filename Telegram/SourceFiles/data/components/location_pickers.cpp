/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "data/components/location_pickers.h"

#include "api/api_common.h"
#include "ui/controls/location_picker.h"

namespace Data {

struct LocationPickers::Entry {
	Api::SendAction action;
	base::weak_ptr<Ui::LocationPicker> picker;
};

LocationPickers::LocationPickers() = default;

LocationPickers::~LocationPickers() = default;

Ui::LocationPicker *LocationPickers::lookup(const Api::SendAction &action) {
	for (auto i = begin(_pickers); i != end(_pickers);) {
		if (const auto strong = i->picker.get()) {
			if (i->action == action) {
				return strong;
			}
			++i;
		} else {
			i = _pickers.erase(i);
		}
	}
	return nullptr;
}

void LocationPickers::emplace(
		const Api::SendAction &action,
		not_null<Ui::LocationPicker*> picker) {
	_pickers.push_back({ action, picker });
}

} // namespace Data
