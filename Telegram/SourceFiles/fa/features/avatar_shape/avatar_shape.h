/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include <QImage>
#include <QSize>
#include <cstdint>

namespace Ui {
enum class PeerUserpicShape : uint8_t;
} // namespace Ui

namespace FA::Features::AvatarShape {

[[nodiscard]] bool IsMaterial();

[[nodiscard]] int Roundness();

[[nodiscard]] int CornerRadius(int size, int dpr = 1);

[[nodiscard]] QImage Mask(QSize size);

[[nodiscard]] QImage Apply(QImage image);

[[nodiscard]] Ui::PeerUserpicShape Resolve(Ui::PeerUserpicShape base);

} // namespace FA::Features::AvatarShape
