/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "stripe/stripe_card.h"

#include <QtCore/QDateTime>

class QJsonObject;

namespace Stripe {

class Token {
public:
	Token(const Token &other) = default;
	Token &operator=(const Token &other) = default;
	Token(Token &&other) = default;
	Token &operator=(Token &&other) = default;
	~Token() = default;

	[[nodiscard]] QString tokenId() const;
	[[nodiscard]] bool livemode() const;
	[[nodiscard]] Card card() const;

	[[nodiscard]] static Token Empty();
	[[nodiscard]] static Token DecodedObjectFromAPIResponse(
		QJsonObject object);

	[[nodiscard]] bool empty() const;
	[[nodiscard]] explicit operator bool() const {
		return !empty();
	}

private:
	Token(QString tokenId, bool livemode, QDateTime created);

	QString _tokenId;
	bool _livemode = false;
	QDateTime _created;
	Card _card = Card::Empty();

};

} // namespace Stripe
