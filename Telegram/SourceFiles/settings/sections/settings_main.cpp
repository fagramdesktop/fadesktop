/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "fa/settings_menu/fa_settings_menu.h"
#include "fa/ui/md3/fa_cards.h"
#include "fa/ui/md3/fa_slider.h"

#include "fa_lang_auto.h"

#include "settings/sections/settings_main.h"
#include "settings/settings_common_session.h"

#include "api/api_cloud_password.h"
#include "api/api_credits.h"
#include "api/api_global_privacy.h"
#include "api/api_peer_photo.h"
#include "api/api_premium.h"
#include "api/api_sensitive_content.h"
#include "apiwrap.h"
#include "base/call_delayed.h"
#include "base/platform/base_platform_info.h"
#include "boxes/language_box.h"
#include "boxes/star_gift_box.h"
#include "boxes/username_box.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "data/components/credits.h"
#include "data/components/promo_suggestions.h"
#include "data/data_chat_filters.h"
#include "data/data_cloud_themes.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "info/profile/info_profile_badge.h"
#include "info/profile/info_profile_emoji_status_panel.h"
#include "info/profile/info_profile_phone_menu.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_cloud_manager.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "lottie/lottie_icon.h"
#include "menu/menu_checked_action.h"
#include "main/main_account.h"
#include "main/main_app_config.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "settings/settings_builder.h"
#include "settings/cloud_password/settings_cloud_password_input.h"
#include "settings/sections/settings_advanced.h"
#include "settings/sections/settings_business.h"
#include "settings/sections/settings_calls.h"
#include "settings/sections/settings_chat.h"
#include "settings/settings_codes.h"
#include "settings/settings_faq_suggestions.h"
#include "settings/sections/settings_credits.h"
#include "settings/sections/settings_folders.h"
#include "settings/sections/settings_information.h"
#include "settings/sections/settings_notifications.h"
#include "settings/settings_power_saving.h"
#include "settings/sections/settings_premium.h"
#include "settings/sections/settings_privacy_security.h"
#include "settings/settings_scale_preview.h"
#include "storage/localstorage.h"
#include "ui/basic_click_handlers.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/peer_qr_box.h"
#include "ui/controls/userpic_button.h"
#include "ui/layers/generic_box.h"
#include "ui/new_badges.h"
#include "ui/painter.h"
#include "ui/power_saving.h"
#include "ui/rect.h"
#include "ui/text/format_values.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/menu/menu_add_action_callback.h"
#include "ui/widgets/menu/menu_item_base.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/slide_wrap.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_info.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

namespace Settings {
namespace FAUi = ::FA::Ui;
namespace {

using namespace Builder;

constexpr auto kSugValidatePhone = "VALIDATE_PHONE_NUMBER"_cs;

class Cover final : public Ui::FixedHeightWidget {
public:
	Cover(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user);
	~Cover();

	[[nodiscard]] not_null<Ui::UserpicButton*> userpic() const {
		return _userpic.data();
	}

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	void setupChildGeometry();
	void initViewers();
	void updatePhoneText();
	void refreshNameGeometry(int newWidth);
	void refreshPhoneGeometry(int newWidth);
	void refreshUsernameGeometry(int newWidth);
	void refreshQrButtonGeometry(int newWidth);

	const not_null<Window::SessionController*> _controller;
	const not_null<UserData*> _user;
	Info::Profile::EmojiStatusPanel _emojiStatusPanel;
	Info::Profile::Badge _badge;

