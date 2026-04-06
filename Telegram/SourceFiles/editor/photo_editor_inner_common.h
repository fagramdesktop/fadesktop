/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Editor {

class Scene;

struct PhotoEditorMode {
	enum class Mode {
		Transform,
		Paint,
		Out,
	} mode = Mode::Transform;

	enum class Action {
		None,
		Save,
		Discard,
	} action = Action::None;
};

struct Brush {
	enum class Tool : uchar {
		Pen,
		Arrow,
		Marker,
		Eraser,
		Blur,
	};
	float64 sizeRatio = 0.;
	QColor color;
	Tool tool = Tool::Pen;
};

enum class SaveState {
	Save,
	Keep,
};

} // namespace Editor
