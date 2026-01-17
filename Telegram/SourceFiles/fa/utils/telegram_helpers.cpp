/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "telegram_helpers.h"

#include "fa/lang/fa_lang.h"
#include "fa/settings/fa_settings.h"

#include "core/application.h"
#include "data/data_peer.h"
#include "ui/text/text_entity.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtNetwork/QNetworkRequest>
#include <QtCore/QEventLoop>
#include <QtCore/QElapsedTimer>
#include <QtCore/QDebug>

#include <list>
#include <optional>

namespace {

constexpr size_t kMaxOnlineStateEntries = 100;

class OnlineStateLRUCache {
public:
	void put(ID key, bool value) {
		auto it = _map.find(key);
		if (it != _map.end()) {
			_list.erase(it->second);
			_list.push_front({key, value});
			it->second = _list.begin();
		} else {
			if (_list.size() >= kMaxOnlineStateEntries) {
				auto last = _list.back();
				_map.erase(last.first);
				_list.pop_back();
			}
			_list.push_front({key, value});
			_map[key] = _list.begin();
		}
	}

	[[nodiscard]] std::optional<bool> get(ID key) {
		auto it = _map.find(key);
		if (it == _map.end()) {
			return std::nullopt;
		}
		_list.splice(_list.begin(), _list, it->second);
		return it->second->second;
	}

	[[nodiscard]] bool contains(ID key) const {
		return _map.find(key) != _map.end();
	}

private:
	std::list<std::pair<ID, bool>> _list;
	std::unordered_map<ID, std::list<std::pair<ID, bool>>::iterator> _map;
};

OnlineStateLRUCache onlineStateCache;

} // namespace

void markAsOnline(not_null<Main::Session*> session) {
	onlineStateCache.put(session->userId().bare, true);
}

// stole from ayugram
int getMediaSizeBytes(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return -1;
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	int64 size = -1;
	if (document) {
		// any file
		size = document->size;
	} else if (photo && photo->hasVideo()) {
		// video
		size = photo->videoByteSize(Data::PhotoSize::Large);
		if (size == 0) {
			size = photo->videoByteSize(Data::PhotoSize::Small);
		}
		if (size == 0) {
			size = photo->videoByteSize(Data::PhotoSize::Thumbnail);
		}
	} else if (photo && !photo->hasVideo()) {
		// photo
		size = photo->imageByteSize(Data::PhotoSize::Large);
		if (size == 0) {
			size = photo->imageByteSize(Data::PhotoSize::Small);
		}
		if (size == 0) {
			size = photo->imageByteSize(Data::PhotoSize::Thumbnail);
		}
	}

	return size;
}

// stole from Ayugram
void readMentions(base::weak_ptr<Data::Thread> weakThread) {
	const auto thread = weakThread.get();
	if (!thread) {
		return;
	}
	const auto peer = thread->peer();
	const auto topic = thread->asTopic();
	const auto rootId = topic ? topic->rootId() : 0;
	using Flag = MTPmessages_ReadMentions::Flag;
	peer->session().api().request(MTPmessages_ReadMentions(
		MTP_flags(rootId ? Flag::f_top_msg_id : Flag()),
		peer->input(),
		MTP_int(rootId)
	)).done([=](const MTPmessages_AffectedHistory &result)
	{
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset > 0) {
			readMentions(weakThread);
		} else {
			peer->owner().history(peer)->clearUnreadMentionsFor(rootId);
		}
	}).send();
}

