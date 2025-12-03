/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#pragma once

#include "apiwrap.h"
#include "lang_auto.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "fa/ui/menu_item_subtext.h"
#include "history/history_item_components.h"

#include "core/mime_type.h"
#include "styles/style_menu_icons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/text/format_values.h"
#include "export/data/export_data_types.h"
#include "ui/widgets/menu/menu_add_action_callback_factory.h"
#include "window/window_peer_menu.h"

#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_user.h"
#include "data/data_chat.h"
#include "data/data_photo.h"
#include "data/data_document.h"
#include "data/data_histories.h"
#include "data/data_forum_topic.h"
#include "data/data_saved_sublist.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_types.h"
#include "history/history.h"
#include "history/history_unread_things.h"
#include "history/view/history_view_context_menu.h"
#include "history/view/history_view_element.h"
#include "storage/storage_account.h"
#include "window/window_session_controller.h"

#include <string>
#include <utility>
#include <functional>
#include <unordered_set>
#include <QtCore/QJsonArray>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

using Callback = Fn<void(const QString &, UserData *)>;

void markAsOnline(not_null<Main::Session*> session);

int getMediaSizeBytes(not_null<HistoryItem*> message);
void readMentions(base::weak_ptr<Data::Thread> weakThread);
void readReactions(base::weak_ptr<Data::Thread> weakThread);

QString getLocationDC(int dc_id);
QString getPeerDC(not_null<PeerData*> peer);
QString getDCbyID(int dc);
QString getOnlyDC(not_null<PeerData*> peer);
QString getIpDC(int dc_id, bool test);

void cleanDebugLogs();

bool is_me(ID userId);

void MessageDetails(not_null<Ui::PopupMenu*> menu, HistoryItem *item);

ID getUserIdFromPackId(uint64 id);
QString formatDateTime(const QDateTime &date);
QString getLocalizedAt();

void searchById(ID userId, Main::Session *session, bool retry, const Callback &callback);
void searchById(ID userId, Main::Session *session, const Callback &callback);