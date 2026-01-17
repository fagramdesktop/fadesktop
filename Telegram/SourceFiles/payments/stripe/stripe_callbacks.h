/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <functional>

namespace Stripe {

class Error;
class Token;

using TokenCompletionCallback = std::function<void(Token, Error)>;

} // namespace Stripe
