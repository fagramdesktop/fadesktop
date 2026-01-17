/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "stripe/stripe_card_params.h"

namespace Stripe {

QString CardParams::rootObjectName() {
	return "card";
}

std::map<QString, QString> CardParams::formFieldValues() const {
	return {
		{ "number", number },
		{ "cvc", cvc },
		{ "name", name },
		{ "address_line1", addressLine1 },
		{ "address_line2", addressLine2 },
		{ "address_city", addressCity },
		{ "address_state", addressState },
		{ "address_zip", addressZip },
		{ "address_country", addressCountry },
		{ "exp_month", QString::number(expMonth) },
		{ "exp_year", QString::number(expYear) },
		{ "currency", currency },
	};
}

} // namespace Stripe