	object_ptr<Ui::UserpicButton> _userpic;
	object_ptr<Ui::FlatLabel> _name = { nullptr };
	object_ptr<Ui::FlatLabel> _phone = { nullptr };
	QString _phoneText;
	object_ptr<Ui::FlatLabel> _username = { nullptr };
	object_ptr<Ui::IconButton> _qrButton = { nullptr };

};

const auto kCustomCoverUserpicStyle = [] {
	auto st = st::infoProfileCover.photo;
	st.size = QSize(96, 96);
	st.photoSize = 96;
	st.photoPosition = QPoint(-1, -1);
	return st;
}();

Cover::Cover(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user)
: FixedHeightWidget(
	parent,
	112)
, _controller(controller)
, _user(user)
, _badge(
	this,
	st::settingsCoverBadge,
	&user->session(),
	Info::Profile::BadgeContentForPeer(user),
	&_emojiStatusPanel,
	[=] {
		return controller->isGifPausedAtLeastFor(
			Window::GifPauseReason::Layer);
	},
	0, // customStatusLoopsLimit
	Info::Profile::BadgeType::Premium)
, _userpic(
	this,
	controller,
	_user,
	Ui::UserpicButton::Role::OpenPhoto,
	Ui::UserpicButton::Source::PeerPhoto,
	kCustomCoverUserpicStyle,
	Ui::PeerUserpicShape::Material)
, _name(this, st::infoProfileCover.name)
, _phone(this, st::defaultFlatLabel, st::popupMenuWithIcons)
, _username(this, st::infoProfileMegagroupCover.status) {
	_user->updateFull();
	_userpic->overrideShape(Ui::PeerUserpicShape::Material);

	_name->setSelectable(true);
	_name->setContextCopyText(tr::lng_profile_copy_fullname(tr::now));

	_phone->setSelectable(true);
	_phone->setContextCopyText(tr::lng_profile_copy_phone(tr::now));
	const auto hook = [=](Ui::FlatLabel::ContextMenuRequest request) {
		if (request.selection.empty()) {
			const auto callback = [=] {
				Info::Profile::CopyPhoneToClipboard(
					Info::Profile::PhoneValue(_user));
			};
			request.menu->addAction(
				tr::lng_profile_copy_phone(tr::now),
				callback,
				&st::menuIconCopy);
		} else {
			_phone->fillContextMenu(request);
		}
		Info::Profile::AddPhoneSpoilerMenu(request.menu, _user);
	};
	_phone->setContextMenuHook(hook);

	initViewers();
	setupChildGeometry();

	_userpic->switchChangePhotoOverlay(_user->isSelf(), [=](
			Ui::UserpicButton::ChosenImage chosen) {
		auto &image = chosen.image;
		_userpic->showCustom(base::duplicate(image));
		const auto isMarkup = (chosen.markup.documentId != 0);
		_user->session().api().peerPhoto().upload(
			_user,
			{
				std::move(image),
				chosen.markup.documentId,
				chosen.markup.colors,
			});
		if (!isMarkup) {
			_userpic->showUploadProgress();
		}
	});

	_badge.setPremiumClickCallback([=] {
		_emojiStatusPanel.show(
			_controller,
			_badge.widget(),
			_badge.sizeTag());
	});
	_badge.updated() | rpl::on_next([=] {
		refreshNameGeometry(width());
	}, _name->lifetime());

	_qrButton.create(this, st::infoProfileLabeledButtonQr);
	_qrButton->setAccessibleName(tr::lng_group_invite_context_qr(tr::now));
	_qrButton->setClickedCallback([=, show = controller->uiShow()] {
		Ui::DefaultShowFillPeerQrBoxCallback(show, _user);
	});
	Info::Profile::UsernamesValue(
		_user
	) | rpl::on_next([=](const auto &usernames) {
		_qrButton->setVisible(!usernames.empty());
		refreshNameGeometry(width());
		refreshQrButtonGeometry(width());
	}, _qrButton->lifetime());
}

Cover::~Cover() = default;

void Cover::paintEvent(QPaintEvent *e) {
	Painter p(this);
	PainterHighQualityEnabler hq(p);
	const auto photoLeft = 16.0;
	const auto cardLeft = photoLeft + _userpic->width() + 12.0;
	const auto cardTop = 8.0;
	const auto cardHeight = height() - 16.0;
	const auto cardWidth = width() - 16.0 - cardLeft;
	if (cardWidth <= 0) {
		return;
	}

	const auto rLeft = 24.0;
	const auto rRight = cardHeight / 2.0;

	auto path = QPainterPath();
	path.moveTo(cardLeft + rLeft, cardTop);
	path.lineTo(cardLeft + cardWidth - rRight, cardTop);
	path.arcTo(QRectF(cardLeft + cardWidth - 2 * rRight, cardTop, 2 * rRight, cardHeight), 90, -180);
	path.lineTo(cardLeft + rLeft, cardTop + cardHeight);
	path.arcTo(QRectF(cardLeft, cardTop + cardHeight - 2 * rLeft, 2 * rLeft, 2 * rLeft), 270, -90);
	path.lineTo(cardLeft, cardTop + rLeft);
	path.arcTo(QRectF(cardLeft, cardTop, 2 * rLeft, 2 * rLeft), 180, -90);
	path.closeSubpath();

	p.setPen(Qt::NoPen);
	p.setBrush(st::settingsThemeNotSupportedBg);
	p.drawPath(path);
}

void Cover::setupChildGeometry() {
	using namespace rpl::mappers;
	widthValue(
	) | rpl::on_next([=](int newWidth) {
		const auto photoLeft = 16;
		const auto photoTop = (height() - _userpic->height()) / 2;
		_userpic->moveToLeft(
			photoLeft,
			photoTop,
			newWidth);
		refreshNameGeometry(newWidth);
		refreshPhoneGeometry(newWidth);
		refreshUsernameGeometry(newWidth);
		refreshQrButtonGeometry(newWidth);
	}, lifetime());
}

void Cover::initViewers() {
	Info::Profile::NameValue(
		_user
	) | rpl::on_next([=](const QString &name) {
		_name->setText(name);
		refreshNameGeometry(width());
	}, lifetime());

	Info::Profile::PhoneValue(
		_user
	) | rpl::on_next([=](const TextWithEntities &value) {
		_phoneText = value.text;
		updatePhoneText();
	}, lifetime());

	_user->session().settings().phoneNumberHiddenValue(
	) | rpl::on_next([=] {
		updatePhoneText();
	}, lifetime());

	Info::Profile::UsernameValue(
		_user
	) | rpl::on_next([=](const TextWithEntities &value) {
		_username->setMarkedText(tr::link(value.text.isEmpty()
			? tr::lng_settings_username_add(tr::now)
			: value.text));
		refreshUsernameGeometry(width());
	}, lifetime());

	_username->overrideLinkClickHandler([=] {
		if (_controller->showFrozenError()) {
			return;
		}
		const auto username = _user->username();
		if (username.isEmpty()) {
			_controller->show(Box(UsernamesBox, _user));
		} else {
			QGuiApplication::clipboard()->setText(
				_user->session().createInternalLinkFull(username));
			_controller->showToast({
				.text = { tr::lng_username_copied(tr::now) },
				.iconLottie = u"toast/voip_invite"_q,
				.iconLottieSize = st::toastLottieIconSize,
			});
		}
	});
}

void Cover::refreshNameGeometry(int newWidth) {
	const auto cardLeft = 16 + _userpic->width() + 12;
	const auto cardPadding = 18;
	const auto textLeft = cardLeft + cardPadding;
	const auto cardTop = (height() - 96) / 2;
	const auto nameTop = cardTop + 16;
	const auto qrSpace = (_qrButton && !_qrButton->isHidden())
		? (_qrButton->width() + 20 + 8)
		: 20;
	const auto cardWidth = newWidth - 16 - cardLeft;
	auto nameWidth = cardWidth
		- cardPadding
		- qrSpace;
	if (const auto width = _badge.widget() ? _badge.widget()->width() : 0) {
		nameWidth -= st::infoVerifiedCheckPosition.x() + width;
	}
	_name->resizeToNaturalWidth(nameWidth);
	_name->moveToLeft(textLeft, nameTop, newWidth);
	const auto badgeLeft = textLeft + _name->width();
	const auto badgeTop = nameTop;
	const auto badgeBottom = nameTop + _name->height();
	_badge.move(badgeLeft, badgeTop, badgeBottom);
}

void Cover::updatePhoneText() {
	if (_user->session().settings().phoneNumberHidden()) {
		_phone->setMarkedText(
			Ui::Text::Wrapped({ _phoneText }, EntityType::Spoiler));
	} else {
		_phone->setText(_phoneText);
	}
	refreshPhoneGeometry(width());
}

void Cover::refreshPhoneGeometry(int newWidth) {
	const auto cardLeft = 16 + _userpic->width() + 12;
	const auto cardPadding = 18;
	const auto textLeft = cardLeft + cardPadding;
	const auto cardTop = (height() - 96) / 2;
	const auto phoneTop = cardTop + 40;
	const auto qrSpace = (_qrButton && !_qrButton->isHidden())
		? (_qrButton->width() + 20 + 8)
		: 20;
	const auto cardWidth = newWidth - 16 - cardLeft;
	const auto phoneWidth = cardWidth
		- cardPadding
		- qrSpace;
	_phone->resizeToWidth(phoneWidth);
	_phone->moveToLeft(textLeft, phoneTop, newWidth);
}

void Cover::refreshUsernameGeometry(int newWidth) {
	const auto cardLeft = 16 + _userpic->width() + 12;
	const auto cardPadding = 18;
	const auto textLeft = cardLeft + cardPadding;
	const auto cardTop = (height() - 96) / 2;
	const auto usernameTop = cardTop + 62;
	const auto qrSpace = (_qrButton && !_qrButton->isHidden())
		? (_qrButton->width() + 20 + 8)
		: 20;
	const auto cardWidth = newWidth - 16 - cardLeft;
	const auto usernameWidth = cardWidth
		- cardPadding
		- qrSpace;
	_username->resizeToWidth(usernameWidth);
	_username->moveToLeft(textLeft, usernameTop, newWidth);
}

void Cover::refreshQrButtonGeometry(int newWidth) {
	if (!_qrButton) {
		return;
	}
	const auto cardTop = (height() - 96) / 2;
	const auto cardHeight = 96;
	const auto buttonTop = cardTop + (cardHeight - _qrButton->height()) / 2;
	const auto buttonRight = 16 + 20;
	_qrButton->moveToRight(buttonRight, buttonTop, newWidth);
}

void BuildSectionButtons(SectionBuilder &builder) {
	const auto session = builder.session();
	const auto controller = builder.controller();
	const auto showOther = builder.showOther();
	const auto container = builder.container();

	if (!container) {
		builder.addSectionButton({
			.title = fatr::fa_client_preferences(),
			.targetSection = FA::Id(),
			.icon = { &st::menuIconSettings },
			.keywords = { u"fagram"_q, u"preferences"_q, u"fa"_q },
		});

		if (!session->supportMode()) {
			builder.addSectionButton({
				.title = tr::lng_settings_my_account(),
				.targetSection = InformationId(),
				.icon = { &st::menuIconProfile },
				.keywords = { u"profile"_q, u"edit"_q, u"information"_q },
			});
		}

		builder.addSectionButton({
			.title = tr::lng_settings_section_notify(),
			.targetSection = NotificationsId(),
			.icon = { &st::menuIconNotifications },
			.keywords = { u"alerts"_q, u"sounds"_q, u"badge"_q },
		});

		builder.addSectionButton({
			.title = tr::lng_settings_section_privacy(),
			.targetSection = PrivacySecurityId(),
			.icon = { &st::menuIconLock },
			.keywords = { u"security"_q, u"passcode"_q, u"password"_q, u"2fa"_q },
		});

		builder.addSectionButton({
			.title = tr::lng_settings_section_chat_settings(),
			.targetSection = ChatId(),
			.icon = { &st::menuIconChatBubble },
			.keywords = { u"themes"_q, u"appearance"_q, u"stickers"_q },
		});

		builder.addButton({
			.title = tr::lng_settings_section_filters(),
			.icon = { &st::menuIconShowInFolder },
			.onClick = [=] { showOther(FoldersId()); },
			.keywords = { u"filters"_q, u"tabs"_q },
		});

		builder.addSectionButton({
			.title = tr::lng_settings_advanced(),
			.targetSection = AdvancedId(),
			.icon = { &st::menuIconManage },
			.keywords = { u"performance"_q, u"proxy"_q, u"experimental"_q },
		});

		builder.addSectionButton({
			.title = tr::lng_settings_section_devices(),
			.targetSection = CallsId(),
			.icon = { &st::menuIconUnmute },
			.keywords = { u"sessions"_q, u"calls"_q },
		});

		builder.addButton({
			.id = u"main/power"_q,
			.title = tr::lng_settings_power_menu(),
			.icon = { &st::menuIconPowerUsage },
			.onClick = [=] {
				controller->show(Box(PowerSavingBox, PowerSaving::Flags()));
			},
			.keywords = { u"battery"_q, u"animations"_q, u"power"_q, u"saving"_q },
		});

		builder.addButton({
			.id = u"main/language"_q,
			.title = tr::lng_settings_language(),
			.icon = { &st::menuIconTranslate },
			.label = rpl::single(
				Lang::GetInstance().id()
			) | rpl::then(
				Lang::GetInstance().idChanges()
			) | rpl::map([] { return Lang::GetInstance().nativeName(); }),
			.onClick = [=] {
				static auto Guard = base::binary_guard();
				Guard = LanguageBox::Show(controller);
			},
			.keywords = { u"translate"_q, u"localization"_q, u"language"_q },
		});
		return;
	}

	const auto card = FAUi::CreateCardContainer(container, 8, 8);

	FAUi::AddCardButton(
		card,
		fatr::fa_client_preferences(),
		[=] { showOther(FA::Id()); },
		&st::menuIconSettings,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	if (!session->supportMode()) {
		FAUi::AddCardButton(
			card,
			tr::lng_settings_my_account(),
			[=] { showOther(InformationId()); },
			&st::menuIconProfile,
			nullptr,
			true);
		FAUi::AddCardDivider(card);
	}

	FAUi::AddCardButton(
		card,
		tr::lng_settings_section_notify(),
		[=] { showOther(NotificationsId()); },
		&st::menuIconNotifications,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_section_privacy(),
		[=] { showOther(PrivacySecurityId()); },
		&st::menuIconLock,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_section_chat_settings(),
		[=] { showOther(ChatId()); },
		&st::menuIconChatBubble,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	{
		const auto preload = [=] {
			session->data().chatsFilters().requestSuggested();
		};
		const auto hasFilters = session->data().chatsFilters().has()
			|| session->settings().dialogsFiltersEnabled();

		if (hasFilters) {
			preload();
		}

		const auto foldersWrap = card->add(
			object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
				card,
				object_ptr<Ui::VerticalLayout>(card)));
		const auto foldersContent = foldersWrap->entity();

		FAUi::AddCardButton(
			foldersContent,
			tr::lng_settings_section_filters(),
			[=] { showOther(FoldersId()); },
			&st::menuIconShowInFolder,
			nullptr,
			true);
		FAUi::AddCardDivider(foldersContent);

		if (!hasFilters) {
			foldersWrap->hide(anim::type::instant);
			session->appConfig().refreshed(
			) | rpl::on_next([=] {
				const auto enabled = session->appConfig().get<bool>(
					u"dialog_filters_enabled"_q,
					false);
				if (enabled) {
					preload();
					foldersWrap->show(anim::type::normal);
				}
			}, foldersWrap->lifetime());
		}
	}

	FAUi::AddCardButton(
		card,
		tr::lng_settings_advanced(),
		[=] { showOther(AdvancedId()); },
		&st::menuIconManage,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_section_devices(),
		[=] { showOther(CallsId()); },
		&st::menuIconUnmute,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_power_menu(),
		[=] {
			controller->show(Box(PowerSavingBox, PowerSaving::Flags()));
		},
		&st::menuIconPowerUsage,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_language(),
		[=] {
			static auto Guard = base::binary_guard();
			Guard = LanguageBox::Show(controller);
		},
		&st::menuIconTranslate,
		rpl::single(
			Lang::GetInstance().id()
		) | rpl::then(
			Lang::GetInstance().idChanges()
		) | rpl::map([] { return Lang::GetInstance().nativeName(); }),
		true);
}

void BuildInterfaceScale(SectionBuilder &builder) {
	if (!HasInterfaceScale()) {
		return;
	}

	const auto container = builder.container();
	if (!container) {
		builder.add([](const WidgetContext &ctx) {
			return SectionBuilder::WidgetToAdd{};
		}, [] {
			return SearchEntry{
				.id = u"main/scale"_q,
				.title = tr::lng_settings_default_scale(tr::now),
				.keywords = { u"zoom"_q, u"size"_q, u"interface"_q, u"ui"_q },
			};
		});
		return;
	}

	const auto window = &builder.controller()->window();
	SetupInterfaceScale(window, container);
}

void BuildPremiumSection(SectionBuilder &builder) {
	const auto session = builder.session();
	const auto controller = builder.controller();
	const auto showOther = builder.showOther();

	if (!session->premiumPossible()) {
		return;
	}

	const auto container = builder.container();
	if (!container) {
		builder.addPremiumButton({
			.id = u"main/premium"_q,
			.title = tr::lng_premium_summary_title(),
			.onClick = [=] {
				controller->setPremiumRef("settings");
				showOther(PremiumId());
			},
			.keywords = { u"subscription"_q },
		});

		session->credits().load();
		builder.addPremiumButton({
			.id = u"main/credits"_q,
			.title = tr::lng_settings_credits(),
			.label = session->credits().balanceValue(
			) | rpl::map([](CreditsAmount c) {
				return c
					? Lang::FormatCreditsAmountToShort(c).string
					: QString();
			}),
			.credits = true,
			.onClick = [=] {
				controller->setPremiumRef("settings");
				showOther(CreditsId());
			},
			.keywords = { u"stars"_q, u"balance"_q },
		});

		session->credits().tonLoad();
		builder.addButton({
			.id = u"main/currency"_q,
			.title = tr::lng_settings_currency(),
			.icon = { &st::menuIconTon },
			.label = session->credits().tonBalanceValue(
			) | rpl::map([](CreditsAmount c) {
				return c ? Lang::FormatCreditsAmountToShort(c).string : u""_q;
			}),
			.onClick = [=] {
				controller->setPremiumRef("settings");
				showOther(CurrencyId());
			},
			.keywords = { u"ton"_q, u"crypto"_q, u"wallet"_q },
			.shown = session->credits().tonBalanceValue(
			) | rpl::map([](CreditsAmount c) { return !c.empty(); }),
		});

		builder.addButton({
			.id = u"main/business"_q,
			.title = tr::lng_business_title(),
			.icon = { .icon = &st::menuIconShop },
			.onClick = [=] { showOther(BusinessId()); },
			.keywords = { u"work"_q, u"company"_q },
		});

		if (session->premiumCanBuy()) {
			builder.addButton({
				.id = u"main/send-gift"_q,
				.title = tr::lng_settings_gift_premium(),
				.icon = { .icon = &st::menuIconGiftPremium, .newBadge = true },
				.onClick = [=] { Ui::ChooseStarGiftRecipient(controller); },
				.keywords = { u"present"_q, u"send"_q },
			});
		}
		return;
	}

	const auto card = FAUi::CreateCardContainer(container, 0, 8);

	FAUi::AddCardButton(
		card,
		tr::lng_premium_summary_title(),
		[=] {
			controller->setPremiumRef("settings");
			showOther(PremiumId());
		},
		&st::menuIconPremium,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	session->credits().load();
	FAUi::AddCardButton(
		card,
		tr::lng_settings_credits(),
		[=] {
			controller->setPremiumRef("settings");
			showOther(CreditsId());
		},
		&st::menuIconStar,
		session->credits().balanceValue(
		) | rpl::map([](CreditsAmount c) {
			return c
				? Lang::FormatCreditsAmountToShort(c).string
				: QString();
		}),
		true);

	session->credits().tonLoad();
	const auto currencyWrap = card->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			card,
			object_ptr<Ui::VerticalLayout>(card)));
	const auto currencyContent = currencyWrap->entity();
	FAUi::AddCardDivider(currencyContent);
	FAUi::AddCardButton(
		currencyContent,
		tr::lng_settings_currency(),
		[=] {
			controller->setPremiumRef("settings");
			showOther(CurrencyId());
		},
		&st::menuIconTon,
		session->credits().tonBalanceValue(
		) | rpl::map([](CreditsAmount c) {
			return c ? Lang::FormatCreditsAmountToShort(c).string : u""_q;
		}),
		true);

	currencyWrap->hide(anim::type::instant);
	session->credits().tonBalanceValue(
	) | rpl::on_next([=](CreditsAmount c) {
		currencyWrap->toggle(!c.empty(), anim::type::instant);
	}, currencyWrap->lifetime());

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_business_title(),
		[=] { showOther(BusinessId()); },
		&st::menuIconShop,
		nullptr,
		true);

