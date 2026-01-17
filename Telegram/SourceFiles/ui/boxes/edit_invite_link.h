/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui {

class GenericBox;
class NumberInput;
class SettingsButton;

struct InviteLinkFields {
	QString link;
	QString label;
	TimeId expireDate = 0;
	int usageLimit = 0;
	int subscriptionCredits = 0;
	bool requestApproval = false;
	bool isGroup = false;
	bool isPublic = false;
};

struct InviteLinkSubscriptionToggle final {
	not_null<Ui::SettingsButton*> button;
	not_null<Ui::NumberInput*> amount;
};

void EditInviteLinkBox(
	not_null<Ui::GenericBox*> box,
	Fn<InviteLinkSubscriptionToggle()> fillSubscription,
	const InviteLinkFields &data,
	Fn<void(InviteLinkFields)> done);

void CreateInviteLinkBox(
	not_null<Ui::GenericBox*> box,
	Fn<InviteLinkSubscriptionToggle()> fillSubscription,
	bool isGroup,
	bool isPublic,
	Fn<void(InviteLinkFields)> done);

} // namespace Ui
