/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "ui/rp_widget.h"
#include "ui/style/style_core.h"
#include "ui/widgets/shadow.h"

#include <vector>

namespace style {
struct IconButton;
} // namespace style

namespace Ui {
class IconButton;
} // namespace Ui

namespace Iv::Editor {

enum class ToolbarButtonState;

class ToolbarPill final : public Ui::RpWidget {
public:
	ToolbarPill(QWidget *parent, const style::BoxShadow &shadow);

	not_null<Ui::IconButton*> addButton(
		const style::IconButton &buttonSt,
		const style::icon *icon,
		const style::icon *iconOver,
		ToolbarButtonState state);

	[[nodiscard]] QSize naturalSize() const;
	[[nodiscard]] QMargins shadowMargins() const;

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void updateGeometryToContent();

	std::vector<object_ptr<Ui::IconButton>> _buttons;
	Ui::BoxShadow _shadow;
	QMargins _shadowMargins;

};

} // namespace Iv::Editor