	if (session->premiumCanBuy()) {
		FAUi::AddCardDivider(card);
		FAUi::AddCardButton(
			card,
			tr::lng_settings_gift_premium(),
			[=] { Ui::ChooseStarGiftRecipient(controller); },
			&st::menuIconGiftPremium,
			nullptr,
			true);
	}
}

void BuildHelpSection(SectionBuilder &builder) {
	const auto controller = builder.controller();
	const auto container = builder.container();

	if (!container) {
		builder.addButton({
			.id = u"main/faq"_q,
			.title = tr::lng_settings_faq(),
			.icon = { &st::menuIconFaq },
			.keywords = { u"help"_q, u"support"_q, u"questions"_q },
		});

		builder.addButton({
			.id = u"main/features"_q,
			.title = tr::lng_settings_features(),
			.icon = { &st::menuIconEmojiObjects },
			.keywords = { u"tips"_q, u"tutorial"_q },
		});

		builder.addButton({
			.id = u"main/ask-question"_q,
			.title = tr::lng_settings_ask_question(),
			.icon = { &st::menuIconDiscussion },
			.keywords = { u"contact"_q, u"feedback"_q },
		});
		return;
	}

	const auto card = FAUi::CreateCardContainer(container, 0, 8);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_faq(),
		[=] { OpenFaq(controller); },
		&st::menuIconFaq,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_features(),
		[] {
			UrlClickHandler::Open(tr::lng_telegram_features_url(tr::now));
		},
		&st::menuIconEmojiObjects,
		nullptr,
		true);