// stole from Ayugram
void readReactions(base::weak_ptr<Data::Thread> weakThread) {
	const auto thread = weakThread.get();
	if (!thread) {
		return;
	}
	const auto topic = thread->asTopic();
	const auto sublist = thread->asSublist();
	const auto peer = thread->peer();
	const auto rootId = topic ? topic->rootId() : 0;
	using Flag = MTPmessages_ReadReactions::Flag;
	peer->session().api().request(MTPmessages_ReadReactions(
		MTP_flags((rootId ? Flag::f_top_msg_id : Flag(0))
			| (sublist ? Flag::f_saved_peer_id : Flag(0))),
		peer->input(),
		MTP_int(rootId),
		sublist ? sublist->sublistPeer()->input() : MTPInputPeer()
	)).done([=](const MTPmessages_AffectedHistory &result)
	{
		const auto offset = peer->session().api().applyAffectedHistory(
			peer,
			result);
		if (offset > 0) {
			readReactions(weakThread);
		} else {
			peer->owner().history(peer)->clearUnreadReactionsFor(rootId, sublist);
		}
	}).send();
}

QString getLocationDC(int dc_id) {
    QString dc_location;

    switch (dc_id) {
        case 1:
            dc_location = "Miami FL, USA";
            break;
        case 2:
            dc_location = "Amsterdam, NL";
            break;
        case 3:
            dc_location = "Miami FL, USA";
            break;
        case 4:
            dc_location = "Amsterdam, NL";
            break;
        case 5:
            dc_location = "Singapore, SG";
            break;
        default:
            dc_location = "UNKNOWN";
            break;
    }

    return dc_location;
}

QString getPeerDC(not_null<PeerData*> peer) {
    int dc = 0;

    if (const auto statsDcId = peer->owner().statsDcId(peer)) {
        dc = statsDcId;
    }
    else if (peer->hasUserpic()) {
        dc = v::match(
            peer->userpicLocation().file().data,
            [&](const StorageFileLocation &data) {
                return data.dcId();
            },
            [&](const WebFileLocation &) {
                return 4;
            },
            [&](const GeoPointLocation &) {
                return 0;
            },
            [&](const AudioAlbumThumbLocation &) {
                return 0;
            },
            [&](const PlainUrlLocation &) {
                return 4;
            },
            [&](const InMemoryLocation &) {
                return 0;
            }
        );
    }
    else {
        return QString("DC_UNKNOWN");
    }

    QString dc_location = getLocationDC(dc);

    if (dc < 1) {
        return QString("DC_UNKNOWN");
    }

    return QString("DC%1, %2").arg(dc).arg(dc_location);
}

QString getDCbyID(int dc) {
    QString dc_location = getLocationDC(dc);

    if (dc < 1) {
        return QString("DC_UNKNOWN");
    }

    return QString("DC%1, %2").arg(dc).arg(dc_location);
}

QString getOnlyDC(not_null<PeerData*> peer) {
    int dc = 0;

    if (const auto statsDcId = peer->owner().statsDcId(peer)) {
        dc = statsDcId;
    }
    else if (peer->hasUserpic()) {
        dc = v::match(
            peer->userpicLocation().file().data,
            [&](const StorageFileLocation &data) {
                return data.dcId();
            },
            [&](const WebFileLocation &) {
                return 4;
            },
            [&](const GeoPointLocation &) {
                return 0;
            },
            [&](const AudioAlbumThumbLocation &) {
                return 0;
            },
            [&](const PlainUrlLocation &) {
                return 4;
            },
            [&](const InMemoryLocation &) {
                return 0;
            }
        );
    }
    else {
        return QString("DC_UNKNOWN");
    }

    if (dc < 1) {
        return QString("DC_UNKNOWN");
    }

    return QString("Datacenter %1").arg(dc);
}

QString getIpDC(int dc_id, bool test) {
    QString ip;

    if (!test) {
        switch (dc_id) {
            case 1:
                ip = "149.154.175.53";
                break;
            case 2:
                ip = "149.154.167.51";
                break;
            case 3:
                ip = "149.154.175.100";
                break;
            case 4:
                ip = "149.154.167.91";
                break;
            case 5:
                ip = "91.108.56.130";
                break;
            default:
                return QString("UNKNOWN");
        }
    }

    else {
        switch (dc_id) {
            case 1:
                ip = "149.154.175.10";
                break;
            case 2:
                ip = "149.154.167.40";
                break;
            case 3:
                ip = "149.154.175.117";
                break;
            default:
                return QString("UNKNOWN");
        }
    }

    return ip;
}

