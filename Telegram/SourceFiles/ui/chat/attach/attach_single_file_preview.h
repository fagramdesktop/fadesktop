/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/chat/attach/attach_abstract_single_file_preview.h"

namespace Ui {

struct PreparedFile;

class SingleFilePreview final : public AbstractSingleFilePreview {
public:
	SingleFilePreview(
		QWidget *parent,
		const style::ComposeControls &st,
		const PreparedFile &file,
		AttachControls::Type type = AttachControls::Type::Full);

private:
	void preparePreview(const PreparedFile &file);

};

} // namespace Ui