	FAUi::AddCardDivider(card);

	FAUi::AddCardButton(
		card,
		tr::lng_settings_ask_question(),
		[=] { OpenAskQuestionConfirm(controller); },
		&st::menuIconDiscussion,
		nullptr,
		true);
}

void BuildValidationSuggestions(SectionBuilder &builder) {
	builder.add([](const WidgetContext &ctx) {
		const auto controller = ctx.controller.get();
		const auto showOther = ctx.showOther;
		auto wrap = object_ptr<Ui::VerticalLayout>(ctx.container);
		SetupValidatePhoneNumberSuggestion(controller, wrap.data(), showOther);
		return SectionBuilder::WidgetToAdd{ .widget = std::move(wrap) };
	});

	builder.add([](const WidgetContext &ctx) {
		const auto controller = ctx.controller.get();
		const auto showOther = ctx.showOther;
		auto wrap = object_ptr<Ui::VerticalLayout>(ctx.container);
		SetupValidatePasswordSuggestion(controller, wrap.data(), showOther);
		return SectionBuilder::WidgetToAdd{ .widget = std::move(wrap) };
	});
}

class Main final : public Section<Main> {
public:
	Main(QWidget *parent, not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

	void fillTopBarMenu(const Ui::Menu::MenuCallback &addAction) override;
	void showFinished() override;

protected:
	void keyPressEvent(QKeyEvent *e) override;

private:
	void setupContent();

	QPointer<Ui::UserpicButton> _userpic;

};

Main::Main(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> Main::title() {
	return tr::lng_menu_settings();
}

void Main::fillTopBarMenu(const Ui::Menu::MenuCallback &addAction) {
	const auto &list = Core::App().domain().accounts();
	if (list.size() < Core::App().domain().maxAccounts()) {
		addAction(tr::lng_menu_add_account(tr::now), [=] {
			Core::App().setActivePrimaryWindow(&controller()->window());
			Core::App().domain().addActivated(MTP::Environment{});
		}, &st::menuIconAddAccount);
	}
	if (!controller()->session().supportMode()) {
		addAction(
			tr::lng_settings_information(tr::now),
			[=] { showOther(InformationId()); },
			&st::menuIconEdit);
	}
	const auto window = &controller()->window();
	const auto logout = addAction({
		.text = tr::lng_settings_logout(tr::now),
		.handler = [=] { window->showLogoutConfirmation(); },
		.icon = &st::menuIconLeaveAttention,
		.isAttention = true,
	});
	logout->setProperty("highlight-control-id", u"settings/log-out"_q);
}

void Main::keyPressEvent(QKeyEvent *e) {
	crl::on_main(this, [=, text = e->text()]{
		CodesFeedString(controller(), text);
	});
	return Section::keyPressEvent(e);
}

void Main::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	const auto window = controller();
	const auto session = &window->session();
	const auto cover = content->add(object_ptr<Cover>(
		content,
		window,
		session->user()));
	_userpic = cover->userpic();

	const SectionBuildMethod buildMethod = [](
			not_null<Ui::VerticalLayout*> container,
			not_null<Window::SessionController*> controller,
			Fn<void(Type)> showOther,
			rpl::producer<> showFinished) {
		auto &lifetime = container->lifetime();
		const auto highlights = lifetime.make_state<HighlightRegistry>();
		const auto isPaused = Window::PausedIn(
			controller,
			Window::GifPauseReason::Layer);
		auto builder = SectionBuilder(WidgetContext{
			.container = container,
			.controller = controller,
			.showOther = std::move(showOther),
			.isPaused = isPaused,
			.highlights = highlights,
		});
		BuildValidationSuggestions(builder);
		BuildSectionButtons(builder);
		BuildInterfaceScale(builder);
		BuildPremiumSection(builder);
		BuildHelpSection(builder);

		std::move(showFinished) | rpl::on_next([=] {
			for (const auto &[id, entry] : *highlights) {
				if (entry.widget) {
					controller->checkHighlightControl(
						id,
						entry.widget,
						base::duplicate(entry.args));
				}
			}
		}, lifetime);
	};
	build(content, buildMethod);

	Ui::ResizeFitChild(this, content);

	session->api().cloudPassword().reload();
	session->api().reloadContactSignupSilent();
	session->api().sensitiveContent().reload();
	session->api().globalPrivacy().reload();
	session->api().premium().reload();
	session->data().cloudThemes().refresh();
	session->faqSuggestions().request();
}

void Main::showFinished() {
	controller()->checkHighlightControl(u"profile-photo"_q, _userpic.data(), {
		.margin = st::settingsPhotoHighlightMargin,
		.shape = HighlightShape::Ellipse,
	});
	const auto emojiId = u"profile-photo/use-emoji"_q;
	if (controller()->takeHighlightControlId(emojiId)) {
		if (const auto popupMenu = _userpic->showChangePhotoMenu()) {
			const auto menu = popupMenu->menu();
			for (const auto &action : menu->actions()) {
				const auto controlId = "highlight-control-id";
				if (action->property(controlId).toString() == emojiId) {
					if (const auto item = menu->itemForAction(action)) {
						HighlightWidget(item);
					}
					break;
				}
			}
		}
	}
	Section<Main>::showFinished();
}

const auto kMeta = BuildHelper({
	.id = Main::Id(),
	.parentId = nullptr,
	.title = &tr::lng_menu_settings,
	.icon = &st::menuIconSettings,
}, [](SectionBuilder &builder) {
	builder.addDivider();
	builder.addSkip();

	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"main/profile-photo"_q,
			.title = tr::lng_profile_set_photo_for(tr::now),
			.keywords = { u"photo"_q, u"avatar"_q, u"picture"_q, u"profile"_q },
			.icon = { &st::menuIconProfile },
			.deeplink = u"tg://settings/profile-photo"_q,
		};
	});

	BuildValidationSuggestions(builder);
	BuildSectionButtons(builder);

	builder.addSkip();

	BuildInterfaceScale(builder);
	BuildPremiumSection(builder);
	BuildHelpSection(builder);
});

} // namespace