void cleanDebugLogs() {
    QString workingDir = cWorkingDir();
    QDir debugLogsDir(cWorkingDir() + "/DebugLogs");

    if (!debugLogsDir.exists()) {
        return;
    }

    QStringList files = debugLogsDir.entryList(QDir::Files);
    
    for (const QString &file : files) {
        debugLogsDir.remove(file);
    }

    return;
}

bool is_me(ID userId) {
    for (const auto &accountWithIndex : Core::App().domain().accounts()) {
        if (const auto *account = accountWithIndex.account.get()) {
            if (const auto *session = account->maybeSession()) {
                if (session->userId().bare == userId) {
                    return true;
                }
            }
        }
    }
    return false;
}

QString getMediaSize(not_null<HistoryItem*> message) {
	const auto size = getMediaSizeBytes(message);

	if (size == -1) {
		return {};
	}

	return Ui::FormatSizeText(size);
}

QString getMediaMime(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	if (document) {
		// any file
		return document->mimeString();
	} else if (photo && photo->hasVideo()) {
		// video
		return "video/mp4";
	} else if (photo && !photo->hasVideo()) {
		// photo
		return "image/jpeg";
	}

	return {};
}

QString getMediaName(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();

	if (document) {
		return document->filename();
	}

	return {};
}

QString getMediaResolution(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	const auto formatQSize = [=](QSize size)
	{
		if (size.isNull() || size.isEmpty() || !size.isValid()) {
			return QString();
		}

		return QString("%1x%2").arg(size.width()).arg(size.height());
	};

	if (document) {
		return formatQSize(document->dimensions);
	} else if (photo) {
		auto result = photo->size(Data::PhotoSize::Large);
		if (!result.has_value()) {
			result = photo->size(Data::PhotoSize::Small);
		}
		if (!result.has_value()) {
			result = photo->size(Data::PhotoSize::Thumbnail);
		}
		return result.has_value() ? formatQSize(result.value()) : QString();
	}

	return {};
}

QString getMediaDC(not_null<HistoryItem*> message) {
	if (!message->media()) {
		return {};
	}

	const auto media = message->media();

	const auto document = media->document();
	const auto photo = media->photo();

	if (document) {
		return getDCbyID(document->getDC());
	} else if (photo) {
		return getDCbyID(photo->getDC());
	}

	return {};
}

