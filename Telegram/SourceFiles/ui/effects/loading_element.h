/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

template <typename Object>
class object_ptr;

namespace style {
struct TextStyle;
struct PeerListItem;
struct DialogRow;
} // namespace style

namespace Ui {

class RpWidget;

object_ptr<Ui::RpWidget> CreateLoadingTextWidget(
	not_null<Ui::RpWidget*> parent,
	const style::TextStyle &st,
	int lines,
	rpl::producer<bool> rtl);

object_ptr<Ui::RpWidget> CreateLoadingPeerListItemWidget(
	not_null<Ui::RpWidget*> parent,
	const style::PeerListItem &st,
	int lines,
	std::optional<QColor> bgOverride);

object_ptr<Ui::RpWidget> CreateLoadingDialogRowWidget(
	not_null<Ui::RpWidget*> parent,
	const style::DialogRow &st,
	int lines);

} // namespace Ui
