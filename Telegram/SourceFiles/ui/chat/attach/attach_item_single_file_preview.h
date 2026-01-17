/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

#include "ui/chat/attach/attach_abstract_single_file_preview.h"

class HistoryItem;
class DocumentData;

namespace Data {
class DocumentMedia;
} // namespace Data

namespace Ui {

struct PreparedFile;
class IconButton;

class ItemSingleFilePreview final : public AbstractSingleFilePreview {
public:
	ItemSingleFilePreview(
		QWidget *parent,
		const style::ComposeControls &st,
		not_null<HistoryItem*> item,
		AttachControls::Type type);

private:
	void preparePreview(not_null<DocumentData*> document);

	std::shared_ptr<::Data::DocumentMedia> _documentMedia;

	rpl::lifetime _lifetimeDownload;

};

} // namespace Ui