// thanks ayugram
void MessageDetails(not_null<Ui::PopupMenu*> menu, HistoryItem *item) {
    bool show_message_details = FASettings::JsonSettings::GetBool("show_message_details");
    if (!show_message_details) {
        return;
    }

	if (item->isLocal()) {
		return;
	}

	const auto view = item->mainView();
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	const auto views = item->Get<HistoryMessageViews>();
	const auto media = item->media();

	const auto isSticker = (media && media->document() && media->document()->sticker()) ? true : false;

	const auto emojiPacks = HistoryView::CollectEmojiPacks(item, HistoryView::EmojiPacksSource::Message);
	auto containsSingleCustomEmojiPack = emojiPacks.size() == 1;
	if (!containsSingleCustomEmojiPack && emojiPacks.size() > 1) {
		const auto author = emojiPacks.front().id >> 32;
		auto sameAuthor = true;
		for (const auto &pack : emojiPacks) {
			if (pack.id >> 32 != author) {
				sameAuthor = false;
				break;
			}
		}

		containsSingleCustomEmojiPack = sameAuthor;
	}

	const auto isForwarded = forwarded && !forwarded->story && forwarded->psaType.isEmpty();

	const auto messageId = QString::number(item->id.bare);
	const auto messageDate = base::unixtime::parse(item->date());
	const auto messageEditDate = base::unixtime::parse(view ? view->displayedEditDate() : TimeId(0));

	const auto messageForwardedDate =
		isForwarded && forwarded
			? base::unixtime::parse(forwarded->originalDate)
			: QDateTime();

	const auto
		messageViews = item->hasViews() && item->viewsCount() > 0 ? QString::number(item->viewsCount()) : QString();
	const auto messageForwards = views && views->forwardsCount > 0 ? QString::number(views->forwardsCount) : QString();

	const auto mediaSize = media ? getMediaSize(item) : QString();
	const auto mediaMime = media ? getMediaMime(item) : QString();
	// todo: bitrate (?)
	const auto mediaName = media ? getMediaName(item) : QString();
	const auto mediaResolution = media ? getMediaResolution(item) : QString();
	const auto mediaDC = media ? getMediaDC(item) : QString();

	const auto hasAnyPostField =
		!messageViews.isEmpty() ||
		!messageForwards.isEmpty();

	const auto hasAnyMediaField =
		!mediaSize.isEmpty() ||
		!mediaMime.isEmpty() ||
		!mediaName.isEmpty() ||
		!mediaResolution.isEmpty() ||
		!mediaDC.isEmpty();

	const auto callback = Ui::Menu::CreateAddActionCallback(menu);

	callback(Window::PeerMenuCallback::Args{
		.text = FAlang::Translate(QString("fa_message_details")),
		.handler = nullptr,
		.icon = &st::menuIconInfo,
		.fillSubmenu = [&](not_null<Ui::PopupMenu*> menu2)
		{
			if (hasAnyPostField) {
				if (!messageViews.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconShowInChat,
						FAlang::Translate(QString("fa_message_details_views")),
						messageViews
					));
				}

				if (!messageForwards.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconViewReplies,
						FAlang::Translate(QString("fa_message_details_share")),
						messageForwards
					));
				}

				menu2->addSeparator();
			}

			menu2->addAction(Ui::ContextActionWithSubText(
				menu2->menu(),
				st::menuIconInfo,
				QString("ID"),
				messageId
			));

			menu2->addAction(Ui::ContextActionWithSubText(
				menu2->menu(),
				st::menuIconSchedule,
				FAlang::Translate(QString("fa_message_details_date")),
				formatDateTime(messageDate)
			));

			if (view && view->displayedEditDate()) {
				menu2->addAction(Ui::ContextActionWithSubText(
					menu2->menu(),
					st::menuIconEdit,
					FAlang::Translate(QString("fa_message_details_edit_date")),
					formatDateTime(messageEditDate)
				));
			}

			if (isForwarded) {
				menu2->addAction(Ui::ContextActionWithSubText(
					menu2->menu(),
					st::menuIconTTL,
					FAlang::Translate(QString("fa_message_details_forward_message_date")),
					formatDateTime(messageForwardedDate)
				));
			}

			if (media && hasAnyMediaField) {
				menu2->addSeparator();

				if (!mediaSize.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconDownload,
						FAlang::Translate(QString("fa_message_details_filesize")),
						mediaSize
					));
				}

				if (!mediaMime.isEmpty()) {
					const auto mime = Core::MimeTypeForName(mediaMime);

					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconShowAll,
						FAlang::Translate(QString("fa_message_details_filetype")),
						mime.name()
					));
				}

				if (!mediaName.isEmpty()) {
					auto const shortified = mediaName.length() > 20 ? "…" + mediaName.right(20) : mediaName;

					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconInfo,
						FAlang::Translate(QString("fa_message_details_filename")),
						shortified,
						[=]
						{
							QGuiApplication::clipboard()->setText(mediaName);
						}
					));
				}

				if (!mediaResolution.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconStats,
						FAlang::Translate(QString("fa_message_details_resolution")),
						mediaResolution
					));
				}

				if (!mediaDC.isEmpty()) {
					menu2->addAction(Ui::ContextActionWithSubText(
						menu2->menu(),
						st::menuIconBoosts,
						FAlang::Translate(QString("fa_message_details_datacenter")),
						mediaDC
					));
				}

				if (isSticker) {
					auto authorId = getUserIdFromPackId(media->document()->sticker()->set.id);

					if (authorId != 0) {
						menu2->addAction(Ui::ContextActionStickerAuthor(
							menu2->menu(),
							&item->history()->session(),
							authorId
						));
					}
				}
			}

			if (containsSingleCustomEmojiPack) {
				auto authorId = getUserIdFromPackId(emojiPacks.front().id);

				if (authorId != 0) {
					menu2->addAction(Ui::ContextActionStickerAuthor(
						menu2->menu(),
						&item->history()->session(),
						authorId
					));
				}
			}
		},
	});
}