void SetupLanguageButton(
		not_null<Window::Controller*> window,
		not_null<Ui::VerticalLayout*> container) {
	const auto button = AddButtonWithLabel(
		container,
		tr::lng_settings_language(),
		rpl::single(
			Lang::GetInstance().id()
		) | rpl::then(
			Lang::GetInstance().idChanges()
		) | rpl::map([] { return Lang::GetInstance().nativeName(); }),
		st::settingsButton,
		{ &st::menuIconTranslate });
	const auto guard = Ui::CreateChild<base::binary_guard>(button.get());
	button->addClickHandler([=] {
		const auto m = button->clickModifiers();
		if ((m & Qt::ShiftModifier) && (m & Qt::AltModifier)) {
			Lang::CurrentCloudManager().switchToLanguage({ u"#custom"_q });
		} else {
			*guard = LanguageBox::Show(window->sessionController());
		}
	});
}

void SetupValidatePhoneNumberSuggestion(
		not_null<Window::SessionController*> controller,
		not_null<Ui::VerticalLayout*> container,
		Fn<void(Type)> showOther) {
	if (!controller->session().promoSuggestions().current(
			kSugValidatePhone.utf8())) {
		return;
	}
	const auto mainWrap = container->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container)));
	const auto content = mainWrap->entity();
	Ui::AddSubsectionTitle(
		content,
		tr::lng_settings_suggestion_phone_number_title(
			lt_phone,
			rpl::single(
				Ui::FormatPhone(controller->session().user()->phone()))),
		QMargins(
			st::boxRowPadding.left()
				- st::defaultSubsectionTitlePadding.left(),
			0,
			0,
			0));
	const auto label = content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			tr::lng_settings_suggestion_phone_number_about(
				lt_link,
				tr::lng_collectible_learn_more(tr::url(
					tr::lng_settings_suggestion_phone_number_about_link(
						tr::now))),
				tr::marked),
			st::boxLabel),
		st::boxRowPadding);
	label->setClickHandlerFilter([=, weak = base::make_weak(controller)](
			const auto &...) {
		UrlClickHandler::Open(
			tr::lng_settings_suggestion_phone_number_about_link(tr::now),
			QVariant::fromValue(ClickHandlerContext{
				.sessionWindow = weak,
			}));
		return false;
	});

	Ui::AddSkip(content);
	Ui::AddSkip(content);

	const auto wrap = content->add(
		object_ptr<Ui::FixedHeightWidget>(
			content,
			st::inviteLinkButton.height),
		st::inviteLinkButtonsPadding);
	const auto yes = Ui::CreateChild<Ui::RoundButton>(
		wrap,
		tr::lng_box_yes(),
		st::inviteLinkButton);
	yes->setFullRadius(true);
	yes->setClickedCallback([=] {
		controller->session().promoSuggestions().dismiss(
			kSugValidatePhone.utf8());
		mainWrap->toggle(false, anim::type::normal);
	});
	const auto no = Ui::CreateChild<Ui::RoundButton>(
		wrap,
		tr::lng_box_no(),
		st::inviteLinkButton);
	no->setFullRadius(true);
	no->setClickedCallback([=] {
		const auto sharedLabel = std::make_shared<base::weak_qptr<Ui::FlatLabel>>();
		const auto height = st::boxLabel.style.font->height;
		const auto customEmojiFactory = [=](
			QStringView data,
			const Ui::Text::MarkedContext &context
		) -> std::unique_ptr<Ui::Text::CustomEmoji> {
			auto repaint = [=] {
				if (*sharedLabel) {
					(*sharedLabel)->update();
				}
			};
			return Lottie::MakeEmoji(
				{ .name = u"change_number"_q, .sizeOverride = Size(height) },
				std::move(repaint));
		};

		controller->uiShow()->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
			*sharedLabel = box->verticalLayout()->add(
				object_ptr<Ui::FlatLabel>(
					box->verticalLayout(),
					tr::lng_settings_suggestion_phone_number_change(
						lt_emoji,
						rpl::single(Ui::Text::SingleCustomEmoji(u"@"_q)),
						tr::marked),
					st::boxLabel,
					st::defaultPopupMenu,
					Ui::Text::MarkedContext{
						.customEmojiFactory = customEmojiFactory,
					}),
				st::boxPadding);
		}));
	});

	wrap->widthValue() | rpl::on_next([=](int width) {
		const auto buttonWidth = (width - st::inviteLinkButtonsSkip) / 2;
		yes->setFullWidth(buttonWidth);
		no->setFullWidth(buttonWidth);
		yes->moveToLeft(0, 0, width);
		no->moveToRight(0, 0, width);
	}, wrap->lifetime());
	Ui::AddSkip(content);
	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);
}

