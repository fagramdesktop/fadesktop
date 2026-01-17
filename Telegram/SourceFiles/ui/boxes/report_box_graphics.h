/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace style {
struct ReportBox;
} // namespace style

namespace Ui {

class AbstractButton;
class GenericBox;
class VerticalLayout;

enum class ReportSource {
	Message,
	Channel,
	Group,
	Bot,
	ProfilePhoto,
	ProfileVideo,
	GroupPhoto,
	GroupVideo,
	ChannelPhoto,
	ChannelVideo,
	Story,
};

enum class ReportReason {
	Spam,
	Fake,
	Violence,
	ChildAbuse,
	Pornography,
	Copyright,
	IllegalDrugs,
	PersonalDetails,
	Other,
};

void ReportReasonBox(
	not_null<GenericBox*> box,
	const style::ReportBox &st,
	ReportSource source,
	Fn<void(ReportReason)> done);

void ReportDetailsBox(
	not_null<GenericBox*> box,
	const style::ReportBox &st,
	Fn<void(QString)> done);

[[nodiscard]] not_null<Ui::AbstractButton*> AddReportOptionButton(
	not_null<Ui::VerticalLayout*> container,
	const QString &text,
	const style::ReportBox *stOverride);

void AddReportDetailsIconButton(not_null<GenericBox*> box);

} // namespace Ui