ID getUserIdFromPackId(uint64 id) {
	auto ownerId = id >> 32;
	if ((id >> 16 & 0xff) == 0x3f) {
		ownerId |= 0x80000000;
	}
	if (id >> 24 & 0xff) {
		ownerId += 0x100000000;
	}

	return ownerId;
}

// stole form ayugram
QString formatDateTime(const QDateTime &date) {
	const auto locale = QLocale::system();
	const auto datePart = locale.toString(date.date(), QLocale::ShortFormat);
	const auto timePart = locale.toString(date, "HH:mm:ss");

	return datePart + getLocalizedAt() + timePart;
}

QString getLocalizedAt() {
	static const auto val = tr::lng_mediaview_date_time(
		tr::now,
		lt_date,
		"",
		lt_time,
		"");
	return val;
}

// thx ayugram
void resolveUser(ID userId, const QString &username, Main::Session *session, const Callback &callback) {
	auto normalized = username.trimmed().toLower();
	if (normalized.isEmpty()) {
		callback(QString(), nullptr);
		return;
	}
	normalized = normalized.startsWith("@") ? normalized.mid(1) : normalized;

	if (normalized.isEmpty()) {
		callback(QString(), nullptr);
		return;
	}

	session->api().request(MTPcontacts_ResolveUsername(
		MTP_flags(0),
		MTP_string(normalized),
		MTP_string()
	)).done([=](const MTPcontacts_ResolvedPeer &result)
	{
		Expects(result.type() == mtpc_contacts_resolvedPeer);

		auto &data = result.c_contacts_resolvedPeer();
		session->data().processUsers(data.vusers());
		session->data().processChats(data.vchats());
		const auto peer = session->data().peerLoaded(
			peerFromMTP(data.vpeer()));
		if (const auto user = peer ? peer->asUser() : nullptr) {
			if ((user->id.value & PeerId::kChatTypeMask) == userId) {
				callback(normalized, user);
				return;
			}
		}

		callback(normalized, nullptr);
	}).fail([=]
	{
		callback(QString(), nullptr);
	}).send();
}