void SetupValidatePasswordSuggestion(
		not_null<Window::SessionController*> controller,
		not_null<Ui::VerticalLayout*> container,
		Fn<void(Type)> showOther) {
	if (!controller->session().promoSuggestions().current(
			Data::PromoSuggestions::SugValidatePassword())
		|| controller->session().promoSuggestions().current(
			kSugValidatePhone.utf8())) {
		return;
	}
	const auto mainWrap = container->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			container,
			object_ptr<Ui::VerticalLayout>(container)));
	const auto content = mainWrap->entity();
	Ui::AddSubsectionTitle(
		content,
		tr::lng_settings_suggestion_password_title(),
		QMargins(
			st::boxRowPadding.left()
				- st::defaultSubsectionTitlePadding.left(),
			0,
			0,
			0));
	content->add(
		object_ptr<Ui::FlatLabel>(
			content,
			tr::lng_settings_suggestion_password_about(),
			st::boxLabel),
		st::boxRowPadding);

	Ui::AddSkip(content);
	Ui::AddSkip(content);

	const auto wrap = content->add(
		object_ptr<Ui::FixedHeightWidget>(
			content,
			st::inviteLinkButton.height),
		st::inviteLinkButtonsPadding);
	const auto yes = Ui::CreateChild<Ui::RoundButton>(
		wrap,
		tr::lng_settings_suggestion_password_yes(),
		st::inviteLinkButton);
	yes->setFullRadius(true);
	yes->setClickedCallback([=] {
		controller->session().promoSuggestions().dismiss(
			Data::PromoSuggestions::SugValidatePassword());
		mainWrap->toggle(false, anim::type::normal);
	});
	const auto no = Ui::CreateChild<Ui::RoundButton>(
		wrap,
		tr::lng_settings_suggestion_password_no(),
		st::inviteLinkButton);
	no->setFullRadius(true);
	no->setClickedCallback([=] {
		showOther(Settings::CloudPasswordSuggestionInputId());
	});

	wrap->widthValue() | rpl::on_next([=](int width) {
		const auto buttonWidth = (width - st::inviteLinkButtonsSkip) / 2;
		yes->setFullWidth(buttonWidth);
		no->setFullWidth(buttonWidth);
		yes->moveToLeft(0, 0, width);
		no->moveToRight(0, 0, width);
	}, wrap->lifetime());
	Ui::AddSkip(content);
	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);
}

