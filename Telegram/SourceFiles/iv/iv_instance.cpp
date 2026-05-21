/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#include "iv/iv_instance.h"

#include "apiwrap.h"
#include "base/platform/base_platform_info.h"
#include "base/qt_signal_producer.h"
#include "base/unixtime.h"
#include "boxes/share_box.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "core/shortcuts.h"
#include "core/click_handler_types.h"
#include "data/data_changes.h"
#include "data/data_channel.h"
#include "data/data_cloud_file.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_location.h"
#include "data/data_media_types.h"
#include "data/data_photo_media.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_web_page.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "info/profile/info_profile_values.h"
#include "iv/markdown/iv_markdown_controller.h"
#include "iv/iv_cached_media.h"
#include "iv/iv_controller.h"
#include "iv/iv_data.h"
#include "iv/iv_prepare.h"
#include "lang/lang_keys.h"
#include "lottie/lottie_common.h" // Lottie::ReadContent.
#include "main/main_account.h"
#include "main/main_session.h"
#include "main/session/session_show.h"
#include "media/streaming/media_streaming_loader.h"
#include "media/view/media_view_open_common.h"
#include "storage/file_download.h"
#include "storage/storage_account.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/layer_widget.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/basic_click_handlers.h"
#include "webview/webview_data_stream_memory.h"
#include "webview/webview_interface.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "window/window_session_controller_link_info.h"

#include <QtCore/QByteArray>
#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <optional>

namespace Iv {
namespace {

constexpr auto kGeoPointScale = 1;
constexpr auto kGeoPointZoomMin = 13;
constexpr auto kMaxLoadParts = 5;
constexpr auto kKeepLoadingParts = 8;
constexpr auto kAllowPageReloadAfter = 3 * crl::time(1000);

struct NativeIvChannelContext {
	uint64 channelId = 0;
	QString username;
};

[[nodiscard]] NativeIvChannelContext ParseNativeIvChannelContext(
		const QString &context) {
	const auto separator = context.indexOf(u'\n');
	return {
		.channelId = (separator >= 0)
			? context.mid(0, separator).toULongLong()
			: context.toULongLong(),
		.username = (separator >= 0) ? context.mid(separator + 1) : QString(),
	};
}

[[nodiscard]] QString ResolveNativeIvChannelUsername(
		const QString &channelUsername,
		const QString &contextUsername) {
	return !channelUsername.isEmpty() ? channelUsername : contextUsername;
}

struct MarkdownMessageContext {
	ClickHandlerContext clickHandlerContext;
	base::weak_ptr<Window::SessionController> sessionWindow;
};

struct LocalMarkdownTarget {
	QString key;
	QString path;
	QString sourceName;
	QString fragment;
};

[[nodiscard]] QString NormalizeLocalMarkdownFragment(QString fragment) {
	fragment = QString::fromUtf8(
		QByteArray::fromPercentEncoding(fragment.toUtf8()));
	fragment = fragment.trimmed().toLower();
	while (fragment.startsWith(QChar('#'))) {
		fragment.remove(0, 1);
	}
	return fragment;
}

[[nodiscard]] LocalMarkdownTarget ParseLocalMarkdownTarget(QString path) {
	auto sourcePath = path;
	auto fragment = QString();
	if (!QFileInfo(sourcePath).exists()) {
		const auto hash = sourcePath.lastIndexOf(QChar('#'));
		const auto candidate = (hash > 0) ? sourcePath.mid(0, hash) : QString();
		if (!candidate.isEmpty() && QFileInfo(candidate).exists()) {
			fragment = NormalizeLocalMarkdownFragment(sourcePath.mid(hash + 1));
			sourcePath = candidate;
		}
	}
	const auto info = QFileInfo(sourcePath);
	if (!info.exists()) {
		return {
			.key = path,
			.path = std::move(path),
		};
	}
	auto result = LocalMarkdownTarget{
		.key = info.absoluteFilePath(),
		.path = info.absoluteFilePath(),
		.sourceName = info.fileName(),
		.fragment = std::move(fragment),
	};
	if (!result.fragment.isEmpty()) {
		result.path += u"#"_q + result.fragment;
	}
	return result;
}

[[nodiscard]] auto ExtractMarkdownMessageContext(const QVariant &context) {
	if (!context.isValid() || !context.canConvert<ClickHandlerContext>()) {
		return std::optional<MarkdownMessageContext>();
	}
	const auto clickHandlerContext = context.value<ClickHandlerContext>();
	return std::make_optional(MarkdownMessageContext{
		.clickHandlerContext = clickHandlerContext,
		.sessionWindow = clickHandlerContext.sessionWindow,
	});
}

[[nodiscard]] Main::Session *ResolveMarkdownSession(
		const MarkdownMessageContext &context) {
	if (const auto controller = context.sessionWindow.get()) {
		return &controller->session();
	}
	return nullptr;
}

[[nodiscard]] HistoryItem *ResolveMarkdownItem(
		const MarkdownMessageContext &context) {
	const auto session = ResolveMarkdownSession(context);
	const auto itemId = context.clickHandlerContext.itemId;
	return (session && itemId) ? session->data().message(itemId) : nullptr;
}

[[nodiscard]] bool CanShareMarkdownItem(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	return peer->allowsForwarding() && !item->forbidsForward();
}

[[nodiscard]] Markdown::OpenOptions PrepareLocalMarkdownOptions(
		QVariant context) {
	auto options = Markdown::OpenOptions{
		.viewerKind = Markdown::ViewerKind::LocalFile,
		.clickHandlerContext = std::move(context),
	};
	const auto messageContext = ExtractMarkdownMessageContext(
		options.clickHandlerContext);
	const auto item = messageContext
		? ResolveMarkdownItem(*messageContext)
		: nullptr;
	if (item && CanShareMarkdownItem(not_null{ item })) {
		options.share = [context = *messageContext](
				std::shared_ptr<Ui::Show> show) {
			const auto session = ResolveMarkdownSession(context);
			const auto itemId = context.clickHandlerContext.itemId;
			const auto current = (session && itemId)
				? session->data().message(itemId)
				: nullptr;
			if (!show || !current || !CanShareMarkdownItem(not_null{ current })) {
				return;
			}
			FastShareMessage(
				Main::MakeSessionShow(show, not_null{ session }),
				not_null{ current });
		};
	}
	return options;
}

} // namespace

class Shown final : public base::has_weak_ptr {
public:
	Shown(
		not_null<Delegate*> delegate,
		not_null<Main::Session*> session,
		not_null<Data*> data,
		QString hash,
		Fn<void(QString)> openChannel,
		Fn<void(QString)> joinChannel);