// thx ayugram
void searchUser(long long userId, Main::Session *session, bool searchUserFlag, const Callback &callback) {
	if (!session) {
		callback(QString(), nullptr);
		return;
	}

	constexpr auto botId = 1696868284;
	const auto bot = session->data().userLoaded(botId);

	if (!bot) {
		if (searchUserFlag) {
			resolveUser(botId,
						"tgdb_bot",
						session,
						[=](const QString &title, UserData *data)
						{
							searchUser(userId, session, false, callback);
						});
		} else {
			callback(QString(), nullptr);
		}
		return;
	}

	session->api().request(MTPmessages_GetInlineBotResults(
		MTP_flags(0),
		bot->inputUser(),
		MTP_inputPeerEmpty(),
		MTPInputGeoPoint(),
		MTP_string(QString::number(userId)),
		MTP_string("")
	)).done([=](const MTPmessages_BotResults &result)
	{
		if (result.type() != mtpc_messages_botResults) {
			callback(QString(), nullptr);
			return;
		}
		auto &d = result.c_messages_botResults();
		session->data().processUsers(d.vusers());

		auto &v = d.vresults().v;

		for (const auto &res : v) {
			const auto message = res.match(
				[&](const MTPDbotInlineResult &data)
				{
					return &data.vsend_message();
				},
				[&](const MTPDbotInlineMediaResult &data)
				{
					return &data.vsend_message();
				});

			const auto text = message->match(
				[&](const MTPDbotInlineMessageMediaAuto &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageText &data)
				{
					return qs(data.vmessage());
				},
				[&](const MTPDbotInlineMessageMediaGeo &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaVenue &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaContact &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaInvoice &data)
				{
					return QString();
				},
				[&](const MTPDbotInlineMessageMediaWebPage &data)
				{
					return QString();
				});

			if (text.isEmpty()) {
				continue;
			}

			ID id = 0; // 🆔
			QString title; // 🏷
			QString username; // 📧

			for (const auto &line : text.split('\n')) {
				if (line.startsWith("🆔")) {
					id = line.mid(line.indexOf(':') + 1).toLongLong();
				} else if (line.startsWith("🏷")) {
					title = line.mid(line.indexOf(':') + 1);
				} else if (line.startsWith("📧")) {
					username = line.mid(line.indexOf(':') + 1);
				}
			}

			if (id == 0) {
				continue;
			}

			if (id != userId) {
				continue;
			}

			if (!username.isEmpty()) {
				resolveUser(id,
							username,
							session,
							[=](const QString &titleInner, UserData *data)
							{
								if (data) {
									callback(titleInner, data);
								} else {
									callback(title, nullptr);
								}
							});
				return;
			}

			if (!title.isEmpty()) {
				callback(title, nullptr);
			}
		}

		callback(QString(), nullptr);
	}).fail([=]
	{
		callback(QString(), nullptr);
	}).handleAllErrors().send();
}

// thx ayugram
void searchById(ID userId, Main::Session *session, const Callback &callback) {
	if (userId == 0 || !session) {
		callback(QString(), nullptr);
		return;
	}

	if (const auto dataLoaded = session->data().userLoaded(userId)) {
		callback(dataLoaded->username(), dataLoaded);
		return;
	}

	searchUser(userId,
			   session,
			   true,
			   [=](const QString &title, UserData *data)
			   {
				   if (data && data->accessHash()) {
					   callback(title, data);
				   } else {
					   callback(QString(), nullptr);
				   }
			   });
}

void searchChannelById(ID channelId, Main::Session *session, const ChannelCallback &callback) {
	if (channelId == 0 || !session) {
		callback(nullptr);
		return;
	}

	if (const auto channel = session->data().channelLoaded(ChannelId(channelId))) {
		callback(channel);
		return;
	}

	if (const auto chat = session->data().chatLoaded(ChatId(channelId))) {
		callback(nullptr);
		return;
	}

	session->api().request(MTPchannels_GetChannels(
		MTP_vector<MTPInputChannel>(
			1,
			MTP_inputChannel(MTP_long(channelId), MTP_long(0)))
	)).done([=](const MTPmessages_Chats &result) {
		result.match([&](const auto &data) {
			const auto peer = session->data().processChats(data.vchats());
			if (peer && peer->id == peerFromChannel(ChannelId(channelId))) {
				callback(peer->asChannel());
			} else {
				callback(nullptr);
			}
		});
	}).fail([=] {
		callback(nullptr);
	}).send();
}

bool shouldHideBlockedUserMessage(PeerData *from) {
	if (!from) {
		return false;
	}
	if (!FASettings::JsonSettings::GetBool("hide_blocked_user_messages")) {
		return false;
	}
	return from->isBlocked();
}

