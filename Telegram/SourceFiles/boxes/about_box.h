/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/generic_box.h"

void AboutBox(not_null<Ui::GenericBox*> box);
void ArchiveHintBox(
	not_null<Ui::GenericBox*> box,
	bool unarchiveOnNewMessage,
	Fn<void()> onUnarchive);

QString telegramFaqLink();
QString currentVersionText();
