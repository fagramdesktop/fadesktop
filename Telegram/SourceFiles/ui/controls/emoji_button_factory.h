/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

namespace ChatHelpers {
class TabbedPanel;
} // namespace ChatHelpers

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {

class BoxContent;
class EmojiButton;
class InputField;

[[nodiscard]] not_null<Ui::EmojiButton*> AddEmojiToggleToField(
	not_null<Ui::InputField*> field,
	not_null<Ui::BoxContent*> box,
	not_null<Window::SessionController*> controller,
	not_null<ChatHelpers::TabbedPanel*> emojiPanel,
	QPoint shift,
	bool fadeOnFocusChange = true);

} // namespace Ui