TextWithEntities applyBlockedUserSpoiler(TextWithEntities text) {
	const auto blockedPrefix = QString("[Blocked User Message]\n");
	const auto originalLength = text.text.length();
	
	for (auto &entity : text.entities) {
		entity = EntityInText(
			entity.type(),
			entity.offset() + blockedPrefix.length(),
			entity.length(),
			entity.data());
	}
	
	text.entities.insert(
		text.entities.begin(),
		EntityInText(EntityType::Bold, 0, blockedPrefix.length() - 1));
	
	text.entities.insert(
		text.entities.begin() + 1,
		EntityInText(EntityType::Spoiler, blockedPrefix.length(), originalLength));
	
	text.entities.insert(
		text.entities.begin() + 2,
		EntityInText(EntityType::Blockquote, blockedPrefix.length(), originalLength, u"1"_q));
	
	text.text = blockedPrefix + text.text;
	
	return text;
}

TextWithTags applyAutoParseMarkdownHyperlink(const TextWithTags &textWithTags) {
	if (!FASettings::JsonSettings::GetBool("auto_format_markdown")) {
		return textWithTags;
	}
	
	const auto &text = textWithTags.text;
	if (text.isEmpty()) {
		return textWithTags;
	}
	
	struct FoundLink {
		int startPos;
		int endPos;
		int textStart;
		int textEnd;
		QString url;
	};
	
	std::vector<FoundLink> links;
	int pos = 0;
	
	while (pos < text.length()) {
		int bracketStart = text.indexOf('[', pos);
		if (bracketStart < 0) break;
		
		int bracketEnd = text.indexOf(']', bracketStart + 1);
		if (bracketEnd < 0) break;
		
		if (bracketEnd + 1 >= text.length() || text[bracketEnd + 1] != '(') {
			pos = bracketStart + 1;
			continue;
		}
		
		int parenStart = bracketEnd + 1;
		int parenEnd = text.indexOf(')', parenStart + 1);
		if (parenEnd < 0) {
			pos = bracketStart + 1;
			continue;
		}
		
		QString linkText = text.mid(bracketStart + 1, bracketEnd - bracketStart - 1);
		QString url = text.mid(parenStart + 1, parenEnd - parenStart - 1).trimmed();
		
		const bool isValidUrl = url.contains('.') || url.contains(':');
		if (!linkText.trimmed().isEmpty() && !url.isEmpty() && isValidUrl) {
			links.push_back({
				bracketStart,
				parenEnd + 1,
				bracketStart + 1,
				bracketEnd,
				url
			});
			pos = parenEnd + 1;
		} else {
			pos = bracketStart + 1;
		}
	}
	
	if (links.empty()) {
		return textWithTags;
	}
	
	TextWithTags result;
	result.text.reserve(text.length());
	result.tags.reserve(textWithTags.tags.size() + links.size());
	
	int lastPos = 0;
	int removedChars = 0;
	
	auto existingTag = textWithTags.tags.begin();
	const auto existingTagsEnd = textWithTags.tags.end();
	
	for (const auto &link : links) {
		while (existingTag != existingTagsEnd
			&& existingTag->offset + existingTag->length <= link.startPos) {
			result.tags.push_back({
				existingTag->offset - removedChars,
				existingTag->length,
				existingTag->id
			});
			++existingTag;
		}
		
		if (link.startPos > lastPos) {
			result.text.append(text.mid(lastPos, link.startPos - lastPos));
		}
		
		const auto linkText = text.mid(link.textStart, link.textEnd - link.textStart);
		const int tagOffset = result.text.length();
		result.text.append(linkText);
		
		result.tags.push_back({
			tagOffset,
			linkText.length(),
			link.url
		});

		const int syntaxLength = (link.endPos - link.startPos) - linkText.length();
		removedChars += syntaxLength;
		
		lastPos = link.endPos;
	}

	while (existingTag != existingTagsEnd) {
		result.tags.push_back({
			existingTag->offset - removedChars,
			existingTag->length,
			existingTag->id
		});
		++existingTag;
	}

	if (lastPos < text.length()) {
		result.text.append(text.mid(lastPos));
	}
	
	return result;
}