	[[nodiscard]] bool showing(
		not_null<Main::Session*> session,
		not_null<Data*> data) const;
	[[nodiscard]] bool showingFrom(not_null<Main::Session*> session) const;
	[[nodiscard]] bool activeFor(not_null<Main::Session*> session) const;
	[[nodiscard]] bool active() const;

	void moveTo(not_null<Data*> data, QString hash);
	void update(not_null<Data*> data);

	void showJoinedTooltip();
	void minimize();

	[[nodiscard]] rpl::producer<Controller::Event> events() const {
		return _events.events();
	}

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	struct MapPreview {
		std::unique_ptr<::Data::CloudFile> file;
		QByteArray bytes;
	};
	struct PartRequest {
		Webview::DataRequest request;
		QByteArray data;
		std::vector<bool> loaded;
		int64 offset = 0;
	};
	struct FileStream {
		not_null<DocumentData*> document;
		std::unique_ptr<::Media::Streaming::Loader> loader;
		std::vector<PartRequest> requests;
		std::string mime;
		rpl::lifetime lifetime;
	};
	struct FileLoad {
		std::shared_ptr<::Data::DocumentMedia> media;
		std::vector<Webview::DataRequest> requests;
	};

	void prepare(not_null<Data*> data, const QString &hash);
	void createController();
	void createMarkdownController(
		Markdown::MarkdownArticleContent content,
		QString title,
		QString initialFragment,
		not_null<WebPageData*> page);
	[[nodiscard]] Markdown::OpenOptions markdownOpenOptions(
		QString initialFragment,
		not_null<WebPageData*> page);

	void showWindowed(Prepared result, Source source, bool refresh);
	void showHtmlWindowed(Prepared result, bool refresh);
	void showMarkdownWindowed(
		Markdown::MarkdownArticleContent content,
		QString title,
		QString initialFragment,
		not_null<WebPageData*> page,
		bool refresh);
	[[nodiscard]] ShareBoxResult shareBox(ShareBoxDescriptor &&descriptor);
	[[nodiscard]] std::shared_ptr<Markdown::MediaRuntime> createMediaRuntime(
		not_null<WebPageData*> page) const;
	[[nodiscard]] bool activateMarkdownMedia(
		const Markdown::MediaActivation &activation,
		Qt::MouseButton button,
		const QVariant &clickHandlerContext) const;

	[[nodiscard]] ::Data::FileOrigin fileOrigin(
		not_null<WebPageData*> page) const;
	void streamPhoto(QStringView idWithPageId, Webview::DataRequest request);
	void streamFile(QStringView idWithPageId, Webview::DataRequest request);
	void streamFile(FileStream &file, Webview::DataRequest request);
	void processPartInFile(
		FileStream &file,
		::Media::Streaming::LoadedPart &&part);
	bool finishRequestWithPart(
		PartRequest &request,
		const ::Media::Streaming::LoadedPart &part);
	void streamMap(QString params, Webview::DataRequest request);
	void sendEmbed(QByteArray hash, Webview::DataRequest request);

	void fillChannelJoinedValues(const Prepared &result);
	void fillEmbeds(base::flat_map<QByteArray, QByteArray> added);
	void subscribeToDocuments();
	[[nodiscard]] QByteArray readFile(
		const std::shared_ptr<::Data::DocumentMedia> &media);
	void requestDone(
		Webview::DataRequest request,
		QByteArray bytes,
		std::string mime,
		int64 offset = 0,
		int64 total = 0);
	void requestFail(Webview::DataRequest request);

	const not_null<Delegate*> _delegate;
	const not_null<Main::Session*> _session;
	const Fn<void(QString)> _openChannel;
	const Fn<void(QString)> _joinChannel;
	std::shared_ptr<Main::SessionShow> _show;
	QString _id;
	std::unique_ptr<Controller> _controller;
	std::unique_ptr<Markdown::Controller> _markdownController;
	base::flat_map<DocumentId, FileStream> _streams;
	base::flat_map<DocumentId, FileLoad> _files;
	base::flat_map<QByteArray, rpl::producer<bool>> _inChannelValues;

	bool _preparing = false;

	base::flat_map<QByteArray, QByteArray> _embeds;
	base::flat_map<QString, MapPreview> _maps;
	std::vector<QByteArray> _resources;

	rpl::event_stream<Controller::Event> _events;

	rpl::lifetime _documentLifetime;
	rpl::lifetime _lifetime;

};

class TonSite final : public base::has_weak_ptr {
public:
	TonSite(not_null<Delegate*> delegate, QString uri);

	[[nodiscard]] bool active() const;

	void moveTo(QString uri);

	void minimize();

	[[nodiscard]] rpl::producer<Controller::Event> events() const {
		return _events.events();
	}

	[[nodiscard]] rpl::lifetime &lifetime() {
		return _lifetime;
	}

private:
	void createController();

	void showWindowed();

	const not_null<Delegate*> _delegate;
	QString _uri;
	std::unique_ptr<Controller> _controller;

	rpl::event_stream<Controller::Event> _events;

	rpl::lifetime _lifetime;

};

struct MarkdownShown {
	std::unique_ptr<Markdown::Controller> controller;
};

class ShareBoxShow final : public Ui::Show {
public:
	ShareBoxShow(QPointer<QWidget> parent, Fn<Ui::LayerStackWidget*()> lookup);

	void showOrHideBoxOrLayer(
			std::variant<
			v::null_t,
			object_ptr<Ui::BoxContent>,
			std::unique_ptr<Ui::LayerWidget>> &&layer,
			Ui::LayerOptions options,
			anim::type animated) const override;

	not_null<QWidget*> toastParent() const override;

	bool valid() const override;

