/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "layout/layout_selection.h"

bool IsSubGroupSelection(TextSelection selection) {
	return (selection.from == 0xFFFF) && (selection.to != 0xFFFF);
}

bool IsGroupItemSelection(
		TextSelection selection,
		int index) {
	Expects(index >= 0 && index < 0x0F);

	return IsSubGroupSelection(selection) && (selection.to & (1 << index));
}

TextSelection AddGroupItemSelection(
		TextSelection selection,
		int index) {
	Expects(index >= 0 && index < 0x0F);

	const auto bit = uint16(1U << index);
	return TextSelection(
		0xFFFF,
		IsSubGroupSelection(selection) ? (selection.to | bit) : bit);
}

TextSelection RemoveGroupItemSelection(
		TextSelection selection,
		int index) {
	Expects(index >= 0 && index < 0x0F);

	const auto bit = uint16(1U << index);
	return IsSubGroupSelection(selection)
		? TextSelection(0xFFFF, selection.to & ~bit)
		: selection;
}
