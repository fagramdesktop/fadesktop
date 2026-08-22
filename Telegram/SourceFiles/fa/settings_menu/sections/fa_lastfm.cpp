/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/settings_menu/sections/fa_lastfm.h"
#include "fa/settings_menu/fa_deeplink_context_menu.h"
#include "fa/settings/fa_settings.h"
#include "fa/lastfm/fa_lastfm_bio_helper.h"
#include "fa/ui/md3/fa_cards.h"
#include "fa_lang_auto.h"

#include "window/window_session_controller.h"
#include "main/main_session.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "styles/style_basic.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include "main/main_session_settings.h"
#include "storage/storage_account.h"

namespace Settings {
namespace {

not_null<Ui::RpWidget*> AddCardInputField(
		not_null<Ui::VerticalLayout*> card,
		rpl::producer<QString> title,
		rpl::producer<QString> subtitle,
		const QString &initialValue,
		bool isPassword,
		Fn<void(QString)> onChange) {
	const auto row = card->add(object_ptr<Ui::VerticalLayout>(card));

	const auto titleLabel = row->add(
		object_ptr<Ui::FlatLabel>(
			row,
			std::move(title),
			st::defaultFlatLabel),
		style::margins(16, 12, 16, 0));
	titleLabel->setFont(st::semiboldFont);

	if (subtitle) {
		const auto subLabel = row->add(
			object_ptr<Ui::FlatLabel>(
				row,
				std::move(subtitle),
				st::defaultFlatLabel),
			style::margins(16, 4, 16, 0));
		subLabel->setTextColorOverride(st::windowSubTextFg->c);
	}

	if (isPassword) {
		const auto container = row->add(
			object_ptr<Ui::RpWidget>(row),
			style::margins(16, 8, 16, 12));
		container->resize(container->width(), st::defaultInputField.heightMin);
		const auto pwdField = Ui::CreateChild<Ui::PasswordInput>(
			container,
			st::defaultInputField,
			nullptr,
			initialValue);
		container->widthValue() | rpl::on_next([=](int width) {
			pwdField->resize(width, pwdField->height());
		}, pwdField->lifetime());
		QObject::connect(pwdField, &Ui::PasswordInput::changed, [=] {
			onChange(pwdField->getLastText());
		});
		row->resizeToWidth(card->width());
		return container;
	} else {
		const auto field = row->add(
			object_ptr<Ui::InputField>(
				row,
				st::defaultInputField,
				nullptr,
				initialValue),
			style::margins(16, 8, 16, 12));
		field->changes() | rpl::on_next([=] {
			onChange(field->getLastText());
		}, field->lifetime());
		row->resizeToWidth(card->width());
		return field;
	}
}

} // namespace

rpl::producer<QString> FALastFm::title() {
	return fatr::fa_lastfm();
}

FALastFm::FALastFm(
	QWidget *parent,
	not_null<Window::SessionController *> controller)
: Section(parent, controller) {
	setupContent(controller);
}

void FALastFm::setupContent(not_null<Window::SessionController *> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	Ui::AddSkip(content);

	FA::Ui::AddModernSectionHeader(content, fatr::fa_lastfm());
	const auto card = FA::Ui::CreateCardContainer(content);
	const auto session = &controller->session();

	const auto usernameRow = AddCardInputField(
		card,
		fatr::fa_lastfm_username(),
		fatr::fa_lastfm_username_desc(),
		session->settings().lastFmUsername(),
		false,
		[=](QString value) {
			const auto trimmed = value.trimmed();
			session->settings().setLastFmUsername(trimmed);
			session->local().writeSessionSettings();
			Fa::LastFm::ScheduleBioTagSync(
				session,
				session->settings().lastFmShowOnProfile(),
				trimmed,
				1000);
		});
	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
		usernameRow, u"fa/lastfm/username"_q, controller);

	FA::Ui::AddCardDivider(card);

	const auto customKeyRow = FA::Ui::AddCardToggle(
		card,
		fatr::fa_lastfm_custom_key(),
		fatr::fa_lastfm_custom_key_desc(),
		session->settings().lastFmUseCustomApiKeyValue(),
		[=](bool enabled) {
			session->settings().setLastFmUseCustomApiKey(enabled);
			session->local().writeSessionSettings();
		});
	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
		customKeyRow, u"fa/lastfm/custom-key"_q, controller);

	const auto keyWrap = card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			card,
			object_ptr<Ui::VerticalLayout>(card)));
	const auto keyContainer = keyWrap->entity();
	Ui::AddSkip(keyContainer, 4);

	AddCardInputField(
		keyContainer,
		fatr::fa_lastfm_api_key_field(),
		nullptr,
		session->settings().lastFmCustomApiKey(),
		true,
		[=](QString value) {
			session->settings().setLastFmCustomApiKey(value.trimmed());
			session->local().writeSessionSettings();
		});

	keyWrap->toggleOn(
		session->settings().lastFmUseCustomApiKeyValue(),
		anim::type::normal);

	FA::Ui::AddCardDivider(card);

	const auto showProfileRow = FA::Ui::AddCardToggle(
		card,
		fatr::fa_lastfm_show_on_profile(),
		fatr::fa_lastfm_show_on_profile_desc(),
		session->settings().lastFmShowOnProfileValue(),
		[=](bool enabled) {
			session->settings().setLastFmShowOnProfile(enabled);
			session->local().writeSessionSettings();
			Fa::LastFm::SyncBioTag(
				session,
				enabled,
				session->settings().lastFmUsername());
		});
	Settings::FADeepLinkMenu::AttachSettingsContextMenu(
		showProfileRow, u"fa/lastfm/show-on-profile"_q, controller);

	Ui::ResizeFitChild(this, content);
}

} // namespace Settings
