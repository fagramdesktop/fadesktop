/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/iv_search_bar.h"

#include "lang/lang_keys.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/multi_select.h"

#include "styles/palette.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_iv.h"
#include "styles/style_widgets.h"

#include <QtGui/QPainter>

namespace Iv {

SearchBar::SearchBar(not_null<Ui::RpWidget*> parent)
: _wrap(parent, object_ptr<Ui::RpWidget>(parent.get()))
, _shadow(parent) {
	setup(parent);
	_wrap.hide(anim::type::instant);
	_shadow.hide();
}

void SearchBar::setup(not_null<Ui::RpWidget*> parent) {
	const auto inner = _wrap.entity();
	inner->resize(inner->width(), st::ivSearchBarHeight);

	_select = Ui::CreateChild<Ui::MultiSelect>(
		inner,
		st::searchInChatMultiSelect,
		tr::lng_dlg_filter());
	_counter = Ui::CreateChild<Ui::FlatLabel>(
		inner,
		QString(),
		st::defaultSettingsRightLabel);
	_counter->setAttribute(Qt::WA_TransparentForMouseEvents);
	_counter->hide();
	_up = Ui::CreateChild<Ui::IconButton>(inner, st::calendarPrevious);
	_down = Ui::CreateChild<Ui::IconButton>(inner, st::calendarNext);
	_close = Ui::CreateChild<Ui::CrossButton>(
		inner,
		st::defaultMultiSelectSearchCancel);
	_close->show(anim::type::instant);

	inner->paintRequest(
	) | rpl::on_next([=](QRect clip) {
		QPainter(inner).fillRect(clip, st::windowBg);
	}, inner->lifetime());

	parent->widthValue(
	) | rpl::on_next([=](int width) {
		_wrap.resizeToWidth(width);
	}, _wrap.lifetime());

	inner->sizeValue(
	) | rpl::on_next([=] {
		updateControlsGeometry();
	}, inner->lifetime());

	_wrap.geometryValue(
	) | rpl::on_next([=](QRect geometry) {
		_shadow.setGeometry(
			geometry.x(),
			geometry.y() + geometry.height(),
			geometry.width(),
			st::lineWidth);
	}, _shadow.lifetime());

	_shadow.showOn(rpl::combine(
		_wrap.shownValue(),
		_wrap.heightValue(),
		rpl::mappers::_1 && rpl::mappers::_2 > 0
	) | rpl::filter([=](bool shown) {
		return (shown == _shadow.isHidden());
	}));

	_select->setQueryChangedCallback([=](const QString &query) {
		_queryChanges.fire_copy(query);
	});
	_select->setSubmittedCallback([=](Qt::KeyboardModifiers) {
		_submits.fire({});
	});
	_select->setCancelledCallback([=] {
		_closeRequests.fire({});
	});

	setResults(0, 0);
}

void SearchBar::updateControlsGeometry() {
	const auto inner = _wrap.entity();
	const auto width = inner->width();
	const auto height = inner->height();
	_close->moveToRight(0, (height - _close->height()) / 2);
	_down->moveToRight(_close->width(), (height - _down->height()) / 2);
	_up->moveToRight(
		_close->width() + _down->width(),
		(height - _up->height()) / 2);
	auto right = _close->width() + _down->width() + _up->width();
	if (!_counter->isHidden()) {
		right += st::ivSearchBarCounterSkip;
		_counter->moveToRight(right, (height - _counter->height()) / 2);
		right += _counter->width() + st::ivSearchBarCounterSkip;
	}
	_select->resizeToWidth(width - right);
	_select->moveToLeft(0, (height - _select->height()) / 2);
}

void SearchBar::setResults(int current, int total) {
	if (total > 0) {
		_counter->setText(tr::lng_search_messages_n_of_amount(
			tr::now,
			lt_n,
			QString::number(current),
			lt_amount,
			QString::number(total)));
		_counter->show();
	} else {
		_counter->hide();
	}
	const auto upDisabled = (total <= 0) || (current <= 1);
	const auto downDisabled = (total <= 0) || (current >= total);
	_up->setIconOverride(upDisabled
		? &st::calendarPreviousDisabled
		: nullptr);
	_down->setIconOverride(downDisabled
		? &st::calendarNextDisabled
		: nullptr);
	_up->setAttribute(Qt::WA_TransparentForMouseEvents, upDisabled);
	_down->setAttribute(Qt::WA_TransparentForMouseEvents, downDisabled);
	updateControlsGeometry();
}

void SearchBar::toggle(bool shown, anim::type animated) {
	_wrap.toggle(shown, animated);
}

void SearchBar::show(anim::type animated) {
	toggle(true, animated);
}

void SearchBar::hide(anim::type animated) {
	toggle(false, animated);
}

bool SearchBar::shown() const {
	return _wrap.toggled();
}

void SearchBar::setInnerFocus() {
	_select->setInnerFocus();
}

void SearchBar::raise() {
	_wrap.raise();
	_shadow.raise();
}

void SearchBar::move(int x, int y) {
	_wrap.move(x, y);
}

rpl::producer<QString> SearchBar::queryChanges() const {
	return _queryChanges.events();
}

rpl::producer<> SearchBar::submits() const {
	return _submits.events();
}

rpl::producer<int> SearchBar::navigateRequests() const {
	return rpl::merge(
		_up->clicks() | rpl::map_to(-1),
		_down->clicks() | rpl::map_to(1));
}

rpl::producer<> SearchBar::closeRequests() const {
	return rpl::merge(
		_closeRequests.events(),
		_close->clicks() | rpl::to_empty);
}

rpl::producer<int> SearchBar::heightValue() const {
	return _wrap.heightValue();
}

rpl::lifetime &SearchBar::lifetime() {
	return _wrap.lifetime();
}

} // namespace Iv
