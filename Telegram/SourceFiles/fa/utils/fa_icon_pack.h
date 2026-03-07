/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#pragma once

#include <QtCore/QString>

namespace FAIcons {

[[nodiscard]] QString MiconPath(const QString &name);
[[nodiscard]] bool UseCustomIconPack();

} // namespace FAIcons
