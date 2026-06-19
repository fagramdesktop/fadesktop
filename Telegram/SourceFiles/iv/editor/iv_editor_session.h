/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "api/api_common.h"
#include "base/basic_types.h"
#include "menu/menu_send_details.h"
#include <rpl/producer.h>

#include <memory>

class HistoryItem;
class PeerData;

namespace Data {
struct Draft;
} // namespace Data

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace Iv::Editor {

using ThreadFieldDraftReader = Fn<std::unique_ptr<::Data::Draft>()>;
using ThreadFieldDraftSaver = Fn<void(std::unique_ptr<::Data::Draft>)>;

void ShowComposeBox(
	not_null<Window::SessionController*> controller,
	not_null<PeerData*> peer,
	Api::SendAction action,
	Fn<SendMenu::Details()> sendMenuDetails);
void ShowEditBox(
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);
[[nodiscard]] bool IsComposeBoxOpen(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId topicRootId,
	PeerId monoforumPeerId);
[[nodiscard]] rpl::producer<bool> FieldVisibleValue(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId topicRootId,
	PeerId monoforumPeerId);
void RegisterThreadFieldBridge(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId topicRootId,
	PeerId monoforumPeerId,
	ThreadFieldDraftReader readDraft,
	ThreadFieldDraftSaver saveDraft);
void UnregisterThreadFieldBridge(
	not_null<Main::Session*> session,
	PeerId peerId,
	MsgId topicRootId,
	PeerId monoforumPeerId);

// Synchronously destroys all open editor windows. Called on application
// shutdown (before ~Sandbox) so that no editor top-level widget survives
// to be destroyed from ~QApplication, where the lib_ui native event filter
// would re-enter the already destroyed Sandbox machinery and crash.
void CloseAllWindows();

} // namespace Iv::Editor
