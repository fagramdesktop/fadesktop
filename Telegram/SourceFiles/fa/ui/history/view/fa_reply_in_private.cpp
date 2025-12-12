/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/
#include "fa/ui/history/view/fa_reply_in_private.h"

#include "fa/settings/fa_settings.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/data_changes.h"
#include "data/data_drafts.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_list_widget.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

namespace FA {
namespace {

[[nodiscard]] UserData* GetReplyableUser(HistoryItem* item) {
	if (!item || !item->isRegular()) {
		return nullptr;
	}

	const auto displayFrom = item->displayFrom();
	const auto from = displayFrom ? displayFrom : item->from().get();

	if (!from->isUser()) {
		return nullptr;
	}

	const auto user = from->asUser();

	if (from == item->history()->peer) {
		return nullptr;
	}

	if (user->isSelf()) {
		return nullptr;
	}

	if (user->isInaccessible()) {
		return nullptr;
	}

	return user;
}

void ReplyInPrivateChat(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user,
		FullMsgId messageId,
		const TextWithEntities& quote,
		int quoteOffset) {
	const auto history = user->owner().history(user);

	auto reply = FullReplyTo{
		.messageId = messageId,
		.quote = quote,
		.quoteOffset = quoteOffset,
	};

	const auto existingDraft = history->localDraft(MsgId(0), PeerId(0));
	const auto textWithTags = existingDraft
		? existingDraft->textWithTags
		: TextWithTags();
	const auto cursor = existingDraft
		? existingDraft->cursor
		: MessageCursor();

	history->setLocalDraft(std::make_unique<Data::Draft>(
		textWithTags,
		reply,
		SuggestOptions(),
		cursor,
		Data::WebPageDraft()));

	history->clearLocalEditDraft(MsgId(0), PeerId(0));

	history->session().changes().entryUpdated(
		history,
		Data::EntryUpdate::Flag::LocalDraftSet);

	controller->showPeerHistory(
		user,
		Window::SectionShow::Way::Forward,
		ShowAtUnreadMsgId);
}

} // namespace

bool AddReplyInPrivateChatAction(
		not_null<Ui::PopupMenu*> menu,
		const HistoryView::ContextMenuRequest &request,
		not_null<HistoryView::ListWidget*> list) {
	if (!FASettings::JsonSettings::GetBool("context_menu_reply_in_private")) {
		return false;
	}

	const auto item = request.quote.item
		? request.quote.item
		: request.item;

	const auto user = GetReplyableUser(item);
	if (!user) {
		return false;
	}

	if (!item->allowsForward()) {
		return false;
	}

	const auto itemId = item->fullId();
	const auto &quote = request.quote;
	const auto controller = list->controller();

	menu->addAction(
		tr::lng_reply_in_private_chat(tr::now),
		[=] {
			ReplyInPrivateChat(
				controller,
				user,
				itemId,
				quote.highlight.quote,
				quote.highlight.quoteOffset);
		},
		&st::menuIconReply);

	return true;
}

} // namespace FA
