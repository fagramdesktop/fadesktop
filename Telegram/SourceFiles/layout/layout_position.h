/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Layout {

struct Position {
	int row = -1;
	int column = -1;
};

[[nodiscard]] Position IndexToPosition(int index);
[[nodiscard]] int PositionToIndex(int row, int column);
[[nodiscard]] int PositionToIndex(const Position &position);

} // namespace Layout
