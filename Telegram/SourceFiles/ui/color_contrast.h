/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class QColor;

namespace Ui {

[[nodiscard]] float64 CountContrast(const QColor &a, const QColor &b);

} // namespace Ui
