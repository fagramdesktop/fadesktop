/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/text/text.h"

constexpr auto FullSelection = TextSelection { 0xFFFF, 0xFFFF };

[[nodiscard]] bool IsSubGroupSelection(TextSelection selection);

[[nodiscard]] bool IsGroupItemSelection(
		TextSelection selection,
		int index);

[[nodiscard]] TextSelection AddGroupItemSelection(
		TextSelection selection,
		int index);

[[nodiscard]] TextSelection RemoveGroupItemSelection(
		TextSelection selection,
		int index);
