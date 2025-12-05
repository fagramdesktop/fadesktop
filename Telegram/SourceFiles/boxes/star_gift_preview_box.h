/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Data {
struct StarGift;
struct UniqueGiftAttributes;
} // namespace Data

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

class GenericBox;

void StarGiftPreviewBox(
	not_null<GenericBox*> box,
	not_null<Window::SessionController*> controller,
	const Data::StarGift &gift,
	const Data::UniqueGiftAttributes &attributes);

} // namespace Ui