bool HasInterfaceScale() {
	return true;
}

void SetupInterfaceScale(
		not_null<Window::Controller*> window,
		not_null<Ui::VerticalLayout*> container,
		bool icon) {
	if (!HasInterfaceScale()) {
		return;
	}

	const auto card = FAUi::CreateCardContainer(container, 0, 8);

	const auto ratio = style::DevicePixelRatio();
	const auto scaleMin = style::kScaleMin;
	const auto scaleMax = style::MaxScaleForRatio(ratio);
	const auto scaleConfig = cConfigScale();
	const auto step = 5;
	Assert(!((scaleMax - scaleMin) % step));
	auto values = std::vector<int>();
	for (auto i = scaleMin; i != scaleMax; i += step) {
		values.push_back(i);
		if (scaleConfig > i && scaleConfig < i + step) {
			values.push_back(scaleConfig);
		}
	}
	values.push_back(scaleMax);
	const auto valuesCount = int(values.size());

	const auto valueFromScale = [=](int scale) {
		scale = cEvalScale(scale);
		auto result = 0;
		for (const auto value : values) {
			if (scale == value) {
				break;
			}
			++result;
		}
		return ((result == valuesCount) ? (result - 1) : result)
			/ float64(valuesCount - 1);
	};

	const auto isAuto = (scaleConfig == style::kScaleAuto);
	const auto autoScaleStream = card->lifetime().make_state<rpl::event_stream<bool>>();

	const auto inSetScale = card->lifetime().make_state<bool>();
	const auto setScaleHolder = card->lifetime().make_state<Fn<void(int)>>();

	FAUi::AddCardToggle(
		card,
		tr::lng_settings_default_scale(),
		nullptr,
		rpl::single(isAuto) | rpl::then(autoScaleStream->events()),
		[=](bool checked) {
			const auto scale = checked ? style::kScaleAuto : cEvalScale(cConfigScale());
			if (*setScaleHolder) {
				(*setScaleHolder)(scale);
			}
		});

	const auto sliderWrap = card->add(
		object_ptr<Ui::RpWidget>(card),
		style::margins(16, 4, 16, 14));
	const auto slider = Ui::CreateChild<FAUi::MaterialSlider>(sliderWrap);
	const auto label = Ui::CreateChild<Ui::FlatLabel>(sliderWrap, st::settingsScaleLabel);

	sliderWrap->resize(sliderWrap->width(), 32);
	rpl::combine(
		sliderWrap->sizeValue(),
		label->sizeValue()
	) | rpl::on_next([=](QSize outer, QSize size) {
		const auto minRight = std::max(size.width(), st::settingsScaleLabel.style.font->width(u"300%"_q)) + st::normalFont->spacew * 2;
		label->moveToRight(0, (outer.height() - size.height()) / 2);
		const auto width = std::max(40, outer.width() - minRight);
		slider->resize(width, 32);
		slider->moveToLeft(0, (outer.height() - slider->height()) / 2);
	}, label->lifetime());

	slider->setAccessibleName(tr::lng_settings_scale(tr::now));

	const auto updateLabel = [=](int scale) {
		const auto labelText = [&](int scale) {
			if constexpr (Platform::IsMac()) {
				return QString::number(scale) + '%';
			} else {
				const auto handle = window->widget()->windowHandle();
				const auto ratio = handle->devicePixelRatio();
				return QString::number(base::SafeRound(scale * ratio)) + '%';
			}
		};
		label->setText(labelText(cEvalScale(scale)));
	};
	updateLabel(cConfigScale());

	const auto setScale = [=](int scale, const auto &repeatSetScale) -> void {
		if (*inSetScale) {
			return;
		}
		*inSetScale = true;
		const auto guard = gsl::finally([=] { *inSetScale = false; });

		updateLabel(scale);
		autoScaleStream->fire(scale == style::kScaleAuto);
		slider->setValue(valueFromScale(scale));
		if (cEvalScale(scale) != cEvalScale(cConfigScale())) {
			const auto confirmed = Fn<void()>(crl::guard(card, [=] {
				cSetConfigScale(scale);
				Local::writeSettings();
				Core::Restart();
			}));
			const auto cancelled = Fn<void(Fn<void()>)>(crl::guard(card, [=](Fn<void()> close) {
				base::call_delayed(
					st::defaultSettingsSlider.duration,
					card,
					[=] { repeatSetScale(cConfigScale(), repeatSetScale); });
				close();
			}));
			window->show(Ui::MakeConfirmBox({
				.text = tr::lng_settings_need_restart(),
				.confirmed = confirmed,
				.cancelled = cancelled,
				.confirmText = tr::lng_settings_restart_now(),
			}));
		} else if (scale != cConfigScale()) {
			cSetConfigScale(scale);
			Local::writeSettings();
		}
	};
	*setScaleHolder = [=](int scale) {
		setScale(scale, setScale);
	};

	const auto shown = card->lifetime().make_state<bool>();
	const auto togglePreview = SetupScalePreview(window, slider);
	const auto toggleForScale = [=](int scale) {
		scale = cEvalScale(scale);
		const auto show = *shown
			? ScalePreviewShow::Update
			: ScalePreviewShow::Show;
		*shown = true;
		for (auto i = 0; i != valuesCount; ++i) {
			if (values[i] <= scale
				&& (i + 1 == valuesCount || values[i + 1] > scale)) {
				const auto x = (slider->width() * i) / (valuesCount - 1);
				togglePreview(show, scale, x);
				return;
			}
		}
		togglePreview(show, scale, slider->width() / 2);
	};
	const auto toggleHidePreview = [=] {
		togglePreview(ScalePreviewShow::Hide, 0, 0);
		*shown = false;
	};

	slider->setPseudoDiscrete(
		valuesCount,
		[=](int index) { return values[index]; },
		cEvalScale(cConfigScale()),
		[=](int scale) { updateLabel(scale); toggleForScale(scale); },
		[=](int scale) { toggleHidePreview(); setScale(scale, setScale); });
	slider->setValue(valueFromScale(cConfigScale()));
}

