/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

class PeerData;
class DocumentData;

namespace Api {
struct SendOptions;
} // namespace Api

namespace SendMenu {
struct Details;
} // namespace SendMenu

namespace HistoryView {
class Element;
} // namespace HistoryView

namespace Ui {

class GenericBox;

void EditCaptionBox(
	not_null<Ui::GenericBox*> box,
	not_null<HistoryView::Element*> view);

void SendGifWithCaptionBox(
	not_null<Ui::GenericBox*> box,
	not_null<DocumentData*> document,
	not_null<PeerData*> peer,
	const SendMenu::Details &details,
	Fn<void(Api::SendOptions, TextWithTags)> done);

} // namespace Ui