	operator bool() const override;

private:
	const QPointer<QWidget> _parent;
	const Fn<Ui::LayerStackWidget*()> _lookup;
};

ShareBoxShow::ShareBoxShow(
	QPointer<QWidget> parent,
	Fn<Ui::LayerStackWidget*()> lookup)
: _parent(parent)
, _lookup(lookup) {
}

void ShareBoxShow::showOrHideBoxOrLayer(
		std::variant<
		v::null_t,
		object_ptr<Ui::BoxContent>,
		std::unique_ptr<Ui::LayerWidget>> &&layer,
		Ui::LayerOptions options,
		anim::type animated) const {
	using UniqueLayer = std::unique_ptr<Ui::LayerWidget>;
	using ObjectBox = object_ptr<Ui::BoxContent>;
	const auto stack = _lookup();
	if (!stack) {
		return;
	} else if (auto layerWidget = std::get_if<UniqueLayer>(&layer)) {
		stack->showLayer(std::move(*layerWidget), options, animated);
	} else if (auto box = std::get_if<ObjectBox>(&layer)) {
		stack->showBox(std::move(*box), options, animated);
	} else {
		stack->hideAll(animated);
	}
}

not_null<QWidget*> ShareBoxShow::toastParent() const {
	return _parent.data();
}

bool ShareBoxShow::valid() const {
	return _lookup() != nullptr;
}

ShareBoxShow::operator bool() const {
	return valid();
}

Shown::Shown(
	not_null<Delegate*> delegate,
	not_null<Main::Session*> session,
	not_null<Data*> data,
	QString hash,
	Fn<void(QString)> openChannel,
	Fn<void(QString)> joinChannel)
: _delegate(delegate)
, _session(session)
, _openChannel(std::move(openChannel))
, _joinChannel(std::move(joinChannel)) {
	prepare(data, hash);
}

void Shown::prepare(not_null<Data*> data, const QString &hash) {
	const auto weak = base::make_weak(this);
	const auto source = data->source();

	_preparing = true;
	const auto id = _id = data->id();
	data->prepare({}, [=, source = source](Prepared result) {
		result.hash = hash;
		crl::on_main(weak, [=, source = source, result = std::move(result)]() mutable {
			result.url = id;
			if (_id != id || !_preparing) {
				return;
			}
			_preparing = false;
			fillChannelJoinedValues(result);
			fillEmbeds(std::move(result.embeds));
			showWindowed(std::move(result), source, false);
		});
	});
}

void Shown::fillChannelJoinedValues(const Prepared &result) {
	for (const auto &id : result.channelIds) {
		const auto channelId = ChannelId(id.toLongLong());
		const auto channel = _session->data().channel(channelId);
		if (!channel->isLoaded() && !channel->username().isEmpty()) {
			channel->session().api().request(MTPcontacts_ResolveUsername(
				MTP_flags(0),
				MTP_string(channel->username()),
				MTP_string()
			)).done([=](const MTPcontacts_ResolvedPeer &result) {
				channel->owner().processUsers(result.data().vusers());
				channel->owner().processChats(result.data().vchats());
			}).send();
		}
		_inChannelValues[id] = Info::Profile::AmInChannelValue(channel);
	}
}

void Shown::fillEmbeds(base::flat_map<QByteArray, QByteArray> added) {
	if (_embeds.empty()) {
		_embeds = std::move(added);
	} else {
		for (auto &[k, v] : added) {
			_embeds[k] = std::move(v);
		}
	}
}

ShareBoxResult Shown::shareBox(ShareBoxDescriptor &&descriptor) {
	const auto url = descriptor.url;
	const auto wrap = descriptor.parent;

	struct State {
		Ui::LayerStackWidget *stack = nullptr;
		rpl::event_stream<> destroyRequests;
	};
	const auto state = wrap->lifetime().make_state<State>();

	const auto weak = base::make_weak(wrap);
	const auto lookup = crl::guard(weak, [state] { return state->stack; });
	const auto layer = Ui::CreateChild<Ui::LayerStackWidget>(
		wrap.get(),
		[=] { return std::make_shared<ShareBoxShow>(weak.get(), lookup); });
	state->stack = layer;
	const auto show = layer->showFactory()();

	layer->setHideByBackgroundClick(false);
	layer->move(0, 0);
	wrap->sizeValue(
	) | rpl::on_next([=](QSize size) {
		layer->resize(size);
	}, layer->lifetime());
	layer->hideFinishEvents(
	) | rpl::filter([=] {
		return !!lookup(); // Last hide finish is sent from destructor.
	}) | rpl::on_next([=] {
		state->destroyRequests.fire({});
	}, wrap->lifetime());

	const auto waiting = layer->lifetime().make_state<rpl::lifetime>();
	const auto focus = crl::guard(layer, [=] {
		const auto set = [=] {
			layer->window()->setFocus();
			layer->setInnerFocus();
		};

		const auto handle = layer->window()->windowHandle();
		if (!handle) {
			waiting->destroy();
			return;
		} else if (QGuiApplication::focusWindow() == handle) {
			waiting->destroy();
			set();
		} else {
			*waiting = base::qt_signal_producer(
				qApp,
				&QGuiApplication::focusWindowChanged
			) | rpl::filter([=](QWindow *focused) {
				const auto handle = layer->window()->windowHandle();
				return handle && (focused == handle);
			}) | rpl::on_next([=] {
				waiting->destroy();
				set();
			});
			layer->window()->activateWindow();
		}
	});
	auto result = ShareBoxResult{
		.focus = focus,
		.hide = [=] { show->hideLayer(); },
		.destroyRequests = state->destroyRequests.events(),
	};

	FastShareLink(Main::MakeSessionShow(show, _session), url);
	return result;
}

void Shown::createController() {
	Expects(!_controller);

	const auto showShareBox = [=](ShareBoxDescriptor &&descriptor) {
		return shareBox(std::move(descriptor));
	};
	_controller = std::make_unique<Controller>(
		_delegate,
		std::move(showShareBox));

	_controller->events(
	) | rpl::start_to_stream(_events, _controller->lifetime());

	_controller->dataRequests(
	) | rpl::on_next([=](Webview::DataRequest request) {
		const auto requested = QString::fromStdString(request.id);
		const auto id = QStringView(requested);
		if (id.startsWith(u"photo/")) {
			streamPhoto(id.mid(6), std::move(request));
		} else if (id.startsWith(u"document/"_q)) {
			streamFile(id.mid(9), std::move(request));
		} else if (id.startsWith(u"map/"_q)) {
			streamMap(id.mid(4).toUtf8(), std::move(request));
		} else if (id.startsWith(u"html/"_q)) {
			sendEmbed(id.mid(5).toUtf8(), std::move(request));
		}
	}, _controller->lifetime());
}

Markdown::OpenOptions Shown::markdownOpenOptions(
		QString initialFragment,
		not_null<WebPageData*> page) {
	const auto clickHandlerContext = std::make_shared<QVariant>();
	auto options = Markdown::OpenOptions{
		.sourceName = page->displayedSiteName(),
		.sourceUrl = page->url,
		.initialFragment = std::move(initialFragment),
		.currentPageId = page->id,
		.viewerKind = Markdown::ViewerKind::InstantView,
		.clickHandlerContextRef = clickHandlerContext,
		.ivWebviewDataRequest = [=](
				QByteArray id,
				Webview::DataRequest request) {
			const auto requested = QString::fromUtf8(id);
			const auto view = QStringView(requested);
			if (view.startsWith(u"photo/")) {
				streamPhoto(view.mid(6), std::move(request));
				return Webview::DataResult::Pending;
			} else if (view.startsWith(u"document/"_q)) {
				streamFile(view.mid(9), std::move(request));
				return Webview::DataResult::Pending;
			} else if (view.startsWith(u"map/"_q)) {
				streamMap(view.mid(4).toString().toUtf8(), std::move(request));
				return Webview::DataResult::Pending;
			} else if (view.startsWith(u"html/"_q)) {
				sendEmbed(view.mid(5).toString().toUtf8(), std::move(request));
				return Webview::DataResult::Pending;
			}
			return Webview::DataResult::Failed;
		},
		.ivWebviewStorageId = _session->local().resolveStorageIdOther(),
		.activateMedia = [=](
				const Markdown::MediaActivation &activation,
				Qt::MouseButton button) {
			return activateMarkdownMedia(activation, button, *clickHandlerContext);
		},
		.downloadTaskFinished = page->session().downloaderTaskFinished(),
	};
	if (!page->url.isEmpty()) {
		options.share = [=, url = page->url](std::shared_ptr<Ui::Show> show) {
			if (!show) {
				return;
			}
			FastShareLink(Main::MakeSessionShow(show, _session), url);
		};
	}
	return options;
}

void Shown::createMarkdownController(
		Markdown::MarkdownArticleContent content,
		QString title,
		QString initialFragment,
		not_null<WebPageData*> page) {
	Expects(!_markdownController);

	auto options = markdownOpenOptions(std::move(initialFragment), page);
	_markdownController = std::make_unique<Markdown::Controller>(
		_delegate,
		std::move(content),
		std::move(title),
		nullptr,
		std::move(options));
	_markdownController->events() | rpl::on_next([=](Markdown::Event event) {
		using FromType = Markdown::Event::Type;
		using ToType = Controller::Event::Type;
		switch (event.type) {
		case FromType::Close:
			_events.fire({ .type = ToType::Close });
			break;
		case FromType::Quit:
			_events.fire({ .type = ToType::Quit });
			break;
		case FromType::OpenPage:
			_events.fire({
				.type = ToType::OpenPage,
				.url = event.url,
				.context = QString::number(event.webpageId),
			});
			break;
		case FromType::OpenFile:
			break;
		}
	}, _markdownController->lifetime());
}

void Shown::showWindowed(Prepared result, Source source, bool refresh) {
	if constexpr (true) {
		const auto page = _session->data().webpage(result.pageId);
		auto native = Markdown::TryPrepareNativeInstantView({
			.source = &source,
			.mediaRuntime = createMediaRuntime(page),
		});
		showMarkdownWindowed(
			std::move(native.content),
			std::move(result.name),
			std::move(result.hash),
			page,
			refresh);
	} else {
		Q_UNUSED(source);
		showHtmlWindowed(std::move(result), refresh);
	}
}

void Shown::showHtmlWindowed(Prepared result, bool refresh) {
	_markdownController = nullptr;
	const auto hadController = (_controller != nullptr);
	if (!_controller) {
		createController();
	}
	if (refresh && hadController) {
		_controller->update(std::move(result));
	} else {
		_controller->show(
			_session->local().resolveStorageIdOther(),
			std::move(result),
			base::duplicate(_inChannelValues));
	}
}

void Shown::showMarkdownWindowed(
		Markdown::MarkdownArticleContent content,
		QString title,
		QString initialFragment,
		not_null<WebPageData*> page,
		bool refresh) {
	_controller = nullptr;
	if (!_markdownController) {
		createMarkdownController(
			std::move(content),
			std::move(title),
			std::move(initialFragment),
			page);
		_markdownController->activate();
		return;
	}
	auto options = markdownOpenOptions(std::move(initialFragment), page);
	if (refresh) {
		_markdownController->update(
			std::move(content),
			std::move(title),
			std::move(options));
	} else {
		_markdownController->show(
			std::move(content),
			std::move(title),
			std::move(options));
	}
}

std::shared_ptr<Markdown::MediaRuntime> Shown::createMediaRuntime(
		not_null<WebPageData*> page) const {
	return CreateCachedPageMediaRuntime(
		_session,
		page,
		_openChannel,
		_joinChannel);
}

bool Shown::activateMarkdownMedia(
		const Markdown::MediaActivation &activation,
		Qt::MouseButton button,
		const QVariant &clickHandlerContext) const {
	if (button != Qt::LeftButton && button != Qt::MiddleButton) {
		return false;
	}
	switch (activation.kind) {
	case Markdown::MediaActivationKind::None:
		return false;
	case Markdown::MediaActivationKind::ExternalUrl:
		if (activation.url.isEmpty()) {
			return false;
		}
		HiddenUrlClickHandler::Open(activation.url, clickHandlerContext);
		return true;
	case Markdown::MediaActivationKind::Embed:
		return false;
	case Markdown::MediaActivationKind::Photo:
		if (!activation.photo) {
			return false;
		}
		activation.photo->open(button);
		return true;
	case Markdown::MediaActivationKind::Document:
		if (!activation.document) {
			return false;
		}
		activation.document->open(button);
		return true;
	case Markdown::MediaActivationKind::OpenChannel:
		if (!activation.channel) {
			return false;
		}
		activation.channel->open(button);
		return true;
	case Markdown::MediaActivationKind::JoinChannel:
		if (!activation.channel) {
			return false;
		}
		activation.channel->join(button);
		return true;
	}
	return false;
}

::Data::FileOrigin Shown::fileOrigin(not_null<WebPageData*> page) const {
	return ::Data::FileOriginWebPage{ page->url };
}

void Shown::streamPhoto(
		QStringView idWithPageId,
		Webview::DataRequest request) {
	using namespace Data;

	const auto parts = idWithPageId.split('/');
	if (parts.size() != 2) {
		requestFail(std::move(request));
		return;
	}
	const auto photo = _session->data().photo(parts[0].toULongLong());
	const auto page = _session->data().webpage(parts[1].toULongLong());
	if (photo->isNull() || page->url.isEmpty()) {
		requestFail(std::move(request));
		return;
	}
	const auto media = photo->createMediaView();
	media->wanted(PhotoSize::Large, fileOrigin(page));
	const auto check = [=] {
		if (!media->loaded() && !media->owner()->failed(PhotoSize::Large)) {
			return false;
		}
		requestDone(
			request,
			media->imageBytes(PhotoSize::Large),
			"image/jpeg");
		return true;
	};
	if (!check()) {
		photo->session().downloaderTaskFinished(
		) | rpl::filter(
			check
		) | rpl::take(1) | rpl::start(_controller->lifetime());
	}
}

void Shown::streamFile(
		QStringView idWithPageId,
		Webview::DataRequest request) {
	using namespace Data;

	const auto parts = idWithPageId.split('/');
	if (parts.size() != 2) {
		requestFail(std::move(request));
		return;
	}
	const auto documentId = DocumentId(parts[0].toULongLong());
	const auto i = _streams.find(documentId);
	if (i != end(_streams)) {
		streamFile(i->second, std::move(request));
		return;
	}
	const auto document = _session->data().document(documentId);
	const auto page = _session->data().webpage(parts[1].toULongLong());
	if (page->url.isEmpty()) {
		requestFail(std::move(request));
		return;
	}
	auto loader = document->createStreamingLoader(fileOrigin(page), false);
	if (!loader) {
		if (document->size >= Storage::kMaxFileInMemory) {
			requestFail(std::move(request));
		} else {
			auto media = document->createMediaView();
			if (const auto content = readFile(media); !content.isEmpty()) {
				requestDone(
					std::move(request),
					content,
					document->mimeString().toStdString());
			} else {
				subscribeToDocuments();
				auto &file = _files[documentId];
				file.media = std::move(media);
				file.requests.push_back(std::move(request));
				document->forceToCache(true);
				document->save(fileOrigin(page), QString());
			}
		}
		return;
	}
	auto &file = _streams.emplace(
		documentId,
		FileStream{
			.document = document,
			.loader = std::move(loader),
			.mime = document->mimeString().toStdString(),
		}).first->second;

	file.loader->parts(
	) | rpl::on_next([=](::Media::Streaming::LoadedPart &&part) {
		const auto i = _streams.find(documentId);
		Assert(i != end(_streams));
		processPartInFile(i->second, std::move(part));
	}, file.lifetime);

	streamFile(file, std::move(request));
}

void Shown::streamFile(FileStream &file, Webview::DataRequest request) {
	constexpr auto kPart = ::Media::Streaming::Loader::kPartSize;
	const auto size = file.document->size;
	const auto last = int((size + kPart - 1) / kPart);
	const auto from = int(std::min(int64(request.offset), size) / kPart);
	const auto till = (request.limit > 0)
		? std::min(int64(request.offset + request.limit), size)
		: size;
	const auto parts = std::min(
		int((till + kPart - 1) / kPart) - from,
		kMaxLoadParts);
	//auto base = IvBaseCacheKey(document);

	const auto length = std::min((from + parts) * kPart, size)
		- from * kPart;
	file.requests.push_back(PartRequest{
		.request = std::move(request),
		.data = QByteArray(length, 0),
		.loaded = std::vector<bool>(parts, false),
		.offset = from * kPart,
	});

	file.loader->resetPriorities();
	const auto load = std::min(from + kKeepLoadingParts, last) - from;
	for (auto i = 0; i != load; ++i) {
		file.loader->load((from + i) * kPart);
	}
}

void Shown::subscribeToDocuments() {
	if (_documentLifetime) {
		return;
	}
	_documentLifetime = _session->data().documentLoadProgress(
	) | rpl::filter([=](not_null<DocumentData*> document) {
		return !document->loading();
	}) | rpl::on_next([=](not_null<DocumentData*> document) {
		const auto i = _files.find(document->id);
		if (i == end(_files)) {
			return;
		}
		auto requests = base::take(i->second.requests);
		const auto content = readFile(i->second.media);
		_files.erase(i);

		if (!content.isEmpty()) {
			for (auto &request : requests) {
				requestDone(
					std::move(request),
					content,
					document->mimeString().toStdString());
			}
		} else {
			for (auto &request : requests) {
				requestFail(std::move(request));
			}
		}
	});
}

QByteArray Shown::readFile(
		const std::shared_ptr<::Data::DocumentMedia> &media) {
	return Lottie::ReadContent(media->bytes(), media->owner()->filepath());
}

void Shown::processPartInFile(
		FileStream &file,
		::Media::Streaming::LoadedPart &&part) {
	for (auto i = begin(file.requests); i != end(file.requests);) {
		if (finishRequestWithPart(*i, part)) {
			auto done = base::take(*i);
			i = file.requests.erase(i);
			requestDone(
				std::move(done.request),
				done.data,
				file.mime,
				done.offset,
				file.document->size);
		} else {
			++i;
		}
	}
}

bool Shown::finishRequestWithPart(
		PartRequest &request,
		const ::Media::Streaming::LoadedPart &part) {
	const auto offset = part.offset;
	if (offset == ::Media::Streaming::LoadedPart::kFailedOffset) {
		request.data = QByteArray();
		return true;
	} else if (offset < request.offset
		|| offset >= request.offset + request.data.size()) {
		return false;
	}
	constexpr auto kPart = ::Media::Streaming::Loader::kPartSize;
	const auto copy = std::min(
		int(part.bytes.size()),
		int(request.data.size() - (offset - request.offset)));
	const auto index = (offset - request.offset) / kPart;
	Assert(index < request.loaded.size());
	if (request.loaded[index]) {
		return false;
	}
	request.loaded[index] = true;
	memcpy(
		request.data.data() + index * kPart,
		part.bytes.constData(),
		copy);
	return !ranges::contains(request.loaded, false);
}

void Shown::streamMap(QString params, Webview::DataRequest request) {
	using namespace ::Data;

	const auto parts = params.split(u'&');
	if (parts.size() != 3) {
		requestFail(std::move(request));
		return;
	}
	const auto point = GeoPointFromId(parts[0].toUtf8());
	const auto size = parts[1].split(',');
	const auto zoom = parts[2].toInt();
	if (size.size() != 2) {
		requestFail(std::move(request));
		return;
	}
	const auto location = GeoPointLocation{
		.lat = point.lat,
		.lon = point.lon,
		.access = point.access,
		.width = size[0].toInt(),
		.height = size[1].toInt(),
		.zoom = std::max(zoom, kGeoPointZoomMin),
		.scale = kGeoPointScale,
	};
	const auto prepared = ImageWithLocation{
		.location = ImageLocation(
			{ location },
			location.width,
			location.height)
	};
	auto &preview = _maps.emplace(params, MapPreview()).first->second;
	preview.file = std::make_unique<CloudFile>();

	UpdateCloudFile(
		*preview.file,
		prepared,
		_session->data().cache(),
		kImageCacheTag,
		[=](FileOrigin origin) { /* restartLoader not used here */ });
	const auto autoLoading = false;
	const auto finalCheck = [=] { return true; };
	const auto done = [=](QByteArray bytes) {
		const auto i = _maps.find(params);
		Assert(i != end(_maps));
		i->second.bytes = std::move(bytes);
		requestDone(request, i->second.bytes, "image/png");
	};
	LoadCloudFile(
		_session,
		*preview.file,
		FileOrigin(),
		LoadFromCloudOrLocal,
		autoLoading,
		kImageCacheTag,
		finalCheck,
		done,
		[=](bool) { done("failed..."); });
}

void Shown::sendEmbed(QByteArray hash, Webview::DataRequest request) {
	const auto i = _embeds.find(hash);
	if (i != end(_embeds)) {
		requestDone(std::move(request), i->second, "text/html; charset=utf-8");
	} else {
		requestFail(std::move(request));
	}
}

void Shown::requestDone(
		Webview::DataRequest request,
		QByteArray bytes,
		std::string mime,
		int64 offset,
		int64 total) {
	if (bytes.isEmpty() && mime.empty()) {
		requestFail(std::move(request));
		return;
	}
	crl::on_main([
		done = std::move(request.done),
		data = std::move(bytes),
		mime = std::move(mime),
		offset,
		total
	] {
		using namespace Webview;
		done({
			.stream = std::make_unique<DataStreamFromMemory>(data, mime),
			.streamOffset = offset,
			.totalSize = total,
		});
	});
}

void Shown::requestFail(Webview::DataRequest request) {
	crl::on_main([done = std::move(request.done)] {
		done({});
	});
}

bool Shown::showing(
		not_null<Main::Session*> session,
		not_null<Data*> data) const {
	return showingFrom(session) && (_id == data->id());
}

bool Shown::showingFrom(not_null<Main::Session*> session) const {
	return (_session == session);
}

bool Shown::activeFor(not_null<Main::Session*> session) const {
	return showingFrom(session) && (_controller || _markdownController);
}

bool Shown::active() const {
	return (_controller && _controller->active())
		|| (_markdownController && _markdownController->active());
}

void Shown::moveTo(not_null<Data*> data, QString hash) {
	prepare(data, hash);
}

void Shown::update(not_null<Data*> data) {
	const auto weak = base::make_weak(this);
	const auto source = data->source();

	const auto id = data->id();
	data->prepare({}, [=, source = source](Prepared result) {
		crl::on_main(weak, [=, source = source, result = std::move(result)]() mutable {
			result.url = id;
			fillChannelJoinedValues(result);
			fillEmbeds(std::move(result.embeds));
			showWindowed(std::move(result), source, true);
		});
	});
}

void Shown::showJoinedTooltip() {
	if (_controller) {
		_controller->showJoinedTooltip();
	} else if (_markdownController) {
		_markdownController->showJoinedTooltip();
	}
}

void Shown::minimize() {
	if (_controller) {
		_controller->minimize();
	} else if (_markdownController) {
		_markdownController->minimize();
	}
}

TonSite::TonSite(not_null<Delegate*> delegate, QString uri)
: _delegate(delegate)
, _uri(uri) {
	showWindowed();
}

void TonSite::createController() {
	Expects(!_controller);

	const auto showShareBox = [=](ShareBoxDescriptor &&descriptor) {
		return ShareBoxResult();
	};
	_controller = std::make_unique<Controller>(
		_delegate,
		std::move(showShareBox));

	_controller->events(
	) | rpl::start_to_stream(_events, _controller->lifetime());
}

void TonSite::showWindowed() {
	if (!_controller) {
		createController();
	}

	_controller->showTonSite(Storage::TonSiteStorageId(), _uri);
}

bool TonSite::active() const {
	return _controller && _controller->active();
}

void TonSite::moveTo(QString uri) {
	_controller->showTonSite({}, uri);
}

void TonSite::minimize() {
	if (_controller) {
		_controller->minimize();
	}
}

Instance::Instance(not_null<Delegate*> delegate) : _delegate(delegate) {
}

Instance::~Instance() = default;

void Instance::show(
		not_null<Window::SessionController*> controller,
		not_null<Data*> data,
		QString hash) {
	show(controller->uiShow(), data, hash);
}

void Instance::show(
		std::shared_ptr<Main::SessionShow> show,
		not_null<Data*> data,
		QString hash) {
	this->show(&show->session(), data, hash);
}

void Instance::show(
		not_null<Main::Session*> session,
		not_null<Data*> data,
		QString hash) {
	if (Platform::IsMac()) {
		// Otherwise IV is not visible under the media viewer.
		Core::App().hideMediaView();
	}

	if (Core::App().settings().normalizeIvZoom()) {
		Core::App().saveSettingsDelayed();
	}

	const auto guard = gsl::finally([&] {
		requestFull(session, data->id());
	});
	if (_shown && _shownSession == session) {
		_shown->moveTo(data, hash);
		return;
	}
	_shown = std::make_unique<Shown>(
		_delegate,
		session,
		data,
		hash,
		[=](QString context) {
			processOpenChannel(context);
		},
		[=](QString context) {
			processJoinChannel(context);
		});
	_shownSession = session;
	_shown->events() | rpl::on_next([=](Controller::Event event) {
		using Type = Controller::Event::Type;
		const auto lower = event.url.toLower();
		const auto urlChecked = lower.startsWith("http://")
			|| lower.startsWith("https://");
		const auto tonsite = lower.startsWith("tonsite://");
		switch (event.type) {
		case Type::Close:
			_shown = nullptr;
			break;
		case Type::Quit:
			Shortcuts::Launch(Shortcuts::Command::Quit);
			break;
		case Type::OpenChannel:
			processOpenChannel(event.context);
			break;
		case Type::JoinChannel:
			processJoinChannel(event.context);
			break;
		case Type::OpenLinkExternal:
			if (urlChecked) {
				File::OpenUrl(event.url);
				closeAll();
			} else if (tonsite) {
				showTonSite(event.url);
			}
			break;
		case Type::OpenMedia:
			if (const auto window = Core::App().activeWindow()) {
				const auto current = window->sessionController();
				const auto controller = (current
					&& &current->session() == _shownSession)
					? current
					: nullptr;
				const auto item = (HistoryItem*)nullptr;
				const auto topicRootId = MsgId(0);
				const auto monoforumPeerId = PeerId(0);
				if (event.context.startsWith("-photo")) {
					const auto id = event.context.mid(6).toULongLong();
					const auto photo = _shownSession->data().photo(id);
					if (!photo->isNull()) {
						window->openInMediaView({
							controller,
							photo,
							item,
							topicRootId,
							monoforumPeerId
						});
					}
				} else if (event.context.startsWith("-video")) {
					const auto id = event.context.mid(6).toULongLong();
					const auto video = _shownSession->data().document(id);
					if (!video->isNull()) {
						window->openInMediaView({
							controller,
							video,
							item,
							topicRootId,
							monoforumPeerId
						});
					}
				}
			}
			break;
		case Type::OpenPage:
		case Type::OpenLink: {
			if (tonsite) {
				showTonSite(event.url);
				break;
			} else if (!urlChecked) {
				break;
			}
			const auto session = _shownSession;
			const auto url = event.url;
			auto &requested = _fullRequested[session][url];
			requested.lastRequestedAt = crl::now();
			session->api().request(MTPmessages_GetWebPage(
				MTP_string(url),
				MTP_int(requested.hash)
			)).done([=](const MTPmessages_WebPage &result) {
				const auto page = processReceivedPage(session, url, result);
				if (page && page->iv) {
					const auto parts = event.url.split('#');
					const auto hash = (parts.size() > 1) ? parts[1] : u""_q;
					this->show(_shownSession, page->iv.get(), hash);
				} else {
					UrlClickHandler::Open(event.url);
				}
			}).fail([=] {
				UrlClickHandler::Open(event.url);
			}).send();
		} break;
		case Type::Report:
			if (const auto controller = _shownSession->tryResolveWindow()) {
				controller->window().activate();
				controller->showPeerByLink(Window::PeerByLinkInfo{
					.usernameOrId = "previews",
					.resolveType = Window::ResolveType::BotStart,
					.startToken = ("webpage"
						+ QString::number(event.context.toULongLong())),
				});
			}
			break;
		}
	}, _shown->lifetime());

	session->changes().peerUpdates(
		::Data::PeerUpdate::Flag::ChannelAmIn
	) | rpl::on_next([=](const ::Data::PeerUpdate &update) {
		if (const auto channel = update.peer->asChannel()) {
			if (channel->amIn()) {
				const auto i = _joining.find(session);
				const auto value = not_null{ channel };
				if (i != end(_joining) && i->second.remove(value)) {
					_shown->showJoinedTooltip();
				}
			}
		}
	}, _shown->lifetime());

	trackSession(session);
}

void Instance::trackSession(not_null<Main::Session*> session) {
	if (!_tracking.emplace(session).second) {
		return;
	}
	session->lifetime().add([=] {
		_tracking.remove(session);
		_joining.remove(session);
		_fullRequested.remove(session);
		_ivCache.remove(session);
		if (_ivRequestSession == session) {
			session->api().request(_ivRequestId).cancel();
			_ivRequestSession = nullptr;
			_ivRequestUri = QString();
			_ivRequestId = 0;
		}
		if (_shownSession == session) {
			_shownSession = nullptr;
		}
		if (_shown && _shown->showingFrom(session)) {
			_shown = nullptr;
		}
	});
}

void Instance::openWithIvPreferred(
		not_null<Window::SessionController*> controller,
		QString uri,
		QVariant context) {
	auto my = context.value<ClickHandlerContext>();
	my.sessionWindow = controller;
	openWithIvPreferred(
		&controller->session(),
		uri,
		QVariant::fromValue(my));
}

void Instance::openWithIvPreferred(
		not_null<Main::Session*> session,
		QString uri,
		QVariant context) {
	const auto openExternal = [=] {
		auto my = context.value<ClickHandlerContext>();
		my.ignoreIv = true;
		const auto updated = QVariant::fromValue(my);
		if (my.forceExternalUrlConfirmation) {
			HiddenUrlClickHandler::Open(uri, updated);
		} else {
			UrlClickHandler::Open(uri, updated);
		}
	};
	const auto parts = uri.split('#');
	if (parts.isEmpty() || parts[0].isEmpty()) {
		return;
	} else if (!ShowButton()) {
		return openExternal();
	}
	trackSession(session);
	const auto hash = (parts.size() > 1) ? parts[1] : u""_q;
	const auto url = parts[0];
	auto &cache = _ivCache[session];
	if (const auto i = cache.find(url); i != end(cache)) {
		const auto page = i->second;
		if (page && page->iv) {
			auto my = context.value<ClickHandlerContext>();
			if (const auto window = my.sessionWindow.get()) {
				show(window, page->iv.get(), hash);
			} else {
				show(session, page->iv.get(), hash);
			}
		} else {
			openExternal();
		}
		return;
	} else if (_ivRequestSession == session.get() && _ivRequestUri == uri) {
		return;
	} else if (_ivRequestId) {
		_ivRequestSession->api().request(_ivRequestId).cancel();
	}
	const auto finish = [=](WebPageData *page) {
		Expects(_ivRequestSession == session);

		_ivRequestId = 0;
		_ivRequestUri = QString();
		_ivRequestSession = nullptr;
		_ivCache[session][url] = page;
		openWithIvPreferred(session, uri, context);
	};
	_ivRequestSession = session;
	_ivRequestUri = uri;
	auto &requested = _fullRequested[session][url];
	requested.lastRequestedAt = crl::now();
	_ivRequestId = session->api().request(MTPmessages_GetWebPage(
		MTP_string(url),
		MTP_int(requested.hash)
	)).done([=](const MTPmessages_WebPage &result) {
		finish(processReceivedPage(session, url, result));
	}).fail([=] {
		finish(nullptr);
	}).send();
}

void Instance::showTonSite(
		const QString &uri,
		QVariant context) {
	if (!Controller::IsGoodTonSiteUrl(uri)) {
		Ui::Toast::Show(tr::lng_iv_not_supported(tr::now));
		return;
	} else if (Platform::IsMac()) {
		// Otherwise IV is not visible under the media viewer.
		Core::App().hideMediaView();
	}
	if (_tonSite) {
		_tonSite->moveTo(uri);
		return;
	}
	_tonSite = std::make_unique<TonSite>(_delegate, uri);
	_tonSite->events() | rpl::on_next([=](Controller::Event event) {
		using Type = Controller::Event::Type;
		const auto lower = event.url.toLower();
		const auto urlChecked = lower.startsWith("http://")
			|| lower.startsWith("https://");
		const auto tonsite = lower.startsWith("tonsite://");
		switch (event.type) {
		case Type::Close:
			_tonSite = nullptr;
			break;
		case Type::Quit:
			Shortcuts::Launch(Shortcuts::Command::Quit);
			break;
		case Type::OpenLinkExternal:
			if (urlChecked) {
				File::OpenUrl(event.url);
				closeAll();
			} else if (tonsite) {
				showTonSite(event.url);
			}
			break;
		case Type::OpenPage:
		case Type::OpenLink:
			if (urlChecked) {
				UrlClickHandler::Open(event.url);
			} else if (tonsite) {
				showTonSite(event.url);
			}
			break;
		}
	}, _tonSite->lifetime());
}

bool Instance::showMarkdown(
		const QString &path,
		QVariant context) {
	const auto target = ParseLocalMarkdownTarget(path);
	auto options = PrepareLocalMarkdownOptions(context);
	if (!target.sourceName.isEmpty()) {
		options.sourceName = target.sourceName;
		options.sourcePath = target.key;
	}
	options.initialFragment = target.fragment;
	auto i = _markdowns.find(target.key);
	if (i == end(_markdowns)) {
		if (auto controller = Markdown::TryOpenLocalFile(
				_delegate,
				target.path,
				std::move(options))) {
			controller->events() | rpl::on_next([=](Markdown::Event event) {
				using Type = Markdown::Event::Type;
				switch (event.type) {
				case Type::Close:
					_markdowns.take(target.key);
					break;
				case Type::Quit:
					Shortcuts::Launch(Shortcuts::Command::Quit);
					break;
				// Don't try opening markdown links inside markdown viewer,
				// messenger-provided markdown files should know nothing
				// about other local files and their paths.
				//
				//case Type::OpenFile:
				//	if (!showMarkdown(event.url, event.context)) {
				//		DEBUG_LOG(("Native Markdown IV: "
				//			"failed local markdown link: %1"
				//			).arg(event.url));
				//	}
				//	break;
				}
			}, controller->lifetime());

			i = _markdowns.emplace(target.key, std::move(controller)).first;
		} else {
			return false;
		}
	} else {
		i->second->updateOptions(std::move(options));
	}
	i->second->activate();
	return true;
}

void Instance::requestFull(
		not_null<Main::Session*> session,
		const QString &id) {
	if (!_tracking.contains(session)) {
		return;
	}
	auto &requested = _fullRequested[session][id];
	const auto last = requested.lastRequestedAt;
	const auto now = crl::now();
	if (last && (now - last) < kAllowPageReloadAfter) {
		return;
	}
	requested.lastRequestedAt = now;
	session->api().request(MTPmessages_GetWebPage(
		MTP_string(id),
		MTP_int(requested.hash)
	)).done([=](const MTPmessages_WebPage &result) {
		const auto page = processReceivedPage(session, id, result);
		if (page && page->iv && _shown && _shownSession == session) {
			_shown->update(page->iv.get());
		}
	}).send();
}

WebPageData *Instance::processReceivedPage(
		not_null<Main::Session*> session,
		const QString &url,
		const MTPmessages_WebPage &result) {
	const auto &data = result.data();
	const auto owner = &session->data();
	owner->processUsers(data.vusers());
	owner->processChats(data.vchats());
	auto &requested = _fullRequested[session][url];
	const auto &mtp = data.vwebpage();
	mtp.match([&](const MTPDwebPageNotModified &data) {
		const auto page = requested.page;
		if (const auto views = data.vcached_page_views()) {
			if (page && page->iv) {
				page->iv->updateCachedViews(views->v);
			}
		}
	}, [&](const MTPDwebPage &data) {
		requested.hash = data.vhash().v;
		requested.page = owner->processWebpage(data).get();
	}, [&](const auto &) {
		requested.page = owner->processWebpage(mtp).get();
	});
	return requested.page;
}

void Instance::processOpenChannel(const QString &context) {
	if (!_shownSession) {
		return;
	}
	const auto parsed = ParseNativeIvChannelContext(context);
	if (const auto channelId = ChannelId(parsed.channelId)) {
		const auto channel = _shownSession->data().channel(channelId);
		if (channel->isLoaded()) {
			if (const auto controller = _shownSession->tryResolveWindow(channel)) {
				controller->showPeerHistory(channel);
				_shown = nullptr;
			}
		} else if (const auto username = ResolveNativeIvChannelUsername(
				channel->username(),
				parsed.username); !username.isEmpty()) {
			if (const auto controller = _shownSession->tryResolveWindow(channel)) {
				controller->showPeerByLink({
					.usernameOrId = username,
				});
				_shown = nullptr;
			}
		}
	}
}

void Instance::processJoinChannel(const QString &context) {
	if (!_shownSession) {
		return;
	}
	const auto parsed = ParseNativeIvChannelContext(context);
	if (const auto channelId = ChannelId(parsed.channelId)) {
		const auto channel = _shownSession->data().channel(channelId);
		_joining[_shownSession].emplace(channel);
		if (channel->isLoaded()) {
			_shownSession->api().joinChannel(channel);
		} else if (const auto username = ResolveNativeIvChannelUsername(
				channel->username(),
				parsed.username); !username.isEmpty()) {
			if (const auto controller = _shownSession->tryResolveWindow(channel)) {
				controller->showPeerByLink({
					.usernameOrId = username,
					.joinChannel = true,
				});
			}
		}
	}
}

bool Instance::hasActiveWindow(not_null<Main::Session*> session) const {
	return _shown && _shown->activeFor(session);
}

bool Instance::closeActive() {
	if (_shown && _shown->active()) {
		_shown = nullptr;
		return true;
	} else if (_tonSite && _tonSite->active()) {
		_tonSite = nullptr;
		return true;
	}
	for (auto &[key, controller] : _markdowns) {
		if (controller->active()) {
			_markdowns.take(key);
			return true;
		}
	}
	return false;
}

bool Instance::minimizeActive() {
	if (_shown && _shown->active()) {
		_shown->minimize();
		return true;
	} else if (_tonSite && _tonSite->active()) {
		_tonSite->minimize();
		return true;
	}
	return false;
}

void Instance::closeAll() {
	_shown = nullptr;
	_tonSite = nullptr;
}

bool PreferForUri(const QString &uri) {
	const auto url = QUrl(uri);
	const auto host = url.host().toLower();
	const auto path = url.path().toLower();
	return (host == u"telegra.ph"_q)
		|| (host == u"te.legra.ph"_q)
		|| (host == u"graph.org"_q)
		|| (host == u"telegram.org"_q
			&& (path.startsWith(u"/faq"_q)
				|| path.startsWith(u"/privacy"_q)
				|| path.startsWith(u"/blog"_q)));
}

} // namespace Iv