Type MainId() {
	return Main::Id();
}

void OpenFaq(base::weak_ptr<Window::SessionController> weak) {
	UrlClickHandler::Open(
		tr::lng_settings_faq_link(tr::now),
		QVariant::fromValue(ClickHandlerContext{
			.sessionWindow = weak,
		}));
}

void OpenAskQuestionConfirm(not_null<Window::SessionController*> window) {
	const auto requestId = std::make_shared<mtpRequestId>();
	const auto sure = [=](Fn<void()> close) {
		if (*requestId) {
			return;
		}
		*requestId = window->session().api().request(
			MTPhelp_GetSupport()
		).done(crl::guard(window, [=](const MTPhelp_Support &result) {
			*requestId = 0;
			result.match([&](const MTPDhelp_support &data) {
				auto &owner = window->session().data();
				if (const auto user = owner.processUser(data.vuser())) {
					window->showPeerHistory(user);
				}
			});
			close();
		})).fail([=] {
			*requestId = 0;
			close();
		}).send();
	};
	window->show(Ui::MakeConfirmBox({
		.text = tr::lng_settings_ask_sure(),
		.confirmed = sure,
		.cancelled = [=](Fn<void()> close) {
			OpenFaq(window);
			close();
		},
		.confirmText = tr::lng_settings_ask_ok(),
		.cancelText = tr::lng_settings_faq_button(),
		.strictCancel = true,
	}));
}

} // namespace Settings
