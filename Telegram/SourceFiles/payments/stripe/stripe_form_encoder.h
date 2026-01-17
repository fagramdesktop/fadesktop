/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "stripe/stripe_form_encodable.h"

namespace Stripe {

class FormEncoder {
public:
	[[nodiscard]] static QByteArray formEncodedDataForObject(
		FormEncodable &&object);

};

} // namespace Stripe
