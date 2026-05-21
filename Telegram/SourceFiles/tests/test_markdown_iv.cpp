#include "iv/markdown/iv_markdown_article.h"
#include "iv/markdown/iv_markdown_article_text.h"
#include "iv/markdown/iv_markdown_document.h"
#include "iv/markdown/iv_markdown_embed_overlay.h"
#include "iv/markdown/iv_markdown_math_renderer.h"
#include "iv/markdown/iv_markdown_microtex.h"
#include "iv/markdown/iv_markdown_parse.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/markdown/iv_markdown_prepare_serialize.h"
#include "iv/markdown/iv_markdown_view.h"
#include "iv/markdown/iv_markdown_view_widget.h"
#include "iv/iv_prepare.h"
#include "scheme.h"

#include "lang/lang_keys.h"
#include "spellcheck/spellcheck_highlight_syntax.h"
#include "ui/basic_click_handlers.h"
#include "ui/chat/chat_style.h"
#include "ui/dynamic_image.h"
#include "ui/style/style_core.h"
#include "ui/style/style_core_scale.h"
#include "ui/widgets/rp_window.h"
#include "ui/widgets/scroll_area.h"

#include "styles/style_layers.h"
#include "styles/style_iv.h"

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QIODevice>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>

#include <rpl/event_stream.h>
#include <rpl/never.h>

#include <algorithm>
#include <iostream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace {

using namespace Iv::Markdown;

struct TestNativeIvChannelContext {
	uint64 channelId = 0;
	QString username;
};

[[nodiscard]] TestNativeIvChannelContext ParseTestNativeIvChannelContext(
		const QString &context) {
	const auto separator = context.indexOf(u'\n');
	return {
		.channelId = (separator >= 0)
			? context.mid(0, separator).toULongLong()
			: context.toULongLong(),
		.username = (separator >= 0) ? context.mid(separator + 1) : QString(),
	};
}

[[nodiscard]] QString SerializeTestNativeIvChannelContext(
		uint64 channelId,
		QString username) {
	auto result = QString::number(channelId);
	if (!username.isEmpty()) {
		result += u"\n"_q + username;
	}
	return result;
}

[[nodiscard]] QString ResolveTestNativeIvChannelUsername(
		const QString &channelUsername,
		const QString &contextUsername) {
	return !channelUsername.isEmpty() ? channelUsername : contextUsername;
}

struct Args {
	QString markdownPath;
	QString latexMarkdownPath;
	bool dump = false;
	bool inlineHtml = false;
	bool ok = true;
	QString error;
};

struct PreparedFixture {
	QString label;
	QString path;
	PreparedDocument parsed;
	MarkdownArticleContent prepared;
};

class TestDynamicImage final : public Ui::DynamicImage {
public:
	explicit TestDynamicImage(QImage frame = QImage())
	: _frame(std::move(frame)) {
	}

	[[nodiscard]] std::shared_ptr<DynamicImage> clone() override {
		return std::make_shared<TestDynamicImage>(_frame);
	}

	[[nodiscard]] QImage image(int size) override {
		requestedSizes.push_back(size);
		return _frame;
	}

	void subscribeToUpdates(Fn<void()> callback) override {
		++subscriptionCount;
		_callback = std::move(callback);
	}

	void setFrame(QImage frame) {
		_frame = std::move(frame);
	}

	void notify() const {
		if (_callback) {
			_callback();
		}
	}

	mutable std::vector<int> requestedSizes;
	mutable int subscriptionCount = 0;

private:
	QImage _frame;
	mutable Fn<void()> _callback;
};

class TestPhotoRuntime final : public PhotoRuntime {
public:
	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> thumbnail(
			QSize size) const override {
		thumbnailSizes.push_back(size);
		return thumbnailImage;
	}

	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> full(
			QSize size) const override {
		fullSizes.push_back(size);
		return fullImage;
	}

	[[nodiscard]] bool loaded() const override {
		return loadedValue;
	}

	[[nodiscard]] bool loading() const override {
		return loadingValue;
	}

	[[nodiscard]] double progress() const override {
		return progressValue;
	}

	void open(Qt::MouseButton button) const override {
		openedButtons.push_back(button);
	}

	std::shared_ptr<TestDynamicImage> thumbnailImage;
	std::shared_ptr<TestDynamicImage> fullImage;
	bool loadedValue = false;
	bool loadingValue = false;
	double progressValue = 0.;
	mutable std::vector<QSize> thumbnailSizes;
	mutable std::vector<QSize> fullSizes;
	mutable std::vector<Qt::MouseButton> openedButtons;
};

class TestDocumentRuntime final : public DocumentRuntime {
public:
	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> thumbnail(
			QSize size) const override {
		thumbnailSizes.push_back(size);
		return thumbnailImage;
	}

	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> full(
			QSize size) const override {
		fullSizes.push_back(size);
		return fullImage;
	}

	[[nodiscard]] bool loaded() const override {
		return loadedValue;
	}

	[[nodiscard]] bool loading() const override {
		return loadingValue;
	}

	[[nodiscard]] double progress() const override {
		return progressValue;
	}

	void open(Qt::MouseButton button) const override {
		openedButtons.push_back(button);
	}

	std::shared_ptr<TestDynamicImage> thumbnailImage;
	std::shared_ptr<TestDynamicImage> fullImage;
	bool loadedValue = false;
	bool loadingValue = false;
	double progressValue = 0.;
	mutable std::vector<QSize> thumbnailSizes;
	mutable std::vector<QSize> fullSizes;
	mutable std::vector<Qt::MouseButton> openedButtons;
};

class TestMapRuntime final : public MapRuntime {
public:
	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> thumbnail(
			QSize size) const override {
		thumbnailSizes.push_back(size);
		return thumbnailImage;
	}

	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> full(
			QSize size) const override {
		fullSizes.push_back(size);
		return fullImage;
	}

	[[nodiscard]] bool loaded() const override {
		return loadedValue;
	}

	[[nodiscard]] bool loading() const override {
		return loadingValue;
	}

	[[nodiscard]] double progress() const override {
		return progressValue;
	}

	std::shared_ptr<TestDynamicImage> thumbnailImage;
	std::shared_ptr<TestDynamicImage> fullImage;
	bool loadedValue = false;
	bool loadingValue = false;
	double progressValue = 0.;
	mutable std::vector<QSize> thumbnailSizes;
	mutable std::vector<QSize> fullSizes;
};

class TestChannelRuntime final : public ChannelRuntime {
public:
	[[nodiscard]] bool joinVisible() const override {
		return joinVisibleValue;
	}

	void open(Qt::MouseButton button) const override {
		openedButtons.push_back(button);
	}

	void join(Qt::MouseButton button) const override {
		joinedButtons.push_back(button);
	}

	bool joinVisibleValue = true;
	mutable std::vector<Qt::MouseButton> openedButtons;
	mutable std::vector<Qt::MouseButton> joinedButtons;
};

enum class TestHostedMediaKind {
	Photo,
	Video,
	Audio,
	Map,
};

class TestHostedMediaBlock final : public MediaBlock {
public:
	TestHostedMediaBlock(
		TestHostedMediaKind kind,
		uint64 stableId,
		QString copyText,
		MediaActivation activation)
	: _kind(kind)
	, _stableId(stableId)
	, _copyText(std::move(copyText))
	, _activation(std::move(activation))
	, _auxiliaryLink(std::make_shared<LambdaClickHandler>([] {
	})) {
	}

	[[nodiscard]] uint64 stableId() const override {
		return _stableId;
	}

	[[nodiscard]] int resizeGetHeight(int width) override {
		resizeWidths.push_back(width);
		return std::max(width / 2, 72);
	}

	void setGeometry(QRect geometry) override {
		requestedGeometries.push_back(geometry);
		_geometry = QRect(
			geometry.topLeft() + QPoint(7, 5),
			QSize(
				std::max(geometry.width() - 32, 1),
				std::max(geometry.height() - 48, 1)));
		appliedGeometries.push_back(_geometry);
	}

	[[nodiscard]] QRect geometry() const override {
		return _geometry;
	}

	[[nodiscard]] int firstLineBaseline() const override {
		return _geometry.y() + std::min(_geometry.height(), 12);
	}

	void paint(
			Painter &p,
			QRect clip,
			const MarkdownArticlePaintCaches &caches) const override {
		Q_UNUSED(caches);
		const auto visible = clip.intersected(_geometry);
		if (visible.isEmpty()) {
			return;
		}
		p.save();
		p.setClipRect(visible);
		p.fillRect(_geometry, color());
		p.restore();
	}

	[[nodiscard]] ClickHandlerPtr linkAt(QPoint point) const override {
		return auxiliaryArticleRect().contains(point) ? _auxiliaryLink : nullptr;
	}

	[[nodiscard]] MediaActivation activationAt(QPoint point) const override {
		return _geometry.contains(point) ? _activation : MediaActivation();
	}

	[[nodiscard]] MediaBlockSelectionData selectionData() const override {
		return {
			.copyText = _copyText,
		};
	}

	[[nodiscard]] QPoint primaryArticlePoint() const {
		return _geometry.center();
	}

	[[nodiscard]] QRect auxiliaryArticleRect() const {
		return QRect(
			_geometry.topLeft() + QPoint(2, 2),
			QSize(
				std::max(std::min(_geometry.width() / 4, 24), 1),
				std::max(std::min(_geometry.height() / 4, 24), 1)));
	}

	[[nodiscard]] ClickHandlerPtr auxiliaryLink() const {
		return _auxiliaryLink;
	}

	void triggerRepaint(QRect localRect) const {
		requestRepaint(localRect.translated(_geometry.topLeft()).intersected(
			_geometry));
	}

	void triggerRelayout(QRect localRect) const {
		requestRelayout(localRect.translated(_geometry.topLeft()).intersected(
			_geometry));
	}

	std::vector<int> resizeWidths;
	std::vector<QRect> requestedGeometries;
	std::vector<QRect> appliedGeometries;

private:
	[[nodiscard]] QColor color() const {
		switch (_kind) {
		case TestHostedMediaKind::Photo:
			return QColor(220, 150, 30);
		case TestHostedMediaKind::Video:
			return QColor(30, 120, 210);
		case TestHostedMediaKind::Audio:
			return QColor(130, 80, 190);
		case TestHostedMediaKind::Map:
			return QColor(50, 170, 120);
		}
		return QColor(0, 0, 0);
	}

	TestHostedMediaKind _kind = TestHostedMediaKind::Photo;
	uint64 _stableId = 0;
	QString _copyText;
	QRect _geometry;
	MediaActivation _activation;
	ClickHandlerPtr _auxiliaryLink;
};

class TestHostedMediaBlockFactory final : public HostedMediaBlockFactory {
public:
	TestHostedMediaBlockFactory()
	: photoRuntime(std::make_shared<TestPhotoRuntime>())
	, documentRuntime(std::make_shared<TestDocumentRuntime>()) {
	}

	[[nodiscard]] std::shared_ptr<MediaBlock> createPhoto(
			const PreparedPhotoBlockData &prepared) const override {
		++photoRequests;
		auto activation = MediaActivation();
		activation.kind = MediaActivationKind::Photo;
		activation.photo = photoRuntime;
		return create(
			photoBlocks,
			TestHostedMediaKind::Photo,
			prepared.id.value,
			u"Hosted Photo"_q,
			std::move(activation));
	}

	[[nodiscard]] std::shared_ptr<MediaBlock> createVideo(
			const PreparedVideoBlockData &prepared) const override {
		++videoRequests;
		auto activation = MediaActivation();
		activation.kind = MediaActivationKind::Document;
		activation.document = documentRuntime;
		return create(
			videoBlocks,
			TestHostedMediaKind::Video,
			prepared.id.value,
			u"Hosted Video"_q,
			std::move(activation));
	}

	[[nodiscard]] std::shared_ptr<MediaBlock> createAudio(
			const PreparedAudioBlockData &prepared) const override {
		++audioRequests;
		auto activation = MediaActivation();
		activation.kind = MediaActivationKind::Document;
		activation.document = documentRuntime;
		return create(
			audioBlocks,
			TestHostedMediaKind::Audio,
			prepared.id.value,
			u"Hosted Audio"_q,
			std::move(activation));
	}

	[[nodiscard]] std::shared_ptr<MediaBlock> createMap(
			const PreparedMapBlockData &prepared) const override {
		++mapRequests;
		auto activation = MediaActivation();
		activation.kind = MediaActivationKind::ExternalUrl;
		activation.url = prepared.url.isEmpty()
			? u"https://maps.example.test/point"_q
			: prepared.url;
		return create(
			mapBlocks,
			TestHostedMediaKind::Map,
			prepared.id.value,
			u"Hosted Map"_q,
			std::move(activation));
	}

	std::shared_ptr<TestPhotoRuntime> photoRuntime;
	std::shared_ptr<TestDocumentRuntime> documentRuntime;
	mutable int photoRequests = 0;
	mutable int videoRequests = 0;
	mutable int audioRequests = 0;
	mutable int mapRequests = 0;
	mutable std::vector<std::shared_ptr<TestHostedMediaBlock>> photoBlocks;
	mutable std::vector<std::shared_ptr<TestHostedMediaBlock>> videoBlocks;
	mutable std::vector<std::shared_ptr<TestHostedMediaBlock>> audioBlocks;
	mutable std::vector<std::shared_ptr<TestHostedMediaBlock>> mapBlocks;

private:
	[[nodiscard]] std::shared_ptr<MediaBlock> create(
			std::vector<std::shared_ptr<TestHostedMediaBlock>> &blocks,
			TestHostedMediaKind kind,
			uint64 stableId,
			QString copyText,
			MediaActivation activation) const {
		auto block = std::make_shared<TestHostedMediaBlock>(
			kind,
			stableId,
			std::move(copyText),
			std::move(activation));
		blocks.push_back(block);
		return block;
	}
};

class TestMediaRuntime final : public MediaRuntime {
public:
	struct InlineBinding {
		uint64 documentId = 0;
		std::shared_ptr<TestDynamicImage> image;
	};

	struct PhotoBinding {
		uint64 photoId = 0;
		std::shared_ptr<TestPhotoRuntime> runtime;
	};

	struct DocumentBinding {
		uint64 documentId = 0;
		std::shared_ptr<TestDocumentRuntime> runtime;
	};

	struct MapBinding {
		double latitude = 0.;
		double longitude = 0.;
		uint64 accessHash = 0;
		int zoom = 0;
		std::shared_ptr<TestMapRuntime> runtime;
	};

	struct ChannelBinding {
		uint64 channelId = 0;
		QString username;
		std::shared_ptr<TestChannelRuntime> runtime;
	};

	struct MapRequest {
		double latitude = 0.;
		double longitude = 0.;
		uint64 accessHash = 0;
		QSize size;
		int zoom = 0;
	};

	struct ChannelRequest {
		uint64 channelId = 0;
		QString username;
	};

	[[nodiscard]] std::shared_ptr<Ui::DynamicImage> resolveInlineImage(
			uint64 documentId,
			QSize size) const override {
		inlineRequests.push_back({ documentId, size });
		const auto i = std::find_if(
			inlineBindings.begin(),
			inlineBindings.end(),
			[=](const InlineBinding &binding) {
				return binding.documentId == documentId;
			});
		return (i != inlineBindings.end()) ? i->image : nullptr;
	}

	[[nodiscard]] std::shared_ptr<PhotoRuntime> resolvePhoto(
			uint64 photoId) const override {
		photoRequests.push_back(photoId);
		const auto i = std::find_if(
			photoBindings.begin(),
			photoBindings.end(),
			[=](const PhotoBinding &binding) {
				return binding.photoId == photoId;
			});
		return (i != photoBindings.end()) ? i->runtime : nullptr;
	}

	[[nodiscard]] std::shared_ptr<DocumentRuntime> resolveDocument(
			uint64 documentId) const override {
		documentRequests.push_back(documentId);
		const auto i = std::find_if(
			documentBindings.begin(),
			documentBindings.end(),
			[=](const DocumentBinding &binding) {
				return binding.documentId == documentId;
			});
		return (i != documentBindings.end()) ? i->runtime : nullptr;
	}

	[[nodiscard]] std::shared_ptr<MapRuntime> resolveMap(
			double latitude,
			double longitude,
			uint64 accessHash,
			QSize size,
			int zoom) const override {
		mapRequests.push_back({
			.latitude = latitude,
			.longitude = longitude,
			.accessHash = accessHash,
			.size = size,
			.zoom = zoom,
		});
		const auto i = std::find_if(
			mapBindings.begin(),
			mapBindings.end(),
			[=](const MapBinding &binding) {
				return binding.latitude == latitude
					&& binding.longitude == longitude
					&& binding.accessHash == accessHash
					&& binding.zoom == zoom;
			});
		return (i != mapBindings.end()) ? i->runtime : nullptr;
	}

	[[nodiscard]] std::shared_ptr<ChannelRuntime> resolveChannel(
			uint64 channelId,
			const QString &username) const override {
		channelRequests.push_back({
			.channelId = channelId,
			.username = username,
		});
		const auto i = std::find_if(
			channelBindings.begin(),
			channelBindings.end(),
			[=](const ChannelBinding &binding) {
				return binding.channelId == channelId
					&& binding.username == username;
			});
		return (i != channelBindings.end()) ? i->runtime : nullptr;
	}

	[[nodiscard]] rpl::producer<uint64> channelJoinedChanges() const override {
		return _channelJoinedChanges.events();
	}

	[[nodiscard]] std::shared_ptr<HostedMediaBlockFactory>
	hostedMediaBlockFactory() const override {
		return hostedFactory;
	}

	[[nodiscard]] int hostedPhotoRequests() const {
		return hostedFactory ? hostedFactory->photoRequests : 0;
	}

	[[nodiscard]] int hostedVideoRequests() const {
		return hostedFactory ? hostedFactory->videoRequests : 0;
	}

	[[nodiscard]] int hostedAudioRequests() const {
		return hostedFactory ? hostedFactory->audioRequests : 0;
	}

	[[nodiscard]] int hostedMapRequests() const {
		return hostedFactory ? hostedFactory->mapRequests : 0;
	}

	void addInlineImage(uint64 documentId, std::shared_ptr<TestDynamicImage> image) {
		inlineBindings.push_back({
			.documentId = documentId,
			.image = std::move(image),
		});
	}

	void addPhotoRuntime(uint64 photoId, std::shared_ptr<TestPhotoRuntime> runtime) {
		photoBindings.push_back({
			.photoId = photoId,
			.runtime = std::move(runtime),
		});
	}

	void addDocumentRuntime(
			uint64 documentId,
			std::shared_ptr<TestDocumentRuntime> runtime) {
		documentBindings.push_back({
			.documentId = documentId,
			.runtime = std::move(runtime),
		});
	}

	void addMapRuntime(
			double latitude,
			double longitude,
			uint64 accessHash,
			int zoom,
			std::shared_ptr<TestMapRuntime> runtime) {
		mapBindings.push_back({
			.latitude = latitude,
			.longitude = longitude,
			.accessHash = accessHash,
			.zoom = zoom,
			.runtime = std::move(runtime),
		});
	}

	void addChannelRuntime(
			uint64 channelId,
			QString username,
			std::shared_ptr<TestChannelRuntime> runtime) {
		channelBindings.push_back({
			.channelId = channelId,
			.username = std::move(username),
			.runtime = std::move(runtime),
		});
	}

	void fireChannelJoinedChange(uint64 channelId) const {
		_channelJoinedChanges.fire_copy(channelId);
	}

	std::vector<InlineBinding> inlineBindings;
	std::vector<PhotoBinding> photoBindings;
	std::vector<DocumentBinding> documentBindings;
	std::vector<MapBinding> mapBindings;
	std::vector<ChannelBinding> channelBindings;
	mutable std::vector<std::pair<uint64, QSize>> inlineRequests;
	mutable std::vector<uint64> photoRequests;
	mutable std::vector<uint64> documentRequests;
	mutable std::vector<MapRequest> mapRequests;
	mutable std::vector<ChannelRequest> channelRequests;
	std::shared_ptr<TestHostedMediaBlockFactory> hostedFactory;

private:
	mutable rpl::event_stream<uint64> _channelJoinedChanges;
};

enum class NativeIvPlaceholderKind {
	Video,
	Embed,
	Collage,
	Slideshow,
	Audio,
	Map,
};

struct NativeIvPlaceholderFixture {
	NativeIvPlaceholderKind kind = NativeIvPlaceholderKind::Video;
	QString caption;
	QString expectedLabel;
	QString expectedFallbackUrl;
	int expectedWidth = 0;
	int expectedHeight = 0;
	bool expectedFullWidth = false;
	bool expectedAllowScrolling = false;
	MTPPageBlock block;
};

struct NativeIvMediaFixture {
	QString label;
	QString caption;
	MTPPageBlock block;
	QVector<MTPPhoto> photos;
	QVector<MTPDocument> documents;
};

constexpr auto kNativeIvEmbedPostDate = 1715347200;
constexpr auto kNativeIvEmbedPostAuthorPhotoId = uint64(9301);

[[nodiscard]] QImage SolidTestImage(
		int width,
		int height,
		const QColor &color) {
	auto result = QImage(
		QSize(std::max(width, 1), std::max(height, 1)),
		QImage::Format_ARGB32_Premultiplied);
	result.fill(color);
	return result;
}

[[nodiscard]] MTPRichText NativeIvText(QString text) {
	return text.isEmpty()
		? MTP_textEmpty()
		: MTP_textPlain(MTP_string(text));
}

[[nodiscard]] MTPRichText NativeIvConcat(
		std::initializer_list<MTPRichText> parts) {
	auto list = QVector<MTPRichText>();
	list.reserve(int(parts.size()));
	for (const auto &part : parts) {
		list.push_back(part);
	}
	return MTP_textConcat(MTP_vector<MTPRichText>(std::move(list)));
}

[[nodiscard]] MTPRichText NativeIvTextUrl(
		QString text,
		QString url,
		int64 webpageId = 0) {
	return MTP_textUrl(
		NativeIvText(std::move(text)),
		MTP_string(url),
		MTP_long(webpageId));
}

[[nodiscard]] MTPRichText NativeIvTextEmail(QString text, QString email) {
	return MTP_textEmail(
		NativeIvText(std::move(text)),
		MTP_string(email));
}

[[nodiscard]] MTPPageRelatedArticle NativeIvRelatedArticle(
		QString url,
		int64 webpageId = 0,
		QString title = QString(),
		QString description = QString(),
		uint64 photoId = 0,
		QString author = QString(),
		int publishedDate = 0) {
	auto flags = MTPDpageRelatedArticle::Flags();
	if (!title.isEmpty()) {
		flags |= MTPDpageRelatedArticle::Flag::f_title;
	}
	if (!description.isEmpty()) {
		flags |= MTPDpageRelatedArticle::Flag::f_description;
	}
	if (photoId) {
		flags |= MTPDpageRelatedArticle::Flag::f_photo_id;
	}
	if (!author.isEmpty()) {
		flags |= MTPDpageRelatedArticle::Flag::f_author;
	}
	if (publishedDate > 0) {
		flags |= MTPDpageRelatedArticle::Flag::f_published_date;
	}
	return MTP_pageRelatedArticle(
		MTP_flags(flags),
		MTP_string(url),
		MTP_long(webpageId),
		MTP_string(title),
		MTP_string(description),
		MTP_long(photoId),
		MTP_string(author),
		MTP_int(publishedDate));
}

[[nodiscard]] MTPPageBlock NativeIvRelatedArticlesBlock(
		QString title,
		std::initializer_list<MTPPageRelatedArticle> articles) {
	auto list = QVector<MTPPageRelatedArticle>();
	list.reserve(int(articles.size()));
	for (const auto &article : articles) {
		list.push_back(article);
	}
	return MTP_pageBlockRelatedArticles(
		NativeIvText(std::move(title)),
		MTP_vector<MTPPageRelatedArticle>(std::move(list)));
}

[[nodiscard]] MTPPageCaption NativeIvCaption(
		QString text = QString(),
		QString credit = QString()) {
	return MTP_pageCaption(
		NativeIvText(std::move(text)),
		NativeIvText(std::move(credit)));
}

[[nodiscard]] MTPPageTableCell NativeIvTableCell(
		QString text = QString(),
		int colspan = 1,
		int rowspan = 1,
		bool header = false,
		TableAlignment alignment = TableAlignment::Left,
		PreparedTableCellVerticalAlignment verticalAlignment
			= PreparedTableCellVerticalAlignment::Top) {
	auto flags = MTPDpageTableCell::Flags();
	if (header) {
		flags |= MTPDpageTableCell::Flag::f_header;
	}
	if (!text.isEmpty()) {
		flags |= MTPDpageTableCell::Flag::f_text;
	}
	if (colspan != 1) {
		flags |= MTPDpageTableCell::Flag::f_colspan;
	}
	if (rowspan != 1) {
		flags |= MTPDpageTableCell::Flag::f_rowspan;
	}
	switch (alignment) {
	case TableAlignment::Center:
		flags |= MTPDpageTableCell::Flag::f_align_center;
		break;
	case TableAlignment::Right:
		flags |= MTPDpageTableCell::Flag::f_align_right;
		break;
	case TableAlignment::None:
	case TableAlignment::Left:
		break;
	}
	switch (verticalAlignment) {
	case PreparedTableCellVerticalAlignment::Middle:
		flags |= MTPDpageTableCell::Flag::f_valign_middle;
		break;
	case PreparedTableCellVerticalAlignment::Bottom:
		flags |= MTPDpageTableCell::Flag::f_valign_bottom;
		break;
	case PreparedTableCellVerticalAlignment::Top:
		break;
	}
	return MTP_pageTableCell(
		MTP_flags(flags),
		NativeIvText(std::move(text)),
		MTP_int(colspan),
		MTP_int(rowspan));
}

[[nodiscard]] MTPPageTableRow NativeIvTableRow(
		std::initializer_list<MTPPageTableCell> cells) {
	auto list = QVector<MTPPageTableCell>();
	list.reserve(int(cells.size()));
	for (const auto &cell : cells) {
		list.push_back(cell);
	}
	return MTP_pageTableRow(MTP_vector<MTPPageTableCell>(std::move(list)));
}

[[nodiscard]] MTPPageBlock NativeIvTableBlock(
		QString title,
		std::initializer_list<MTPPageTableRow> rows,
		bool bordered = true,
		bool striped = false) {
	auto flags = MTPDpageBlockTable::Flags();
	if (bordered) {
		flags |= MTPDpageBlockTable::Flag::f_bordered;
	}
	if (striped) {
		flags |= MTPDpageBlockTable::Flag::f_striped;
	}
	auto list = QVector<MTPPageTableRow>();
	list.reserve(int(rows.size()));
	for (const auto &row : rows) {
		list.push_back(row);
	}
	return MTP_pageBlockTable(
		MTP_flags(flags),
		NativeIvText(std::move(title)),
		MTP_vector<MTPPageTableRow>(std::move(list)));
}

[[nodiscard]] MTPPhoto NativeIvPhoto(uint64 id, int width, int height) {
	auto sizes = QVector<MTPPhotoSize>();
	sizes.push_back(MTP_photoSize(
		MTP_string("y"),
		MTP_int(width),
		MTP_int(height),
		MTP_int(0)));
	return MTP_photo(
		MTP_flags(0),
		MTP_long(id),
		MTP_long(0),
		MTP_bytes(),
		MTP_int(0),
		MTP_vector<MTPPhotoSize>(std::move(sizes)),
		MTPVector<MTPVideoSize>(),
		MTP_int(0));
}

[[nodiscard]] MTPPageBlock NativeIvPhotoBlock(
		uint64 photoId,
		QString caption = QString(),
		QString credit = QString(),
		QString urlOverride = QString()) {
	const auto flags = urlOverride.isEmpty()
		? MTP_flags(0)
		: MTP_flags(MTPDpageBlockPhoto::Flag::f_url);
	return MTP_pageBlockPhoto(
		flags,
		MTP_long(photoId),
		NativeIvCaption(std::move(caption), std::move(credit)),
		MTP_string(urlOverride),
		MTP_long(0));
}

[[nodiscard]] MTPPageBlock NativeIvCoveredPhotoBlock(
		uint64 photoId,
		QString caption = QString(),
		QString credit = QString(),
		QString urlOverride = QString()) {
	return MTP_pageBlockCover(NativeIvPhotoBlock(
		photoId,
		std::move(caption),
		std::move(credit),
		std::move(urlOverride)));
}

[[nodiscard]] MTPDocument NativeIvDocument(
		uint64 id,
		QString mimeType,
		QVector<MTPDocumentAttribute> attributes) {
	return MTP_document(
		MTP_flags(0),
		MTP_long(id),
		MTP_long(id + 1000),
		MTP_bytes(),
		MTP_int(0),
		MTP_string(mimeType),
		MTP_long(0),
		MTPVector<MTPPhotoSize>(),
		MTPVector<MTPVideoSize>(),
		MTP_int(0),
		MTP_vector<MTPDocumentAttribute>(std::move(attributes)));
}

[[nodiscard]] MTPDocument NativeIvVideoDocument(
		uint64 id,
		int width,
		int height,
		QString fileName = u"video.mp4"_q,
		double duration = 0.) {
	const auto flags = MTPDdocumentAttributeVideo::Flags(
		MTPDdocumentAttributeVideo::Flag::f_supports_streaming);
	auto attributes = QVector<MTPDocumentAttribute>();
	attributes.push_back(MTP_documentAttributeFilename(MTP_string(fileName)));
	attributes.push_back(MTP_documentAttributeVideo(
		MTP_flags(flags),
		MTP_double(duration),
		MTP_int(width),
		MTP_int(height),
		MTPint(),
		MTPdouble(),
		MTPstring()));
	return NativeIvDocument(id, u"video/mp4"_q, std::move(attributes));
}

[[nodiscard]] MTPDocument NativeIvAnimationDocument(
		uint64 id,
		int width,
		int height,
		QString fileName = u"animation.mp4"_q) {
	auto attributes = QVector<MTPDocumentAttribute>();
	attributes.push_back(MTP_documentAttributeFilename(MTP_string(fileName)));
	attributes.push_back(MTP_documentAttributeAnimated());
	attributes.push_back(MTP_documentAttributeImageSize(
		MTP_int(width),
		MTP_int(height)));
	return NativeIvDocument(id, u"video/mp4"_q, std::move(attributes));
}

[[nodiscard]] MTPDocument NativeIvImageDocument(
		uint64 id,
		int width,
		int height,
		QString fileName = u"image.jpg"_q) {
	auto attributes = QVector<MTPDocumentAttribute>();
	attributes.push_back(MTP_documentAttributeFilename(MTP_string(fileName)));
	attributes.push_back(MTP_documentAttributeImageSize(
		MTP_int(width),
		MTP_int(height)));
	return NativeIvDocument(id, u"image/jpeg"_q, std::move(attributes));
}

[[nodiscard]] MTPDocument NativeIvAudioDocument(
		uint64 id,
		QString title,
		QString performer,
		QString fileName,
		int duration) {
	auto flags = MTPDdocumentAttributeAudio::Flags(0);
	if (!title.isEmpty()) {
		flags |= MTPDdocumentAttributeAudio::Flag::f_title;
	}
	if (!performer.isEmpty()) {
		flags |= MTPDdocumentAttributeAudio::Flag::f_performer;
	}
	auto attributes = QVector<MTPDocumentAttribute>();
	attributes.push_back(MTP_documentAttributeFilename(MTP_string(fileName)));
	attributes.push_back(MTP_documentAttributeAudio(
		MTP_flags(flags),
		MTP_int(duration),
		MTP_string(title),
		MTP_string(performer),
		MTPbytes()));
	return NativeIvDocument(id, u"audio/mpeg"_q, std::move(attributes));
}

[[nodiscard]] MTPGeoPoint NativeIvGeoPoint(
		double latitude,
		double longitude,
		uint64 accessHash) {
	return MTP_geoPoint(
		MTP_flags(0),
		MTP_double(longitude),
		MTP_double(latitude),
		MTP_long(accessHash),
		MTPint());
}

[[nodiscard]] MTPChat NativeIvChannelChat(
		uint64 id,
		QString title,
		QString username = QString(),
		uint64 accessHash = 0x12345678ULL) {
	auto flags = MTPDchannel::Flags(
		MTPDchannel::Flag::f_broadcast
		| MTPDchannel::Flag::f_access_hash);
	if (!username.isEmpty()) {
		flags |= MTPDchannel::Flag::f_username;
	}
	return MTP_channel(
		MTP_flags(flags),
		MTP_long(id),
		MTP_long(accessHash),
		MTP_string(title),
		MTP_string(username),
		MTP_chatPhotoEmpty(),
		MTP_int(0),
		MTPVector<MTPRestrictionReason>(),
		MTPChatAdminRights(),
		MTPChatBannedRights(),
		MTPChatBannedRights(),
		MTPint(),
		MTPVector<MTPUsername>(),
		MTPrecentStory(),
		MTPPeerColor(),
		MTPPeerColor(),
		MTPEmojiStatus(),
		MTPint(),
		MTPint(),
		MTPlong(),
		MTPlong(),
		MTPlong());
}

[[nodiscard]] NativeIvMediaFixture NativeIvVideoFixture() {
	return {
		.label = u"Video"_q,
		.caption = u"Video caption"_q,
		.block = MTP_pageBlockVideo(
			MTP_flags(0),
			MTP_long(7001),
			NativeIvCaption(u"Video caption"_q)),
		.documents = {
			NativeIvVideoDocument(7001, 1280, 720, u"video.mp4"_q, 42.),
		},
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvAnimationFixture() {
	return {
		.label = u"Animation"_q,
		.caption = u"Animation caption"_q,
		.block = MTP_pageBlockVideo(
			MTP_flags(0),
			MTP_long(7601),
			NativeIvCaption(u"Animation caption"_q)),
		.documents = {
			NativeIvAnimationDocument(7601, 480, 270),
		},
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvAudioFixture() {
	return {
		.label = u"Audio"_q,
		.caption = u"Audio caption"_q,
		.block = MTP_pageBlockAudio(
			MTP_long(7002),
			NativeIvCaption(u"Audio caption"_q)),
		.documents = {
			NativeIvAudioDocument(
				7002,
				u"Song Title"_q,
				u"Sample Artist"_q,
				u"track.mp3"_q,
				215),
		},
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvMapFixture() {
	return {
		.label = u"Map"_q,
		.caption = u"Map caption"_q,
		.block = MTP_pageBlockMap(
			NativeIvGeoPoint(51.5007, -0.1246, 880088),
			MTP_int(13),
			MTP_int(320),
			MTP_int(180),
			NativeIvCaption(u"Map caption"_q)),
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvChannelFixture() {
	return {
		.label = u"Channel"_q,
		.block = MTP_pageBlockChannel(NativeIvChannelChat(
			7006,
			u"Native IV Channel"_q,
			u"nativeiv"_q)),
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvCollageFixture() {
	auto items = QVector<MTPPageBlock>();
	items.push_back(NativeIvPhotoBlock(9102));
	items.push_back(MTP_pageBlockVideo(
		MTP_flags(0),
		MTP_long(7003),
		NativeIvCaption()));
	return {
		.label = u"Collage"_q,
		.caption = u"Collage caption"_q,
		.block = MTP_pageBlockCollage(
			MTP_vector<MTPPageBlock>(std::move(items)),
			NativeIvCaption(u"Collage caption"_q)),
		.photos = {
			NativeIvPhoto(9102, 400, 300),
		},
		.documents = {
			NativeIvVideoDocument(7003, 640, 360, u"collage.mp4"_q, 13.),
		},
	};
}

[[nodiscard]] NativeIvMediaFixture NativeIvSlideshowFixture(int count = 2) {
	count = std::max(count, 2);
	auto items = QVector<MTPPageBlock>();
	auto documents = QVector<MTPDocument>();
	items.reserve(count);
	documents.reserve(count);
	for (auto i = 0; i != count; ++i) {
		const auto id = uint64(7004 + i);
		const auto wide = (i % 2 == 0) ? 960 : 854;
		const auto high = (i % 2 == 0) ? 540 : 480;
		items.push_back(MTP_pageBlockVideo(
			MTP_flags(0),
			MTP_long(id),
			NativeIvCaption()));
		documents.push_back(NativeIvVideoDocument(
			id,
			wide,
			high,
			u"slide-"_q + QString::number(i + 1) + u".mp4"_q,
			11. + i));
	}
	return {
		.label = u"Slideshow"_q,
		.caption = u"Slideshow caption"_q,
		.block = MTP_pageBlockSlideshow(
			MTP_vector<MTPPageBlock>(std::move(items)),
			NativeIvCaption(u"Slideshow caption"_q)),
		.documents = std::move(documents),
	};
}

[[nodiscard]] QString NativeIvPlaceholderLabel(NativeIvPlaceholderKind kind) {
	switch (kind) {
	case NativeIvPlaceholderKind::Video:
		return u"Video Placeholder"_q;
	case NativeIvPlaceholderKind::Embed:
		return u"Click to View"_q;
	case NativeIvPlaceholderKind::Collage:
		return u"Collage placeholder"_q;
	case NativeIvPlaceholderKind::Slideshow:
		return u"Grouped Media Placeholder"_q;
	case NativeIvPlaceholderKind::Audio:
		return u"Audio File Placeholder"_q;
	case NativeIvPlaceholderKind::Map:
		return u"Map Placeholder"_q;
	}
	return QString();
}

[[nodiscard]] MTPPageBlock NativeIvPlaceholderBlock(
		NativeIvPlaceholderKind kind,
		QString caption = QString()) {
	const auto pageCaption = NativeIvCaption(std::move(caption));
	switch (kind) {
	case NativeIvPlaceholderKind::Video:
		return MTP_pageBlockVideo(
			MTP_flags(0),
			MTP_long(7001),
			pageCaption);
	case NativeIvPlaceholderKind::Embed:
		return MTP_pageBlockEmbed(
			MTP_flags(
				MTPDpageBlockEmbed::Flag::f_full_width
				| MTPDpageBlockEmbed::Flag::f_allow_scrolling
				| MTPDpageBlockEmbed::Flag::f_url
				| MTPDpageBlockEmbed::Flag::f_w
				| MTPDpageBlockEmbed::Flag::f_h),
			MTP_string("https://example.com/embed"),
			MTP_string(),
			MTP_long(0),
			MTP_int(640),
			MTP_int(360),
			pageCaption);
	case NativeIvPlaceholderKind::Collage:
		return MTP_pageBlockCollage(
			MTP_vector<MTPPageBlock>(),
			pageCaption);
	case NativeIvPlaceholderKind::Slideshow:
		return MTP_pageBlockSlideshow(
			MTP_vector<MTPPageBlock>(),
			pageCaption);
	case NativeIvPlaceholderKind::Audio:
		return MTP_pageBlockAudio(MTP_long(7002), pageCaption);
	case NativeIvPlaceholderKind::Map:
		return MTP_pageBlockMap(
			MTP_geoPointEmpty(),
			MTP_int(10),
			MTP_int(120),
			MTP_int(72),
			pageCaption);
	}
	return MTP_pageBlockUnsupported();
}

[[nodiscard]] QVector<MTPPageBlock> NativeIvDefaultEmbedPostBlocks() {
	return {
		MTP_pageBlockParagraph(NativeIvConcat({
			NativeIvText(u"Links: "_q),
			NativeIvTextUrl(
				u"instant view"_q,
				u"https://telegra.ph/embed-post-link"_q,
				777),
			NativeIvText(u" and "_q),
			NativeIvTextUrl(
				u"external"_q,
				u"https://example.com/external-link"_q),
		})),
	};
}

[[nodiscard]] QString NativeIvEmbedPostDateText(int date) {
	return langDateTimeFull(QDateTime::fromSecsSinceEpoch(date));
}

[[nodiscard]] QRect NativeIvEmbedPostAvatarRect() {
	const auto &embedPostStyle = st::defaultMarkdown.embedPost;
	const auto textLineHeight = [](const ::style::TextStyle &textStyle) {
		return std::max(textStyle.lineHeight, textStyle.font->height);
	};
	const auto contentLeft = st::defaultMarkdown.textPadding.left()
		+ embedPostStyle.accentWidth
		+ embedPostStyle.accentSkip
		+ embedPostStyle.padding.left();
	const auto headerHeight = std::max(
		textLineHeight(embedPostStyle.authorStyle)
			+ textLineHeight(embedPostStyle.dateStyle),
		embedPostStyle.avatarSize);
	return QRect(
		contentLeft,
		embedPostStyle.padding.top()
			+ std::max((headerHeight - embedPostStyle.avatarSize) / 2, 0),
		embedPostStyle.avatarSize,
		embedPostStyle.avatarSize);
}

[[nodiscard]] MTPPageBlock NativeIvEmbedPostBlock(
		QVector<MTPPageBlock> blocks,
		QString caption = u"Embed post caption"_q,
		QString credit = QString(),
		QString url = u"https://example.com/embed-post"_q,
		QString author = u"Author"_q,
		int date = kNativeIvEmbedPostDate,
		uint64 authorPhotoId = kNativeIvEmbedPostAuthorPhotoId) {
	return MTP_pageBlockEmbedPost(
		MTP_string(url),
		MTP_long(0),
		MTP_long(authorPhotoId),
		MTP_string(author),
		MTP_int(date),
		MTP_vector<MTPPageBlock>(std::move(blocks)),
		NativeIvCaption(std::move(caption), std::move(credit)));
}

[[nodiscard]] MTPRichText NativeIvInlineImageText(
		uint64 documentId,
		int width,
		int height) {
	return MTP_textImage(
		MTP_long(documentId),
		MTP_int(width),
		MTP_int(height));
}

[[nodiscard]] MTPPageBlock NativeIvInlineImageParagraph(
		uint64 documentId,
		int width,
		int height,
		QString prefix,
		QString suffix) {
	return MTP_pageBlockParagraph(NativeIvConcat({
		NativeIvText(std::move(prefix)),
		NativeIvInlineImageText(documentId, width, height),
		NativeIvText(std::move(suffix)),
	}));
}

[[nodiscard]] Iv::Source NativeIvSource(
		QVector<MTPPageBlock> blocks,
		QVector<MTPPhoto> photos = {},
		QVector<MTPDocument> documents = {}) {
	auto result = Iv::Source();
	result.pageId = 1;
	result.page = MTP_page(
		MTP_flags(0),
		MTP_string("https://example.com/native-iv"),
		MTP_vector<MTPPageBlock>(std::move(blocks)),
		MTP_vector<MTPPhoto>(std::move(photos)),
		MTP_vector<MTPDocument>(std::move(documents)),
		MTP_int(0));
	result.name = u"native-iv-test"_q;
	return result;
}

[[nodiscard]] QString FromLatin1(const char *value) {
	return QString::fromLatin1(value);
}

void PrintStreamLine(std::ostream &stream, const QString &line) {
	const auto bytes = line.toUtf8();
	stream.write(bytes.constData(), static_cast<std::streamsize>(bytes.size()));
	stream << '\n';
}

void PrintLine(const QString &line) {
	PrintStreamLine(std::cout, line);
}

void PrintError(const QString &line) {
	PrintStreamLine(std::cerr, line);
}

[[nodiscard]] Args ParseArgs(int argc, char **argv) {
	auto result = Args();
	for (auto i = 1; i != argc; ++i) {
		const auto argument = QString::fromLocal8Bit(argv[i]);
		if (argument == FromLatin1("--dump")) {
			result.dump = true;
		} else if (argument == FromLatin1("--inline-html")) {
			result.inlineHtml = true;
		} else if (argument == FromLatin1("--markdown")
			|| argument == FromLatin1("--latex-md")) {
			if (i + 1 == argc) {
				result.ok = false;
				result.error = FromLatin1("missing value for ") + argument;
				return result;
			}
			const auto path = QString::fromLocal8Bit(argv[++i]);
			if (argument == FromLatin1("--markdown")) {
				result.markdownPath = path;
			} else {
				result.latexMarkdownPath = path;
			}
		} else {
			result.ok = false;
			result.error = FromLatin1("unknown argument: ") + argument;
			return result;
		}
	}
	return result;
}

[[nodiscard]] QString DefaultFixturePath(const QString &name) {
	const auto applicationDir = QDir(QCoreApplication::applicationDirPath());
	const auto applicationCandidate = applicationDir.filePath(name);
	if (QFileInfo::exists(applicationCandidate)) {
		return applicationCandidate;
	}
	const auto outDebug = QDir::current().filePath(
		FromLatin1("out/Debug/") + name);
	if (QFileInfo::exists(outDebug)) {
		return outDebug;
	}
	const auto repoFixtureFromApplication = QDir::cleanPath(
		applicationDir.filePath(
			FromLatin1(
				"../../Telegram/SourceFiles/tests/fixtures/markdown_iv/")
				+ name));
	if (QFileInfo::exists(repoFixtureFromApplication)) {
		return repoFixtureFromApplication;
	}
	const auto repoFixtureFromCurrent = QDir::current().filePath(
		FromLatin1("Telegram/SourceFiles/tests/fixtures/markdown_iv/")
			+ name);
	if (QFileInfo::exists(repoFixtureFromCurrent)) {
		return repoFixtureFromCurrent;
	}
	return outDebug;
}

[[nodiscard]] bool ReadFile(const QString &path, QByteArray *bytes) {
	if (!bytes) {
		return false;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	*bytes = file.readAll();
	return true;
}

[[nodiscard]] int CountNodes(const MarkdownNode &node) {
	auto result = 1;
	for (const auto &child : node.children) {
		result += CountNodes(child);
	}
	return result;
}

[[nodiscard]] bool HasKind(const MarkdownNode &node, NodeKind kind) {
	if (node.kind == kind) {
		return true;
	}
	for (const auto &child : node.children) {
		if (HasKind(child, kind)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HasTextContaining(
		const MarkdownNode &node,
		const QString &text) {
	if (node.text.contains(text) || node.raw.contains(text)) {
		return true;
	}
	for (const auto &child : node.children) {
		if (HasTextContaining(child, text)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HasExactInlineHtmlTriplet(
		const MarkdownNode &node,
		const QString &openingTag,
		const QString &innerText,
		const QString &closingTag) {
	const auto count = int(node.children.size());
	for (auto i = 0; (i + 2) < count; ++i) {
		const auto &opening = node.children[i];
		const auto &text = node.children[i + 1];
		const auto &closing = node.children[i + 2];
		if (opening.kind == NodeKind::HtmlInline
			&& opening.raw == openingTag
			&& text.kind == NodeKind::Text
			&& text.text == innerText
			&& closing.kind == NodeKind::HtmlInline
			&& closing.raw == closingTag) {
			return true;
		}
	}
	for (const auto &child : node.children) {
		if (HasExactInlineHtmlTriplet(
				child,
				openingTag,
				innerText,
				closingTag)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HasTaskState(const MarkdownNode &node, TaskState state) {
	if (node.taskState == state) {
		return true;
	}
	for (const auto &child : node.children) {
		if (HasTaskState(child, state)) {
			return true;
		}
	}
	return false;
}

void CollectTables(
		const MarkdownNode &node,
		std::vector<const MarkdownNode*> *out) {
	if (!out) {
		return;
	}
	if (node.kind == NodeKind::Table) {
		out->push_back(&node);
	}
	for (const auto &child : node.children) {
		CollectTables(child, out);
	}
}

[[nodiscard]] int TableHeaderRowCount(const MarkdownNode &table) {
	auto result = 0;
	for (const auto &row : table.children) {
		if (row.kind == NodeKind::TableRow && row.tableHeader) {
			++result;
		}
	}
	return result;
}

[[nodiscard]] bool HasTableHeaderRow(const MarkdownNode &table) {
	return TableHeaderRowCount(table) > 0;
}

[[nodiscard]] bool HasSequentialTableColumns(const MarkdownNode &table) {
	for (const auto &row : table.children) {
		if (row.kind != NodeKind::TableRow) {
			return false;
		}
		auto expectedColumn = 0;
		for (const auto &cell : row.children) {
			if (cell.kind != NodeKind::TableCell
				|| cell.tableColumn != expectedColumn) {
				return false;
			}
			++expectedColumn;
		}
	}
	return !table.children.empty();
}

[[nodiscard]] bool HasTableAlignments(
		const MarkdownNode &table,
		const std::vector<TableAlignment> &expected) {
	if (table.tableAlignments.size() != expected.size()) {
		return false;
	}
	for (auto i = 0, count = int(expected.size()); i != count; ++i) {
		if (table.tableAlignments[i] != expected[i]) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] int TableColumnCount(const MarkdownNode &table) {
	auto result = 0;
	for (const auto &row : table.children) {
		if (row.kind != NodeKind::TableRow) {
			return 0;
		}
		const auto count = int(row.children.size());
		if (count > result) {
			result = count;
		}
	}
	return result;
}

[[nodiscard]] int CountDisplayMathNodes(const MarkdownNode &node) {
	auto result = (node.kind == NodeKind::DisplayMath) ? 1 : 0;
	for (const auto &child : node.children) {
		result += CountDisplayMathNodes(child);
	}
	return result;
}

[[nodiscard]] bool SameRange(
		const SourceRange &range,
		int startLine,
		int startColumn,
		int endLine,
		int endColumn) {
	return range.available
		&& (range.startLine == startLine)
		&& (range.startColumn == startColumn)
		&& (range.endLine == endLine)
		&& (range.endColumn == endColumn);
}

[[nodiscard]] bool CoversLineRange(
		const SourceRange &range,
		int firstLine,
		int lastLine) {
	return range.available
		&& (range.startLine <= firstLine)
		&& (range.endLine >= lastLine);
}

[[nodiscard]] const MarkdownNode *FindNodeByKindAndRange(
		const MarkdownNode &node,
		NodeKind kind,
		int startLine,
		int startColumn,
		int endLine,
		int endColumn) {
	if (node.kind == kind
		&& SameRange(
			node.range,
			startLine,
			startColumn,
			endLine,
			endColumn)) {
		return &node;
	}
	for (const auto &child : node.children) {
		if (const auto found = FindNodeByKindAndRange(
				child,
				kind,
				startLine,
				startColumn,
				endLine,
				endColumn)) {
			return found;
		}
	}
	return nullptr;
}

[[nodiscard]] int CountNodesByKindAndRange(
		const MarkdownNode &node,
		NodeKind kind,
		int startLine,
		int startColumn,
		int endLine,
		int endColumn) {
	auto result = (node.kind == kind
		&& SameRange(
			node.range,
			startLine,
			startColumn,
			endLine,
			endColumn))
		? 1
		: 0;
	for (const auto &child : node.children) {
		result += CountNodesByKindAndRange(
			child,
			kind,
			startLine,
			startColumn,
			endLine,
			endColumn);
	}
	return result;
}

using NodeKindPath = std::initializer_list<NodeKind>;
using NodeKindPathIter = NodeKindPath::const_iterator;

[[nodiscard]] const MarkdownNode *FindNodeByPathAndRange(
		const MarkdownNode &node,
		NodeKindPathIter begin,
		NodeKindPathIter end,
		int startLine,
		int startColumn,
		int endLine,
		int endColumn) {
	if (begin == end || node.kind != *begin) {
		return nullptr;
	}
	if (std::next(begin) == end) {
		return SameRange(
			node.range,
			startLine,
			startColumn,
			endLine,
			endColumn)
			? &node
			: nullptr;
	}
	const auto next = std::next(begin);
	for (const auto &child : node.children) {
		if (const auto found = FindNodeByPathAndRange(
				child,
				next,
				end,
				startLine,
				startColumn,
				endLine,
				endColumn)) {
			return found;
		}
	}
	return nullptr;
}

[[nodiscard]] const MarkdownNode *FindNodeByPathAndRange(
		const MarkdownNode &node,
		NodeKindPath path,
		int startLine,
		int startColumn,
		int endLine,
		int endColumn) {
	return path.size()
		? FindNodeByPathAndRange(
			node,
			path.begin(),
			path.end(),
			startLine,
			startColumn,
			endLine,
			endColumn)
		: nullptr;
}

[[nodiscard]] const MarkdownNode *FindNodeByKindAndLineRange(
		const MarkdownNode &node,
		NodeKind kind,
		int firstLine,
		int lastLine) {
	if (node.kind == kind && CoversLineRange(node.range, firstLine, lastLine)) {
		return &node;
	}
	for (const auto &child : node.children) {
		if (const auto found = FindNodeByKindAndLineRange(
				child,
				kind,
				firstLine,
				lastLine)) {
			return found;
		}
	}
	return nullptr;
}

[[nodiscard]] const MarkdownNode *FindHtmlInlineByRaw(
		const MarkdownNode &node,
		const QString &raw) {
	if (node.kind == NodeKind::HtmlInline && node.raw == raw) {
		return &node;
	}
	for (const auto &child : node.children) {
		if (const auto found = FindHtmlInlineByRaw(child, raw)) {
			return found;
		}
	}
	return nullptr;
}

[[nodiscard]] const MarkdownNode *FindHtmlBlockContaining(
		const MarkdownNode &node,
		const QString &text) {
	if (node.kind == NodeKind::HtmlBlock && node.raw.contains(text)) {
		return &node;
	}
	for (const auto &child : node.children) {
		if (const auto found = FindHtmlBlockContaining(child, text)) {
			return found;
		}
	}
	return nullptr;
}

void CollectNodesByKind(
		const MarkdownNode &node,
		NodeKind kind,
		std::vector<const MarkdownNode*> *out) {
	if (!out) {
		return;
	}
	if (node.kind == kind) {
		out->push_back(&node);
	}
	for (const auto &child : node.children) {
		CollectNodesByKind(child, kind, out);
	}
}

[[nodiscard]] const MarkdownNode *FindLinkByTarget(
		const MarkdownNode &node,
		const QString &target) {
	if (node.kind == NodeKind::Link && node.url == target) {
		return &node;
	}
	for (const auto &child : node.children) {
		if (const auto found = FindLinkByTarget(child, target)) {
			return found;
		}
	}
	return nullptr;
}

[[nodiscard]] bool WarningContains(
		const PreparedDocument &document,
		const QString &snippet) {
	for (const auto &warning : document.warnings) {
		if (warning.contains(snippet, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] int CountFormulas(
		const PreparedDocument &document,
		MathKind kind) {
	auto result = 0;
	for (const auto &formula : document.formulas) {
		if (formula.kind == kind) {
			++result;
		}
	}
	return result;
}

[[nodiscard]] bool HasFormulaOnLine(
		const PreparedDocument &document,
		int line,
		const QString &tex) {
	for (const auto &formula : document.formulas) {
		if (formula.range.available
			&& formula.range.startLine == line
			&& formula.tex == tex) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HasFormulaInLineRange(
		const PreparedDocument &document,
		int firstLine,
		int lastLine) {
	if (lastLine < firstLine) {
		return false;
	}
	for (const auto &formula : document.formulas) {
		if (!formula.range.available) {
			continue;
		}
		if (formula.range.startLine <= lastLine
			&& formula.range.endLine >= firstLine) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] int CountFormulasInLineRange(
		const PreparedDocument &document,
		MathKind kind,
		int firstLine,
		int lastLine) {
	auto result = 0;
	for (const auto &formula : document.formulas) {
		if (!formula.range.available || formula.kind != kind) {
			continue;
		}
		if (formula.range.startLine <= lastLine
			&& formula.range.endLine >= firstLine) {
			++result;
		}
	}
	return result;
}

[[nodiscard]] QString YesNo(bool value) {
	return FromLatin1(value ? "yes" : "no");
}

[[nodiscard]] bool HasBothTaskStates(const PreparedDocument &document) {
	return HasTaskState(document.document, TaskState::Checked)
		&& HasTaskState(document.document, TaskState::Unchecked);
}

[[nodiscard]] bool ExclusionsPass(const PreparedDocument &document) {
	return !HasFormulaInLineRange(document, 281, 281)
		&& HasFormulaOnLine(document, 285, FromLatin1("5x + 3"))
		&& !HasFormulaInLineRange(document, 332, 340);
}

[[nodiscard]] bool HasFormula(
		const PreparedDocument &document,
		MathKind kind,
		const QString &tex) {
	for (const auto &formula : document.formulas) {
		if (formula.kind == kind && formula.tex == tex) {
			return true;
		}
	}
	return false;
}

void Check(bool condition, const QString &message, bool *ok);

[[nodiscard]] MarkdownSourceValidationResult CheckValidationSuccess(
		const QByteArray &source,
		const QString &label,
		bool *ok) {
	auto validated = ValidateMarkdownSourceForIv(source, ParseOptions{ label });
	Check(
		validated.ok,
		label + FromLatin1(" validation failed: ") + validated.error,
		ok);
	return validated;
}

void CheckValidationFailure(
		const QByteArray &source,
		const QString &label,
		const QString &expectedError,
		bool *ok) {
	const auto validated = ValidateMarkdownSourceForIv(
		source,
		ParseOptions{ label });
	Check(
		!validated.ok,
		label + FromLatin1(" validation should fail"),
		ok);
	if (!validated.ok) {
		Check(
			validated.error == expectedError,
			label + FromLatin1(" validation error should be ")
				+ expectedError
				+ FromLatin1(", got ")
				+ validated.error,
			ok);
	}
}

void CheckMatchingParseCounts(
		const PreparedDocument &legacy,
		const PreparedDocument &validated,
		const QString &label,
		bool *ok) {
	Check(
		legacy.stats.cmarkNodeCount == validated.stats.cmarkNodeCount,
		label + FromLatin1(" validated path cmark node count"),
		ok);
	Check(
		CountNodes(legacy.document) == CountNodes(validated.document),
		label + FromLatin1(" validated path converted node count"),
		ok);
	Check(
		legacy.formulas.size() == validated.formulas.size(),
		label + FromLatin1(" validated path formula count"),
		ok);
	Check(
		CountFormulas(legacy, MathKind::Inline)
			== CountFormulas(validated, MathKind::Inline),
		label + FromLatin1(" validated path inline formula count"),
		ok);
	Check(
		CountFormulas(legacy, MathKind::Display)
			== CountFormulas(validated, MathKind::Display),
		label + FromLatin1(" validated path display formula count"),
		ok);
	Check(
		CountDisplayMathNodes(legacy.document)
			== CountDisplayMathNodes(validated.document),
		label + FromLatin1(" validated path display math node count"),
		ok);
}

void AppendSummaryCounts(QString *line, const PreparedDocument &document) {
	line->append(FromLatin1(" nodes="));
	line->append(QString::number(document.stats.cmarkNodeCount));
	line->append(FromLatin1(" converted="));
	line->append(QString::number(CountNodes(document.document)));
	line->append(FromLatin1(" formulas_inline="));
	line->append(QString::number(CountFormulas(document, MathKind::Inline)));
	line->append(FromLatin1(" formulas_display="));
	line->append(QString::number(CountFormulas(document, MathKind::Display)));
}

void PrintSummary(const PreparedDocument &document, const QString &label) {
	auto line = label;
	AppendSummaryCounts(&line, document);
	line.append(FromLatin1(" tables="));
	line.append(YesNo(HasKind(document.document, NodeKind::Table)));
	if (label == FromLatin1("markdown-example.md")) {
		line.append(FromLatin1(" tasks="));
		line.append(YesNo(HasBothTaskStates(document)));
		line.append(FromLatin1(" strike="));
		line.append(YesNo(HasKind(document.document, NodeKind::Strike)));
	} else if (label == FromLatin1("latex-markdown-test.md")) {
		line.append(FromLatin1(" exclusions="));
		line.append(YesNo(ExclusionsPass(document)));
	}
	PrintLine(line);
}

[[nodiscard]] bool ParseFixture(
		const QString &path,
		const QString &label,
		PreparedDocument *document) {
	auto bytes = QByteArray();
	if (!ReadFile(path, &bytes)) {
		PrintError(label + FromLatin1(" read-failed: ") + path);
		return false;
	}
	auto parsed = ParseMarkdownForIv(bytes, ParseOptions{ label });
	if (!parsed.ok) {
		PrintError(label + FromLatin1(" parse-failed: ") + parsed.error);
		return false;
	}
	auto validated = ValidateMarkdownSourceForIv(
		bytes,
		ParseOptions{ label });
	if (!validated.ok) {
		PrintError(label + FromLatin1(" validate-failed: ") + validated.error);
		return false;
	}
	auto parsedValidated = ParseMarkdownForIv(std::move(validated.source));
	if (!parsedValidated.ok) {
		PrintError(
			label + FromLatin1(" validated-parse-failed: ")
				+ parsedValidated.error);
		return false;
	}
	auto countsOk = true;
	CheckMatchingParseCounts(
		parsed.document,
		parsedValidated.document,
		label,
		&countsOk);
	if (!countsOk) {
		return false;
	}
	PrintSummary(parsed.document, label);
	if (document) {
		*document = std::move(parsed.document);
	}
	return true;
}

[[nodiscard]] QString AbsolutePath(const QString &path) {
	return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

[[nodiscard]] QString PrepareFailureReason(
		const PrepareFailureStatus &failure) {
	return !failure.debugReason.isEmpty()
		? failure.debugReason
		: QString::number(int(failure.terminal));
}

[[nodiscard]] MarkdownArticleContent PrepareParsedDocumentForTest(
		std::shared_ptr<const PreparedDocument> document,
		const QString &sourcePath,
		const std::shared_ptr<MathRenderer> &renderer,
		MarkdownPrepareDimensions dimensions = CaptureMarkdownPrepareDimensions()) {
	return PrepareSynchronously({
		.document = std::move(document),
		.renderer = renderer,
		.dimensions = std::move(dimensions),
		.sourcePath = AbsolutePath(sourcePath),
	});
}

[[nodiscard]] MarkdownArticleContent PrepareParsedDocumentForTest(
		const PreparedDocument &document,
		const QString &sourcePath,
		const std::shared_ptr<MathRenderer> &renderer,
		MarkdownPrepareDimensions dimensions = CaptureMarkdownPrepareDimensions()) {
	return PrepareParsedDocumentForTest(
		std::make_shared<const PreparedDocument>(document),
		sourcePath,
		renderer,
		std::move(dimensions));
}

[[nodiscard]] int CountPreparedFormulaSlots(
		const MarkdownArticleContent &prepared) {
	auto result = 0;
	for (const auto &slot : prepared.formulas) {
		if (slot.present) {
			++result;
		}
	}
	return result;
}

[[nodiscard]] int CountMeasuredFormulaSlots(
		const MarkdownArticleContent &prepared) {
	auto result = 0;
	for (const auto &slot : prepared.formulas) {
		if (slot.present && slot.measured.success) {
			++result;
		}
	}
	return result;
}

void PrintPrepareSummary(
		const QString &label,
		const MarkdownArticleContent &prepared) {
	auto line = label;
	line.append(FromLatin1(" prepare_ms="));
	line.append(QString::number(prepared.debug.prepareMs));
	line.append(FromLatin1(" formula_measure_ms="));
	line.append(QString::number(prepared.debug.formulaMeasureMs));
	line.append(FromLatin1(" formula_render_ms="));
	line.append(QString::number(prepared.debug.formulaRenderMs));
	line.append(FromLatin1(" prepare_warnings="));
	line.append(QString::number(prepared.debug.prepareWarningCount));
	line.append(FromLatin1(" formula_warnings="));
	line.append(QString::number(prepared.debug.formulaWarningCount));
	line.append(FromLatin1(" prepared_formulas="));
	line.append(QString::number(CountPreparedFormulaSlots(prepared)));
	PrintLine(line);
}

[[nodiscard]] bool PrepareFixture(
		const QString &path,
		const QString &label,
		const std::shared_ptr<MathRenderer> &renderer,
		PreparedFixture *fixture) {
	auto parsed = PreparedDocument();
	if (!ParseFixture(path, label, &parsed)) {
		return false;
	}
	auto prepared = PrepareParsedDocumentForTest(parsed, path, renderer);
	if (prepared.failure.failed()) {
		PrintError(
			label + FromLatin1(" prepare-failed: ")
				+ PrepareFailureReason(prepared.failure));
		return false;
	}
	PrintPrepareSummary(label, prepared);
	if (fixture) {
		fixture->label = label;
		fixture->path = AbsolutePath(path);
		fixture->parsed = std::move(parsed);
		fixture->prepared = std::move(prepared);
	}
	return true;
}

[[nodiscard]] std::unique_ptr<MarkdownArticle> BuildArticleForTest(
		MarkdownArticleContent content,
		const std::shared_ptr<MathRenderer> &renderer,
		int width,
		int *height = nullptr) {
	auto article = std::make_unique<MarkdownArticle>(renderer);
	article->setContent(std::move(content));
	if (height) {
		*height = article->resizeGetHeight(width);
	} else {
		const auto computed = article->resizeGetHeight(width);
		(void)computed;
	}
	return article;
}

[[nodiscard]] QImage PaintArticleForTest(
		MarkdownArticle *article,
		int width,
		int height,
		int devicePixelRatio = 1,
		std::optional<MarkdownArticleSelection> selection = std::nullopt) {
	const auto previousDevicePixelRatio = style::DevicePixelRatio();
	style::SetDevicePixelRatio(devicePixelRatio);

	auto image = QImage(
		QSize(width * devicePixelRatio, height * devicePixelRatio),
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(devicePixelRatio);
	image.fill(Qt::transparent);
	{
		auto painter = Painter(&image);
		article->paint(
			painter,
			QRect(0, 0, width, height),
			MarkdownArticlePaintCaches(),
			selection.value_or(MarkdownArticleSelection()));
	}

	style::SetDevicePixelRatio(previousDevicePixelRatio);
	return image;
}

[[nodiscard]] QImage PaintArticleForSyntaxHighlightTest(
		MarkdownArticle *article,
		int width,
		int height,
		int devicePixelRatio = 1) {
	static auto preCache = [] {
		auto result = Ui::Text::QuotePaintCache();
		const auto color = st::defaultMarkdownTextPalette.monoFg->c;
		result.bg = color;
		result.bg.setAlpha(Ui::kDefaultBgOpacity * 255);
		result.outlines[0] = color;
		result.outlines[0].setAlpha(Ui::kDefaultOutline1Opacity * 255);
		result.outlines[1] = result.outlines[2] = QColor(0, 0, 0, 0);
		result.header = color;
		result.header.setAlpha(Ui::kDefaultOutline2Opacity * 255);
		result.icon = color;
		result.icon.setAlpha(Ui::kDefaultOutline3Opacity * 255);
		return result;
	}();
	static auto highlightColors = Ui::SyntaxHighlightColors(
		style::main_palette::get());

	const auto previousDevicePixelRatio = style::DevicePixelRatio();
	style::SetDevicePixelRatio(devicePixelRatio);

	auto image = QImage(
		QSize(width * devicePixelRatio, height * devicePixelRatio),
		QImage::Format_ARGB32_Premultiplied);
	image.setDevicePixelRatio(devicePixelRatio);
	image.fill(Qt::transparent);
	{
		auto painter = Painter(&image);
		article->paint(
			painter,
			QRect(0, 0, width, height),
			MarkdownArticlePaintCaches{
				.pre = &preCache,
				.colors = highlightColors,
			});
	}

	style::SetDevicePixelRatio(previousDevicePixelRatio);
	return image;
}

[[nodiscard]] QImage RenderWidgetForTest(QWidget *widget) {
	if (!widget || widget->size().isEmpty()) {
		return QImage();
	}
	auto image = QImage(
		widget->size(),
		QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	widget->render(&image);
	return image;
}

class MarkdownArticleHighlightWaiter final {
public:
	explicit MarkdownArticleHighlightWaiter(MarkdownArticle *article)
	: _article(article) {
		Spellchecker::HighlightReady(
		) | rpl::on_next([=](Spellchecker::HighlightProcessId processId) {
			if (_article && _article->highlightProcessDone(processId)) {
				_done = true;
			}
		}, _lifetime);
	}

	[[nodiscard]] bool wait(int timeoutMs = 5000) {
		auto timer = QElapsedTimer();
		timer.start();
		while (!_done && (timer.elapsed() < timeoutMs)) {
			QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
		}
		return _done;
	}

private:
	MarkdownArticle *_article = nullptr;
	rpl::lifetime _lifetime;
	bool _done = false;
};

[[nodiscard]] bool HasPaintedPixels(const QImage &image) {
	const auto bits = image.constBits();
	if (!bits) {
		return false;
	}
	const auto count = image.sizeInBytes();
	for (auto i = qsizetype(0); i != count; ++i) {
		if (bits[i] != 0) {
			return true;
		}
	}
	return false;
}

void FlushPendingWidgetEvents() {
	for (auto i = 0; i != 3; ++i) {
		QCoreApplication::sendPostedEvents();
		QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
	}
}

void SendMouseClick(QWidget *widget, QPoint point, Qt::MouseButton button) {
	if (!widget) {
		return;
	}
	const auto global = widget->mapToGlobal(point);
	auto press = QMouseEvent(
		QEvent::MouseButtonPress,
		point,
		global,
		button,
		button,
		Qt::NoModifier);
	QApplication::sendEvent(widget, &press);
	auto release = QMouseEvent(
		QEvent::MouseButtonRelease,
		point,
		global,
		button,
		Qt::NoButton,
		Qt::NoModifier);
	QApplication::sendEvent(widget, &release);
}

class WidgetEventCounter final : public QObject {
public:
	void reset() {
		paintCount = 0;
		updateRequestCount = 0;
	}

	int paintCount = 0;
	int updateRequestCount = 0;

protected:
	bool eventFilter(QObject *object, QEvent *event) override {
		Q_UNUSED(object);
		if (event->type() == QEvent::Paint) {
			++paintCount;
		} else if (event->type() == QEvent::UpdateRequest) {
			++updateRequestCount;
		}
		return false;
	}
};

class ArticleMediaBlockHost final : public MediaBlockHost {
public:
	void reset() {
		repaintRects.clear();
		relayoutRects.clear();
	}

	void requestRepaint(QRect articleRect) override {
		repaintRects.push_back(articleRect);
	}

	void requestRelayout(QRect articleRect) override {
		relayoutRects.push_back(articleRect);
	}

	std::vector<QRect> repaintRects;
	std::vector<QRect> relayoutRects;
};

template <typename Object>
[[nodiscard]] Object *FindChildObject(QObject *root) {
	if (!root) {
		return nullptr;
	}
	for (auto child : root->children()) {
		if (const auto object = dynamic_cast<Object*>(child)) {
			return object;
		}
		if (const auto nested = FindChildObject<Object>(child)) {
			return nested;
		}
	}
	return nullptr;
}

[[nodiscard]] QJsonDocument NativeIvPreferredSizeMessage(QSize bodySize) {
	return QJsonDocument(QJsonObject{
		{ u"event"_q, u"preferred_size"_q },
		{ u"width"_q, bodySize.width() },
		{ u"height"_q, bodySize.height() },
	});
}

[[nodiscard]] QSize NativeIvOverlayCloseHitSize() {
	const auto &style = st::markdownEmbedOverlayClose;
	return QSize(
		std::max(
			style.width,
			style.rippleAreaSize + 2 * style.rippleAreaPosition.x()),
		std::max(
			style.height,
			style.rippleAreaSize + 2 * style.rippleAreaPosition.y()));
}

[[nodiscard]] int NativeIvOverlayCloseHitHeight() {
	return NativeIvOverlayCloseHitSize().height();
}

[[nodiscard]] QSize NativeIvOverlayShellSizeForBody(
		QSize bodySize,
		bool fullWidth = false) {
	if (fullWidth) {
		return bodySize;
	}
	const auto padding = st::markdownEmbedOverlay.padding;
	return QSize(
		bodySize.width() + padding.left() + padding.right(),
		bodySize.height() + padding.top() + padding.bottom());
}

[[nodiscard]] QRect NativeIvOverlayAvailableRect(
		const EmbedOverlay *overlay,
		bool fullWidth = false) {
	if (!overlay) {
		return QRect();
	}
	const auto margin = st::markdownEmbedOverlay.margin;
	return overlay->rect().marginsRemoved(
		fullWidth
			? QMargins(0, margin.top(), 0, margin.bottom())
			: margin);
}

[[nodiscard]] QRect NativeIvOverlayFullWidthBodyRect(
		const EmbedOverlay *overlay) {
	const auto available = NativeIvOverlayAvailableRect(overlay, true);
	const auto skip = NativeIvOverlayCloseHitHeight();
	return available.isValid()
		? available.marginsRemoved(QMargins(0, skip, 0, skip))
		: QRect();
}

[[nodiscard]] bool PaintTouchesBottomOrRightImageEdge(const QImage &image) {
	if (image.isNull()) {
		return false;
	}
	const auto width = image.width();
	const auto height = image.height();
	const auto painted = [&](int x, int y) {
		return (image.pixel(x, y) & 0xFF000000U) != 0;
	};
	for (auto x = 0; x != width; ++x) {
		if (painted(x, height - 1)) {
			return true;
		}
	}
	for (auto y = 0; y != height; ++y) {
		if (painted(width - 1, y)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::optional<QRect> PaintedBoundsInRect(
		const QImage &image,
		QRect rect) {
	rect = rect.intersected(QRect(QPoint(), image.size()));
	if (rect.isEmpty()) {
		return std::nullopt;
	}
	auto left = rect.right();
	auto top = rect.bottom();
	auto right = rect.left() - 1;
	auto bottom = rect.top() - 1;
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (!(image.pixel(x, y) & 0xFF000000U)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] bool PixelsDifferInRect(
		const QImage &first,
		const QImage &second,
		QRect rect) {
	if (first.size() != second.size()) {
		return false;
	}
	rect = rect.intersected(QRect(QPoint(), first.size()));
	if (rect.isEmpty()) {
		return false;
	}
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (first.pixel(x, y) != second.pixel(x, y)) {
				return true;
			}
		}
	}
	return false;
}

[[nodiscard]] std::optional<QRect> ChangedBoundsInRect(
		const QImage &first,
		const QImage &second,
		QRect rect) {
	if (first.size() != second.size()) {
		return std::nullopt;
	}
	rect = rect.intersected(QRect(QPoint(), first.size()));
	if (rect.isEmpty()) {
		return std::nullopt;
	}
	auto left = rect.right();
	auto top = rect.bottom();
	auto right = rect.left() - 1;
	auto bottom = rect.top() - 1;
	for (auto y = rect.top(); y <= rect.bottom(); ++y) {
		for (auto x = rect.left(); x <= rect.right(); ++x) {
			if (first.pixel(x, y) == second.pixel(x, y)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] std::optional<QRect> SymbolHitBounds(
		MarkdownArticle *article,
		int width,
		int height,
		int offset,
		int expectedSegmentIndex = -1) {
	if (!article
		|| (width <= 0)
		|| (height <= 0)
		|| (offset < 0)
		|| (expectedSegmentIndex < -1)) {
		return std::nullopt;
	}
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto left = width;
	auto top = height;
	auto right = -1;
	auto bottom = -1;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != width; ++x) {
			const auto hit = article->hitTest(QPoint(x, y), flags);
			if (!hit.valid()
				|| !hit.state.uponSymbol
				|| ((expectedSegmentIndex >= 0)
					&& (hit.segmentIndex != expectedSegmentIndex))
				|| (int(hit.state.symbol) != offset)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] std::optional<QRect> SymbolRangeHitBounds(
		MarkdownArticle *article,
		int width,
		int height,
		int offset,
		int length,
		int expectedSegmentIndex = -1) {
	if (!article
		|| (width <= 0)
		|| (height <= 0)
		|| (offset < 0)
		|| (length <= 0)
		|| (expectedSegmentIndex < -1)) {
		return std::nullopt;
	}
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto left = width;
	auto top = height;
	auto right = -1;
	auto bottom = -1;
	const auto end = offset + length;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != width; ++x) {
			const auto hit = article->hitTest(QPoint(x, y), flags);
			if (!hit.valid()
				|| !hit.state.uponSymbol
				|| ((expectedSegmentIndex >= 0)
					&& (hit.segmentIndex != expectedSegmentIndex))) {
				continue;
			}
			const auto symbol = int(hit.state.symbol);
			if (symbol < offset || symbol >= end) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] std::optional<QRect> SegmentHitBounds(
		MarkdownArticle *article,
		int width,
		int height,
		int expectedSegmentIndex) {
	if (!article
		|| (width <= 0)
		|| (height <= 0)
		|| (expectedSegmentIndex < 0)) {
		return std::nullopt;
	}
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto left = width;
	auto top = height;
	auto right = -1;
	auto bottom = -1;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != width; ++x) {
			const auto hit = article->hitTest(QPoint(x, y), flags);
			if (!hit.valid()
				|| !hit.direct
				|| (hit.segmentIndex != expectedSegmentIndex)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

template <typename Predicate>
[[nodiscard]] std::optional<QRect> HitBoundsWhere(
		MarkdownArticle *article,
		int width,
		int height,
		Ui::Text::StateRequest::Flags flags,
		Predicate &&predicate) {
	if (!article || (width <= 0) || (height <= 0)) {
		return std::nullopt;
	}
	auto left = width;
	auto top = height;
	auto right = -1;
	auto bottom = -1;
	for (auto y = 0; y != height; ++y) {
		for (auto x = 0; x != width; ++x) {
			const auto hit = article->hitTest(QPoint(x, y), flags);
			if (!hit.valid() || !hit.direct || !predicate(hit)) {
				continue;
			}
			left = std::min(left, x);
			top = std::min(top, y);
			right = std::max(right, x);
			bottom = std::max(bottom, y);
		}
	}
	return (right >= left) && (bottom >= top)
		? std::make_optional(QRect(QPoint(left, top), QPoint(right, bottom)))
		: std::nullopt;
}

[[nodiscard]] bool IsChannelJoinLinkHit(
		const MarkdownArticleHitTestResult &hit) {
	return hit.state.link
		&& !hit.preparedLink
		&& (hit.mediaActivation.kind == MediaActivationKind::None);
}

[[nodiscard]] int HostedRequestCount(
		const TestMediaRuntime &runtime,
		TestHostedMediaKind kind) {
	switch (kind) {
	case TestHostedMediaKind::Photo:
		return runtime.hostedPhotoRequests();
	case TestHostedMediaKind::Video:
		return runtime.hostedVideoRequests();
	case TestHostedMediaKind::Audio:
		return runtime.hostedAudioRequests();
	case TestHostedMediaKind::Map:
		return runtime.hostedMapRequests();
	}
	return 0;
}

[[nodiscard]] const std::vector<std::shared_ptr<TestHostedMediaBlock>> &HostedBlocks(
		const TestHostedMediaBlockFactory &factory,
		TestHostedMediaKind kind) {
	switch (kind) {
	case TestHostedMediaKind::Photo:
		return factory.photoBlocks;
	case TestHostedMediaKind::Video:
		return factory.videoBlocks;
	case TestHostedMediaKind::Audio:
		return factory.audioBlocks;
	case TestHostedMediaKind::Map:
		return factory.mapBlocks;
	}
	return factory.photoBlocks;
}

[[nodiscard]] MediaActivationKind HostedActivationKind(
		TestHostedMediaKind kind) {
	switch (kind) {
	case TestHostedMediaKind::Photo:
		return MediaActivationKind::Photo;
	case TestHostedMediaKind::Video:
	case TestHostedMediaKind::Audio:
		return MediaActivationKind::Document;
	case TestHostedMediaKind::Map:
		return MediaActivationKind::ExternalUrl;
	}
	return MediaActivationKind::None;
}

void CheckHostedNativeIvSingleMediaCase(
		const NativeIvMediaFixture &fixture,
		TestHostedMediaKind kind,
		const std::shared_ptr<MathRenderer> &renderer,
		bool *ok) {
	const auto label = u"native-iv-hosted-"_q
		+ fixture.label.toLower()
		+ u"-article"_q;
	auto runtime = std::make_shared<TestMediaRuntime>();
	runtime->hostedFactory = std::make_shared<TestHostedMediaBlockFactory>();
	auto source = NativeIvSource(
		QVector<MTPPageBlock>{ fixture.block },
		fixture.photos,
		fixture.documents);
	auto prepared = TryPrepareNativeInstantView({
		.source = &source,
		.mediaRuntime = runtime,
	});
	Check(prepared.supported(), label + u" prepare supported"_q, ok);
	if (!prepared.supported()) {
		return;
	}
	auto height = 0;
	auto article = BuildArticleForTest(
		std::move(prepared.content),
		renderer,
		420,
		&height);
	const auto image = PaintArticleForTest(article.get(), 420, height);
	Check(HasPaintedPixels(image), label + u" paint produced pixels"_q, ok);
	Check(
		HostedRequestCount(*runtime, kind) == 1,
		label + u" hosted create request"_q,
		ok);
	switch (kind) {
	case TestHostedMediaKind::Photo:
		Check(
			runtime->photoRequests.empty(),
			label + u" skips markdown photo resolve"_q,
			ok);
		break;
	case TestHostedMediaKind::Video:
		Check(
			runtime->documentRequests.empty(),
			label + u" skips markdown document resolve"_q,
			ok);
		break;
	case TestHostedMediaKind::Map:
		Check(
			runtime->mapRequests.empty(),
			label + u" skips markdown map resolve"_q,
			ok);
		break;
	case TestHostedMediaKind::Audio:
		break;
	}
	const auto &blocks = HostedBlocks(*runtime->hostedFactory, kind);
	Check(blocks.size() == 1, label + u" hosted block count"_q, ok);
	if (blocks.empty()) {
		return;
	}
	const auto block = blocks.front();
	Check(block->stableId() != 0, label + u" stable id"_q, ok);
	Check(
		!block->requestedGeometries.empty()
			&& (block->geometry() != block->requestedGeometries.front())
			&& (block->geometry().width()
				< block->requestedGeometries.front().width())
			&& (block->geometry().height()
				< block->requestedGeometries.front().height()),
		label + u" geometry read-back differs from provisional rect"_q,
		ok);
	const auto initialRequested = block->requestedGeometries.empty()
		? QRect()
		: block->requestedGeometries.back();
	auto lookupFlags = Ui::Text::StateRequest::Flags();
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupLink;
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	const auto expectedActivation = HostedActivationKind(kind);
	const auto activationBounds = HitBoundsWhere(
		article.get(),
		420,
		height,
		lookupFlags,
		[=](const MarkdownArticleHitTestResult &hit) {
			return (hit.segmentIndex == 0)
				&& (hit.mediaActivation.kind == expectedActivation);
		});
	Check(
		activationBounds.has_value()
			&& (*activationBounds == block->geometry()),
		label + u" activation bounds use hosted geometry"_q,
		ok);
	const auto captionBounds = SegmentHitBounds(article.get(), 420, height, 1);
	Check(captionBounds.has_value(), label + u" caption bounds"_q, ok);
	if (captionBounds && !initialRequested.isEmpty()) {
		Check(
			captionBounds->top() < initialRequested.bottom(),
			label + u" caption follows hosted geometry read-back"_q,
			ok);
	}
	const auto primaryHit = article->hitTest(
		block->primaryArticlePoint(),
		lookupFlags);
	Check(
		primaryHit.valid()
			&& primaryHit.direct
			&& (primaryHit.segmentIndex == 0),
		label + u" primary direct hit"_q,
		ok);
	Check(
		primaryHit.mediaActivation.kind == expectedActivation,
		label + u" primary activation kind"_q,
		ok);
	if (kind == TestHostedMediaKind::Photo) {
		Check(
			primaryHit.mediaActivation.photo.get()
				== runtime->hostedFactory->photoRuntime.get(),
			label + u" primary photo runtime"_q,
			ok);
		Check(
			!primaryHit.state.link,
			label + u" primary photo hit has no auxiliary link"_q,
			ok);
	} else if (kind == TestHostedMediaKind::Video) {
		Check(
			primaryHit.mediaActivation.document.get()
				== runtime->hostedFactory->documentRuntime.get(),
			label + u" primary document runtime"_q,
			ok);
		Check(
			!primaryHit.state.link,
			label + u" primary video hit has no auxiliary link"_q,
			ok);
	} else if (kind == TestHostedMediaKind::Map) {
		Check(
			primaryHit.preparedLink.has_value()
				&& (primaryHit.mediaActivation.url
					== primaryHit.preparedLink->target),
			label + u" primary map prepared link"_q,
			ok);
	}
	const auto context = article->textForContext(primaryHit);
	Check(
		context.expanded
			== (block->selectionData().copyText
				+ u"\n"_q
				+ fixture.caption),
		label + u" context export text"_q,
		ok);
	const auto selection = article->textForSelection({
		.from = { .segment = 0, .offset = 0 },
		.to = { .segment = 0, .offset = 1 },
	}, nullptr);
	Check(
		selection.expanded == context.expanded,
		label + u" selection export text"_q,
		ok);
	const auto auxiliaryHit = article->hitTest(
		block->auxiliaryArticleRect().center(),
		lookupFlags);
	Check(
		auxiliaryHit.valid()
			&& auxiliaryHit.direct
			&& (auxiliaryHit.segmentIndex == 0),
		label + u" auxiliary direct hit"_q,
		ok);
	Check(
		auxiliaryHit.state.link == block->auxiliaryLink(),
		label + u" auxiliary click handler"_q,
		ok);
	Check(
		auxiliaryHit.mediaActivation.kind == MediaActivationKind::None,
		label + u" auxiliary suppresses primary activation"_q,
		ok);
	auto host = ArticleMediaBlockHost();
	article->setMediaBlockHost(&host);
	const auto previousGeometry = block->geometry();
	const auto previousSetGeometryCount = block->requestedGeometries.size();
	const auto previousCreateCount = HostedRequestCount(*runtime, kind);
	const auto narrowWidth = 300;
	const auto narrowHeight = article->resizeGetHeight(narrowWidth);
	Check(
		HostedRequestCount(*runtime, kind) == previousCreateCount,
		label + u" relayout reuses hosted block"_q,
		ok);
	Check(
		(block->requestedGeometries.size() > previousSetGeometryCount)
			&& (block->geometry() != previousGeometry)
			&& !block->resizeWidths.empty()
			&& (block->resizeWidths.front() != block->resizeWidths.back()),
		label + u" hosted block sees width-only relayout"_q,
		ok);
	host.reset();
	const auto repaintLocal = QRect(3, 4, 12, 10);
	const auto expectedRepaint = repaintLocal
		.translated(block->geometry().topLeft())
		.intersected(block->geometry());
	block->triggerRepaint(repaintLocal);
	Check(
		host.repaintRects.size() == 1
			&& (host.repaintRects.front() == expectedRepaint)
			&& !host.repaintRects.front().isEmpty()
			&& (host.repaintRects.front() != block->geometry())
			&& (host.repaintRects.front()
				!= QRect(0, 0, narrowWidth, narrowHeight)),
		label + u" rect-scoped repaint request"_q,
		ok);
	const auto relayoutLocal = QRect(6, 7, 9, 8);
	const auto expectedRelayout = relayoutLocal
		.translated(block->geometry().topLeft())
		.intersected(block->geometry());
	block->triggerRelayout(relayoutLocal);
	Check(
		host.relayoutRects.size() == 1
			&& (host.relayoutRects.front() == expectedRelayout)
			&& !host.relayoutRects.front().isEmpty()
			&& (host.relayoutRects.front() != block->geometry())
			&& (host.relayoutRects.front()
				!= QRect(0, 0, narrowWidth, narrowHeight)),
		label + u" rect-scoped relayout request"_q,
		ok);
}

void CheckNativeInstantViewHostedMediaCoverage(bool *ok) {
	const auto renderer = std::make_shared<MathRenderer>();
	auto photoFixture = NativeIvMediaFixture{
		.label = u"Photo"_q,
		.caption = u"Hosted photo caption"_q,
		.block = NativeIvPhotoBlock(9401, u"Hosted photo caption"_q),
		.photos = { NativeIvPhoto(9401, 640, 360) },
	};
	CheckHostedNativeIvSingleMediaCase(
		photoFixture,
		TestHostedMediaKind::Photo,
		renderer,
		ok);
	CheckHostedNativeIvSingleMediaCase(
		NativeIvVideoFixture(),
		TestHostedMediaKind::Video,
		renderer,
		ok);
	CheckHostedNativeIvSingleMediaCase(
		NativeIvAnimationFixture(),
		TestHostedMediaKind::Video,
		renderer,
		ok);
	CheckHostedNativeIvSingleMediaCase(
		NativeIvMapFixture(),
		TestHostedMediaKind::Map,
		renderer,
		ok);
}

void CheckNativeInstantViewHostedMediaFallbackCoverage(bool *ok) {
	const auto renderer = std::make_shared<MathRenderer>();
	const auto videoFixture = NativeIvVideoFixture();
	const auto audioFixture = NativeIvAudioFixture();
	const auto mapFixture = NativeIvMapFixture();

	const auto noHostedLabel = u"native-iv-no-hosted-media-fallback"_q;
	auto noHostedRuntime = std::make_shared<TestMediaRuntime>();
	noHostedRuntime->addPhotoRuntime(
		9501,
		std::make_shared<TestPhotoRuntime>());
	noHostedRuntime->addDocumentRuntime(
		7001,
		std::make_shared<TestDocumentRuntime>());
	noHostedRuntime->addDocumentRuntime(
		7002,
		std::make_shared<TestDocumentRuntime>());
	noHostedRuntime->addMapRuntime(
		51.5007,
		-0.1246,
		880088,
		13,
		std::make_shared<TestMapRuntime>());
	auto noHostedSource = NativeIvSource(
		QVector<MTPPageBlock>{
			NativeIvPhotoBlock(9501, u"Fallback photo caption"_q),
			videoFixture.block,
			mapFixture.block,
			audioFixture.block,
		},
		QVector<MTPPhoto>{ NativeIvPhoto(9501, 640, 360) },
		QVector<MTPDocument>{
			videoFixture.documents.front(),
			audioFixture.documents.front(),
		});
	auto noHostedPrepared = TryPrepareNativeInstantView({
		.source = &noHostedSource,
		.mediaRuntime = noHostedRuntime,
	});
	Check(
		noHostedPrepared.supported(),
		noHostedLabel + u" prepare supported"_q,
		ok);
	if (noHostedPrepared.supported()) {
		auto height = 0;
		auto article = BuildArticleForTest(
			std::move(noHostedPrepared.content),
			renderer,
			420,
			&height);
		const auto image = PaintArticleForTest(article.get(), 420, height);
		Check(
			HasPaintedPixels(image),
			noHostedLabel + u" paint produced pixels"_q,
			ok);
		Check(
			noHostedRuntime->photoRequests.size() == 1
				&& (noHostedRuntime->photoRequests.front() == 9501),
			noHostedLabel + u" photo resolves through markdown runtime"_q,
			ok);
		Check(
			std::find(
				noHostedRuntime->documentRequests.begin(),
				noHostedRuntime->documentRequests.end(),
				7001) != noHostedRuntime->documentRequests.end(),
			noHostedLabel + u" video resolves through markdown runtime"_q,
			ok);
		Check(
			std::find(
				noHostedRuntime->documentRequests.begin(),
				noHostedRuntime->documentRequests.end(),
				7002) != noHostedRuntime->documentRequests.end(),
			noHostedLabel + u" audio resolves through markdown runtime"_q,
			ok);
		Check(
			noHostedRuntime->mapRequests.size() == 1,
			noHostedLabel + u" map resolves through markdown runtime"_q,
			ok);
	}

	const auto urlPhotoLabel = u"native-iv-hosted-photo-url-fallback"_q;
	auto urlPhotoRuntime = std::make_shared<TestMediaRuntime>();
	urlPhotoRuntime->hostedFactory = std::make_shared<TestHostedMediaBlockFactory>();
	urlPhotoRuntime->addPhotoRuntime(
		9502,
		std::make_shared<TestPhotoRuntime>());
	auto urlPhotoSource = NativeIvSource(
		QVector<MTPPageBlock>{
			NativeIvPhotoBlock(
				9502,
				u"Linked photo caption"_q,
				QString(),
				u"https://example.com/photo"_q),
		},
		QVector<MTPPhoto>{ NativeIvPhoto(9502, 640, 360) });
	auto urlPhotoPrepared = TryPrepareNativeInstantView({
		.source = &urlPhotoSource,
		.mediaRuntime = urlPhotoRuntime,
	});
	Check(
		urlPhotoPrepared.supported(),
		urlPhotoLabel + u" prepare supported"_q,
		ok);
	if (urlPhotoPrepared.supported()) {
		auto height = 0;
		auto article = BuildArticleForTest(
			std::move(urlPhotoPrepared.content),
			renderer,
			420,
			&height);
		const auto bounds = SegmentHitBounds(article.get(), 420, height, 0);
		Check(bounds.has_value(), urlPhotoLabel + u" media bounds"_q, ok);
		Check(
			urlPhotoRuntime->hostedPhotoRequests() == 0,
			urlPhotoLabel + u" skips hosted photo"_q,
			ok);
		Check(
			urlPhotoRuntime->photoRequests.size() == 1
				&& (urlPhotoRuntime->photoRequests.front() == 9502),
			urlPhotoLabel + u" resolves markdown photo"_q,
			ok);
		if (bounds) {
			auto lookupFlags = Ui::Text::StateRequest::Flags();
			lookupFlags |= Ui::Text::StateRequest::Flag::LookupLink;
			lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
			const auto hit = article->hitTest(bounds->center(), lookupFlags);
			Check(
				hit.mediaActivation.kind == MediaActivationKind::ExternalUrl
					&& hit.preparedLink.has_value(),
				urlPhotoLabel + u" keeps external-url activation"_q,
				ok);
		}
	}

	const auto viewerClosedLabel = u"markdown-photo-viewer-closed-fallback"_q;
	auto viewerClosedRuntime = std::make_shared<TestMediaRuntime>();
	viewerClosedRuntime->hostedFactory =
		std::make_shared<TestHostedMediaBlockFactory>();
	viewerClosedRuntime->addPhotoRuntime(
		9503,
		std::make_shared<TestPhotoRuntime>());
	auto viewerClosedContent = MarkdownArticleContent();
	viewerClosedContent.mediaRuntime = viewerClosedRuntime;
	auto viewerClosedBlock = PreparedBlock();
	viewerClosedBlock.kind = PreparedBlockKind::Photo;
	viewerClosedBlock.photo.id.value = 9503;
	viewerClosedBlock.photo.photoId = 9503;
	viewerClosedBlock.photo.width = 640;
	viewerClosedBlock.photo.height = 360;
	viewerClosedBlock.photo.viewerOpen = false;
	viewerClosedContent.blocks.blocks.push_back(std::move(viewerClosedBlock));
	auto viewerClosedHeight = 0;
	auto viewerClosedArticle = BuildArticleForTest(
		std::move(viewerClosedContent),
		renderer,
		420,
		&viewerClosedHeight);
	const auto viewerClosedBounds = SegmentHitBounds(
		viewerClosedArticle.get(),
		420,
		viewerClosedHeight,
		0);
	Check(
		viewerClosedBounds.has_value(),
		viewerClosedLabel + u" media bounds"_q,
		ok);
	Check(
		viewerClosedRuntime->hostedPhotoRequests() == 0,
		viewerClosedLabel + u" skips hosted photo"_q,
		ok);
	Check(
		viewerClosedRuntime->photoRequests.size() == 1
			&& (viewerClosedRuntime->photoRequests.front() == 9503),
		viewerClosedLabel + u" resolves markdown photo"_q,
		ok);
	if (viewerClosedBounds) {
		auto lookupFlags = Ui::Text::StateRequest::Flags();
		lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
		const auto hit = viewerClosedArticle->hitTest(
			viewerClosedBounds->center(),
			lookupFlags);
		Check(
			hit.valid()
				&& hit.direct
				&& (hit.mediaActivation.kind == MediaActivationKind::None),
			viewerClosedLabel + u" keeps photo activation disabled"_q,
			ok);
	}

	const auto hostedAudioLabel = u"native-iv-hosted-audio-fallback"_q;
	auto hostedAudioRuntime = std::make_shared<TestMediaRuntime>();
	hostedAudioRuntime->hostedFactory =
		std::make_shared<TestHostedMediaBlockFactory>();
	auto audioDocumentRuntime = std::make_shared<TestDocumentRuntime>();
	hostedAudioRuntime->addDocumentRuntime(7002, audioDocumentRuntime);
	auto hostedAudioSource = NativeIvSource(
		QVector<MTPPageBlock>{ audioFixture.block },
		audioFixture.photos,
		audioFixture.documents);
	auto hostedAudioPrepared = TryPrepareNativeInstantView({
		.source = &hostedAudioSource,
		.mediaRuntime = hostedAudioRuntime,
	});
	Check(
		hostedAudioPrepared.supported(),
		hostedAudioLabel + u" prepare supported"_q,
		ok);
	if (hostedAudioPrepared.supported()) {
		auto height = 0;
		auto article = BuildArticleForTest(
			std::move(hostedAudioPrepared.content),
			renderer,
			420,
			&height);
		const auto bounds = SegmentHitBounds(article.get(), 420, height, 0);
		Check(bounds.has_value(), hostedAudioLabel + u" media bounds"_q, ok);
		Check(
			hostedAudioRuntime->hostedAudioRequests() == 0,
			hostedAudioLabel + u" skips hosted audio"_q,
			ok);
		Check(
			hostedAudioRuntime->documentRequests.size() == 1
				&& (hostedAudioRuntime->documentRequests.front() == 7002),
			hostedAudioLabel + u" resolves markdown audio document"_q,
			ok);
		if (bounds) {
			auto lookupFlags = Ui::Text::StateRequest::Flags();
			lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
			const auto hit = article->hitTest(bounds->center(), lookupFlags);
			Check(
				hit.mediaActivation.kind == MediaActivationKind::Document
					&& (hit.mediaActivation.document.get()
						== audioDocumentRuntime.get()),
				hostedAudioLabel + u" keeps document activation"_q,
				ok);
		}
	}
}

[[nodiscard]] std::vector<const MeasuredFormula*> MeasuredFormulaPointers(
		const MarkdownArticleContent &prepared) {
	auto result = std::vector<const MeasuredFormula*>();
	for (const auto &slot : prepared.formulas) {
		if (slot.present && slot.measuredData) {
			result.push_back(slot.measuredData.get());
		}
	}
	return result;
}

[[nodiscard]] const PreparedFormulaSlot *FindPreparedFormulaSlot(
		const MarkdownArticleContent &prepared,
		const QString &trimmedTex,
		MathKind kind) {
	for (const auto &slot : prepared.formulas) {
		if (slot.present
			&& slot.kind == kind
			&& slot.trimmedTex == trimmedTex) {
			return &slot;
		}
	}
	return nullptr;
}

template <typename Callback>
void ForEachPreparedBlock(
		const std::vector<PreparedBlock> &blocks,
		Callback &&callback) {
	for (const auto &block : blocks) {
		callback(block);
		ForEachPreparedBlock(block.children, callback);
	}
}

template <typename Callback>
void ForEachPreparedLink(
		const std::vector<PreparedBlock> &blocks,
		Callback &&callback) {
	ForEachPreparedBlock(blocks, [&](const PreparedBlock &block) {
		for (const auto &link : block.links) {
			callback(link);
		}
		for (const auto &row : block.tableRows) {
			for (const auto &cell : row.cells) {
				for (const auto &link : cell.links) {
					callback(link);
				}
			}
		}
	});
}

template <typename Predicate>
[[nodiscard]] const PreparedLink *FindPreparedLink(
		const std::vector<PreparedBlock> &blocks,
		Predicate &&predicate) {
	const PreparedLink *result = nullptr;
	ForEachPreparedLink(blocks, [&](const PreparedLink &link) {
		if (!result && predicate(link)) {
			result = &link;
		}
	});
	return result;
}

[[nodiscard]] const PreparedFootnote *FindPreparedFootnote(
		const MarkdownArticleContent &prepared,
		const QString &label) {
	for (const auto &footnote : prepared.footnotes) {
		if (footnote.label == label) {
			return &footnote;
		}
	}
	return nullptr;
}

template <typename Callback>
void ForEachPreparedFootnoteLink(
		const MarkdownArticleContent &prepared,
		Callback &&callback) {
	for (const auto &footnote : prepared.footnotes) {
		for (const auto &link : footnote.links) {
			callback(link);
		}
		ForEachPreparedLink(footnote.blocks, callback);
	}
}

[[nodiscard]] const PreparedBlock *FindPreparedParagraphContaining(
		const MarkdownArticleContent &prepared,
		const QString &text) {
	const PreparedBlock *result = nullptr;
	ForEachPreparedBlock(prepared.blocks.blocks, [&](const PreparedBlock &block) {
		if (!result
			&& block.kind == PreparedBlockKind::Paragraph
			&& block.text.text.contains(text)) {
			result = &block;
		}
	});
	return result;
}

[[nodiscard]] bool HasEntityRange(
		const TextWithEntities &text,
		EntityType type,
		int offset,
		int length) {
	for (const auto &entity : text.entities) {
		if (entity.type() == type
			&& entity.offset() == offset
			&& entity.length() == length) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] uint16 PreparedLinkIndexFromCustomUrlData(
		const QString &data) {
	const auto prefix = FromLatin1("internal:index");
	if (!data.startsWith(prefix) || data.size() != prefix.size() + 1) {
		return 0;
	}
	return static_cast<uint16>(data[prefix.size()].unicode());
}

[[nodiscard]] const PreparedLink *FindPreparedLinkByCustomUrlRange(
		const TextWithEntities &text,
		const std::vector<PreparedLink> &links,
		int offset,
		int length) {
	for (const auto &entity : text.entities) {
		if (entity.type() != EntityType::CustomUrl
			|| entity.offset() != offset
			|| entity.length() != length) {
			continue;
		}
		const auto index = PreparedLinkIndexFromCustomUrlData(entity.data());
		if (!index) {
			continue;
		}
		for (const auto &link : links) {
			if (link.index == index) {
				return &link;
			}
		}
	}
	return nullptr;
}

[[nodiscard]] std::vector<const PreparedBlock*> CollectPreparedBlocksByKind(
		const std::vector<PreparedBlock> &blocks,
		PreparedBlockKind kind) {
	auto result = std::vector<const PreparedBlock*>();
	ForEachPreparedBlock(blocks, [&](const PreparedBlock &block) {
		if (block.kind == kind) {
			result.push_back(&block);
		}
	});
	return result;
}

struct InlineTextObjectMatch {
	EntityInText entity;
	InlineTextObjectEntity object;
};

[[nodiscard]] std::vector<InlineTextObjectMatch> CollectInlineTextObjectMatches(
		const TextWithEntities &text) {
	auto result = std::vector<InlineTextObjectMatch>();
	for (const auto &entity : text.entities) {
		if (entity.type() != EntityType::CustomEmoji
			|| entity.length() != 1
			|| entity.offset() < 0
			|| entity.offset() >= text.text.size()
			|| text.text[entity.offset()] != QChar::ObjectReplacementCharacter) {
			continue;
		}
		if (const auto parsed = ParseInlineTextObjectEntity(entity.data())) {
			result.push_back(InlineTextObjectMatch{
				.entity = entity,
				.object = *parsed,
			});
		}
	}
	return result;
}

[[nodiscard]] bool TextHasInlineFormulaEntity(
		const TextWithEntities &text,
		const QString &copySource,
		const QString &trimmedTex = QString()) {
	for (const auto &match : CollectInlineTextObjectMatches(text)) {
		if (match.object.kind != InlineTextObjectKind::Formula) {
			continue;
		}
		const auto formula = std::get_if<InlineTextObjectFormulaData>(
			&match.object.data);
		if (!formula) {
			continue;
		}
		if ((formula->copySource == copySource)
			&& (trimmedTex.isEmpty() || formula->trimmedTex == trimmedTex)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] bool HasEntityType(
		const EntitiesInText &entities,
		EntityType type) {
	for (const auto &entity : entities) {
		if (entity.type() == type) {
			return true;
		}
	}
	return false;
}

void Check(bool condition, const QString &message, bool *ok) {
	if (condition) {
		return;
	}
	if (ok) {
		*ok = false;
	}
	PrintError(FromLatin1("assertion failed: ") + message);
}

void CheckParseSuccess(
		const QByteArray &source,
		const QString &label,
		bool *ok) {
	const auto parsed = ParseMarkdownForIv(source, ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
}

void CheckParseFailure(
		const QByteArray &source,
		const QString &label,
		const QString &expectedError,
		bool *ok) {
	const auto parsed = ParseMarkdownForIv(source, ParseOptions{ label });
	Check(
		!parsed.ok,
		label + FromLatin1(" should fail"),
		ok);
	if (!parsed.ok) {
		Check(
			parsed.error == expectedError,
			label + FromLatin1(" error should be ")
				+ expectedError
				+ FromLatin1(", got ")
				+ parsed.error,
			ok);
	}
}

void CheckValidationEdges(bool *ok) {
	const auto &limits = ParseLimitsForIv();
	auto utf8BomSource = QByteArray::fromHex("EFBBBF");
	utf8BomSource.append("# Title\n");
	auto validatedUtf8Bom = CheckValidationSuccess(
		utf8BomSource,
		FromLatin1("utf8 bom"),
		ok);
	if (validatedUtf8Bom.ok) {
		Check(
			validatedUtf8Bom.source.normalized == QByteArray("# Title\n"),
			FromLatin1("utf8 bom normalized bytes"),
			ok);
		const auto parsedValidated = ParseMarkdownForIv(
			std::move(validatedUtf8Bom.source));
		Check(
			parsedValidated.ok,
			FromLatin1("utf8 bom validated parse failed: ")
				+ parsedValidated.error,
			ok);
	}
	CheckParseSuccess(
		utf8BomSource,
		FromLatin1("utf8 bom"),
		ok);

	CheckValidationFailure(
		QByteArray::fromHex("FFFE2300"),
		FromLatin1("utf16 bom"),
		FromLatin1("source-unsupported-bom"),
		ok);
	CheckParseFailure(
		QByteArray::fromHex("FFFE2300"),
		FromLatin1("utf16 bom"),
		FromLatin1("source-unsupported-bom"),
		ok);
	CheckValidationFailure(
		QByteArray("a\0b", 3),
		FromLatin1("nul byte"),
		FromLatin1("source-binary"),
		ok);
	CheckParseFailure(
		QByteArray("a\0b", 3),
		FromLatin1("nul byte"),
		FromLatin1("source-binary"),
		ok);
	CheckValidationFailure(
		QByteArray::fromHex("C328"),
		FromLatin1("invalid utf8"),
		FromLatin1("source-invalid-utf8"),
		ok);
	CheckParseFailure(
		QByteArray::fromHex("C328"),
		FromLatin1("invalid utf8"),
		FromLatin1("source-invalid-utf8"),
		ok);

	const auto oversizedSource = QByteArray(limits.maxSourceBytes + 1, 'a');
	CheckValidationFailure(
		oversizedSource,
		FromLatin1("source size"),
		FromLatin1("source-too-large"),
		ok);
	CheckParseFailure(
		oversizedSource,
		FromLatin1("source size"),
		FromLatin1("source-too-large"),
		ok);

	auto oversizedFormula = QByteArray();
	oversizedFormula.reserve(limits.maxFormulaBytes + 2);
	oversizedFormula.append('$');
	oversizedFormula.append(QByteArray(limits.maxFormulaBytes + 1, '+'));
	oversizedFormula.append('$');
	CheckParseFailure(
		oversizedFormula,
		FromLatin1("formula size"),
		FromLatin1("formula-too-large"),
		ok);

	const auto generated = QByteArray(
		"Inline code `$code$`.\n"
		"```\n"
		"$block$\n"
		"```\n"
		"Escaped \\$ and price $5.99$.\n"
		"Real $x + y$ done.\n");
	const auto parsed = ParseMarkdownForIv(
		generated,
		ParseOptions{ FromLatin1("generated-edge-checks.md") });
	Check(
		parsed.ok,
		FromLatin1("generated exclusions parse failed: ") + parsed.error,
		ok);
	if (parsed.ok) {
		Check(
			static_cast<int>(parsed.document.formulas.size()) == 1,
			FromLatin1("generated exclusions formula count"),
			ok);
		Check(
			CountFormulas(parsed.document, MathKind::Inline) == 1,
			FromLatin1("generated exclusions inline formula count"),
			ok);
		Check(
			CountFormulas(parsed.document, MathKind::Display) == 0,
			FromLatin1("generated exclusions display formula count"),
			ok);
		Check(
			HasFormula(parsed.document, MathKind::Inline, FromLatin1("x + y")),
			FromLatin1("generated exclusions real formula"),
			ok);
	}
}

void CheckInlineHtmlCoverage(bool dump, bool *ok) {
	const auto source = QByteArray(
		"H<sub>2</sub>O\n"
		"E = mc<sup>2</sup>\n"
		"<mark>Highlighted text using HTML mark</mark>\n"
		"<br>\n");
	const auto label = FromLatin1("generated-inline-html.md");
	const auto parsed = ParseMarkdownForIv(source, ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto validated = CheckValidationSuccess(source, label, ok);
	if (!validated.ok) {
		return;
	}
	const auto parsedValidated = ParseMarkdownForIv(std::move(validated.source));
	Check(
		parsedValidated.ok,
		label + FromLatin1(" validated parse failed: ")
			+ parsedValidated.error,
		ok);
	if (!parsedValidated.ok) {
		return;
	}
	CheckMatchingParseCounts(
		parsed.document,
		parsedValidated.document,
		label,
		ok);
	if (dump) {
		PrintLine(DumpForDebug(parsed.document));
	}
	const auto &document = parsed.document.document;
	const auto subscriptParagraph = FindNodeByKindAndLineRange(
		document,
		NodeKind::Paragraph,
		1,
		1);
	Check(
		subscriptParagraph != nullptr,
		label + FromLatin1(" subscript paragraph range"),
		ok);
	if (subscriptParagraph) {
		Check(
			HasExactInlineHtmlTriplet(
				*subscriptParagraph,
				FromLatin1("<sub>"),
				FromLatin1("2"),
				FromLatin1("</sub>")),
			label + FromLatin1(" subscript HTML triplet"),
			ok);
	}
	const auto superscriptParagraph = FindNodeByKindAndLineRange(
		document,
		NodeKind::Paragraph,
		2,
		2);
	Check(
		superscriptParagraph != nullptr,
		label + FromLatin1(" superscript paragraph range"),
		ok);
	if (superscriptParagraph) {
		Check(
			HasExactInlineHtmlTriplet(
				*superscriptParagraph,
				FromLatin1("<sup>"),
				FromLatin1("2"),
				FromLatin1("</sup>")),
			label + FromLatin1(" superscript HTML triplet"),
			ok);
	}
	const auto markParagraph = FindNodeByKindAndLineRange(
		document,
		NodeKind::Paragraph,
		3,
		3);
	Check(
		markParagraph != nullptr,
		label + FromLatin1(" mark paragraph range"),
		ok);
	if (markParagraph) {
		Check(
			HasExactInlineHtmlTriplet(
				*markParagraph,
				FromLatin1("<mark>"),
				FromLatin1("Highlighted text using HTML mark"),
				FromLatin1("</mark>")),
			label + FromLatin1(" mark HTML triplet"),
			ok);
	}
	const auto htmlLineBreakParagraph = FindNodeByKindAndLineRange(
		document,
		NodeKind::Paragraph,
		4,
		4);
	Check(
		htmlLineBreakParagraph != nullptr,
		label + FromLatin1(" html line break paragraph range"),
		ok);
	if (htmlLineBreakParagraph) {
		const auto brInline = FindHtmlInlineByRaw(
			*htmlLineBreakParagraph,
			FromLatin1("<br>"));
		Check(
			brInline != nullptr,
			label + FromLatin1(" html line break raw node"),
			ok);
		if (brInline) {
			Check(
				brInline->text.isEmpty() && brInline->children.empty(),
				label + FromLatin1(" html line break lone HtmlInline"),
				ok);
		}
	}
}

void CheckInlineHtmlPrepareCoverage(bool *ok) {
	const auto source = QByteArray(
		"HTML line break test:<br>\n"
		"This should also be on a new line.\n");
	const auto label = FromLatin1("generated-inline-html-prepare.md");
	const auto parsed = ParseMarkdownForIv(source, ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		std::make_shared<MathRenderer>());
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	const auto paragraph = FindPreparedParagraphContaining(
		prepared,
		FromLatin1("HTML line break test:"));
	Check(
		paragraph != nullptr,
		label + FromLatin1(" prepared paragraph found"),
		ok);
	if (!paragraph) {
		return;
	}
	const auto expected = FromLatin1(
		"HTML line break test:\nThis should also be on a new line.");
	const auto &text = paragraph->text.text;
	Check(
		text.contains(expected),
		label + FromLatin1(" prepared text keeps real newline"),
		ok);
	Check(
		!text.contains(FromLatin1("<br>")),
		label + FromLatin1(" prepared text drops literal br"),
		ok);
	Check(
		!text.contains(FromLatin1("\n ")),
		label + FromLatin1(" prepared text has no post-break space"),
		ok);
}

void CheckFixtureSemanticCoverage(
		const PreparedDocument &document,
		const QString &path,
		bool *ok) {
	auto footnoteReferences = std::vector<const MarkdownNode*>();
	CollectNodesByKind(
		document.document,
		NodeKind::FootnoteReference,
		&footnoteReferences);
	Check(
		footnoteReferences.size() >= 2,
		FromLatin1("markdown-example.md footnote reference count"),
		ok);
	if (footnoteReferences.size() >= 2) {
		Check(
			footnoteReferences[0]->footnoteLabel == FromLatin1("1")
				&& footnoteReferences[0]->footnoteOrdinal == 1,
			FromLatin1("markdown-example.md first footnote reference label"),
			ok);
		Check(
			footnoteReferences[1]->footnoteLabel == FromLatin1("long-note")
				&& footnoteReferences[1]->footnoteOrdinal == 2,
			FromLatin1("markdown-example.md second footnote reference label"),
			ok);
	}

	auto footnoteDefinitions = std::vector<const MarkdownNode*>();
	CollectNodesByKind(
		document.document,
		NodeKind::FootnoteDefinition,
		&footnoteDefinitions);
	Check(
		footnoteDefinitions.size() >= 2,
		FromLatin1("markdown-example.md footnote definition count"),
		ok);
	if (footnoteDefinitions.size() >= 2) {
		Check(
			footnoteDefinitions[0]->anchorId == FromLatin1("fn-1")
				&& footnoteDefinitions[1]->anchorId == FromLatin1("fn-2"),
			FromLatin1("markdown-example.md footnote anchors"),
			ok);
	}

	const auto headingsLink = FindLinkByTarget(document.document, FromLatin1("#headings"));
	Check(
		headingsLink != nullptr,
		FromLatin1("markdown-example.md toc fragment link"),
		ok);

	const auto relativeLink = FindLinkByTarget(
		document.document,
		FromLatin1("./docs/getting-started.md"));
	Check(
		relativeLink != nullptr,
		FromLatin1("markdown-example.md relative link parse"),
		ok);

	const auto headings = FindNodeByKindAndLineRange(
		document.document,
		NodeKind::Heading,
		27,
		27);
	Check(
		headings != nullptr && headings->anchorId == FromLatin1("headings"),
		FromLatin1("markdown-example.md headings anchor id"),
		ok);
	const auto definitionLists = FindNodeByKindAndLineRange(
		document.document,
		NodeKind::Heading,
		266,
		266);
	Check(
		definitionLists != nullptr
			&& definitionLists->anchorId
				== FromLatin1("definition-lists-renderer-dependent"),
		FromLatin1("markdown-example.md punctuation heading anchor id"),
		ok);

	const auto details = FindNodeByKindAndLineRange(
		document.document,
		NodeKind::HtmlBlock,
		261,
		264);
	Check(
		details != nullptr,
		FromLatin1("markdown-example.md details block range"),
		ok);
	if (details) {
		Check(
			details->htmlBlockKind == HtmlBlockKind::Details
				&& details->detailsSummary
					== FromLatin1("Click to expand details/summary block"),
			FromLatin1("markdown-example.md details classification"),
			ok);
	}
	const auto comment = FindHtmlBlockContaining(
		document.document,
		FromLatin1("markdown-renderer-test"));
	Check(
		comment != nullptr && comment->htmlBlockKind == HtmlBlockKind::Comment,
		FromLatin1("markdown-example.md comment classification"),
		ok);
	Check(
		WarningContains(document, FromLatin1("Unsupported HTML block")),
		FromLatin1("markdown-example.md unsupported html warning"),
		ok);

	const auto duplicateHeadings = ParseMarkdownForIv(
		QByteArray("## Same\n## Same\n"),
		ParseOptions{ FromLatin1("generated-duplicate-headings.md") });
	Check(
		duplicateHeadings.ok,
		FromLatin1("generated duplicate headings parse failed"),
		ok);
	if (duplicateHeadings.ok) {
		auto duplicateNodes = std::vector<const MarkdownNode*>();
		CollectNodesByKind(
			duplicateHeadings.document.document,
			NodeKind::Heading,
			&duplicateNodes);
		Check(
			duplicateNodes.size() == 2
				&& duplicateNodes[0]->anchorId == FromLatin1("same")
				&& duplicateNodes[1]->anchorId == FromLatin1("same-2"),
			FromLatin1("generated duplicate headings anchors"),
			ok);
	}

}

void CheckInlineTextObjectPrepareCoverage(bool *ok) {
	const auto label = FromLatin1("generated-inline-text-objects.md");
	const auto parsed = ParseMarkdownForIv(
		QByteArray(
			"Paragraph $a^2 + b^2 = c^2$ text.\n\n"
			"#### Heading $ax^2 + bx + c = 0$\n\n"
			"| Formula | Value |\n"
			"| --- | --- |\n"
			"| $x^n$ | n |\n"),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		std::make_shared<MathRenderer>());
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}

	const PreparedBlock *paragraph = nullptr;
	const PreparedBlock *heading = nullptr;
	const PreparedBlock *table = nullptr;
	ForEachPreparedBlock(prepared.blocks.blocks, [&](const PreparedBlock &block) {
		if (!paragraph
			&& block.kind == PreparedBlockKind::Paragraph
			&& block.text.text.contains(FromLatin1("Paragraph"))) {
			paragraph = &block;
		}
		if (!heading
			&& block.kind == PreparedBlockKind::Heading
			&& block.text.text.contains(FromLatin1("Heading"))) {
			heading = &block;
		}
		if (!table && block.kind == PreparedBlockKind::Table) {
			table = &block;
		}
	});
	Check(
		paragraph != nullptr,
		label + FromLatin1(" prepared paragraph block"),
		ok);
	if (paragraph) {
		Check(
			paragraph->text.text.count(QChar::ObjectReplacementCharacter) == 1,
			label + FromLatin1(" paragraph ORC count"),
			ok);
		Check(
			TextHasInlineFormulaEntity(
				paragraph->text,
				FromLatin1("$a^2 + b^2 = c^2$"),
				FromLatin1("a^2 + b^2 = c^2")),
			label + FromLatin1(" paragraph inline formula entity"),
			ok);
	}
	Check(
		heading != nullptr,
		label + FromLatin1(" prepared heading block"),
		ok);
	if (heading) {
		Check(
			heading->text.text.count(QChar::ObjectReplacementCharacter) == 1,
			label + FromLatin1(" heading ORC count"),
			ok);
		Check(
			TextHasInlineFormulaEntity(
				heading->text,
				FromLatin1("$ax^2 + bx + c = 0$"),
				FromLatin1("ax^2 + bx + c = 0")),
			label + FromLatin1(" heading inline formula entity"),
			ok);
	}
	Check(
		table != nullptr,
		label + FromLatin1(" prepared table block"),
		ok);
	if (table) {
		Check(
			table->tableRows.size() == 2,
			label + FromLatin1(" table row count"),
			ok);
		if (table->tableRows.size() == 2
			&& table->tableRows[1].cells.size() == 2) {
			const auto &formulaCell = table->tableRows[1].cells[0];
			Check(
				formulaCell.text.text.count(QChar::ObjectReplacementCharacter) == 1,
				label + FromLatin1(" table cell ORC count"),
				ok);
			Check(
				TextHasInlineFormulaEntity(
					formulaCell.text,
					FromLatin1("$x^n$"),
					FromLatin1("x^n")),
				label + FromLatin1(" table cell inline formula entity"),
				ok);
		}
	}
}

void CheckNativeInstantViewPrepareCoverage(bool *ok) {
	const auto entityLabel = u"native-iv-inline-image-entity"_q;
	const auto serialized = SerializeInlineTextObjectEntity({
		.kind = InlineTextObjectKind::IvImage,
		.data = InlineTextObjectIvImageData{
			.documentId = 8801,
			.width = 24,
			.height = 18,
			.replacementText = u"[inline image]"_q,
		},
	});
	Check(!serialized.isEmpty(), entityLabel + u" serialize"_q, ok);
	const auto parsedEntity = ParseInlineTextObjectEntity(serialized);
	Check(parsedEntity.has_value(), entityLabel + u" parse"_q, ok);
	if (parsedEntity) {
		Check(
			parsedEntity->kind == InlineTextObjectKind::IvImage,
			entityLabel + u" parsed kind"_q,
			ok);
		if (const auto image = std::get_if<InlineTextObjectIvImageData>(
				&parsedEntity->data)) {
			Check(
				image->documentId == 8801,
				entityLabel + u" parsed document id"_q,
				ok);
			Check(
				image->width == 24 && image->height == 18,
				entityLabel + u" parsed size"_q,
				ok);
			Check(
				image->replacementText == u"[inline image]"_q,
				entityLabel + u" parsed replacement text"_q,
				ok);
		} else {
			Check(false, entityLabel + u" parsed payload"_q, ok);
		}
	}

	const auto videoFixture = NativeIvVideoFixture();
	const auto audioFixture = NativeIvAudioFixture();
	const auto mapFixture = NativeIvMapFixture();
	const auto channelFixture = NativeIvChannelFixture();
	const auto collageFixture = NativeIvCollageFixture();
	const auto slideshowFixture = NativeIvSlideshowFixture();

	auto placeholderFixtures = std::vector<NativeIvPlaceholderFixture>();
	const auto addPlaceholder = [&](
			NativeIvPlaceholderKind kind,
			QString caption,
			QString fallbackUrl = QString(),
			int width = 0,
			int height = 0,
			bool fullWidth = false,
			bool allowScrolling = false) {
		placeholderFixtures.push_back({
			.kind = kind,
			.caption = caption,
			.expectedLabel = NativeIvPlaceholderLabel(kind),
			.expectedFallbackUrl = fallbackUrl,
			.expectedWidth = width,
			.expectedHeight = height,
			.expectedFullWidth = fullWidth,
			.expectedAllowScrolling = allowScrolling,
			.block = NativeIvPlaceholderBlock(kind, caption),
		});
	};
	addPlaceholder(
		NativeIvPlaceholderKind::Embed,
		u"Embed caption"_q,
		u"https://example.com/embed"_q,
		640,
		360,
		true,
		true);
	const auto embedPostBlock = NativeIvEmbedPostBlock(
		NativeIvDefaultEmbedPostBlocks(),
		u"Embed post caption"_q,
		u"Embed post credit"_q);

	auto supportedBlocks = QVector<MTPPageBlock>();
	supportedBlocks.push_back(NativeIvInlineImageParagraph(
		8801,
		24,
		18,
		u"Lead "_q,
		u" tail."_q));
	supportedBlocks.push_back(NativeIvPhotoBlock(
		9001,
		u"Photo caption"_q,
		u"Photo credit"_q,
		u"https://example.com/photo"_q));
	supportedBlocks.push_back(videoFixture.block);
	for (const auto &fixture : placeholderFixtures) {
		supportedBlocks.push_back(fixture.block);
	}
	supportedBlocks.push_back(embedPostBlock);
	supportedBlocks.push_back(collageFixture.block);
	supportedBlocks.push_back(slideshowFixture.block);
	supportedBlocks.push_back(audioFixture.block);
	supportedBlocks.push_back(mapFixture.block);
	supportedBlocks.push_back(channelFixture.block);
	supportedBlocks.push_back(NativeIvCoveredPhotoBlock(
		9002,
		u"Cover caption"_q));

	auto supportedPhotos = QVector<MTPPhoto>();
	supportedPhotos.push_back(NativeIvPhoto(9001, 640, 360));
	supportedPhotos.push_back(NativeIvPhoto(9002, 320, 200));
	for (const auto &photo : collageFixture.photos) {
		supportedPhotos.push_back(photo);
	}

	auto supportedDocuments = QVector<MTPDocument>();
	for (const auto &document : videoFixture.documents) {
		supportedDocuments.push_back(document);
	}
	for (const auto &document : audioFixture.documents) {
		supportedDocuments.push_back(document);
	}
	for (const auto &document : collageFixture.documents) {
		supportedDocuments.push_back(document);
	}
	for (const auto &document : slideshowFixture.documents) {
		supportedDocuments.push_back(document);
	}

	auto supportedSource = NativeIvSource(
		std::move(supportedBlocks),
		std::move(supportedPhotos),
		std::move(supportedDocuments));
	const auto supported = TryPrepareNativeInstantView({
		.source = &supportedSource,
	});
	Check(
		supported.supported(),
		u"native-iv supported source classification"_q,
		ok);
	Check(
		!supported.unsupported() && !supported.failed(),
		u"native-iv supported source nonterminal classification"_q,
		ok);
	Check(
		!supported.content.failure.failed(),
		u"native-iv supported source failure state"_q,
		ok);
	Check(
		supported.content.blocks.blocks.size() == 11,
		u"native-iv supported prepared block count"_q,
		ok);

	const PreparedBlock *inlineParagraph = nullptr;
	const PreparedBlock *photo = nullptr;
	const PreparedBlock *video = nullptr;
	const PreparedBlock *collage = nullptr;
	const PreparedBlock *slideshow = nullptr;
	const PreparedBlock *audio = nullptr;
	const PreparedBlock *map = nullptr;
	const PreparedBlock *channel = nullptr;
	const PreparedBlock *embedPost = nullptr;
	const PreparedBlock *coveredPhoto = nullptr;
	auto preparedPlaceholders = std::vector<const PreparedBlock*>();
	auto preparedPlaceholderIds = std::vector<
		std::pair<QString, PreparedPlaceholderBlockId>>();
	ForEachPreparedBlock(
		supported.content.blocks.blocks,
		[&](const PreparedBlock &block) {
			if (!inlineParagraph
				&& block.kind == PreparedBlockKind::Paragraph
				&& block.text.text.contains(QChar::ObjectReplacementCharacter)) {
				inlineParagraph = &block;
			}
			if (!photo
				&& block.kind == PreparedBlockKind::Photo
				&& block.photo.photoId == 9001) {
				photo = &block;
			}
			if (!video
				&& block.kind == PreparedBlockKind::Video
				&& block.video.media.id == 7001) {
				video = &block;
			}
			if (!collage
				&& block.kind == PreparedBlockKind::GroupedMedia
				&& block.text.text == collageFixture.caption) {
				collage = &block;
			}
			if (!slideshow
				&& block.kind == PreparedBlockKind::GroupedMedia
				&& block.text.text == slideshowFixture.caption) {
				slideshow = &block;
			}
			if (!audio
				&& block.kind == PreparedBlockKind::Audio
				&& block.audio.documentId == 7002) {
				audio = &block;
			}
			if (!map && block.kind == PreparedBlockKind::Map) {
				map = &block;
			}
			if (!channel
				&& block.kind == PreparedBlockKind::Channel
				&& block.channel.channelId == 7006) {
				channel = &block;
			}
			if (!embedPost
				&& block.kind == PreparedBlockKind::EmbedPost
				&& block.text.text == u"Embed post caption\nEmbed post credit"_q) {
				embedPost = &block;
			}
			if (!coveredPhoto
				&& block.kind == PreparedBlockKind::Photo
				&& block.photo.photoId == 9002) {
				coveredPhoto = &block;
			}
			if (block.kind == PreparedBlockKind::Placeholder) {
				preparedPlaceholders.push_back(&block);
			}
		});

	Check(
		inlineParagraph != nullptr,
		u"native-iv inline-image paragraph block"_q,
		ok);
	if (inlineParagraph) {
		const auto matches = CollectInlineTextObjectMatches(inlineParagraph->text);
		Check(
			matches.size() == 1,
			u"native-iv inline-image entity count"_q,
			ok);
		Check(
			inlineParagraph->text.text.count(
				QChar::ObjectReplacementCharacter) == 1,
			u"native-iv inline-image ORC count"_q,
			ok);
		if (matches.size() == 1) {
			Check(
				matches[0].object.kind == InlineTextObjectKind::IvImage,
				u"native-iv inline-image prepared kind"_q,
				ok);
			if (const auto image = std::get_if<InlineTextObjectIvImageData>(
					&matches[0].object.data)) {
				Check(
					image->documentId == 8801,
					u"native-iv inline-image prepared document id"_q,
					ok);
				Check(
					image->width == 24 && image->height == 18,
					u"native-iv inline-image prepared size"_q,
					ok);
				Check(
					image->replacementText == u"[image]"_q,
					u"native-iv inline-image prepared replacement text"_q,
					ok);
			} else {
				Check(false, u"native-iv inline-image prepared payload"_q, ok);
			}
		}
	}

	Check(photo != nullptr, u"native-iv photo block"_q, ok);
	if (photo) {
		Check(
			photo->text.text == u"Photo caption\nPhoto credit"_q,
			u"native-iv photo caption text"_q,
			ok);
		Check(photo->photo.photoId == 9001, u"native-iv photo id"_q, ok);
		Check(
			photo->photo.width == 640 && photo->photo.height == 360,
			u"native-iv photo aspect metadata"_q,
			ok);
		Check(
			photo->photo.urlOverride == u"https://example.com/photo"_q,
			u"native-iv photo url override"_q,
			ok);
		Check(photo->photo.viewerOpen, u"native-iv photo viewer flag"_q, ok);
	}

	Check(video != nullptr, u"native-iv video block"_q, ok);
	if (video) {
		Check(
			video->text.text == videoFixture.caption,
			u"native-iv video caption text"_q,
			ok);
		Check(
			video->video.media.kind == PreparedMediaItemKind::Document,
			u"native-iv video media kind"_q,
			ok);
		Check(video->video.media.id == 7001, u"native-iv video id"_q, ok);
		Check(
			video->video.media.width == 1280
				&& video->video.media.height == 720,
			u"native-iv video dimensions"_q,
			ok);
	}

	Check(audio != nullptr, u"native-iv audio block"_q, ok);
	if (audio) {
		Check(
			audio->text.text == audioFixture.caption,
			u"native-iv audio caption text"_q,
			ok);
		Check(audio->audio.documentId == 7002, u"native-iv audio id"_q, ok);
		Check(audio->audio.title == u"Song Title"_q, u"native-iv audio title"_q, ok);
		Check(
			audio->audio.performer == u"Sample Artist"_q,
			u"native-iv audio performer"_q,
			ok);
		Check(
			audio->audio.fileName == u"track.mp3"_q,
			u"native-iv audio file name"_q,
			ok);
		Check(audio->audio.duration == 215, u"native-iv audio duration"_q, ok);
	}

	Check(map != nullptr, u"native-iv map block"_q, ok);
	if (map) {
		Check(
			map->text.text == mapFixture.caption,
			u"native-iv map caption text"_q,
			ok);
		Check(
			map->map.latitude == 51.5007
				&& map->map.longitude == -0.1246,
			u"native-iv map coordinates"_q,
			ok);
		Check(map->map.accessHash == 880088, u"native-iv map access hash"_q, ok);
		Check(
			map->map.width == 320 && map->map.height == 180,
			u"native-iv map dimensions"_q,
			ok);
		Check(map->map.zoom == 13, u"native-iv map zoom"_q, ok);
		Check(!map->map.url.isEmpty(), u"native-iv map url"_q, ok);
	}

	Check(channel != nullptr, u"native-iv channel block"_q, ok);
	if (channel) {
		Check(channel->text.text.isEmpty(), u"native-iv channel text empty"_q, ok);
		Check(
			channel->channel.channelId == 7006,
			u"native-iv channel id"_q,
			ok);
		Check(
			channel->channel.title == u"Native IV Channel"_q,
			u"native-iv channel title"_q,
			ok);
		Check(
			channel->channel.username == u"nativeiv"_q,
			u"native-iv channel username"_q,
			ok);
	}
	const auto unresolvedChannelLabel = u"native-iv-channel-unresolved"_q;
	const auto unresolvedChannelContext = SerializeTestNativeIvChannelContext(
		7006,
		u"nativeiv"_q);
	const auto parsedChannelContext = ParseTestNativeIvChannelContext(
		unresolvedChannelContext);
	Check(
		parsedChannelContext.channelId == 7006,
		unresolvedChannelLabel + u" parsed id"_q,
		ok);
	Check(
		parsedChannelContext.username == u"nativeiv"_q,
		unresolvedChannelLabel + u" parsed username"_q,
		ok);
	Check(
		ResolveTestNativeIvChannelUsername(
			QString(),
			parsedChannelContext.username) == u"nativeiv"_q,
		unresolvedChannelLabel
			+ u" unresolved local peer uses cached-page username"_q,
		ok);
	Check(
		ResolveTestNativeIvChannelUsername(
			u"localnativeiv"_q,
			parsedChannelContext.username) == u"localnativeiv"_q,
		unresolvedChannelLabel + u" loaded local username wins"_q,
		ok);

	Check(embedPost != nullptr, u"native-iv embed-post block"_q, ok);
	if (embedPost) {
		Check(
			embedPost->embedPost.url == u"https://example.com/embed-post"_q,
			u"native-iv embed-post url"_q,
			ok);
		Check(
			embedPost->embedPost.authorPhotoId == kNativeIvEmbedPostAuthorPhotoId,
			u"native-iv embed-post author photo id"_q,
			ok);
		Check(
			embedPost->embedPost.author == u"Author"_q,
			u"native-iv embed-post author"_q,
			ok);
		Check(
			embedPost->embedPost.dateText
				== NativeIvEmbedPostDateText(kNativeIvEmbedPostDate),
			u"native-iv embed-post date"_q,
			ok);
		Check(
			embedPost->text.text == u"Embed post caption\nEmbed post credit"_q,
			u"native-iv embed-post caption text"_q,
			ok);
		Check(
			embedPost->children.size() == 1,
			u"native-iv embed-post child count"_q,
			ok);
		if (embedPost->children.size() == 1) {
			const auto &child = embedPost->children.front();
			const auto ivOffset = int(child.text.text.indexOf(u"instant view"_q));
			const auto externalOffset = int(child.text.text.indexOf(u"external"_q));
			Check(
				child.kind == PreparedBlockKind::Paragraph,
				u"native-iv embed-post child paragraph"_q,
				ok);
			Check(
				child.text.text == u"Links: instant view and external"_q,
				u"native-iv embed-post child text"_q,
				ok);
			const auto ivLink = (ivOffset >= 0)
				? FindPreparedLinkByCustomUrlRange(
					child.text,
					child.links,
					ivOffset,
					int(u"instant view"_q.size()))
				: nullptr;
			Check(
				ivLink != nullptr,
				u"native-iv embed-post iv link"_q,
				ok);
			if (ivLink) {
				Check(
					ivLink->kind == PreparedLinkKind::InstantViewPage,
					u"native-iv embed-post iv link kind"_q,
					ok);
				Check(
					ivLink->target == u"https://telegra.ph/embed-post-link"_q,
					u"native-iv embed-post iv link target"_q,
					ok);
				Check(
					ivLink->webpageId == 777,
					u"native-iv embed-post iv link webpage id"_q,
					ok);
			}
			const auto externalLink = (externalOffset >= 0)
				? FindPreparedLinkByCustomUrlRange(
					child.text,
					child.links,
					externalOffset,
					int(QString(u"external"_q).size()))
				: nullptr;
			Check(
				externalLink != nullptr,
				u"native-iv embed-post external link"_q,
				ok);
			if (externalLink) {
				Check(
					externalLink->kind == PreparedLinkKind::External,
					u"native-iv embed-post external link kind"_q,
					ok);
				Check(
					externalLink->target == u"https://example.com/external-link"_q,
					u"native-iv embed-post external link target"_q,
					ok);
			}
		}
	}

	Check(collage != nullptr, u"native-iv collage block"_q, ok);
	if (collage) {
		Check(
			collage->groupedMedia.items.size() == 2,
			u"native-iv collage item count"_q,
			ok);
		Check(
			collage->text.text == collageFixture.caption,
			u"native-iv collage caption text"_q,
			ok);
		if (collage->groupedMedia.items.size() == 2) {
			Check(
				collage->groupedMedia.items[0].media.kind
					== PreparedMediaItemKind::Photo,
				u"native-iv collage first item kind"_q,
				ok);
			Check(
				collage->groupedMedia.items[0].media.id == 9102,
				u"native-iv collage first item id"_q,
				ok);
			Check(
				collage->groupedMedia.items[1].media.kind
					== PreparedMediaItemKind::Document,
				u"native-iv collage second item kind"_q,
				ok);
			Check(
				collage->groupedMedia.items[1].media.id == 7003,
				u"native-iv collage second item id"_q,
				ok);
		}
	}

	Check(slideshow != nullptr, u"native-iv slideshow block"_q, ok);
	if (slideshow) {
		Check(
			slideshow->groupedMedia.items.size() == 2,
			u"native-iv slideshow item count"_q,
			ok);
		Check(
			slideshow->text.text == slideshowFixture.caption,
			u"native-iv slideshow caption text"_q,
			ok);
		if (slideshow->groupedMedia.items.size() == 2) {
			Check(
				slideshow->groupedMedia.items[0].media.kind
					== PreparedMediaItemKind::Document,
				u"native-iv slideshow first item kind"_q,
				ok);
			Check(
				slideshow->groupedMedia.items[0].media.id == 7004,
				u"native-iv slideshow first item id"_q,
				ok);
			Check(
				slideshow->groupedMedia.items[1].media.kind
					== PreparedMediaItemKind::Document,
				u"native-iv slideshow second item kind"_q,
				ok);
			Check(
				slideshow->groupedMedia.items[1].media.id == 7005,
				u"native-iv slideshow second item id"_q,
				ok);
		}
	}

	Check(
		coveredPhoto != nullptr,
		u"native-iv covered photo unwrapped"_q,
		ok);
	if (coveredPhoto) {
		Check(
			coveredPhoto->text.text == u"Cover caption"_q,
			u"native-iv covered photo caption"_q,
			ok);
		Check(
			coveredPhoto->photo.photoId == 9002,
			u"native-iv covered photo id"_q,
			ok);
		Check(
			coveredPhoto->photo.width == 320
				&& coveredPhoto->photo.height == 200,
			u"native-iv covered photo size"_q,
			ok);
	}

	const auto mergedTableLabel = u"native-iv merged-table prepare"_q;
	auto mergedTableSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Merged native table"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(
						u"Stub"_q,
						1,
						2,
						true),
					NativeIvTableCell(
						u"A"_q,
						1,
						1,
						true,
						TableAlignment::Center),
					NativeIvTableCell(
						u"B"_q,
						1,
						1,
						true,
						TableAlignment::Right),
				}),
				NativeIvTableRow({
					NativeIvTableCell(
						u"Bottom merged"_q,
						2,
						2,
						false,
						TableAlignment::Center,
						PreparedTableCellVerticalAlignment::Bottom),
				}),
				NativeIvTableRow({
					NativeIvTableCell(
						u"Tail"_q,
						1,
						1,
						false,
						TableAlignment::Left),
				}),
			},
			true,
			true),
	});
	const auto mergedTablePrepared = TryPrepareNativeInstantView({
		.source = &mergedTableSource,
	});
	Check(
		mergedTablePrepared.supported(),
		mergedTableLabel + u" prepare supported"_q,
		ok);
	Check(
		!mergedTablePrepared.content.failure.failed(),
		mergedTableLabel + u" prepare failure"_q,
		ok);
	Check(
		mergedTablePrepared.content.blocks.blocks.size() == 1,
		mergedTableLabel + u" prepared block count"_q,
		ok);
	if (mergedTablePrepared.supported()
		&& !mergedTablePrepared.content.failure.failed()
		&& (mergedTablePrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = mergedTablePrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			mergedTableLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				table.text.text == u"Merged native table"_q,
				mergedTableLabel + u" caption text"_q,
				ok);
			Check(
				table.tableBordered && table.tableStriped,
				mergedTableLabel + u" table flags"_q,
				ok);
			Check(
				table.tableColumnCount == 3,
				mergedTableLabel + u" resolved column count"_q,
				ok);
			Check(
				table.tableRows.size() == 3,
				mergedTableLabel + u" prepared row count"_q,
				ok);
			if (table.tableRows.size() == 3) {
				Check(
					table.tableRows[0].header,
					mergedTableLabel + u" header row state"_q,
					ok);
				Check(
					!table.tableRows[1].header
						&& !table.tableRows[2].header,
					mergedTableLabel + u" body row state"_q,
					ok);
				Check(
					table.tableRows[0].cells.size() == 3
						&& table.tableRows[1].cells.size() == 1
						&& table.tableRows[2].cells.size() == 1,
					mergedTableLabel + u" logical origin cell counts"_q,
					ok);
				if ((table.tableRows[0].cells.size() == 3)
					&& (table.tableRows[1].cells.size() == 1)
					&& (table.tableRows[2].cells.size() == 1)) {
					const auto &stub = table.tableRows[0].cells[0];
					const auto &headerCenter = table.tableRows[0].cells[1];
					const auto &headerRight = table.tableRows[0].cells[2];
					const auto &bottomMerged = table.tableRows[1].cells[0];
					const auto &tail = table.tableRows[2].cells[0];
					Check(
						stub.text.text == u"Stub"_q
							&& (stub.column == 0)
							&& stub.header
							&& (stub.alignment == TableAlignment::Left)
							&& (stub.verticalAlignment
								== PreparedTableCellVerticalAlignment::Top)
							&& (stub.colspan == 1)
							&& (stub.rowspan == 2),
						mergedTableLabel + u" stub metadata"_q,
						ok);
					Check(
						headerCenter.text.text == u"A"_q
							&& (headerCenter.column == 1)
							&& headerCenter.header
							&& (headerCenter.alignment == TableAlignment::Center)
							&& (headerCenter.verticalAlignment
								== PreparedTableCellVerticalAlignment::Top)
							&& (headerCenter.colspan == 1)
							&& (headerCenter.rowspan == 1),
						mergedTableLabel + u" center header metadata"_q,
						ok);
					Check(
						headerRight.text.text == u"B"_q
							&& (headerRight.column == 2)
							&& headerRight.header
							&& (headerRight.alignment == TableAlignment::Right)
							&& (headerRight.verticalAlignment
								== PreparedTableCellVerticalAlignment::Top)
							&& (headerRight.colspan == 1)
							&& (headerRight.rowspan == 1),
						mergedTableLabel + u" right header metadata"_q,
						ok);
					Check(
						bottomMerged.text.text == u"Bottom merged"_q
							&& (bottomMerged.column == 1)
							&& !bottomMerged.header
							&& (bottomMerged.alignment
								== TableAlignment::Center)
							&& (bottomMerged.verticalAlignment
								== PreparedTableCellVerticalAlignment::Bottom)
							&& (bottomMerged.colspan == 2)
							&& (bottomMerged.rowspan == 2),
						mergedTableLabel + u" merged body metadata"_q,
						ok);
					Check(
						tail.text.text == u"Tail"_q
							&& (tail.column == 0)
							&& !tail.header
							&& (tail.alignment == TableAlignment::Left)
							&& (tail.verticalAlignment
								== PreparedTableCellVerticalAlignment::Top)
							&& (tail.colspan == 1)
							&& (tail.rowspan == 1),
						mergedTableLabel + u" tail metadata"_q,
						ok);
				}
			}
		}
	}

	const auto invalidTableLabel = u"native-iv invalid-table salvage"_q;
	const auto invalidTableTitleAnchor = u"invalid-table-title"_q;
	auto invalidTableSource = NativeIvSource(QVector<MTPPageBlock>{
		MTP_pageBlockTable(
			MTP_flags(MTPDpageBlockTable::Flag::f_bordered),
			MTP_textAnchor(
				NativeIvText(u"Invalid table"_q),
				MTP_string(invalidTableTitleAnchor)),
			MTP_vector<MTPPageTableRow>(QVector<MTPPageTableRow>{
				NativeIvTableRow({
					NativeIvTableCell(
						u"Too tall"_q,
						1,
						3),
				}),
				NativeIvTableRow({
					NativeIvTableCell(u"Tail"_q),
				}),
			})),
	});
	const auto invalidTablePrepared = TryPrepareNativeInstantView({
		.source = &invalidTableSource,
	});
	Check(
		invalidTablePrepared.supported(),
		invalidTableLabel + u" prepare supported"_q,
		ok);
	Check(
		!invalidTablePrepared.content.failure.failed(),
		invalidTableLabel + u" prepare failure"_q,
		ok);
	Check(
		invalidTablePrepared.content.blocks.blocks.size() == 1,
		invalidTableLabel + u" block count"_q,
		ok);
	if (invalidTablePrepared.supported()
		&& !invalidTablePrepared.content.failure.failed()
		&& (invalidTablePrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = invalidTablePrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			invalidTableLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				table.text.text == u"Invalid table"_q,
				invalidTableLabel + u" title text"_q,
				ok);
			Check(
				table.anchorId == invalidTableTitleAnchor,
				invalidTableLabel + u" title anchor"_q,
				ok);
			Check(
				table.tableColumnCount == 2,
				invalidTableLabel + u" resolved column count"_q,
				ok);
			Check(
				table.tableRows.size() == 2,
				invalidTableLabel + u" row count"_q,
				ok);
			if (table.tableRows.size() == 2) {
				Check(
					(table.tableRows[0].cells.size() == 1)
						&& (table.tableRows[1].cells.size() == 1),
					invalidTableLabel + u" logical origin cell counts"_q,
					ok);
				if ((table.tableRows[0].cells.size() == 1)
					&& (table.tableRows[1].cells.size() == 1)) {
					const auto &tooTall = table.tableRows[0].cells[0];
					const auto &tail = table.tableRows[1].cells[0];
					Check(
						tooTall.text.text == u"Too tall"_q
							&& (tooTall.column == 0)
							&& (tooTall.colspan == 1)
							&& (tooTall.rowspan == 2),
						invalidTableLabel + u" clamped rowspan metadata"_q,
						ok);
					Check(
						tail.text.text == u"Tail"_q
							&& (tail.column == 1)
							&& (tail.colspan == 1)
							&& (tail.rowspan == 1),
						invalidTableLabel + u" trailing cell placement"_q,
						ok);
				}
			}
		}
	}

	const auto crashTableLabel = u"native-iv short-row-occupancy salvage"_q;
	auto crashTableSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Short row occupancy"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(u"A"_q, 1, 4),
					NativeIvTableCell(u"B"_q, 1, 4),
				}),
				NativeIvTableRow({}),
				NativeIvTableRow({
					NativeIvTableCell(u"C"_q),
					NativeIvTableCell(u"D"_q),
					NativeIvTableCell(u"E"_q, 1, 2),
				}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
				NativeIvTableRow({}),
			},
			false,
			false),
	});
	const auto crashTablePrepared = TryPrepareNativeInstantView({
		.source = &crashTableSource,
	});
	Check(
		crashTablePrepared.supported(),
		crashTableLabel + u" prepare supported"_q,
		ok);
	Check(
		!crashTablePrepared.content.failure.failed(),
		crashTableLabel + u" prepare failure"_q,
		ok);
	Check(
		crashTablePrepared.content.blocks.blocks.size() == 1,
		crashTableLabel + u" block count"_q,
		ok);
	if (crashTablePrepared.supported()
		&& !crashTablePrepared.content.failure.failed()
		&& (crashTablePrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = crashTablePrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			crashTableLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				table.text.text == u"Short row occupancy"_q,
				crashTableLabel + u" title text"_q,
				ok);
			Check(
				table.tableColumnCount == 5,
				crashTableLabel + u" resolved column count"_q,
				ok);
			Check(
				table.tableRows.size() == 10,
				crashTableLabel + u" row count"_q,
				ok);
			if (table.tableRows.size() == 10) {
				Check(
					(table.tableRows[0].cells.size() == 2)
						&& table.tableRows[1].cells.empty()
						&& (table.tableRows[2].cells.size() == 3)
						&& table.tableRows[3].cells.empty()
						&& table.tableRows[4].cells.empty()
						&& table.tableRows[5].cells.empty()
						&& table.tableRows[6].cells.empty()
						&& table.tableRows[7].cells.empty()
						&& table.tableRows[8].cells.empty()
						&& table.tableRows[9].cells.empty(),
					crashTableLabel + u" sparse row occupancy"_q,
					ok);
				if ((table.tableRows[0].cells.size() == 2)
					&& (table.tableRows[2].cells.size() == 3)) {
					const auto &a = table.tableRows[0].cells[0];
					const auto &b = table.tableRows[0].cells[1];
					const auto &c = table.tableRows[2].cells[0];
					const auto &d = table.tableRows[2].cells[1];
					const auto &e = table.tableRows[2].cells[2];
					Check(
						a.text.text == u"A"_q
							&& (a.column == 0)
							&& (a.rowspan == 4),
						crashTableLabel + u" left carry metadata"_q,
						ok);
					Check(
						b.text.text == u"B"_q
							&& (b.column == 1)
							&& (b.rowspan == 4),
						crashTableLabel + u" right carry metadata"_q,
						ok);
					Check(
						c.text.text == u"C"_q
							&& (c.column == 2)
							&& (c.colspan == 1)
							&& (c.rowspan == 1),
						crashTableLabel + u" third-column salvage"_q,
						ok);
					Check(
						d.text.text == u"D"_q
							&& (d.column == 3)
							&& (d.colspan == 1)
							&& (d.rowspan == 1),
						crashTableLabel + u" fourth-column salvage"_q,
						ok);
					Check(
						e.text.text == u"E"_q
							&& (e.column == 4)
							&& (e.colspan == 1)
							&& (e.rowspan == 2),
						crashTableLabel + u" crash-shape trailing placement"_q,
						ok);
				}
			}
		}
	}

	const auto spanNormalizationLabel = u"native-iv span-normalization salvage"_q;
	auto spanNormalizationSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Span normalization"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(u"Zero"_q, 0, 0),
					NativeIvTableCell(u"Negative"_q, -2, -3),
				}),
			},
			false,
			false),
	});
	const auto spanNormalizationPrepared = TryPrepareNativeInstantView({
		.source = &spanNormalizationSource,
	});
	Check(
		spanNormalizationPrepared.supported(),
		spanNormalizationLabel + u" prepare supported"_q,
		ok);
	Check(
		!spanNormalizationPrepared.content.failure.failed(),
		spanNormalizationLabel + u" prepare failure"_q,
		ok);
	Check(
		spanNormalizationPrepared.content.blocks.blocks.size() == 1,
		spanNormalizationLabel + u" block count"_q,
		ok);
	if (spanNormalizationPrepared.supported()
		&& !spanNormalizationPrepared.content.failure.failed()
		&& (spanNormalizationPrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = spanNormalizationPrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			spanNormalizationLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				(table.tableColumnCount == 2)
					&& (table.tableRows.size() == 1)
					&& (table.tableRows[0].cells.size() == 2),
				spanNormalizationLabel + u" normalized table shape"_q,
				ok);
			if ((table.tableRows.size() == 1)
				&& (table.tableRows[0].cells.size() == 2)) {
				const auto &zero = table.tableRows[0].cells[0];
				const auto &negative = table.tableRows[0].cells[1];
				Check(
					zero.text.text == u"Zero"_q
						&& (zero.column == 0)
						&& (zero.colspan == 1)
						&& (zero.rowspan == 1),
					spanNormalizationLabel + u" zero span normalization"_q,
					ok);
				Check(
					negative.text.text == u"Negative"_q
						&& (negative.column == 1)
						&& (negative.colspan == 1)
						&& (negative.rowspan == 1),
					spanNormalizationLabel + u" negative span normalization"_q,
					ok);
			}
		}
	}

	const auto rowspanOverflowLabel = u"native-iv rowspan-overflow salvage"_q;
	auto rowspanOverflowSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Rowspan overflow"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(u"Tall"_q, 1, 5),
				}),
				NativeIvTableRow({}),
				NativeIvTableRow({
					NativeIvTableCell(u"Tail"_q),
				}),
			},
			false,
			false),
	});
	const auto rowspanOverflowPrepared = TryPrepareNativeInstantView({
		.source = &rowspanOverflowSource,
	});
	Check(
		rowspanOverflowPrepared.supported(),
		rowspanOverflowLabel + u" prepare supported"_q,
		ok);
	Check(
		!rowspanOverflowPrepared.content.failure.failed(),
		rowspanOverflowLabel + u" prepare failure"_q,
		ok);
	Check(
		rowspanOverflowPrepared.content.blocks.blocks.size() == 1,
		rowspanOverflowLabel + u" block count"_q,
		ok);
	if (rowspanOverflowPrepared.supported()
		&& !rowspanOverflowPrepared.content.failure.failed()
		&& (rowspanOverflowPrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = rowspanOverflowPrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			rowspanOverflowLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				(table.tableColumnCount == 2)
					&& (table.tableRows.size() == 3),
				rowspanOverflowLabel + u" salvaged table shape"_q,
				ok);
			if (table.tableRows.size() == 3) {
				Check(
					(table.tableRows[0].cells.size() == 1)
						&& table.tableRows[1].cells.empty()
						&& (table.tableRows[2].cells.size() == 1),
					rowspanOverflowLabel + u" sparse row layout"_q,
					ok);
				if ((table.tableRows[0].cells.size() == 1)
					&& (table.tableRows[2].cells.size() == 1)) {
					const auto &tall = table.tableRows[0].cells[0];
					const auto &tail = table.tableRows[2].cells[0];
					Check(
						tall.text.text == u"Tall"_q
							&& (tall.column == 0)
							&& (tall.colspan == 1)
							&& (tall.rowspan == 3),
						rowspanOverflowLabel + u" clamped overflow rowspan"_q,
						ok);
					Check(
						tail.text.text == u"Tail"_q
							&& (tail.column == 1)
							&& (tail.colspan == 1)
							&& (tail.rowspan == 1),
						rowspanOverflowLabel + u" tail placement after clamp"_q,
						ok);
				}
			}
		}
	}

	const auto oversizedSpanLabel = u"native-iv oversized-span salvage"_q;
	const auto &tableLimits = PrepareTableRenderLimitsForIv();
	auto oversizedSpanSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Oversized span table"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(
						u"Too wide"_q,
						tableLimits.maxColumns + 1),
				}),
			}),
	});
	const auto oversizedSpanPrepared = TryPrepareNativeInstantView({
		.source = &oversizedSpanSource,
	});
	Check(
		oversizedSpanPrepared.supported(),
		oversizedSpanLabel + u" prepare supported"_q,
		ok);
	Check(
		!oversizedSpanPrepared.content.failure.failed(),
		oversizedSpanLabel + u" prepare failure"_q,
		ok);
	Check(
		oversizedSpanPrepared.content.blocks.blocks.size() == 1,
		oversizedSpanLabel + u" block count"_q,
		ok);
	if (oversizedSpanPrepared.supported()
		&& !oversizedSpanPrepared.content.failure.failed()
		&& (oversizedSpanPrepared.content.blocks.blocks.size() == 1)) {
		const auto &table = oversizedSpanPrepared.content.blocks.blocks.front();
		Check(
			table.kind == PreparedBlockKind::Table,
			oversizedSpanLabel + u" prepared block kind"_q,
			ok);
		if (table.kind == PreparedBlockKind::Table) {
			Check(
				table.text.text == u"Oversized span table"_q,
				oversizedSpanLabel + u" title text"_q,
				ok);
			Check(
				table.tableColumnCount == tableLimits.maxColumns,
				oversizedSpanLabel + u" clamped column count"_q,
				ok);
			Check(
				(table.tableRows.size() == 1)
					&& (table.tableRows[0].cells.size() == 1),
				oversizedSpanLabel + u" logical origin cell counts"_q,
				ok);
			if ((table.tableRows.size() == 1)
				&& (table.tableRows[0].cells.size() == 1)) {
				const auto &wide = table.tableRows[0].cells[0];
				Check(
					wide.text.text == u"Too wide"_q
						&& (wide.column == 0)
						&& (wide.colspan == tableLimits.maxColumns)
						&& (wide.rowspan == 1),
					oversizedSpanLabel + u" clamped colspan metadata"_q,
					ok);
			}
		}
	}

	Check(
		preparedPlaceholders.size() == placeholderFixtures.size(),
		u"native-iv placeholder prepared count"_q,
		ok);
	Check(
		supported.content.embedHtmlResources.size() == placeholderFixtures.size(),
		u"native-iv placeholder embed resource count"_q,
		ok);
	for (const auto &fixture : placeholderFixtures) {
		const auto it = std::find_if(
			preparedPlaceholders.begin(),
			preparedPlaceholders.end(),
			[&](const PreparedBlock *block) {
				return block
					&& block->placeholder.label == fixture.expectedLabel
					&& block->text.text == fixture.caption;
			});
		Check(
			it != preparedPlaceholders.end(),
			fixture.expectedLabel + u" prepared placeholder"_q,
			ok);
		if (it != preparedPlaceholders.end()) {
			const auto *block = *it;
			Check(
				block->placeholder.copyText
					== (fixture.expectedLabel + u"\n"_q + fixture.caption),
				fixture.expectedLabel + u" placeholder copy text"_q,
				ok);
			Check(
				bool(block->placeholder.id),
				fixture.expectedLabel + u" placeholder id"_q,
				ok);
			Check(
				block->placeholder.embed.has_value(),
				fixture.expectedLabel + u" placeholder embed metadata"_q,
				ok);
			if (block->placeholder.embed) {
				const auto &embed = *block->placeholder.embed;
				preparedPlaceholderIds.emplace_back(
					embed.fallbackUrl,
					block->placeholder.id);
				Check(
					!embed.resourceId.isEmpty(),
					fixture.expectedLabel + u" placeholder embed resource id"_q,
					ok);
				Check(
					embed.fallbackUrl == fixture.expectedFallbackUrl,
					fixture.expectedLabel + u" placeholder embed fallback url"_q,
					ok);
				Check(
					embed.width == fixture.expectedWidth
						&& embed.height == fixture.expectedHeight,
					fixture.expectedLabel + u" placeholder embed size"_q,
					ok);
				Check(
					embed.fullWidth == fixture.expectedFullWidth,
					fixture.expectedLabel + u" placeholder embed full-width"_q,
					ok);
				Check(
					embed.allowScrolling == fixture.expectedAllowScrolling,
					fixture.expectedLabel + u" placeholder embed scrolling"_q,
					ok);
				Check(
					supported.content.embedHtmlResources.find(embed.resourceId)
						!= supported.content.embedHtmlResources.end(),
					fixture.expectedLabel + u" placeholder embed resource stored"_q,
					ok);
				const auto resource = supported.content.embedHtmlResources.find(
					embed.resourceId);
				if (resource != supported.content.embedHtmlResources.end()) {
					Check(
						resource->second.contains("window.external.invoke"),
						fixture.expectedLabel
							+ u" embed wrapper native invoke bridge"_q,
						ok);
					Check(
						resource->second.contains("resize_frame"),
						fixture.expectedLabel
							+ u" embed wrapper resize frame bridge"_q,
						ok);
					Check(
						resource->second.contains("preferred_size"),
						fixture.expectedLabel
							+ u" embed wrapper preferred size event"_q,
						ok);
					Check(
						!resource->second.contains("padding-bottom:"),
						fixture.expectedLabel
							+ u" embed wrapper removed iframe spacer"_q,
						ok);
				}
			}
		}
	}

	const auto emptyEmbedPostLabel = u"native-iv-embed-post-empty-body"_q;
	const auto emptyEmbedPostUrl = u"https://example.com/embed-post-empty"_q;
	const auto emptyEmbedPostDate = kNativeIvEmbedPostDate + 3600;
	auto emptyEmbedPostSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvEmbedPostBlock(
			{},
			u"Empty embed post caption"_q,
			QString(),
			emptyEmbedPostUrl,
			u"Fallback Author"_q,
			emptyEmbedPostDate),
	});
	const auto emptyEmbedPostPrepared = TryPrepareNativeInstantView({
		.source = &emptyEmbedPostSource,
	});
	Check(
		emptyEmbedPostPrepared.supported(),
		emptyEmbedPostLabel + u" prepare supported"_q,
		ok);
	Check(
		!emptyEmbedPostPrepared.content.failure.failed(),
		emptyEmbedPostLabel + u" prepare failure"_q,
		ok);
	Check(
		emptyEmbedPostPrepared.content.embedHtmlResources.empty(),
		emptyEmbedPostLabel + u" embed resources empty"_q,
		ok);
	Check(
		emptyEmbedPostPrepared.content.blocks.blocks.size() == 1,
		emptyEmbedPostLabel + u" block count"_q,
		ok);
	if (emptyEmbedPostPrepared.supported()
		&& !emptyEmbedPostPrepared.content.failure.failed()
		&& (emptyEmbedPostPrepared.content.blocks.blocks.size() == 1)) {
		const auto &block = emptyEmbedPostPrepared.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::EmbedPost,
			emptyEmbedPostLabel + u" prepared block kind"_q,
			ok);
		Check(
			block.embedPost.author == u"Fallback Author"_q,
			emptyEmbedPostLabel + u" author"_q,
			ok);
		Check(
			block.embedPost.dateText
				== NativeIvEmbedPostDateText(emptyEmbedPostDate),
			emptyEmbedPostLabel + u" date"_q,
			ok);
		Check(
			block.text.text == u"Empty embed post caption"_q,
			emptyEmbedPostLabel + u" caption"_q,
			ok);
		Check(
			block.children.size() == 1,
			emptyEmbedPostLabel + u" fallback child count"_q,
			ok);
		if (block.children.size() == 1) {
			const auto &child = block.children.front();
			Check(
				child.kind == PreparedBlockKind::Paragraph,
				emptyEmbedPostLabel + u" fallback child kind"_q,
				ok);
			Check(
				child.text.text == emptyEmbedPostUrl,
				emptyEmbedPostLabel + u" fallback child text"_q,
				ok);
			const auto fallbackLink = FindPreparedLinkByCustomUrlRange(
				child.text,
				child.links,
				0,
				child.text.text.size());
			Check(
				fallbackLink != nullptr,
				emptyEmbedPostLabel + u" fallback child link"_q,
				ok);
			if (fallbackLink) {
				Check(
					fallbackLink->kind == PreparedLinkKind::External,
					emptyEmbedPostLabel + u" fallback link kind"_q,
					ok);
				Check(
					fallbackLink->target == emptyEmbedPostUrl,
					emptyEmbedPostLabel + u" fallback link target"_q,
					ok);
			}
		}
	}

	const auto mediaEmbedPostLabel = u"native-iv-embed-post-native-child-media"_q;
	auto mediaEmbedPostSource = NativeIvSource(
		QVector<MTPPageBlock>{
			NativeIvEmbedPostBlock(
				QVector<MTPPageBlock>{
					NativeIvPhotoBlock(9201, u"Nested photo caption"_q),
				},
				u"Media embed post caption"_q,
				QString(),
				u"https://example.com/embed-post-media"_q,
				u"Media Author"_q,
				kNativeIvEmbedPostDate + 7200),
		},
		QVector<MTPPhoto>{
			NativeIvPhoto(9201, 320, 180),
		});
	const auto mediaEmbedPostPrepared = TryPrepareNativeInstantView({
		.source = &mediaEmbedPostSource,
	});
	Check(
		mediaEmbedPostPrepared.supported(),
		mediaEmbedPostLabel + u" prepare supported"_q,
		ok);
	Check(
		!mediaEmbedPostPrepared.content.failure.failed(),
		mediaEmbedPostLabel + u" prepare failure"_q,
		ok);
	Check(
		mediaEmbedPostPrepared.content.embedHtmlResources.empty(),
		mediaEmbedPostLabel + u" embed resources empty"_q,
		ok);
	Check(
		CollectPreparedBlocksByKind(
			mediaEmbedPostPrepared.content.blocks.blocks,
			PreparedBlockKind::Placeholder).empty(),
		mediaEmbedPostLabel + u" no placeholder children"_q,
		ok);
	Check(
		mediaEmbedPostPrepared.content.blocks.blocks.size() == 1,
		mediaEmbedPostLabel + u" block count"_q,
		ok);
	if (mediaEmbedPostPrepared.supported()
		&& !mediaEmbedPostPrepared.content.failure.failed()
		&& (mediaEmbedPostPrepared.content.blocks.blocks.size() == 1)) {
		const auto &block = mediaEmbedPostPrepared.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::EmbedPost,
			mediaEmbedPostLabel + u" prepared block kind"_q,
			ok);
		Check(
			block.children.size() == 1,
			mediaEmbedPostLabel + u" child count"_q,
			ok);
		if (block.children.size() == 1) {
			const auto &child = block.children.front();
			Check(
				child.kind == PreparedBlockKind::Photo,
				mediaEmbedPostLabel + u" native photo child kind"_q,
				ok);
			Check(
				child.photo.photoId == 9201,
				mediaEmbedPostLabel + u" native photo child id"_q,
				ok);
			Check(
				child.text.text == u"Nested photo caption"_q,
				mediaEmbedPostLabel + u" native photo child caption"_q,
				ok);
		}
	}

	const auto placeholderArticleLabel = u"native-iv-embed-placeholders-article"_q;
	auto placeholderArticleSource = NativeIvSource(QVector<MTPPageBlock>{
		placeholderFixtures.front().block,
	});
	const auto placeholderArticlePrepared = TryPrepareNativeInstantView({
		.source = &placeholderArticleSource,
	});
	Check(
		placeholderArticlePrepared.supported(),
		placeholderArticleLabel + u" prepare supported"_q,
		ok);
	Check(
		!placeholderArticlePrepared.content.failure.failed(),
		placeholderArticleLabel + u" prepare failure"_q,
		ok);
	if (placeholderArticlePrepared.supported()
		&& !placeholderArticlePrepared.content.failure.failed()) {
		auto placeholderArticleRenderer = std::make_shared<MathRenderer>();
		auto placeholderArticleHeight = 0;
		auto placeholderArticle = BuildArticleForTest(
			std::move(placeholderArticlePrepared.content),
			placeholderArticleRenderer,
			420,
			&placeholderArticleHeight);
		auto lookupFlags = Ui::Text::StateRequest::Flags();
		lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
		for (const auto &fixture : placeholderFixtures) {
			const auto bounds = HitBoundsWhere(
				placeholderArticle.get(),
				420,
				placeholderArticleHeight,
				lookupFlags,
				[&](const MarkdownArticleHitTestResult &hit) {
					return hit.mediaActivation.kind == MediaActivationKind::Embed
						&& hit.mediaActivation.embed.fallbackUrl
							== fixture.expectedFallbackUrl;
				});
			Check(
				bounds.has_value(),
				fixture.expectedLabel + u" article embed hit bounds"_q,
				ok);
			if (bounds) {
				const auto hit = placeholderArticle->hitTest(
					bounds->center(),
					lookupFlags);
				const auto preparedId = std::find_if(
					preparedPlaceholderIds.begin(),
					preparedPlaceholderIds.end(),
					[&](const auto &entry) {
						return entry.first == fixture.expectedFallbackUrl;
					});
				Check(
					hit.mediaActivation.kind == MediaActivationKind::Embed,
					fixture.expectedLabel + u" article embed activation kind"_q,
					ok);
				Check(
					hit.mediaActivation.embed.fallbackUrl
						== fixture.expectedFallbackUrl,
					fixture.expectedLabel + u" article embed activation fallback url"_q,
					ok);
				Check(
					hit.mediaActivation.embed.allowScrolling
						== fixture.expectedAllowScrolling,
					fixture.expectedLabel + u" article embed activation scrolling"_q,
					ok);
				Check(
					bool(hit.mediaActivation.placeholderId),
					fixture.expectedLabel + u" article embed placeholder id"_q,
					ok);
				Check(
					(preparedId != preparedPlaceholderIds.end())
						&& (hit.mediaActivation.placeholderId.value
							== preparedId->second.value),
					fixture.expectedLabel + u" article embed placeholder id match"_q,
					ok);
			}
		}
	}

	const auto makePreviewEmbedBlock = [](
			QString url,
			QString caption,
			bool fullWidth = false) {
		auto flags = MTPDpageBlockEmbed::Flag::f_allow_scrolling
			| MTPDpageBlockEmbed::Flag::f_url
			| MTPDpageBlockEmbed::Flag::f_w;
		if (fullWidth) {
			flags |= MTPDpageBlockEmbed::Flag::f_full_width;
		}
		return MTP_pageBlockEmbed(
			MTP_flags(flags),
			MTP_string(url),
			MTP_string(),
			MTP_long(0),
			MTP_int(640),
			MTP_int(0),
			NativeIvCaption(caption));
	};

	const auto previewEmbedLabel = u"native-iv-embed-preview-overlay"_q;
	const auto previewEmbedBlock = makePreviewEmbedBlock(
		u"https://example.com/embed"_q,
		u"Embed caption"_q,
		true);
	auto previewEmbedSource = NativeIvSource(QVector<MTPPageBlock>{
		previewEmbedBlock,
	});
	const auto previewEmbedPrepared = TryPrepareNativeInstantView({
		.source = &previewEmbedSource,
	});
	Check(
		previewEmbedPrepared.supported(),
		previewEmbedLabel + u" prepare supported"_q,
		ok);
	Check(
		!previewEmbedPrepared.content.failure.failed(),
		previewEmbedLabel + u" prepare failure"_q,
		ok);
	if (previewEmbedPrepared.supported()
		&& !previewEmbedPrepared.content.failure.failed()) {
		auto window = Ui::RpWindow();
		window.setGeometry(QRect(0, 0, 420, 320));
		window.show();
		FlushPendingWidgetEvents();
		const auto expectedStorageId = Webview::StorageId{
			u"native-iv-preview-overlay"_q,
			QByteArray("phase-3"),
		};
		auto previewOptions = OpenOptions();
		previewOptions.ivWebviewStorageId = expectedStorageId;
		auto preview = CreateMarkdownPreviewWidget(
			window.body(),
			std::move(previewEmbedPrepared.content),
			std::make_shared<MathRenderer>(),
			[](Event) {
			},
			previewOptions);
		preview->setGeometry(QRect(QPoint(), window.body()->size()));
		preview->show();
		FlushPendingWidgetEvents();
		const auto body = FindChildObject<MarkdownDocumentWidget>(preview.get());
		const auto overlay = preview->findChild<EmbedOverlay*>(
			u"nativeIvEmbedOverlay"_q);
		const auto overlayShell = preview->findChild<QWidget*>(
			u"nativeIvEmbedOverlayShell"_q);
		const auto overlayClose = preview->findChild<QWidget*>(
			u"nativeIvEmbedOverlayClose"_q);
		Check(
			body != nullptr,
			previewEmbedLabel + u" preview body widget"_q,
			ok);
		Check(
			overlay != nullptr,
			previewEmbedLabel + u" overlay object"_q,
			ok);
		Check(
			overlayShell != nullptr,
			previewEmbedLabel + u" overlay shell object"_q,
			ok);
		Check(
			overlayClose != nullptr,
			previewEmbedLabel + u" overlay close object"_q,
			ok);
		if (overlay) {
			Check(
				overlay->testEffectiveStorageId().path == expectedStorageId.path
					&& overlay->testEffectiveStorageId().token
						== expectedStorageId.token,
				previewEmbedLabel + u" overlay storage id propagated"_q,
				ok);
		}
		if (body && overlay && overlayShell && overlayClose) {
			auto previewProbeSource = NativeIvSource(QVector<MTPPageBlock>{
				previewEmbedBlock,
			});
			const auto previewProbePrepared = TryPrepareNativeInstantView({
				.source = &previewProbeSource,
			});
			Check(
				previewProbePrepared.supported(),
				previewEmbedLabel + u" probe prepare supported"_q,
				ok);
			Check(
				!previewProbePrepared.content.failure.failed(),
				previewEmbedLabel + u" probe prepare failure"_q,
				ok);
			if (previewProbePrepared.supported()
				&& !previewProbePrepared.content.failure.failed()) {
				auto probeHeight = 0;
				auto probeArticle = BuildArticleForTest(
					std::move(previewProbePrepared.content),
					std::make_shared<MathRenderer>(),
					body->width(),
					&probeHeight);
				auto lookupFlags = Ui::Text::StateRequest::Flags();
				lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
				const auto clickBounds = HitBoundsWhere(
					probeArticle.get(),
					body->width(),
					probeHeight,
					lookupFlags,
					[](const MarkdownArticleHitTestResult &hit) {
						return hit.mediaActivation.kind
							== MediaActivationKind::Embed;
					});
				Check(
					clickBounds.has_value(),
					previewEmbedLabel + u" click bounds"_q,
					ok);
				if (clickBounds) {
					const auto bodyBeforeClick = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay hidden before click"_q,
						ok);
					Check(
						!overlayClose->isVisible(),
						previewEmbedLabel + u" overlay close hidden before click"_q,
						ok);
					SendMouseClick(body, clickBounds->center(), Qt::LeftButton);
					FlushPendingWidgetEvents();
					const auto loadingBody = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay hidden after click"_q,
						ok);
#ifdef Q_OS_LINUX
					Check(
						overlay->testMode() == EmbedOverlay::Mode::External,
						previewEmbedLabel + u" click enters external mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" external click skips loading cover"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" external click skips ready delay"_q,
						ok);
					Check(
						!PixelsDifferInRect(
							bodyBeforeClick,
							loadingBody,
							*clickBounds),
						previewEmbedLabel
							+ u" external click skips placeholder loading"_q,
						ok);
					overlay->testHandleNavigationDone(true);
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel
							+ u" external success keeps overlay hidden"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::External,
						previewEmbedLabel
							+ u" external success keeps external mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewEmbedLabel
							+ u" external success keeps loading cover hidden"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewEmbedLabel
							+ u" external success keeps ready delay idle"_q,
						ok);
					overlay->testHandleNavigationDone(false);
					FlushPendingWidgetEvents();
					const auto overlayError = preview->findChild<QWidget*>(
						u"nativeIvEmbedOverlayErrorWrap"_q);
					Check(
						overlayShell->isVisible(),
						previewEmbedLabel
							+ u" external failure shows overlay shell"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::Error,
						previewEmbedLabel
							+ u" external failure switches to error mode"_q,
						ok);
					Check(
						overlayError && overlayError->isVisible(),
						previewEmbedLabel
							+ u" external failure shows overlay error"_q,
						ok);
					Check(
						overlayClose->isVisible(),
						previewEmbedLabel
							+ u" overlay close visible on external failure"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewEmbedLabel
							+ u" external failure keeps loading cover hidden"_q,
						ok);
#else // Q_OS_LINUX
					Check(
						overlay->testMode() == EmbedOverlay::Mode::EmbeddedPreload,
						previewEmbedLabel + u" preload mode after click"_q,
						ok);
					Check(
						overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" preload active after click"_q,
						ok);
					Check(
						PixelsDifferInRect(
							bodyBeforeClick,
							loadingBody,
							*clickBounds),
						previewEmbedLabel + u" placeholder loading visible"_q,
						ok);
					const auto availableRect = NativeIvOverlayAvailableRect(
						overlay,
						true);
					Check(
						availableRect.isValid(),
						previewEmbedLabel + u" overlay available rect"_q,
						ok);
					overlay->testHandleWebviewMessage(
						NativeIvPreferredSizeMessage(QSize(180, 96)));
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay stays hidden after preferred size"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" ready delay waits for readiness"_q,
						ok);
					overlay->testHandleNavigationDone(true);
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay stays hidden after readiness"_q,
						ok);
					Check(
						overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" preload stays active after readiness"_q,
						ok);
					Check(
						overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" ready delay scheduled after readiness"_q,
						ok);
					const auto readyGeometry = overlayShell->geometry();
					if (availableRect.isValid()) {
						overlay->testHandleWebviewMessage(
							NativeIvPreferredSizeMessage(QSize(
								availableRect.width() * 2,
								availableRect.height() * 2)));
						FlushPendingWidgetEvents();
					}
					const auto settledGeometry = overlayShell->geometry();
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel
							+ u" overlay stays hidden during settled resize"_q,
						ok);
					Check(
						overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" ready delay remains scheduled"_q,
						ok);
					if (availableRect.isValid()) {
						const auto expectedFullWidthGeometry
							= NativeIvOverlayFullWidthBodyRect(overlay);
						Check(
							readyGeometry != settledGeometry,
							previewEmbedLabel
								+ u" hidden preload accepts later preferred size"_q,
							ok);
						Check(
							settledGeometry == expectedFullWidthGeometry,
							previewEmbedLabel
								+ u" hidden preload leaves close reserve"_q,
							ok);
					}
					overlay->testFireReadyDelay();
					FlushPendingWidgetEvents();
					const auto revealedBody = RenderWidgetForTest(body);
					const auto revealedBodyGeometry = overlay->testBodyGeometry();
					const auto expectedBodyGeometry
						= NativeIvOverlayFullWidthBodyRect(overlay);
					Check(
						overlayShell->isVisible(),
						previewEmbedLabel + u" overlay reveals after ready delay"_q,
						ok);
					Check(
						overlayClose->isVisible(),
						previewEmbedLabel + u" overlay close visible on reveal"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::EmbeddedVisible,
						previewEmbedLabel + u" reveal switches to visible mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" preload clears after reveal"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" ready delay clears after reveal"_q,
						ok);
					Check(
						overlayShell->geometry() == settledGeometry,
						previewEmbedLabel
							+ u" reveal uses latest settled geometry"_q,
						ok);
					Check(
						revealedBodyGeometry == expectedBodyGeometry,
						previewEmbedLabel
							+ u" reveal body geometry keeps close reserve"_q,
						ok);
					if (availableRect.isValid() && !revealedBodyGeometry.isEmpty()) {
						Check(
							(revealedBodyGeometry.top() - availableRect.top())
									== NativeIvOverlayCloseHitHeight()
								&& (availableRect.bottom()
										- revealedBodyGeometry.bottom())
									== NativeIvOverlayCloseHitHeight(),
							previewEmbedLabel
								+ u" full-width body keeps equal top and bottom skip"_q,
							ok);
						Check(
							overlayShell->size() == NativeIvOverlayShellSizeForBody(
								revealedBodyGeometry.size(),
								true),
							previewEmbedLabel
								+ u" full-width shell size matches body"_q,
							ok);
					}
					Check(
						overlayClose->geometry().right() == overlay->rect().right()
							&& overlayClose->geometry().top()
								== overlay->rect().top(),
						previewEmbedLabel
							+ u" overlay close anchored to top right"_q,
						ok);
					Check(
						overlayClose->geometry().bottom()
								<= revealedBodyGeometry.top(),
						previewEmbedLabel
							+ u" overlay close stays inside reserved band"_q,
						ok);
					Check(
						PixelsDifferInRect(
							loadingBody,
							revealedBody,
							*clickBounds),
						previewEmbedLabel + u" placeholder loading clears on reveal"_q,
						ok);
					overlay->testHandleNavigationDone(false);
					FlushPendingWidgetEvents();
					const auto lateFailureError = preview->findChild<QWidget*>(
						u"nativeIvEmbedOverlayErrorWrap"_q);
					Check(
						overlayShell->isVisible(),
						previewEmbedLabel
							+ u" late navigation failure keeps overlay visible"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::Error,
						previewEmbedLabel
							+ u" late navigation failure switches to error mode"_q,
						ok);
					Check(
						lateFailureError && lateFailureError->isVisible(),
						previewEmbedLabel
							+ u" late navigation failure shows overlay error"_q,
						ok);
					Check(
						overlayClose->isVisible(),
						previewEmbedLabel
							+ u" overlay close visible on late failure"_q,
						ok);
					overlay->closeEmbed();
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay hides before failure retry"_q,
						ok);
					Check(
						!overlayClose->isVisible(),
						previewEmbedLabel
							+ u" overlay close hides before failure retry"_q,
						ok);
					const auto bodyBeforeFailureClick = RenderWidgetForTest(body);
					SendMouseClick(body, clickBounds->center(), Qt::LeftButton);
					FlushPendingWidgetEvents();
					const auto failureLoadingBody = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewEmbedLabel + u" overlay stays hidden on failure click"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::EmbeddedPreload,
						previewEmbedLabel + u" retry reenters preload mode"_q,
						ok);
					Check(
						overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" preload active before failure"_q,
						ok);
					Check(
						PixelsDifferInRect(
							bodyBeforeFailureClick,
							failureLoadingBody,
							*clickBounds),
						previewEmbedLabel + u" placeholder loading visible on retry"_q,
						ok);
					overlay->testHandleNavigationDone(false);
					FlushPendingWidgetEvents();
					const auto bodyAfterFailure = RenderWidgetForTest(body);
					const auto overlayError = preview->findChild<QWidget*>(
						u"nativeIvEmbedOverlayErrorWrap"_q);
					Check(
						overlayShell->isVisible(),
						previewEmbedLabel + u" preload failure shows overlay shell"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::Error,
						previewEmbedLabel
							+ u" preload failure switches to error mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewEmbedLabel + u" preload clears after failure"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewEmbedLabel + u" ready delay clears after failure"_q,
						ok);
					Check(
						PixelsDifferInRect(
							failureLoadingBody,
							bodyAfterFailure,
							*clickBounds),
						previewEmbedLabel
							+ u" placeholder loading clears after failure callback"_q,
						ok);
					Check(
						overlayError && overlayError->isVisible(),
						previewEmbedLabel
							+ u" preload failure shows overlay error"_q,
						ok);
					Check(
						overlayClose->isVisible(),
						previewEmbedLabel
							+ u" overlay close visible on preload failure"_q,
						ok);
#endif // Q_OS_LINUX
				}
			}
		}
	}

	const auto previewCancelLabel = u"native-iv-embed-preview-cancel"_q;
	const auto firstPreviewBlock = makePreviewEmbedBlock(
		u"https://example.com/embed-first"_q,
		u"First embed"_q);
	const auto secondPreviewBlock = makePreviewEmbedBlock(
		u"https://example.com/embed-second"_q,
		u"Second embed"_q);
	auto previewCancelSource = NativeIvSource(QVector<MTPPageBlock>{
		firstPreviewBlock,
		secondPreviewBlock,
	});
	const auto previewCancelPrepared = TryPrepareNativeInstantView({
		.source = &previewCancelSource,
	});
	Check(
		previewCancelPrepared.supported(),
		previewCancelLabel + u" prepare supported"_q,
		ok);
	Check(
		!previewCancelPrepared.content.failure.failed(),
		previewCancelLabel + u" prepare failure"_q,
		ok);
	if (previewCancelPrepared.supported()
		&& !previewCancelPrepared.content.failure.failed()) {
		auto window = Ui::RpWindow();
		window.setGeometry(QRect(0, 0, 420, 400));
		window.show();
		FlushPendingWidgetEvents();
		auto previewOptions = OpenOptions();
		previewOptions.ivWebviewStorageId = {
			u"native-iv-preview-cancel"_q,
			QByteArray("phase-5"),
		};
		auto preview = CreateMarkdownPreviewWidget(
			window.body(),
			std::move(previewCancelPrepared.content),
			std::make_shared<MathRenderer>(),
			[](Event) {
			},
			previewOptions);
		preview->setGeometry(QRect(QPoint(), window.body()->size()));
		preview->show();
		FlushPendingWidgetEvents();
		const auto body = FindChildObject<MarkdownDocumentWidget>(preview.get());
		const auto overlay = preview->findChild<EmbedOverlay*>(
			u"nativeIvEmbedOverlay"_q);
		const auto overlayShell = preview->findChild<QWidget*>(
			u"nativeIvEmbedOverlayShell"_q);
		Check(
			body != nullptr,
			previewCancelLabel + u" preview body widget"_q,
			ok);
		Check(
			overlay != nullptr,
			previewCancelLabel + u" overlay object"_q,
			ok);
		Check(
			overlayShell != nullptr,
			previewCancelLabel + u" overlay shell object"_q,
			ok);
		if (body && overlay && overlayShell) {
			auto previewProbeSource = NativeIvSource(QVector<MTPPageBlock>{
				firstPreviewBlock,
				secondPreviewBlock,
			});
			const auto previewProbePrepared = TryPrepareNativeInstantView({
				.source = &previewProbeSource,
			});
			Check(
				previewProbePrepared.supported(),
				previewCancelLabel + u" probe prepare supported"_q,
				ok);
			Check(
				!previewProbePrepared.content.failure.failed(),
				previewCancelLabel + u" probe prepare failure"_q,
				ok);
			if (previewProbePrepared.supported()
				&& !previewProbePrepared.content.failure.failed()) {
				auto probeHeight = 0;
				auto probeArticle = BuildArticleForTest(
					std::move(previewProbePrepared.content),
					std::make_shared<MathRenderer>(),
					body->width(),
					&probeHeight);
				auto lookupFlags = Ui::Text::StateRequest::Flags();
				lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
				const auto firstBounds = HitBoundsWhere(
					probeArticle.get(),
					body->width(),
					probeHeight,
					lookupFlags,
					[](const MarkdownArticleHitTestResult &hit) {
						return hit.mediaActivation.kind == MediaActivationKind::Embed
							&& (hit.mediaActivation.embed.fallbackUrl
								== u"https://example.com/embed-first"_q);
					});
				const auto secondBounds = HitBoundsWhere(
					probeArticle.get(),
					body->width(),
					probeHeight,
					lookupFlags,
					[](const MarkdownArticleHitTestResult &hit) {
						return hit.mediaActivation.kind == MediaActivationKind::Embed
							&& (hit.mediaActivation.embed.fallbackUrl
								== u"https://example.com/embed-second"_q);
					});
				Check(
					firstBounds.has_value(),
					previewCancelLabel + u" first click bounds"_q,
					ok);
				Check(
					secondBounds.has_value(),
					previewCancelLabel + u" second click bounds"_q,
					ok);
				if (firstBounds && secondBounds) {
					const auto bodyBeforeFirstClick = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel + u" overlay hidden before first click"_q,
						ok);
					SendMouseClick(body, firstBounds->center(), Qt::LeftButton);
					FlushPendingWidgetEvents();
					const auto firstLoadingBody = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel + u" overlay hidden after first click"_q,
						ok);
#ifdef Q_OS_LINUX
					Check(
						overlay->testMode() == EmbedOverlay::Mode::External,
						previewCancelLabel + u" first click enters external mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" first click skips loading cover"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewCancelLabel + u" first click skips ready delay"_q,
						ok);
					Check(
						!PixelsDifferInRect(
							bodyBeforeFirstClick,
							firstLoadingBody,
							*firstBounds),
						previewCancelLabel
							+ u" first click skips placeholder loading"_q,
						ok);
					overlay->testHandleNavigationDone(true);
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel
							+ u" external success keeps overlay hidden"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::External,
						previewCancelLabel
							+ u" external success keeps external mode"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewCancelLabel
							+ u" external success keeps ready delay idle"_q,
						ok);
					SendMouseClick(body, secondBounds->center(), Qt::LeftButton);
					FlushPendingWidgetEvents();
					const auto secondLoadingBody = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel
							+ u" overlay hidden after second click"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::External,
						previewCancelLabel + u" second click enters external mode"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" second click skips loading cover"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewCancelLabel + u" second click skips ready delay"_q,
						ok);
					Check(
						!PixelsDifferInRect(
							firstLoadingBody,
							secondLoadingBody,
							*firstBounds),
						previewCancelLabel
							+ u" first placeholder stays non-loading"_q,
						ok);
					Check(
						!PixelsDifferInRect(
							firstLoadingBody,
							secondLoadingBody,
							*secondBounds),
						previewCancelLabel
							+ u" second click skips placeholder loading"_q,
						ok);
					overlay->testHandleNavigationDone(false);
					FlushPendingWidgetEvents();
					const auto overlayError = preview->findChild<QWidget*>(
						u"nativeIvEmbedOverlayErrorWrap"_q);
					Check(
						overlayShell->isVisible(),
						previewCancelLabel
							+ u" external failure shows overlay shell"_q,
						ok);
					Check(
						overlay->testMode() == EmbedOverlay::Mode::Error,
						previewCancelLabel
							+ u" external failure switches to error mode"_q,
						ok);
					Check(
						overlayError && overlayError->isVisible(),
						previewCancelLabel
							+ u" external failure shows overlay error"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewCancelLabel
							+ u" external failure keeps loading cover hidden"_q,
						ok);
#else // Q_OS_LINUX
					Check(
						overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" first preload active"_q,
						ok);
					Check(
						PixelsDifferInRect(
							bodyBeforeFirstClick,
							firstLoadingBody,
							*firstBounds),
						previewCancelLabel + u" first placeholder loading visible"_q,
						ok);
					overlay->testHandleNavigationDone(true);
					FlushPendingWidgetEvents();
					Check(
						overlay->testReadyDelayScheduled(),
						previewCancelLabel + u" first ready delay scheduled"_q,
						ok);
					SendMouseClick(body, secondBounds->center(), Qt::LeftButton);
					FlushPendingWidgetEvents();
					const auto secondLoadingBody = RenderWidgetForTest(body);
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel + u" overlay hidden after second click"_q,
						ok);
					Check(
						overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" second preload active"_q,
						ok);
					Check(
						!overlay->testReadyDelayScheduled(),
						previewCancelLabel + u" first ready delay canceled"_q,
						ok);
					Check(
						PixelsDifferInRect(
							firstLoadingBody,
							secondLoadingBody,
							*firstBounds),
						previewCancelLabel + u" first placeholder loading cleared"_q,
						ok);
					Check(
						PixelsDifferInRect(
							firstLoadingBody,
							secondLoadingBody,
							*secondBounds),
						previewCancelLabel + u" second placeholder loading visible"_q,
						ok);
					overlay->testFireReadyDelay();
					FlushPendingWidgetEvents();
					Check(
						!overlayShell->isVisible(),
						previewCancelLabel + u" stale first ready delay does not reveal"_q,
						ok);
					Check(
						overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" second preload stays active"_q,
						ok);
					overlay->testHandleWebviewMessage(
						NativeIvPreferredSizeMessage(QSize(220, 120)));
					FlushPendingWidgetEvents();
					overlay->testHandleNavigationDone(true);
					FlushPendingWidgetEvents();
					Check(
						overlay->testReadyDelayScheduled(),
						previewCancelLabel + u" second ready delay scheduled"_q,
						ok);
					const auto secondSettledGeometry = overlayShell->geometry();
					overlay->testFireReadyDelay();
					FlushPendingWidgetEvents();
					const auto revealedBody = RenderWidgetForTest(body);
					Check(
						overlayShell->isVisible(),
						previewCancelLabel + u" second preload reveals"_q,
						ok);
					Check(
						!overlay->testLoadingCoverVisible(),
						previewCancelLabel + u" second preload clears after reveal"_q,
						ok);
					Check(
						overlayShell->geometry() == secondSettledGeometry,
						previewCancelLabel + u" second reveal uses settled geometry"_q,
						ok);
					Check(
						PixelsDifferInRect(
							secondLoadingBody,
							revealedBody,
							*secondBounds),
						previewCancelLabel + u" second placeholder loading clears"_q,
						ok);
					overlay->closeEmbed();
					FlushPendingWidgetEvents();
#endif // Q_OS_LINUX
				}
			}
		}
	}

	const auto relatedLabel = u"native-iv-related-articles"_q;
	const auto relatedPhotoId = uint64(9201);
	auto relatedBlocks = QVector<MTPPageBlock>();
	relatedBlocks.push_back(NativeIvRelatedArticlesBlock(
		u"Related articles"_q,
		{
			NativeIvRelatedArticle(
				u"https://example.com/native-iv-page#section"_q,
				88001,
				u"Native IV article"_q,
				u"Native IV description"_q,
				relatedPhotoId,
				u"Test Author"_q),
			NativeIvRelatedArticle(
				u"https://example.com/external-article"_q,
				0,
				u"External article"_q,
				u"External article description"_q),
		}));
	relatedBlocks.push_back(MTP_pageBlockParagraph(NativeIvConcat({
		NativeIvText(u"Read "_q),
		NativeIvTextUrl(
			u"native page"_q,
			u"https://example.com/native-iv-inline#details"_q,
			88002),
		NativeIvText(u" or "_q),
		NativeIvTextUrl(
			u"external page"_q,
			u"https://example.com/external-inline"_q),
		NativeIvText(u"."_q),
	})));
	auto relatedSource = NativeIvSource(
		std::move(relatedBlocks),
		QVector<MTPPhoto>{ NativeIvPhoto(relatedPhotoId, 87, 87) });
	const auto relatedPrepared = TryPrepareNativeInstantView({
		.source = &relatedSource,
	});
	Check(
		relatedPrepared.supported(),
		relatedLabel + u" prepare supported"_q,
		ok);
	Check(
		!relatedPrepared.content.failure.failed(),
		relatedLabel + u" prepare failure"_q,
		ok);
	if (relatedPrepared.supported()
		&& !relatedPrepared.content.failure.failed()) {
		const auto relatedArticleBlocks = CollectPreparedBlocksByKind(
			relatedPrepared.content.blocks.blocks,
			PreparedBlockKind::RelatedArticle);
		const PreparedBlock *relatedHeading = nullptr;
		const PreparedBlock *relatedParagraph = nullptr;
		ForEachPreparedBlock(
			relatedPrepared.content.blocks.blocks,
			[&](const PreparedBlock &block) {
				if (!relatedHeading
					&& block.kind == PreparedBlockKind::Heading
					&& block.text.text == u"Related articles"_q) {
					relatedHeading = &block;
				}
				if (!relatedParagraph
					&& block.kind == PreparedBlockKind::Paragraph
					&& block.text.text.contains(u"native page"_q)) {
					relatedParagraph = &block;
				}
			});
		Check(
			relatedPrepared.content.blocks.blocks.size() == 4,
			relatedLabel + u" prepared block count"_q,
			ok);
		Check(
			relatedHeading != nullptr
				&& (relatedHeading->headingLevel == 4),
			relatedLabel + u" heading block"_q,
			ok);
		Check(
			relatedArticleBlocks.size() == 2,
			relatedLabel + u" related article block count"_q,
			ok);
		Check(
			relatedParagraph != nullptr,
			relatedLabel + u" paragraph block"_q,
			ok);
		if (relatedArticleBlocks.size() == 2) {
			const auto *internalArticle = relatedArticleBlocks[0];
			const auto *externalArticle = relatedArticleBlocks[1];
			Check(
				internalArticle->relatedArticle.link.kind
					== PreparedLinkKind::InstantViewPage
					&& (internalArticle->relatedArticle.link.webpageId == 88001),
				relatedLabel + u" internal article IV link"_q,
				ok);
			Check(
				internalArticle->relatedArticle.title == u"Native IV article"_q
					&& internalArticle->relatedArticle.description
						== u"Native IV description"_q
					&& internalArticle->relatedArticle.footer
						== u"Test Author"_q
					&& (internalArticle->relatedArticle.photoId == relatedPhotoId),
				relatedLabel + u" internal article fields"_q,
				ok);
			Check(
				internalArticle->relatedArticle.copyText
					== u"Native IV article\nNative IV description\nTest Author"_q,
				relatedLabel + u" internal article copy text"_q,
				ok);
			Check(
				externalArticle->relatedArticle.link.kind
					== PreparedLinkKind::External,
				relatedLabel + u" external article link kind"_q,
				ok);
			Check(
				externalArticle->relatedArticle.copyText
					== u"External article\nExternal article description"_q,
				relatedLabel + u" external article copy text"_q,
				ok);
		}
		if (relatedParagraph) {
			const auto ivText = u"native page"_q;
			const auto ivOffset = relatedParagraph->text.text.indexOf(ivText);
			const auto externalText = u"external page"_q;
			const auto externalOffset = relatedParagraph->text.text.indexOf(
				externalText);
			Check(
				ivOffset >= 0,
				relatedLabel + u" inline IV text range"_q,
				ok);
			Check(
				externalOffset >= 0,
				relatedLabel + u" inline external text range"_q,
				ok);
			if ((ivOffset >= 0) && (externalOffset >= 0)) {
				const auto ivPreparedLink = FindPreparedLinkByCustomUrlRange(
					relatedParagraph->text,
					relatedParagraph->links,
					ivOffset,
					ivText.size());
				const auto externalPreparedLink = FindPreparedLinkByCustomUrlRange(
					relatedParagraph->text,
					relatedParagraph->links,
					externalOffset,
					externalText.size());
				const auto ivColorized = std::find_if(
					relatedParagraph->text.entities.begin(),
					relatedParagraph->text.entities.end(),
					[&](const EntityInText &entity) {
						return entity.type() == EntityType::Colorized
							&& (entity.offset() == ivOffset)
							&& (entity.length() == ivText.size());
					});
				Check(
					ivPreparedLink != nullptr
						&& ivPreparedLink->kind
							== PreparedLinkKind::InstantViewPage
						&& (ivPreparedLink->webpageId == 88002),
					relatedLabel + u" inline IV prepared link"_q,
					ok);
				Check(
					externalPreparedLink != nullptr
						&& externalPreparedLink->kind
							== PreparedLinkKind::External,
					relatedLabel + u" inline external prepared link"_q,
					ok);
				Check(
					ivColorized != relatedParagraph->text.entities.end()
						&& (ivColorized->data().size() == 2)
						&& (ivColorized->data()[0].unicode() == 9)
						&& (ivColorized->data()[1].unicode() == 9),
					relatedLabel + u" inline IV colorized entity"_q,
					ok);
			}
		}
	}

	auto unsupportedBlocks = QVector<MTPPageBlock>();
	unsupportedBlocks.push_back(MTP_pageBlockCover(MTP_pageBlockUnsupported()));
	auto unsupportedSource = NativeIvSource(std::move(unsupportedBlocks));
	const auto unsupported = TryPrepareNativeInstantView({
		.source = &unsupportedSource,
	});
	Check(
		unsupported.supported(),
		u"native-iv unsupported block placeholder classification"_q,
		ok);
	Check(
		!unsupported.unsupported() && !unsupported.failed(),
		u"native-iv unsupported block nonterminal classification"_q,
		ok);
	Check(
		unsupported.content.blocks.blocks.size() == 1,
		u"native-iv unsupported block placeholder count"_q,
		ok);
	if (unsupported.content.blocks.blocks.size() == 1) {
		const auto &block = unsupported.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv unsupported block placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Unsupported Content"_q,
			u"native-iv unsupported block placeholder label"_q,
			ok);
	}

	auto missingPhotoSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvPhotoBlock(9999, u"Missing photo"_q),
	});
	const auto missingPhoto = TryPrepareNativeInstantView({
		.source = &missingPhotoSource,
	});
	Check(
		missingPhoto.supported(),
		u"native-iv missing-photo placeholder classification"_q,
		ok);
	Check(
		missingPhoto.content.blocks.blocks.size() == 1,
		u"native-iv missing-photo placeholder count"_q,
		ok);
	if (missingPhoto.content.blocks.blocks.size() == 1) {
		const auto &block = missingPhoto.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv missing-photo placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Photo Placeholder"_q,
			u"native-iv missing-photo placeholder label"_q,
			ok);
		Check(
			block.text.text == u"Missing photo"_q,
			u"native-iv missing-photo placeholder caption"_q,
			ok);
	}

	auto missingVideoSource = NativeIvSource(QVector<MTPPageBlock>{
		videoFixture.block,
	});
	const auto missingVideo = TryPrepareNativeInstantView({
		.source = &missingVideoSource,
	});
	Check(
		missingVideo.supported(),
		u"native-iv missing-video placeholder classification"_q,
		ok);
	if (missingVideo.content.blocks.blocks.size() == 1) {
		const auto &block = missingVideo.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv missing-video placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Video Placeholder"_q,
			u"native-iv missing-video placeholder label"_q,
			ok);
		Check(
			block.text.text == videoFixture.caption,
			u"native-iv missing-video placeholder caption"_q,
			ok);
	}

	auto nonVideoVideoSource = NativeIvSource(
		QVector<MTPPageBlock>{ videoFixture.block },
		QVector<MTPPhoto>(),
		QVector<MTPDocument>{
			NativeIvImageDocument(7001, 1280, 720, u"video-poster.jpg"_q),
		});
	const auto nonVideoVideo = TryPrepareNativeInstantView({
		.source = &nonVideoVideoSource,
	});
	Check(
		nonVideoVideo.supported(),
		u"native-iv non-video-video placeholder classification"_q,
		ok);
	if (nonVideoVideo.content.blocks.blocks.size() == 1) {
		const auto &block = nonVideoVideo.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv non-video-video placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Video Placeholder"_q,
			u"native-iv non-video-video placeholder label"_q,
			ok);
		Check(
			block.text.text == videoFixture.caption,
			u"native-iv non-video-video placeholder caption"_q,
			ok);
	}

	auto duplicatePoorerVideoSource = NativeIvSource(
		QVector<MTPPageBlock>{ videoFixture.block },
		QVector<MTPPhoto>(),
		videoFixture.documents);
	duplicatePoorerVideoSource.webpageDocument = NativeIvDocument(
		7001,
		u"video/mp4"_q,
		QVector<MTPDocumentAttribute>());
	const auto duplicatePoorerVideo = TryPrepareNativeInstantView({
		.source = &duplicatePoorerVideoSource,
	});
	Check(
		duplicatePoorerVideo.supported(),
		u"native-iv duplicate poorer video classification"_q,
		ok);
	if (duplicatePoorerVideo.content.blocks.blocks.size() == 1) {
		const auto &block = duplicatePoorerVideo.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Video,
			u"native-iv duplicate poorer video kind"_q,
			ok);
		if (block.kind == PreparedBlockKind::Video) {
			Check(
				block.video.media.id == 7001,
				u"native-iv duplicate poorer video id"_q,
				ok);
			Check(
				block.video.media.width == 1280
					&& block.video.media.height == 720,
				u"native-iv duplicate poorer video dimensions"_q,
				ok);
		}
	}

	auto missingAudioSource = NativeIvSource(QVector<MTPPageBlock>{
		audioFixture.block,
	});
	const auto missingAudio = TryPrepareNativeInstantView({
		.source = &missingAudioSource,
	});
	Check(
		missingAudio.supported(),
		u"native-iv missing-audio placeholder classification"_q,
		ok);
	if (missingAudio.content.blocks.blocks.size() == 1) {
		const auto &block = missingAudio.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv missing-audio placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Audio File Placeholder"_q,
			u"native-iv missing-audio placeholder label"_q,
			ok);
		Check(
			block.text.text == audioFixture.caption,
			u"native-iv missing-audio placeholder caption"_q,
			ok);
	}

	auto invalidMapBlocks = QVector<MTPPageBlock>();
	invalidMapBlocks.push_back(MTP_pageBlockMap(
		MTP_geoPointEmpty(),
		MTP_int(13),
		MTP_int(320),
		MTP_int(180),
		NativeIvCaption(u"Invalid map"_q)));
	auto invalidMapSource = NativeIvSource(std::move(invalidMapBlocks));
	const auto invalidMap = TryPrepareNativeInstantView({
		.source = &invalidMapSource,
	});
	Check(
		invalidMap.supported(),
		u"native-iv invalid-map placeholder classification"_q,
		ok);
	if (invalidMap.content.blocks.blocks.size() == 1) {
		const auto &block = invalidMap.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv invalid-map placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Map Placeholder"_q,
			u"native-iv invalid-map placeholder label"_q,
			ok);
		Check(
			block.text.text == u"Invalid map"_q,
			u"native-iv invalid-map placeholder caption"_q,
			ok);
	}

	auto unsupportedCollageItems = QVector<MTPPageBlock>();
	unsupportedCollageItems.push_back(NativeIvPhotoBlock(9102));
	unsupportedCollageItems.push_back(MTP_pageBlockParagraph(NativeIvText(
		u"unsupported child"_q)));
	auto unsupportedCollageSource = NativeIvSource(
		QVector<MTPPageBlock>{
			MTP_pageBlockCollage(
			MTP_vector<MTPPageBlock>(std::move(unsupportedCollageItems)),
			NativeIvCaption(u"Broken collage"_q)),
		},
		collageFixture.photos,
		collageFixture.documents);
	const auto unsupportedCollage = TryPrepareNativeInstantView({
		.source = &unsupportedCollageSource,
	});
	Check(
		unsupportedCollage.supported(),
		u"native-iv unsupported-collage placeholder classification"_q,
		ok);
	if (unsupportedCollage.content.blocks.blocks.size() == 1) {
		const auto &block = unsupportedCollage.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv unsupported-collage placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Collage placeholder"_q,
			u"native-iv unsupported-collage placeholder label"_q,
			ok);
		Check(
			block.text.text == u"Broken collage"_q,
			u"native-iv unsupported-collage placeholder caption"_q,
			ok);
	}

	auto nonVideoCollageSource = NativeIvSource(
		QVector<MTPPageBlock>{ collageFixture.block },
		collageFixture.photos,
		QVector<MTPDocument>{
			NativeIvImageDocument(7003, 640, 360, u"collage-still.jpg"_q),
		});
	const auto nonVideoCollage = TryPrepareNativeInstantView({
		.source = &nonVideoCollageSource,
	});
	Check(
		nonVideoCollage.supported(),
		u"native-iv non-video-collage placeholder classification"_q,
		ok);
	if (nonVideoCollage.content.blocks.blocks.size() == 1) {
		const auto &block = nonVideoCollage.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv non-video-collage placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Collage placeholder"_q,
			u"native-iv non-video-collage placeholder label"_q,
			ok);
		Check(
			block.text.text == collageFixture.caption,
			u"native-iv non-video-collage placeholder caption"_q,
			ok);
	}

	auto nonVideoSlideshowSource = NativeIvSource(
		QVector<MTPPageBlock>{ slideshowFixture.block },
		slideshowFixture.photos,
		QVector<MTPDocument>{
			NativeIvImageDocument(7004, 960, 540, u"slide-a.jpg"_q),
			slideshowFixture.documents[1],
		});
	const auto nonVideoSlideshow = TryPrepareNativeInstantView({
		.source = &nonVideoSlideshowSource,
	});
	Check(
		nonVideoSlideshow.supported(),
		u"native-iv non-video-slideshow placeholder classification"_q,
		ok);
	if (nonVideoSlideshow.content.blocks.blocks.size() == 1) {
		const auto &block = nonVideoSlideshow.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv non-video-slideshow placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Grouped Media Placeholder"_q,
			u"native-iv non-video-slideshow placeholder label"_q,
			ok);
		Check(
			block.text.text == slideshowFixture.caption,
			u"native-iv non-video-slideshow placeholder caption"_q,
			ok);
	}

	auto missingGroupedDocumentSource = NativeIvSource(
		QVector<MTPPageBlock>{ slideshowFixture.block },
		slideshowFixture.photos,
		QVector<MTPDocument>{ slideshowFixture.documents.front() });
	const auto missingGroupedDocument = TryPrepareNativeInstantView({
		.source = &missingGroupedDocumentSource,
	});
	Check(
		missingGroupedDocument.supported(),
		u"native-iv missing-grouped-document placeholder classification"_q,
		ok);
	if (missingGroupedDocument.content.blocks.blocks.size() == 1) {
		const auto &block = missingGroupedDocument.content.blocks.blocks.front();
		Check(
			block.kind == PreparedBlockKind::Placeholder,
			u"native-iv missing-grouped-document placeholder kind"_q,
			ok);
		Check(
			block.placeholder.label == u"Grouped Media Placeholder"_q,
			u"native-iv missing-grouped-document placeholder label"_q,
			ok);
		Check(
			block.text.text == slideshowFixture.caption,
			u"native-iv missing-grouped-document placeholder caption"_q,
			ok);
	}
}

void CheckCodeBlockTrailingNewlineTrim(bool *ok) {
	const auto markdownLabel = u"generated-code-block-trailing-newline"_q;
	const auto parsed = ParseMarkdownForIv(QByteArray(R"(```cpp extra-token
alpha

```

    beta
)"), ParseOptions{ markdownLabel });
	Check(
		parsed.ok,
		markdownLabel + u" parse failed: "_q + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	auto renderer = std::make_shared<MathRenderer>();
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		markdownLabel,
		renderer);
	Check(
		!prepared.failure.failed(),
		markdownLabel + u" prepare failed: "_q
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	const auto markdownCodeBlocks = CollectPreparedBlocksByKind(
		prepared.blocks.blocks,
		PreparedBlockKind::CodeBlock);
	Check(
		markdownCodeBlocks.size() == 2,
		markdownLabel + u" code block count"_q,
		ok);
	if (markdownCodeBlocks.size() == 2) {
		Check(
			markdownCodeBlocks[0]->text.text == u"alpha\n"_q,
			markdownLabel + u" fenced block trims one newline"_q,
			ok);
		Check(
			markdownCodeBlocks[0]->codeLanguage == u"cpp"_q,
			markdownLabel + u" fenced block keeps first info token"_q,
			ok);
		Check(
			markdownCodeBlocks[1]->text.text == u"beta"_q,
			markdownLabel + u" indented block trims one newline"_q,
			ok);
		Check(
			markdownCodeBlocks[1]->codeLanguage.isEmpty(),
			markdownLabel + u" indented block keeps empty language"_q,
			ok);
	}

	const auto nativeLabel = u"native-iv-preformatted-trailing-newline"_q;
	auto nativeBlocks = QVector<MTPPageBlock>();
	nativeBlocks.push_back(MTP_pageBlockPreformatted(
		NativeIvText(u"single\n"_q),
		MTP_string(" txt ")));
	nativeBlocks.push_back(MTP_pageBlockPreformatted(
		NativeIvText(u"double\n\n"_q),
		MTP_string(" txt ")));
	auto nativeSource = NativeIvSource(std::move(nativeBlocks));
	const auto nativePrepared = TryPrepareNativeInstantView({
		.source = &nativeSource,
	});
	Check(
		nativePrepared.supported(),
		nativeLabel + u" prepare supported"_q,
		ok);
	Check(
		!nativePrepared.content.failure.failed(),
		nativeLabel + u" prepare failed"_q,
		ok);
	if (!nativePrepared.supported()
		|| nativePrepared.content.failure.failed()) {
		return;
	}
	const auto nativeCodeBlocks = CollectPreparedBlocksByKind(
		nativePrepared.content.blocks.blocks,
		PreparedBlockKind::CodeBlock);
	Check(
		nativeCodeBlocks.size() == 2,
		nativeLabel + u" code block count"_q,
		ok);
	if (nativeCodeBlocks.size() == 2) {
		Check(
			nativeCodeBlocks[0]->text.text == u"single"_q,
			nativeLabel + u" single trailing newline trimmed"_q,
			ok);
		Check(
			nativeCodeBlocks[0]->codeLanguage == u"txt"_q,
			nativeLabel + u" native language trimmed"_q,
			ok);
		Check(
			nativeCodeBlocks[1]->text.text == u"double\n"_q,
			nativeLabel + u" extra trailing newline preserved"_q,
			ok);
		Check(
			nativeCodeBlocks[1]->codeLanguage == u"txt"_q,
			nativeLabel + u" native language trimmed on second block"_q,
			ok);
	}
}

void CheckCodeBlockSelectionExportCoverage(bool *ok) {
	const auto renderer = std::make_shared<MathRenderer>();
	const auto checkSelectionExport = [&](std::unique_ptr<MarkdownArticle> article,
			const QString &expectedLanguage,
			const QString &label) {
		Check(
			article != nullptr,
			label + u" article built"_q,
			ok);
		if (!article) {
			return;
		}
		Check(
			article->segmentIsText(0),
			label + u" code block segment is text"_q,
			ok);
		const auto length = article->segmentLength(0);
		Check(
			length >= 4,
			label + u" code block segment length"_q,
			ok);
		if (!article->segmentIsText(0) || (length < 4)) {
			return;
		}
		const auto checkExport = [&](const TextForMimeData &exported,
				const QString &selectionLabel) {
			auto preCount = 0;
			const auto pre = [&] {
				const EntityInText *result = nullptr;
				for (const auto &entity : exported.rich.entities) {
					if (entity.type() != EntityType::Pre) {
						continue;
					}
					++preCount;
					if (!result) {
						result = &entity;
					}
				}
				return result;
			}();
			Check(
				preCount == 1,
				selectionLabel + u" exports exactly one pre entity"_q,
				ok);
			Check(
				pre != nullptr
					&& pre->offset() == 0
					&& pre->length() == exported.rich.text.size(),
				selectionLabel + u" pre entity spans exported text"_q,
				ok);
			Check(
				pre != nullptr && pre->data() == expectedLanguage,
				selectionLabel + u" pre entity keeps language"_q,
				ok);
		};
		checkExport(
			article->textForSelection({
				.from = { .segment = 0, .offset = 0 },
				.to = { .segment = 0, .offset = length },
			}, nullptr),
			label + u" full selection"_q);
		checkExport(
			article->textForSelection({
				.from = { .segment = 0, .offset = 1 },
				.to = { .segment = 0, .offset = length - 1 },
			}, nullptr),
			label + u" partial selection"_q);
	};

	const auto markdownLabel = u"generated-code-block-selection-language"_q;
	const auto markdownParsed = ParseMarkdownForIv(QByteArray(R"(```cpp ignored-token
alpha
beta
```
)"), ParseOptions{ markdownLabel });
	Check(
		markdownParsed.ok,
		markdownLabel + u" parse failed: "_q + markdownParsed.error,
		ok);
	if (markdownParsed.ok) {
		auto markdownPrepared = PrepareParsedDocumentForTest(
			markdownParsed.document,
			markdownLabel,
			renderer);
		Check(
			!markdownPrepared.failure.failed(),
			markdownLabel + u" prepare failed: "_q
				+ PrepareFailureReason(markdownPrepared.failure),
			ok);
		if (!markdownPrepared.failure.failed()) {
			auto markdownArticleHeight = 0;
			checkSelectionExport(
				BuildArticleForTest(
					std::move(markdownPrepared),
					renderer,
					420,
					&markdownArticleHeight),
				u"cpp"_q,
				markdownLabel);
		}
	}

	const auto nativeLabel = u"native-iv-code-block-selection-language"_q;
	auto nativeBlocks = QVector<MTPPageBlock>();
	nativeBlocks.push_back(MTP_pageBlockPreformatted(
		NativeIvText(u"alpha\nbeta\n"_q),
		MTP_string("  rust  ")));
	auto nativeSource = NativeIvSource(std::move(nativeBlocks));
	auto nativePrepared = TryPrepareNativeInstantView({
		.source = &nativeSource,
	});
	Check(
		nativePrepared.supported(),
		nativeLabel + u" prepare supported"_q,
		ok);
	Check(
		!nativePrepared.content.failure.failed(),
		nativeLabel + u" prepare failed"_q,
		ok);
	if (nativePrepared.supported() && !nativePrepared.content.failure.failed()) {
		auto nativeArticleHeight = 0;
		checkSelectionExport(
			BuildArticleForTest(
				std::move(nativePrepared.content),
				renderer,
				420,
				&nativeArticleHeight),
			u"rust"_q,
			nativeLabel);
	}
}

void CheckCodeBlockAsyncSyntaxHighlightCoverage(bool *ok) {
	const auto label = u"generated-code-block-async-highlight"_q;
	const auto parsed = ParseMarkdownForIv(QByteArray(R"(```cpp
namespace phase3_async_highlight_unique_z {
auto value = 42;
auto text = "native-markdown-iv-phase-z-highlight";
}
```

```cpp
namespace phase3_async_highlight_unique_z {
auto value = 42;
auto text = "native-markdown-iv-phase-z-highlight";
}
```
)"), ParseOptions{ label });
	Check(
		parsed.ok,
		label + u" parse failed: "_q + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	auto renderer = std::make_shared<MathRenderer>();
	auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		renderer);
	Check(
		!prepared.failure.failed(),
		label + u" prepare failed: "_q
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	auto height = 0;
	auto article = BuildArticleForTest(
		std::move(prepared),
		renderer,
		420,
		&height);
	auto highlightWaiter = MarkdownArticleHighlightWaiter(article.get());
	const auto firstImage = PaintArticleForSyntaxHighlightTest(
		article.get(),
		420,
		height);
	Check(
		HasPaintedPixels(firstImage),
		label + u" first paint produced pixels"_q,
		ok);
	const auto firstBounds = SegmentHitBounds(article.get(), 420, height, 0);
	const auto repeatedBounds = SegmentHitBounds(article.get(), 420, height, 1);
	Check(
		firstBounds.has_value(),
		label + u" code block segment hit bounds"_q,
		ok);
	Check(
		repeatedBounds.has_value(),
		label + u" repeated code block segment hit bounds"_q,
		ok);
	const auto highlightCompleted = highlightWaiter.wait();
	Check(
		highlightCompleted,
		label + u" async highlight completion"_q,
		ok);
	if (!firstBounds || !repeatedBounds || !highlightCompleted) {
		return;
	}
	const auto heightAfterHighlight = article->resizeGetHeight(420);
	const auto secondBounds = SegmentHitBounds(
		article.get(),
		420,
		heightAfterHighlight,
		0);
	const auto repeatedSecondBounds = SegmentHitBounds(
		article.get(),
		420,
		heightAfterHighlight,
		1);
	const auto secondImage = PaintArticleForSyntaxHighlightTest(
		article.get(),
		420,
		heightAfterHighlight);
	Check(
		HasPaintedPixels(secondImage),
		label + u" second paint produced pixels"_q,
		ok);
	Check(
		heightAfterHighlight == height,
		label + u" highlight keeps article height"_q,
		ok);
	Check(
		secondBounds.has_value() && (*secondBounds == *firstBounds),
		label + u" highlight keeps code block bounds"_q,
		ok);
	Check(
		repeatedSecondBounds.has_value()
			&& (*repeatedSecondBounds == *repeatedBounds),
		label + u" highlight keeps repeated code block bounds"_q,
		ok);
	Check(
		PixelsDifferInRect(firstImage, secondImage, *firstBounds),
		label + u" highlight repaint changes code block pixels"_q,
		ok);
	Check(
		PixelsDifferInRect(firstImage, secondImage, *repeatedBounds),
		label + u" highlight repaint changes repeated code block pixels"_q,
		ok);
}

void CheckNativeInstantViewArticleCoverage(bool *ok) {
	const auto renderer = std::make_shared<MathRenderer>();
	auto lookupFlags = Ui::Text::StateRequest::Flags();
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupLink;
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	const auto insetRect = [](QRect rect, int margin) {
		const auto inset = rect.marginsRemoved(
			QMargins(margin, margin, margin, margin));
		return inset.isEmpty() ? rect : inset;
	};
	const auto anyPixelInRect = [](
			const QImage &image,
			QRect rect,
			auto &&predicate) {
		rect = rect.intersected(QRect(QPoint(), image.size()));
		if (rect.isEmpty()) {
			return false;
		}
		for (auto y = rect.top(); y <= rect.bottom(); ++y) {
			for (auto x = rect.left(); x <= rect.right(); ++x) {
				if (predicate(image.pixel(x, y))) {
					return true;
				}
			}
		}
		return false;
	};

	const auto videoFixture = NativeIvVideoFixture();
	const auto audioFixture = NativeIvAudioFixture();
	const auto mapFixture = NativeIvMapFixture();
	const auto channelFixture = NativeIvChannelFixture();
	const auto collageFixture = NativeIvCollageFixture();
	const auto slideshowFixture = NativeIvSlideshowFixture();

	const auto relatedArticleLabel = u"native-iv-related-article"_q;
	const auto relatedPhotoId = uint64(9201);
	const auto relatedThumbnailColor = QColor(24, 160, 220);
	auto relatedRuntime = std::make_shared<TestMediaRuntime>();
	auto relatedPhotoRuntime = std::make_shared<TestPhotoRuntime>();
	relatedPhotoRuntime->thumbnailImage = std::make_shared<TestDynamicImage>(
		SolidTestImage(87, 87, relatedThumbnailColor));
	relatedPhotoRuntime->fullImage = std::make_shared<TestDynamicImage>(
		SolidTestImage(174, 174, QColor(12, 110, 180)));
	relatedRuntime->addPhotoRuntime(relatedPhotoId, relatedPhotoRuntime);
	auto relatedSource = NativeIvSource(
		QVector<MTPPageBlock>{
			NativeIvRelatedArticlesBlock(
				u"Related articles"_q,
				{
					NativeIvRelatedArticle(
						u"https://example.com/native-iv-page#section"_q,
						88001,
						u"Native IV article"_q,
						u"Native IV description"_q,
						relatedPhotoId,
						u"Test Author"_q),
					NativeIvRelatedArticle(
						u"https://example.com/external-article"_q,
						0,
						u"External article"_q,
						u"External article description"_q),
				}),
		},
		QVector<MTPPhoto>{ NativeIvPhoto(relatedPhotoId, 87, 87) });
	auto relatedPrepared = TryPrepareNativeInstantView({
		.source = &relatedSource,
		.mediaRuntime = relatedRuntime,
	});
	Check(
		relatedPrepared.supported(),
		relatedArticleLabel + u" prepare supported"_q,
		ok);
	if (relatedPrepared.supported()) {
		auto relatedHeight = 0;
		auto relatedArticle = BuildArticleForTest(
			std::move(relatedPrepared.content),
			renderer,
			420,
			&relatedHeight);
		const auto relatedImage = PaintArticleForTest(
			relatedArticle.get(),
			420,
			relatedHeight);
		Check(
			HasPaintedPixels(relatedImage),
			relatedArticleLabel + u" paint produced pixels"_q,
			ok);
		Check(
			relatedRuntime->photoRequests.size() == 1
				&& (relatedRuntime->photoRequests.front() == relatedPhotoId),
			relatedArticleLabel + u" photo runtime resolve request"_q,
			ok);
		Check(
			!relatedPhotoRuntime->thumbnailSizes.empty()
				&& !relatedPhotoRuntime->fullSizes.empty(),
			relatedArticleLabel + u" thumbnail size requests"_q,
			ok);
		const auto headingBounds = SegmentHitBounds(
			relatedArticle.get(),
			420,
			relatedHeight,
			0);
		const auto internalBounds = HitBoundsWhere(
			relatedArticle.get(),
			420,
			relatedHeight,
			lookupFlags,
			[](const MarkdownArticleHitTestResult &hit) {
				return hit.preparedLink.has_value()
					&& hit.preparedLink->kind
						== PreparedLinkKind::InstantViewPage
					&& (hit.preparedLink->webpageId == 88001);
			});
		const auto externalBounds = HitBoundsWhere(
			relatedArticle.get(),
			420,
			relatedHeight,
			lookupFlags,
			[](const MarkdownArticleHitTestResult &hit) {
				return hit.preparedLink.has_value()
					&& hit.preparedLink->kind == PreparedLinkKind::External
					&& hit.preparedLink->copyText
						== u"https://example.com/external-article"_q;
			});
		Check(
			headingBounds.has_value(),
			relatedArticleLabel + u" heading bounds"_q,
			ok);
		Check(
			internalBounds.has_value(),
			relatedArticleLabel + u" internal related bounds"_q,
			ok);
		Check(
			externalBounds.has_value(),
			relatedArticleLabel + u" external related bounds"_q,
			ok);
		if (headingBounds && internalBounds && externalBounds) {
			Check(
				internalBounds->top() > headingBounds->bottom(),
				relatedArticleLabel + u" internal article below heading"_q,
				ok);
			Check(
				externalBounds->top() > internalBounds->bottom(),
				relatedArticleLabel + u" external article below internal"_q,
				ok);
			Check(
				anyPixelInRect(
					relatedImage,
					*internalBounds,
					[&](QRgb pixel) {
						return pixel == relatedThumbnailColor.rgba();
					}),
				relatedArticleLabel + u" thumbnail paint"_q,
				ok);
		}
		if (internalBounds) {
			const auto internalHit = relatedArticle->hitTest(
				internalBounds->center(),
				lookupFlags);
			Check(
				internalHit.preparedLink.has_value()
					&& internalHit.preparedLink->kind
						== PreparedLinkKind::InstantViewPage
					&& (internalHit.preparedLink->webpageId == 88001),
				relatedArticleLabel + u" internal hit prepared link"_q,
				ok);
			Check(
				!relatedArticle->segmentIsText(internalHit.segmentIndex),
				relatedArticleLabel + u" related row remains block segment"_q,
				ok);
			const auto internalContext = relatedArticle->textForContext(
				internalHit);
			Check(
				internalContext.expanded
					== u"Native IV article\nNative IV description\nTest Author"_q,
				relatedArticleLabel + u" internal context export"_q,
				ok);
			const auto internalSelection = relatedArticle->textForSelection({
				.from = { .segment = internalHit.segmentIndex, .offset = 0 },
				.to = { .segment = internalHit.segmentIndex, .offset = 1 },
			}, nullptr);
			Check(
				internalSelection.expanded == internalContext.expanded,
				relatedArticleLabel + u" internal selection export"_q,
				ok);
		}
		if (externalBounds) {
			const auto externalHit = relatedArticle->hitTest(
				externalBounds->center(),
				lookupFlags);
			Check(
				externalHit.preparedLink.has_value()
					&& externalHit.preparedLink->kind
						== PreparedLinkKind::External,
				relatedArticleLabel + u" external hit prepared link"_q,
				ok);
			const auto externalContext = relatedArticle->textForContext(
				externalHit);
			Check(
				externalContext.expanded
					== u"External article\nExternal article description"_q,
				relatedArticleLabel + u" external context export"_q,
				ok);
		}
	}

	const auto mixedLabel = u"native-iv-mixed-article"_q;
	auto mixedRuntime = std::make_shared<TestMediaRuntime>();
	auto inlineImage = std::make_shared<TestDynamicImage>();
	auto videoRuntime = std::make_shared<TestDocumentRuntime>();
	videoRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	videoRuntime->fullImage = std::make_shared<TestDynamicImage>();
	videoRuntime->loadingValue = true;
	videoRuntime->progressValue = 0.25;
	auto mixedPhotoRuntime = std::make_shared<TestPhotoRuntime>();
	mixedPhotoRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	mixedPhotoRuntime->fullImage = std::make_shared<TestDynamicImage>();
	mixedPhotoRuntime->loadingValue = true;
	mixedPhotoRuntime->progressValue = 0.5;
	mixedRuntime->addInlineImage(8801, inlineImage);
	mixedRuntime->addDocumentRuntime(7001, videoRuntime);
	mixedRuntime->addPhotoRuntime(9101, mixedPhotoRuntime);

	auto mixedBlocks = QVector<MTPPageBlock>();
	mixedBlocks.push_back(NativeIvInlineImageParagraph(
		8801,
		24,
		18,
		u"Intro "_q,
		u" tail."_q));
	mixedBlocks.push_back(videoFixture.block);
	mixedBlocks.push_back(NativeIvPhotoBlock(9101));
	mixedBlocks.push_back(MTP_pageBlockParagraph(NativeIvText(
		u"Outro paragraph."_q)));
	auto mixedPhotos = QVector<MTPPhoto>();
	mixedPhotos.push_back(NativeIvPhoto(9101, 320, 180));
	auto mixedSource = NativeIvSource(
		std::move(mixedBlocks),
		std::move(mixedPhotos),
		videoFixture.documents);
	auto mixedPrepared = TryPrepareNativeInstantView({
		.source = &mixedSource,
		.mediaRuntime = mixedRuntime,
	});
	Check(
		mixedPrepared.supported(),
		mixedLabel + u" prepare supported"_q,
		ok);
	if (!mixedPrepared.supported()) {
		return;
	}
	Check(
		mixedPrepared.content.mediaRuntime.get() == mixedRuntime.get(),
		mixedLabel + u" media runtime forwarded"_q,
		ok);
	auto wideHeight = 0;
	auto mixedArticle = BuildArticleForTest(
		std::move(mixedPrepared.content),
		renderer,
		520,
		&wideHeight);
	const auto wideImage = PaintArticleForTest(
		mixedArticle.get(),
		520,
		wideHeight);
	Check(
		HasPaintedPixels(wideImage),
		mixedLabel + u" wide paint produced pixels"_q,
		ok);
	Check(
		!mixedRuntime->inlineRequests.empty(),
		mixedLabel + u" inline image resolve request"_q,
		ok);
	if (!mixedRuntime->inlineRequests.empty()) {
		Check(
			mixedRuntime->inlineRequests.front().first == 8801,
			mixedLabel + u" inline image resolved document id"_q,
			ok);
		Check(
			mixedRuntime->inlineRequests.front().second == QSize(24, 18),
			mixedLabel + u" inline image resolved size"_q,
			ok);
	}
	Check(
		!mixedRuntime->documentRequests.empty()
			&& (mixedRuntime->documentRequests.front() == 7001),
		mixedLabel + u" video runtime resolve request"_q,
		ok);
	Check(
		!mixedRuntime->photoRequests.empty()
			&& (mixedRuntime->photoRequests.front() == 9101),
		mixedLabel + u" photo runtime resolve request"_q,
		ok);
	Check(
		!videoRuntime->thumbnailSizes.empty()
			&& !videoRuntime->fullSizes.empty(),
		mixedLabel + u" video image size requests"_q,
		ok);
	Check(
		!mixedPhotoRuntime->thumbnailSizes.empty()
			&& !mixedPhotoRuntime->fullSizes.empty(),
		mixedLabel + u" photo image size requests"_q,
		ok);
	Check(
		mixedArticle->segmentIsText(0),
		mixedLabel + u" inline-image paragraph remains text segment"_q,
		ok);
	if (mixedArticle->segmentIsText(0)) {
		const auto exported = mixedArticle->textForSelection({
			.from = { .segment = 0, .offset = 0 },
			.to = {
				.segment = 0,
				.offset = mixedArticle->segmentLength(0),
			},
		}, nullptr);
		Check(
			exported.expanded == u"Intro [image] tail."_q,
			mixedLabel + u" inline-image export text"_q,
			ok);
		Check(
			!HasEntityType(exported.rich.entities, EntityType::CustomEmoji),
			mixedLabel + u" inline-image export drops custom entity"_q,
			ok);
	}

	auto mixedBlockHost = ArticleMediaBlockHost();
	auto inlineFallbackRepaintCount = 0;
	auto inlineFallbackRepaintRects = std::vector<QRect>();
	mixedArticle->setMediaBlockHost(&mixedBlockHost);
	mixedArticle->setTextRepaintCallbacks(
		[&] {
			++inlineFallbackRepaintCount;
		},
		[&](QRect articleRect) {
			inlineFallbackRepaintRects.push_back(articleRect);
		});
	const auto narrowHeight = mixedArticle->resizeGetHeight(260);
	const auto narrowImage = PaintArticleForTest(
		mixedArticle.get(),
		260,
		narrowHeight);
	Check(
		HasPaintedPixels(narrowImage),
		mixedLabel + u" narrow paint produced pixels"_q,
		ok);
	Check(
		narrowHeight > wideHeight,
		mixedLabel + u" narrow relayout grows height"_q,
		ok);
	Check(
		mixedRuntime->documentRequests.size() == 1,
		mixedLabel + u" video runtime persists across relayout"_q,
		ok);
	Check(
		mixedRuntime->photoRequests.size() == 1,
		mixedLabel + u" photo runtime persists across relayout"_q,
		ok);
	Check(
		videoRuntime->thumbnailSizes.size() == 2
			&& videoRuntime->fullSizes.size() == 2
			&& (videoRuntime->thumbnailSizes.front()
				!= videoRuntime->thumbnailSizes.back())
			&& (videoRuntime->fullSizes.front()
				!= videoRuntime->fullSizes.back()),
		mixedLabel + u" video images refresh across relayout"_q,
		ok);
	Check(
		mixedPhotoRuntime->thumbnailSizes.size() == 2
			&& mixedPhotoRuntime->fullSizes.size() == 2
			&& (mixedPhotoRuntime->thumbnailSizes.front()
				!= mixedPhotoRuntime->thumbnailSizes.back())
			&& (mixedPhotoRuntime->fullSizes.front()
				!= mixedPhotoRuntime->fullSizes.back()),
		mixedLabel + u" photo images refresh across relayout"_q,
		ok);
	Check(
		inlineImage->subscriptionCount >= 1,
		mixedLabel + u" inline image subscribed for repaint"_q,
		ok);
	Check(
		!inlineImage->requestedSizes.empty()
			&& (inlineImage->requestedSizes.front() == 24),
		mixedLabel + u" inline image paint request size"_q,
		ok);

	auto segmentBounds = std::vector<std::optional<QRect>>();
	for (auto segmentIndex = 0; segmentIndex != 4; ++segmentIndex) {
		segmentBounds.push_back(SegmentHitBounds(
			mixedArticle.get(),
			260,
			narrowHeight,
			segmentIndex));
		Check(
			segmentBounds.back().has_value(),
			mixedLabel + u" segment hit bounds "_q
				+ QString::number(segmentIndex),
			ok);
	}
	auto haveBounds = true;
	for (const auto &bounds : segmentBounds) {
		if (!bounds) {
			haveBounds = false;
			break;
		}
	}
	if (!haveBounds) {
		return;
	}
	for (auto segmentIndex = 1; segmentIndex != 4; ++segmentIndex) {
		Check(
			segmentBounds[segmentIndex]->top()
				> segmentBounds[segmentIndex - 1]->bottom(),
			mixedLabel + u" segment order "_q + QString::number(segmentIndex),
			ok);
	}
	mixedBlockHost.reset();
	videoRuntime->fullImage->notify();
	Check(
		mixedBlockHost.relayoutRects.empty(),
		mixedLabel + u" media update avoids relayout request"_q,
		ok);
	Check(
		mixedBlockHost.repaintRects.size() == 1,
		mixedLabel + u" media update requests one block repaint"_q,
		ok);
	if (mixedBlockHost.repaintRects.size() == 1) {
		Check(
			mixedBlockHost.repaintRects.front().contains(
				segmentBounds[1]->center()),
			mixedLabel + u" media repaint covers video block"_q,
			ok);
		Check(
			!mixedBlockHost.repaintRects.front().contains(
				segmentBounds[2]->center()),
			mixedLabel + u" media repaint stays local"_q,
			ok);
	}
	inlineFallbackRepaintCount = 0;
	inlineFallbackRepaintRects.clear();
	inlineImage->notify();
	Check(
		inlineFallbackRepaintCount == 0,
		mixedLabel + u" inline image repaint prefers rect callback"_q,
		ok);
	Check(
		inlineFallbackRepaintRects.size() == 1,
		mixedLabel + u" inline image repaint emits one rect"_q,
		ok);
	if (inlineFallbackRepaintRects.size() == 1) {
		Check(
			inlineFallbackRepaintRects.front().size() == QSize(24, 18),
			mixedLabel + u" inline image repaint rect keeps image size"_q,
			ok);
		Check(
			segmentBounds[0]->contains(
				inlineFallbackRepaintRects.front().center()),
			mixedLabel + u" inline image repaint stays inside paragraph"_q,
			ok);
	}

	const auto videoHit = mixedArticle->hitTest(
		segmentBounds[1]->center(),
		lookupFlags);
	Check(
		videoHit.valid() && videoHit.direct && (videoHit.segmentIndex == 1),
		mixedLabel + u" video direct hit"_q,
		ok);
	Check(
		videoHit.mediaActivation.kind == MediaActivationKind::Document,
		mixedLabel + u" video activation kind"_q,
		ok);
	Check(
		videoHit.mediaActivation.document.get() == videoRuntime.get(),
		mixedLabel + u" video runtime forwarded to hit"_q,
		ok);
	Check(
		!videoHit.preparedLink.has_value(),
		mixedLabel + u" video hit has no prepared link"_q,
		ok);
	const auto videoContext = mixedArticle->textForContext(videoHit);
	Check(
		videoContext.expanded
			== (tr::lng_in_dlg_video(tr::now)
				+ u"\n"_q
				+ videoFixture.caption),
		mixedLabel + u" video context export text"_q,
		ok);
	const auto videoSelection = mixedArticle->textForSelection({
		.from = { .segment = 1, .offset = 0 },
		.to = { .segment = 1, .offset = 1 },
	}, nullptr);
	Check(
		videoSelection.expanded == videoContext.expanded,
		mixedLabel + u" video selection export text"_q,
		ok);
	if (videoHit.mediaActivation.document) {
		videoHit.mediaActivation.document->open(Qt::LeftButton);
	}
	Check(
		videoRuntime->openedButtons.size() == 1
			&& (videoRuntime->openedButtons.front() == Qt::LeftButton),
		mixedLabel + u" video open intent"_q,
		ok);

	const auto photoHit = mixedArticle->hitTest(
		segmentBounds[2]->center(),
		lookupFlags);
	Check(
		photoHit.valid() && photoHit.direct && (photoHit.segmentIndex == 2),
		mixedLabel + u" photo direct hit"_q,
		ok);
	Check(
		photoHit.mediaActivation.kind == MediaActivationKind::Photo,
		mixedLabel + u" photo activation kind"_q,
		ok);
	Check(
		photoHit.mediaActivation.photo.get() == mixedPhotoRuntime.get(),
		mixedLabel + u" photo runtime forwarded to hit"_q,
		ok);
	if (photoHit.mediaActivation.photo) {
		photoHit.mediaActivation.photo->open(Qt::LeftButton);
	}
	Check(
		mixedPhotoRuntime->openedButtons.size() == 1
			&& (mixedPhotoRuntime->openedButtons.front() == Qt::LeftButton),
		mixedLabel + u" photo open intent"_q,
		ok);

	inlineImage->setFrame(SolidTestImage(24, 18, QColor(0, 160, 220)));
	inlineImage->notify();
	videoRuntime->thumbnailImage->setFrame(SolidTestImage(
		160,
		90,
		QColor(20, 120, 220)));
	videoRuntime->fullImage->setFrame(SolidTestImage(
		320,
		180,
		QColor(0, 120, 90)));
	videoRuntime->loadingValue = false;
	videoRuntime->loadedValue = true;
	videoRuntime->progressValue = 1.;
	mixedPhotoRuntime->thumbnailImage->setFrame(SolidTestImage(
		160,
		90,
		QColor(220, 160, 0)));
	mixedPhotoRuntime->fullImage->setFrame(SolidTestImage(
		320,
		180,
		QColor(0, 200, 90)));
	mixedPhotoRuntime->loadingValue = false;
	mixedPhotoRuntime->loadedValue = true;
	mixedPhotoRuntime->progressValue = 1.;

	const auto narrowHeightAfterUpdate = mixedArticle->resizeGetHeight(260);
	Check(
		narrowHeightAfterUpdate == narrowHeight,
		mixedLabel + u" media repaint keeps layout height"_q,
		ok);
	const auto updatedImage = PaintArticleForTest(
		mixedArticle.get(),
		260,
		narrowHeightAfterUpdate);
	Check(
		HasPaintedPixels(updatedImage),
		mixedLabel + u" updated paint produced pixels"_q,
		ok);
	const auto updatedVideoBounds = SegmentHitBounds(
		mixedArticle.get(),
		260,
		narrowHeightAfterUpdate,
		1);
	const auto updatedPhotoBounds = SegmentHitBounds(
		mixedArticle.get(),
		260,
		narrowHeightAfterUpdate,
		2);
	const auto updatedInlineBounds = SegmentHitBounds(
		mixedArticle.get(),
		260,
		narrowHeightAfterUpdate,
		0);
	Check(
		updatedVideoBounds.has_value()
			&& (*updatedVideoBounds == *segmentBounds[1]),
		mixedLabel + u" video repaint keeps bounds"_q,
		ok);
	Check(
		updatedPhotoBounds.has_value()
			&& (*updatedPhotoBounds == *segmentBounds[2]),
		mixedLabel + u" photo repaint keeps bounds"_q,
		ok);
	Check(
		updatedInlineBounds.has_value()
			&& (*updatedInlineBounds == *segmentBounds[0]),
		mixedLabel + u" inline-image repaint keeps bounds"_q,
		ok);
	const auto restoredWideHeight = mixedArticle->resizeGetHeight(520);
	const auto restoredWideImage = PaintArticleForTest(
		mixedArticle.get(),
		520,
		restoredWideHeight);
	Check(
		HasPaintedPixels(restoredWideImage),
		mixedLabel + u" restored wide paint produced pixels"_q,
		ok);
	Check(
		mixedRuntime->documentRequests.size() == 1,
		mixedLabel + u" loaded video runtime survives width restore"_q,
		ok);
	Check(
		mixedRuntime->photoRequests.size() == 1,
		mixedLabel + u" loaded photo runtime survives width restore"_q,
		ok);
	Check(
		(videoRuntime->thumbnailSizes.size() >= 2)
			&& (videoRuntime->thumbnailSizes.size() <= 3)
			&& (videoRuntime->fullSizes.size() >= 2)
			&& (videoRuntime->fullSizes.size() <= 3)
			&& videoRuntime->thumbnailImage->subscriptionCount == 1
			&& videoRuntime->fullImage->subscriptionCount == 1,
		mixedLabel + u" loaded video images stay stable across width restore"_q,
		ok);
	Check(
		(mixedPhotoRuntime->thumbnailSizes.size() >= 2)
			&& (mixedPhotoRuntime->thumbnailSizes.size() <= 3)
			&& (mixedPhotoRuntime->fullSizes.size() >= 2)
			&& (mixedPhotoRuntime->fullSizes.size() <= 3)
			&& mixedPhotoRuntime->thumbnailImage->subscriptionCount == 1
			&& mixedPhotoRuntime->fullImage->subscriptionCount == 1,
		mixedLabel + u" loaded photo images stay stable across width restore"_q,
		ok);

	const auto tableLabel = u"native-iv-table-article"_q;
	enum : int {
		kTableCaptionSegment = 0,
		kTableWholeSegment = 1,
		kTableStubSegment = 2,
		kTableHeaderCenterSegment = 3,
		kTableHeaderRightSegment = 4,
		kTableMergedBodySegment = 5,
		kTableTailSegment = 6,
	};
	auto tableSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Merged native table"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(
						u"Stub"_q,
						1,
						2,
						true),
					NativeIvTableCell(
						u"A"_q,
						1,
						1,
						true,
						TableAlignment::Center),
					NativeIvTableCell(
						u"B"_q,
						1,
						1,
						true,
						TableAlignment::Right),
				}),
				NativeIvTableRow({
					NativeIvTableCell(
						u"Bottom merged"_q,
						2,
						2,
						false,
						TableAlignment::Center,
						PreparedTableCellVerticalAlignment::Bottom),
				}),
				NativeIvTableRow({
					NativeIvTableCell(u"Tail"_q),
				}),
			},
			true,
			true),
	});
	auto tablePrepared = TryPrepareNativeInstantView({
		.source = &tableSource,
	});
	Check(tablePrepared.supported(), tableLabel + u" prepare supported"_q, ok);
	if (tablePrepared.supported()) {
		auto tableHeight = 0;
		auto tableArticle = BuildArticleForTest(
			std::move(tablePrepared.content),
			renderer,
			360,
			&tableHeight);
		const auto tableImage = PaintArticleForTest(
			tableArticle.get(),
			360,
			tableHeight);
		Check(
			HasPaintedPixels(tableImage),
			tableLabel + u" paint produced pixels"_q,
			ok);
		auto segmentBounds = std::vector<std::optional<QRect>>();
		for (auto segmentIndex = 0; segmentIndex != 7; ++segmentIndex) {
			segmentBounds.push_back(SegmentHitBounds(
				tableArticle.get(),
				360,
				tableHeight,
				segmentIndex));
			Check(
				segmentBounds.back().has_value(),
				tableLabel + u" segment bounds "_q + QString::number(segmentIndex),
				ok);
		}
		Check(
			tableArticle->segmentIsText(kTableCaptionSegment),
			tableLabel + u" caption segment is text"_q,
			ok);
		Check(
			!tableArticle->segmentIsText(kTableWholeSegment),
			tableLabel + u" whole table segment is block"_q,
			ok);
		Check(
			tableArticle->segmentIsText(kTableStubSegment)
				&& tableArticle->segmentIsText(kTableHeaderCenterSegment)
				&& tableArticle->segmentIsText(kTableHeaderRightSegment)
				&& tableArticle->segmentIsText(kTableMergedBodySegment)
				&& tableArticle->segmentIsText(kTableTailSegment),
			tableLabel + u" logical cell segments are text"_q,
			ok);
		Check(
			!tableArticle->segmentIsText(7)
				&& (tableArticle->segmentLength(7) == 0),
			tableLabel + u" expected segment count"_q,
			ok);
		auto haveAllBounds = true;
		for (const auto &bounds : segmentBounds) {
			if (!bounds) {
				haveAllBounds = false;
				break;
			}
		}
		if (haveAllBounds) {
			Check(
				segmentBounds[kTableCaptionSegment]->bottom()
					< segmentBounds[kTableWholeSegment]->top(),
				tableLabel + u" caption precedes table grid"_q,
				ok);
			Check(
				segmentBounds[kTableWholeSegment]->contains(
					segmentBounds[kTableStubSegment]->center())
					&& segmentBounds[kTableWholeSegment]->contains(
						segmentBounds[kTableMergedBodySegment]->center())
					&& segmentBounds[kTableWholeSegment]->contains(
						segmentBounds[kTableTailSegment]->center()),
				tableLabel + u" whole table contains logical cells"_q,
				ok);
			const auto mergedBodyPainted = PaintedBoundsInRect(
				tableImage,
				insetRect(*segmentBounds[kTableMergedBodySegment], 4));
			Check(
				mergedBodyPainted.has_value(),
				tableLabel + u" merged body painted bounds"_q,
				ok);
			if (mergedBodyPainted) {
				Check(
					mergedBodyPainted->center().y()
						> segmentBounds[kTableMergedBodySegment]->center().y(),
					tableLabel + u" bottom-aligned text stays low in merged rect"_q,
					ok);
			}
			const auto headerBg = st::defaultMarkdown.table.headerBg->c.rgba();
			Check(
				anyPixelInRect(
					tableImage,
					insetRect(*segmentBounds[kTableTailSegment], 4),
					[&](QRgb pixel) { return pixel == headerBg; }),
				tableLabel + u" striped row tint"_q,
				ok);
			Check(
				!anyPixelInRect(
					tableImage,
					insetRect(*segmentBounds[kTableMergedBodySegment], 4),
					[&](QRgb pixel) { return pixel == headerBg; }),
				tableLabel + u" unstriped merged body avoids tint"_q,
				ok);
		}
		const auto wholeTableSelection = tableArticle->textForSelection({
			.from = { .segment = kTableCaptionSegment, .offset = 0 },
			.to = { .segment = kTableWholeSegment, .offset = 1 },
		}, nullptr);
		Check(
			wholeTableSelection.expanded
				== u"Merged native table\nStub\tA\tB\n\tBottom merged\t\nTail\t\t"_q,
			tableLabel + u" whole-table export includes caption once"_q,
			ok);
		const auto mergedCellSelection = tableArticle->textForSelection({
			.from = { .segment = kTableMergedBodySegment, .offset = 0 },
			.to = {
				.segment = kTableMergedBodySegment,
				.offset = tableArticle->segmentLength(kTableMergedBodySegment),
			},
		}, nullptr);
		Check(
			mergedCellSelection.expanded == u"Bottom merged"_q,
			tableLabel + u" single merged cell export"_q,
			ok);
	}

	const auto salvagedTableLabel = u"native-iv-salvaged-table-article"_q;
	enum : int {
		kSalvagedTableCaptionSegment = 0,
		kSalvagedTableWholeSegment = 1,
		kSalvagedTableASegment = 2,
		kSalvagedTableBSegment = 3,
		kSalvagedTableCSegment = 4,
		kSalvagedTableDSegment = 5,
		kSalvagedTableESegment = 6,
	};
	auto salvagedTableSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			u"Short row occupancy"_q,
			{
				NativeIvTableRow({
					NativeIvTableCell(u"A"_q, 1, 4),
					NativeIvTableCell(u"B"_q, 1, 4),
				}),
				NativeIvTableRow({}),
				NativeIvTableRow({
					NativeIvTableCell(u"C"_q),
					NativeIvTableCell(u"D"_q),
					NativeIvTableCell(u"E"_q, 1, 2),
				}),
				NativeIvTableRow({}),
			},
			false,
			false),
	});
	auto salvagedTablePrepared = TryPrepareNativeInstantView({
		.source = &salvagedTableSource,
	});
	Check(
		salvagedTablePrepared.supported(),
		salvagedTableLabel + u" prepare supported"_q,
		ok);
	if (salvagedTablePrepared.supported()) {
		auto salvagedTableHeight = 0;
		auto salvagedTableArticle = BuildArticleForTest(
			std::move(salvagedTablePrepared.content),
			renderer,
			360,
			&salvagedTableHeight);
		const auto salvagedTableImage = PaintArticleForTest(
			salvagedTableArticle.get(),
			360,
			salvagedTableHeight);
		Check(
			HasPaintedPixels(salvagedTableImage),
			salvagedTableLabel + u" paint produced pixels"_q,
			ok);
		auto segmentBounds = std::vector<std::optional<QRect>>();
		for (auto segmentIndex = 0; segmentIndex != 7; ++segmentIndex) {
			segmentBounds.push_back(SegmentHitBounds(
				salvagedTableArticle.get(),
				360,
				salvagedTableHeight,
				segmentIndex));
			Check(
				segmentBounds.back().has_value(),
				salvagedTableLabel + u" segment bounds "_q
					+ QString::number(segmentIndex),
				ok);
		}
		Check(
			salvagedTableArticle->segmentIsText(kSalvagedTableCaptionSegment)
				&& !salvagedTableArticle->segmentIsText(
					kSalvagedTableWholeSegment)
				&& salvagedTableArticle->segmentIsText(kSalvagedTableASegment)
				&& salvagedTableArticle->segmentIsText(kSalvagedTableBSegment)
				&& salvagedTableArticle->segmentIsText(kSalvagedTableCSegment)
				&& salvagedTableArticle->segmentIsText(kSalvagedTableDSegment)
				&& salvagedTableArticle->segmentIsText(kSalvagedTableESegment),
			salvagedTableLabel + u" segment kinds"_q,
			ok);
		Check(
			!salvagedTableArticle->segmentIsText(7)
				&& (salvagedTableArticle->segmentLength(7) == 0),
			salvagedTableLabel + u" expected segment count"_q,
			ok);
		auto haveAllBounds = true;
		for (const auto &bounds : segmentBounds) {
			if (!bounds) {
				haveAllBounds = false;
				break;
			}
		}
		if (haveAllBounds) {
			Check(
				segmentBounds[kSalvagedTableCaptionSegment]->bottom()
					< segmentBounds[kSalvagedTableWholeSegment]->top(),
				salvagedTableLabel + u" caption precedes table grid"_q,
				ok);
			Check(
				segmentBounds[kSalvagedTableWholeSegment]->contains(
					segmentBounds[kSalvagedTableASegment]->center())
					&& segmentBounds[kSalvagedTableWholeSegment]->contains(
						segmentBounds[kSalvagedTableBSegment]->center())
					&& segmentBounds[kSalvagedTableWholeSegment]->contains(
						segmentBounds[kSalvagedTableCSegment]->center())
					&& segmentBounds[kSalvagedTableWholeSegment]->contains(
						segmentBounds[kSalvagedTableDSegment]->center())
					&& segmentBounds[kSalvagedTableWholeSegment]->contains(
						segmentBounds[kSalvagedTableESegment]->center()),
				salvagedTableLabel + u" whole table contains logical cells"_q,
				ok);
		}
		const auto wholeTableSelection = salvagedTableArticle->textForSelection({
			.from = { .segment = kSalvagedTableCaptionSegment, .offset = 0 },
			.to = { .segment = kSalvagedTableWholeSegment, .offset = 1 },
		}, nullptr);
		Check(
			wholeTableSelection.expanded
				== u"Short row occupancy\nA\tB\t\t\t\n\t\t\t\t\n\t\tC\tD\tE\n\t\t\t\t"_q,
			salvagedTableLabel + u" whole-table export preserves sparse grid"_q,
			ok);
	}

	const auto borderlessTableLabel = u"native-iv-borderless-table-article"_q;
	auto borderlessTableSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvTableBlock(
			QString(),
			{
				NativeIvTableRow({
					NativeIvTableCell(u"H1"_q, 1, 1, true),
					NativeIvTableCell(u"H2"_q, 1, 1, true),
					NativeIvTableCell(u"H3"_q, 1, 1, true),
				}),
				NativeIvTableRow({
					NativeIvTableCell(u"Left"_q),
					NativeIvTableCell(u"Wide"_q, 2, 2),
				}),
				NativeIvTableRow({
					NativeIvTableCell(u"Tail"_q),
				}),
			},
			false,
			false),
	});
	auto borderlessPrepared = TryPrepareNativeInstantView({
		.source = &borderlessTableSource,
	});
	Check(
		borderlessPrepared.supported(),
		borderlessTableLabel + u" prepare supported"_q,
		ok);
	if (borderlessPrepared.supported()) {
		auto borderlessHeight = 0;
		auto borderlessArticle = BuildArticleForTest(
			std::move(borderlessPrepared.content),
			renderer,
			360,
			&borderlessHeight);
		const auto borderlessImage = PaintArticleForTest(
			borderlessArticle.get(),
			360,
			borderlessHeight);
		Check(
			HasPaintedPixels(borderlessImage),
			borderlessTableLabel + u" paint produced pixels"_q,
			ok);
		const auto headerCenterBounds = SegmentHitBounds(
			borderlessArticle.get(),
			360,
			borderlessHeight,
			2);
		const auto headerRightBounds = SegmentHitBounds(
			borderlessArticle.get(),
			360,
			borderlessHeight,
			3);
		const auto wideBounds = SegmentHitBounds(
			borderlessArticle.get(),
			360,
			borderlessHeight,
			5);
		Check(
			headerCenterBounds.has_value()
				&& headerRightBounds.has_value()
				&& wideBounds.has_value(),
			borderlessTableLabel + u" merged-cell bounds"_q,
			ok);
		if (headerCenterBounds && headerRightBounds && wideBounds) {
			const auto boundaryX = headerRightBounds->left();
			const auto coveredStrip = QRect(
				boundaryX - 1,
				wideBounds->center().y() - 6,
				3,
				13).intersected(insetRect(*wideBounds, 4));
			Check(
				!coveredStrip.isEmpty(),
				borderlessTableLabel + u" covered strip bounds"_q,
				ok);
			if (!coveredStrip.isEmpty()) {
				Check(
					!PaintedBoundsInRect(borderlessImage, coveredStrip).has_value(),
					borderlessTableLabel
						+ u" borderless merged slot has no separator pixels"_q,
					ok);
			}
		}
	}

	const auto audioLabel = u"native-iv-audio-article"_q;
	auto audioRuntime = std::make_shared<TestMediaRuntime>();
	auto audioDocumentRuntime = std::make_shared<TestDocumentRuntime>();
	audioRuntime->addDocumentRuntime(7002, audioDocumentRuntime);
	auto audioSource = NativeIvSource(
		QVector<MTPPageBlock>{ audioFixture.block },
		audioFixture.photos,
		audioFixture.documents);
	auto audioPrepared = TryPrepareNativeInstantView({
		.source = &audioSource,
		.mediaRuntime = audioRuntime,
	});
	Check(audioPrepared.supported(), audioLabel + u" prepare supported"_q, ok);
	if (audioPrepared.supported()) {
		auto audioHeight = 0;
		auto audioArticle = BuildArticleForTest(
			std::move(audioPrepared.content),
			renderer,
			420,
			&audioHeight);
		const auto audioImage = PaintArticleForTest(
			audioArticle.get(),
			420,
			audioHeight);
		Check(
			HasPaintedPixels(audioImage),
			audioLabel + u" paint produced pixels"_q,
			ok);
		Check(
			audioRuntime->documentRequests.size() == 1
				&& (audioRuntime->documentRequests.front() == 7002),
			audioLabel + u" document resolve request"_q,
			ok);
		const auto audioBounds = SegmentHitBounds(
			audioArticle.get(),
			420,
			audioHeight,
			0);
		Check(audioBounds.has_value(), audioLabel + u" media bounds"_q, ok);
		if (audioBounds) {
			const auto audioHit = audioArticle->hitTest(
				audioBounds->center(),
				lookupFlags);
			Check(
				audioHit.valid()
					&& audioHit.direct
					&& (audioHit.segmentIndex == 0),
				audioLabel + u" direct hit"_q,
				ok);
			Check(
				audioHit.mediaActivation.kind == MediaActivationKind::Document,
				audioLabel + u" activation kind"_q,
				ok);
			Check(
				audioHit.mediaActivation.document.get()
					== audioDocumentRuntime.get(),
				audioLabel + u" runtime forwarded"_q,
				ok);
			const auto audioContext = audioArticle->textForContext(audioHit);
			Check(
				audioContext.expanded
					== u"Song Title\nSample Artist\nAudio caption"_q,
				audioLabel + u" context export text"_q,
				ok);
			const auto audioSelection = audioArticle->textForSelection({
				.from = { .segment = 0, .offset = 0 },
				.to = { .segment = 0, .offset = 1 },
			}, nullptr);
			Check(
				audioSelection.expanded == audioContext.expanded,
				audioLabel + u" selection export text"_q,
				ok);
			if (audioHit.mediaActivation.document) {
				audioHit.mediaActivation.document->open(Qt::LeftButton);
			}
			Check(
				audioDocumentRuntime->openedButtons.size() == 1
					&& (audioDocumentRuntime->openedButtons.front()
						== Qt::LeftButton),
				audioLabel + u" open intent"_q,
				ok);
		}
		const auto audioCaptionSelection = audioArticle->textForSelection({
			.from = { .segment = 1, .offset = 0 },
			.to = {
				.segment = 1,
				.offset = audioArticle->segmentLength(1),
			},
		}, nullptr);
		Check(
			audioCaptionSelection.expanded == audioFixture.caption,
			audioLabel + u" caption selection export"_q,
			ok);
	}

	const auto mapLabel = u"native-iv-map-article"_q;
	auto mapRuntime = std::make_shared<TestMediaRuntime>();
	auto mapImageRuntime = std::make_shared<TestMapRuntime>();
	mapImageRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	mapImageRuntime->fullImage = std::make_shared<TestDynamicImage>();
	mapRuntime->addMapRuntime(51.5007, -0.1246, 880088, 13, mapImageRuntime);
	auto mapSource = NativeIvSource(QVector<MTPPageBlock>{
		mapFixture.block,
	});
	auto mapPrepared = TryPrepareNativeInstantView({
		.source = &mapSource,
		.mediaRuntime = mapRuntime,
	});
	Check(mapPrepared.supported(), mapLabel + u" prepare supported"_q, ok);
	if (mapPrepared.supported()) {
		auto mapHeight = 0;
		auto mapArticle = BuildArticleForTest(
			std::move(mapPrepared.content),
			renderer,
			420,
			&mapHeight);
		const auto mapImage = PaintArticleForTest(
			mapArticle.get(),
			420,
			mapHeight);
		Check(
			HasPaintedPixels(mapImage),
			mapLabel + u" paint produced pixels"_q,
			ok);
		Check(
			mapRuntime->mapRequests.size() == 1,
			mapLabel + u" runtime resolve request"_q,
			ok);
		if (!mapRuntime->mapRequests.empty()) {
			const auto &request = mapRuntime->mapRequests.front();
			Check(
				request.latitude == 51.5007
					&& request.longitude == -0.1246,
				mapLabel + u" coordinates forwarded"_q,
				ok);
			Check(
				request.accessHash == 880088 && request.zoom == 13,
				mapLabel + u" access hash and zoom forwarded"_q,
				ok);
		}
		const auto mapBounds = SegmentHitBounds(
			mapArticle.get(),
			420,
			mapHeight,
			0);
		Check(mapBounds.has_value(), mapLabel + u" media bounds"_q, ok);
		if (mapBounds) {
			const auto mapHit = mapArticle->hitTest(
				mapBounds->center(),
				lookupFlags);
			Check(
				mapHit.valid() && mapHit.direct && (mapHit.segmentIndex == 0),
				mapLabel + u" direct hit"_q,
				ok);
			Check(
				mapHit.mediaActivation.kind == MediaActivationKind::ExternalUrl,
				mapLabel + u" activation kind"_q,
				ok);
			Check(
				mapHit.preparedLink.has_value()
					&& !mapHit.preparedLink->target.isEmpty(),
				mapLabel + u" prepared link created"_q,
				ok);
			if (mapHit.preparedLink) {
				Check(
					mapHit.mediaActivation.url == mapHit.preparedLink->target,
					mapLabel + u" media activation url matches prepared link"_q,
					ok);
			}
			const auto mapContext = mapArticle->textForContext(mapHit);
			Check(
				mapContext.expanded
					== (tr::lng_maps_point(tr::now)
						+ u"\n"_q
						+ mapFixture.caption),
				mapLabel + u" context export text"_q,
				ok);
			const auto mapSelection = mapArticle->textForSelection({
				.from = { .segment = 0, .offset = 0 },
				.to = { .segment = 0, .offset = 1 },
			}, nullptr);
			Check(
				mapSelection.expanded == mapContext.expanded,
				mapLabel + u" selection export text"_q,
				ok);
		}
		const auto mapCaptionSelection = mapArticle->textForSelection({
			.from = { .segment = 1, .offset = 0 },
			.to = {
				.segment = 1,
				.offset = mapArticle->segmentLength(1),
			},
		}, nullptr);
		Check(
			mapCaptionSelection.expanded == mapFixture.caption,
			mapLabel + u" caption selection export"_q,
			ok);
		auto narrowMapHeight = mapArticle->resizeGetHeight(280);
		const auto narrowMapImage = PaintArticleForTest(
			mapArticle.get(),
			280,
			narrowMapHeight);
		Check(
			HasPaintedPixels(narrowMapImage),
			mapLabel + u" narrow paint produced pixels"_q,
			ok);
		Check(
			mapRuntime->mapRequests.size() == 2
				&& (mapRuntime->mapRequests.front().size
					!= mapRuntime->mapRequests.back().size),
			mapLabel + u" size change refreshes map runtime"_q,
			ok);
		Check(
			mapImageRuntime->thumbnailSizes.size() == 2
				&& mapImageRuntime->fullSizes.size() == 2
				&& (mapImageRuntime->thumbnailSizes.front()
					!= mapImageRuntime->thumbnailSizes.back())
				&& (mapImageRuntime->fullSizes.front()
					!= mapImageRuntime->fullSizes.back()),
			mapLabel + u" size change refreshes map images"_q,
			ok);
		const auto restoredMapHeight = mapArticle->resizeGetHeight(420);
		const auto restoredMapImage = PaintArticleForTest(
			mapArticle.get(),
			420,
			restoredMapHeight);
		Check(
			HasPaintedPixels(restoredMapImage),
			mapLabel + u" restored paint produced pixels"_q,
			ok);
		Check(
			(mapRuntime->mapRequests.size() >= 2)
				&& (mapRuntime->mapRequests.size() <= 3)
				&& mapImageRuntime->thumbnailImage->subscriptionCount == 1
				&& mapImageRuntime->fullImage->subscriptionCount == 1,
			mapLabel + u" map subscriptions stay stable across width restore"_q,
			ok);
	}

	const auto channelLabel = u"native-iv-channel-article"_q;
	auto channelRuntime = std::make_shared<TestMediaRuntime>();
	auto channelCardRuntime = std::make_shared<TestChannelRuntime>();
	channelRuntime->addChannelRuntime(7006, u"nativeiv"_q, channelCardRuntime);
	auto channelSource = NativeIvSource(QVector<MTPPageBlock>{
		channelFixture.block,
	});
	auto channelPrepared = TryPrepareNativeInstantView({
		.source = &channelSource,
		.mediaRuntime = channelRuntime,
	});
	Check(
		channelPrepared.supported(),
		channelLabel + u" prepare supported"_q,
		ok);
	if (channelPrepared.supported()) {
		auto channelHeight = 0;
		auto channelArticle = BuildArticleForTest(
			std::move(channelPrepared.content),
			renderer,
			420,
			&channelHeight);
		const auto channelBefore = PaintArticleForTest(
			channelArticle.get(),
			420,
			channelHeight);
		Check(
			HasPaintedPixels(channelBefore),
			channelLabel + u" paint produced pixels"_q,
			ok);
		Check(
			channelRuntime->channelRequests.size() == 1
				&& (channelRuntime->channelRequests.front().channelId == 7006)
				&& (channelRuntime->channelRequests.front().username
					== u"nativeiv"_q),
			channelLabel + u" runtime resolve request"_q,
			ok);
		const auto openBounds = HitBoundsWhere(
			channelArticle.get(),
			420,
			channelHeight,
			lookupFlags,
			[](const MarkdownArticleHitTestResult &hit) {
				return hit.mediaActivation.kind
					== MediaActivationKind::OpenChannel;
			});
		const auto joinBounds = HitBoundsWhere(
			channelArticle.get(),
			420,
			channelHeight,
			lookupFlags,
			IsChannelJoinLinkHit);
		Check(openBounds.has_value(), channelLabel + u" open bounds"_q, ok);
		Check(joinBounds.has_value(), channelLabel + u" join bounds"_q, ok);
		if (openBounds) {
			const auto openHit = channelArticle->hitTest(
				openBounds->center(),
				lookupFlags);
			Check(
				openHit.mediaActivation.kind == MediaActivationKind::OpenChannel,
				channelLabel + u" open activation kind"_q,
				ok);
			Check(
				openHit.mediaActivation.channel.get() == channelCardRuntime.get(),
				channelLabel + u" open runtime forwarded"_q,
				ok);
			if (openHit.mediaActivation.channel) {
				openHit.mediaActivation.channel->open(Qt::LeftButton);
			}
		}
		if (joinBounds) {
			const auto joinHit = channelArticle->hitTest(
				joinBounds->center(),
				lookupFlags);
			Check(
				IsChannelJoinLinkHit(joinHit),
				channelLabel + u" join link hit"_q,
				ok);
			const auto joinContext = channelArticle->textForContext(joinHit);
			Check(
				joinContext.expanded == u"Native IV Channel"_q,
				channelLabel + u" context export text"_q,
				ok);
			const auto joinSelection = channelArticle->textForSelection({
				.from = { .segment = 0, .offset = 0 },
				.to = { .segment = 0, .offset = 1 },
			}, nullptr);
			Check(
				joinSelection.expanded == joinContext.expanded,
				channelLabel + u" selection export text"_q,
				ok);
			if (joinHit.state.link) {
				joinHit.state.link->onClick({ .button = Qt::LeftButton });
			}
		}
		Check(
			channelCardRuntime->openedButtons.size() == 1
				&& (channelCardRuntime->openedButtons.front()
					== Qt::LeftButton),
			channelLabel + u" open intent"_q,
			ok);
		Check(
			channelCardRuntime->joinedButtons.size() == 1
				&& (channelCardRuntime->joinedButtons.front()
					== Qt::LeftButton),
			channelLabel + u" join intent"_q,
			ok);
		if (joinBounds) {
			channelCardRuntime->joinVisibleValue = false;
			const auto joinedHit = channelArticle->hitTest(
				joinBounds->center(),
				lookupFlags);
			Check(
				joinedHit.mediaActivation.kind == MediaActivationKind::OpenChannel,
				channelLabel + u" joined state hides join action"_q,
				ok);
			const auto channelAfter = PaintArticleForTest(
				channelArticle.get(),
				420,
				channelHeight);
			Check(
				PixelsDifferInRect(channelBefore, channelAfter, *joinBounds),
				channelLabel + u" joined state repaint changes button pixels"_q,
				ok);
		}

		auto previewRuntime = std::make_shared<TestMediaRuntime>();
		auto previewChannelRuntime = std::make_shared<TestChannelRuntime>();
		previewRuntime->addChannelRuntime(
			7006,
			u"nativeiv"_q,
			previewChannelRuntime);
		auto previewSource = NativeIvSource(QVector<MTPPageBlock>{
			channelFixture.block,
		});
		auto previewPrepared = TryPrepareNativeInstantView({
			.source = &previewSource,
			.mediaRuntime = previewRuntime,
		});
		Check(
			previewPrepared.supported(),
			channelLabel + u" preview prepare supported"_q,
			ok);
		if (previewPrepared.supported()) {
			auto window = Ui::RpWindow();
			window.setGeometry(QRect(0, 0, 420, 240));
			window.show();
			FlushPendingWidgetEvents();
			auto preview = CreateMarkdownPreviewWidget(
				window.body(),
				std::move(previewPrepared.content),
				renderer,
				[](Event) {
				});
			preview->setGeometry(QRect(QPoint(), window.body()->size()));
			preview->show();
			FlushPendingWidgetEvents();
			const auto body = FindChildObject<MarkdownDocumentWidget>(
				preview.get());
			Check(
				body != nullptr,
				channelLabel + u" preview body widget"_q,
				ok);
			if (body) {
				auto counter = WidgetEventCounter();
				body->installEventFilter(&counter);
				const auto previewBefore = RenderWidgetForTest(body);
				counter.reset();
				previewChannelRuntime->joinVisibleValue = false;
				previewRuntime->fireChannelJoinedChange(7006);
				FlushPendingWidgetEvents();
				const auto repainted = (counter.paintCount > 0)
					|| (counter.updateRequestCount > 0);
				const auto previewAfter = RenderWidgetForTest(body);
				Check(
					repainted,
					channelLabel + u" joined change triggers preview repaint"_q,
					ok);
				Check(
					PixelsDifferInRect(
						previewBefore,
						previewAfter,
						QRect(QPoint(), previewAfter.size())),
					channelLabel + u" preview repaint changes rendered pixels"_q,
					ok);
				body->removeEventFilter(&counter);
			}
		}
	}

	const auto channelRelayoutLabel = u"native-iv-channel-relayout"_q;
	const auto relayoutChannelId = uint64(7007);
	const auto relayoutChannelUsername = u"nativeinstantviewrelayout"_q;
	const auto relayoutChannelBlock = MTP_pageBlockChannel(NativeIvChannelChat(
		relayoutChannelId,
		u"Native Instant View Channel Title That Wraps When The Join Button Is Visible"_q,
		relayoutChannelUsername));
	auto joinedLayoutRuntime = std::make_shared<TestMediaRuntime>();
	auto joinedLayoutChannelRuntime = std::make_shared<TestChannelRuntime>();
	joinedLayoutChannelRuntime->joinVisibleValue = false;
	joinedLayoutRuntime->addChannelRuntime(
		relayoutChannelId,
		relayoutChannelUsername,
		joinedLayoutChannelRuntime);
	auto joinedLayoutSource = NativeIvSource(QVector<MTPPageBlock>{
		relayoutChannelBlock,
	});
	auto joinedLayoutPrepared = TryPrepareNativeInstantView({
		.source = &joinedLayoutSource,
		.mediaRuntime = joinedLayoutRuntime,
	});
	Check(
		joinedLayoutPrepared.supported(),
		channelRelayoutLabel + u" hidden prepare supported"_q,
		ok);
	auto joinedLayoutHeight = 0;
	if (joinedLayoutPrepared.supported()) {
		auto joinedLayoutArticle = BuildArticleForTest(
			std::move(joinedLayoutPrepared.content),
			renderer,
			320,
			&joinedLayoutHeight);
		const auto joinedLayoutJoinBounds = HitBoundsWhere(
			joinedLayoutArticle.get(),
			320,
			joinedLayoutHeight,
			lookupFlags,
			IsChannelJoinLinkHit);
		Check(
			!joinedLayoutJoinBounds.has_value(),
			channelRelayoutLabel + u" hidden layout has no join bounds"_q,
			ok);
	}

	auto articleRelayoutRuntime = std::make_shared<TestMediaRuntime>();
	auto articleRelayoutChannelRuntime = std::make_shared<TestChannelRuntime>();
	articleRelayoutRuntime->addChannelRuntime(
		relayoutChannelId,
		relayoutChannelUsername,
		articleRelayoutChannelRuntime);
	auto articleRelayoutSource = NativeIvSource(QVector<MTPPageBlock>{
		relayoutChannelBlock,
	});
	auto articleRelayoutPrepared = TryPrepareNativeInstantView({
		.source = &articleRelayoutSource,
		.mediaRuntime = articleRelayoutRuntime,
	});
	Check(
		articleRelayoutPrepared.supported(),
		channelRelayoutLabel + u" article prepare supported"_q,
		ok);
	if (articleRelayoutPrepared.supported() && (joinedLayoutHeight > 0)) {
		auto articleRelayoutHeight = 0;
		auto articleRelayout = BuildArticleForTest(
			std::move(articleRelayoutPrepared.content),
			renderer,
			320,
			&articleRelayoutHeight);
		const auto articleJoinBounds = HitBoundsWhere(
			articleRelayout.get(),
			320,
			articleRelayoutHeight,
			lookupFlags,
			IsChannelJoinLinkHit);
		Check(
			articleJoinBounds.has_value(),
			channelRelayoutLabel + u" visible layout has join bounds"_q,
			ok);
		articleRelayoutChannelRuntime->joinVisibleValue = false;
		articleRelayout->invalidateLayout();
		articleRelayoutHeight = articleRelayout->resizeGetHeight(320);
		const auto articleJoinedBounds = HitBoundsWhere(
			articleRelayout.get(),
			320,
			articleRelayoutHeight,
			lookupFlags,
			IsChannelJoinLinkHit);
		Check(
			!articleJoinedBounds.has_value(),
			channelRelayoutLabel
				+ u" invalidated relayout hides join bounds"_q,
			ok);
		Check(
			articleRelayoutHeight == joinedLayoutHeight,
			channelRelayoutLabel
				+ u" invalidated relayout matches hidden layout"_q,
			ok);
	}

	auto previewRelayoutRuntime = std::make_shared<TestMediaRuntime>();
	auto previewRelayoutChannelRuntime = std::make_shared<TestChannelRuntime>();
	previewRelayoutRuntime->addChannelRuntime(
		relayoutChannelId,
		relayoutChannelUsername,
		previewRelayoutChannelRuntime);
	auto previewRelayoutSource = NativeIvSource(QVector<MTPPageBlock>{
		relayoutChannelBlock,
	});
	auto previewRelayoutPrepared = TryPrepareNativeInstantView({
		.source = &previewRelayoutSource,
		.mediaRuntime = previewRelayoutRuntime,
	});
	Check(
		previewRelayoutPrepared.supported(),
		channelRelayoutLabel + u" preview prepare supported"_q,
		ok);
	if (previewRelayoutPrepared.supported() && (joinedLayoutHeight > 0)) {
		auto window = Ui::RpWindow();
		window.setGeometry(QRect(0, 0, 320, 320));
		window.show();
		FlushPendingWidgetEvents();
		auto preview = CreateMarkdownPreviewWidget(
			window.body(),
			std::move(previewRelayoutPrepared.content),
			renderer,
			[](Event) {
			});
		preview->setGeometry(QRect(QPoint(), window.body()->size()));
		preview->show();
		FlushPendingWidgetEvents();
		const auto body = FindChildObject<MarkdownDocumentWidget>(
			preview.get());
		Check(
			body != nullptr,
			channelRelayoutLabel + u" preview body widget"_q,
			ok);
		if (body) {
			auto counter = WidgetEventCounter();
			body->installEventFilter(&counter);
			const auto beforeHeight = body->height();
			counter.reset();
			previewRelayoutChannelRuntime->joinVisibleValue = false;
			previewRelayoutRuntime->fireChannelJoinedChange(relayoutChannelId);
			FlushPendingWidgetEvents();
			const auto repainted = (counter.paintCount > 0)
				|| (counter.updateRequestCount > 0);
			const auto afterHeight = body->height();
			Check(
				beforeHeight > joinedLayoutHeight,
				channelRelayoutLabel + u" join visible height larger"_q,
				ok);
			Check(
				afterHeight == joinedLayoutHeight,
				channelRelayoutLabel
					+ u" joined change relayout matches hidden layout"_q,
				ok);
			Check(
				repainted,
				channelRelayoutLabel + u" joined change triggers preview repaint"_q,
				ok);
			body->removeEventFilter(&counter);
		}
	}

	const auto collageLabel = u"native-iv-collage-article"_q;
	auto collageRuntime = std::make_shared<TestMediaRuntime>();
	auto collagePhotoRuntime = std::make_shared<TestPhotoRuntime>();
	collagePhotoRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	collagePhotoRuntime->fullImage = std::make_shared<TestDynamicImage>();
	auto collageDocumentRuntime = std::make_shared<TestDocumentRuntime>();
	collageDocumentRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	collageDocumentRuntime->fullImage = std::make_shared<TestDynamicImage>();
	collageRuntime->addPhotoRuntime(9102, collagePhotoRuntime);
	collageRuntime->addDocumentRuntime(7003, collageDocumentRuntime);
	auto collageSource = NativeIvSource(
		QVector<MTPPageBlock>{ collageFixture.block },
		collageFixture.photos,
		collageFixture.documents);
	auto collagePrepared = TryPrepareNativeInstantView({
		.source = &collageSource,
		.mediaRuntime = collageRuntime,
	});
	Check(
		collagePrepared.supported(),
		collageLabel + u" prepare supported"_q,
		ok);
	if (collagePrepared.supported()) {
		auto collageHeight = 0;
		auto collageArticle = BuildArticleForTest(
			std::move(collagePrepared.content),
			renderer,
			460,
			&collageHeight);
		const auto collageImage = PaintArticleForTest(
			collageArticle.get(),
			460,
			collageHeight);
		Check(
			HasPaintedPixels(collageImage),
			collageLabel + u" paint produced pixels"_q,
			ok);
		Check(
			collageRuntime->photoRequests.size() == 1
				&& (collageRuntime->photoRequests.front() == 9102),
			collageLabel + u" photo resolve request"_q,
			ok);
		Check(
			collageRuntime->documentRequests.size() == 1
				&& (collageRuntime->documentRequests.front() == 7003),
			collageLabel + u" document resolve request"_q,
			ok);
		const auto collagePhotoBounds = HitBoundsWhere(
			collageArticle.get(),
			460,
			collageHeight,
			lookupFlags,
			[&](const MarkdownArticleHitTestResult &hit) {
				return hit.mediaActivation.kind == MediaActivationKind::Photo
					&& hit.mediaActivation.photo.get()
						== collagePhotoRuntime.get();
			});
		const auto collageVideoBounds = HitBoundsWhere(
			collageArticle.get(),
			460,
			collageHeight,
			lookupFlags,
			[&](const MarkdownArticleHitTestResult &hit) {
				return hit.mediaActivation.kind == MediaActivationKind::Document
					&& hit.mediaActivation.document.get()
						== collageDocumentRuntime.get();
			});
		Check(
			collagePhotoBounds.has_value(),
			collageLabel + u" photo item bounds"_q,
			ok);
		Check(
			collageVideoBounds.has_value(),
			collageLabel + u" video item bounds"_q,
			ok);
		if (collagePhotoBounds) {
			const auto hit = collageArticle->hitTest(
				collagePhotoBounds->center(),
				lookupFlags);
			if (hit.mediaActivation.photo) {
				hit.mediaActivation.photo->open(Qt::LeftButton);
			}
			const auto context = collageArticle->textForContext(hit);
			Check(
				context.expanded == collageFixture.caption,
				collageLabel + u" context export text"_q,
				ok);
		}
		if (collageVideoBounds) {
			const auto hit = collageArticle->hitTest(
				collageVideoBounds->center(),
				lookupFlags);
			if (hit.mediaActivation.document) {
				hit.mediaActivation.document->open(Qt::LeftButton);
			}
		}
		Check(
			collagePhotoRuntime->openedButtons.size() == 1,
			collageLabel + u" photo open intent"_q,
			ok);
		Check(
			collageDocumentRuntime->openedButtons.size() == 1,
			collageLabel + u" video open intent"_q,
			ok);
		const auto collageSelection = collageArticle->textForSelection({
			.from = { .segment = 0, .offset = 0 },
			.to = { .segment = 0, .offset = 1 },
		}, nullptr);
		Check(
			collageSelection.expanded == collageFixture.caption,
			collageLabel + u" selection export text"_q,
			ok);
		const auto collageCaptionSelection = collageArticle->textForSelection({
			.from = { .segment = 1, .offset = 0 },
			.to = {
				.segment = 1,
				.offset = collageArticle->segmentLength(1),
			},
		}, nullptr);
		Check(
			collageCaptionSelection.expanded == collageFixture.caption,
			collageLabel + u" caption selection export"_q,
			ok);
	}

	const auto slideshowLabel = u"native-iv-slideshow-article"_q;
	auto slideshowRuntime = std::make_shared<TestMediaRuntime>();
	auto slideshowDocumentA = std::make_shared<TestDocumentRuntime>();
	slideshowDocumentA->thumbnailImage = std::make_shared<TestDynamicImage>();
	slideshowDocumentA->fullImage = std::make_shared<TestDynamicImage>();
	auto slideshowDocumentB = std::make_shared<TestDocumentRuntime>();
	slideshowDocumentB->thumbnailImage = std::make_shared<TestDynamicImage>();
	slideshowDocumentB->fullImage = std::make_shared<TestDynamicImage>();
	slideshowRuntime->addDocumentRuntime(7004, slideshowDocumentA);
	slideshowRuntime->addDocumentRuntime(7005, slideshowDocumentB);
	auto slideshowSource = NativeIvSource(
		QVector<MTPPageBlock>{ slideshowFixture.block },
		slideshowFixture.photos,
		slideshowFixture.documents);
	auto slideshowPrepared = TryPrepareNativeInstantView({
		.source = &slideshowSource,
		.mediaRuntime = slideshowRuntime,
	});
	Check(
		slideshowPrepared.supported(),
		slideshowLabel + u" prepare supported"_q,
		ok);
	if (slideshowPrepared.supported()) {
		auto slideshowHeight = 0;
		auto slideshowArticle = BuildArticleForTest(
			std::move(slideshowPrepared.content),
			renderer,
			460,
			&slideshowHeight);
		const auto slideshowImage = PaintArticleForTest(
			slideshowArticle.get(),
			460,
			slideshowHeight);
		Check(
			HasPaintedPixels(slideshowImage),
			slideshowLabel + u" paint produced pixels"_q,
			ok);
		Check(
			(slideshowRuntime->documentRequests.size() == 1)
				&& (slideshowRuntime->documentRequests.front() == 7004),
			slideshowLabel + u" resolves only the active slide"_q,
			ok);
		const auto slideshowBlockBounds = HitBoundsWhere(
			slideshowArticle.get(),
			460,
			slideshowHeight,
			lookupFlags,
			[&](const MarkdownArticleHitTestResult &hit) {
				return hit.segmentIndex == 0;
			});
		Check(
			slideshowBlockBounds.has_value(),
			slideshowLabel + u" slideshow media bounds"_q,
			ok);
		const auto slideshowMediaBounds = HitBoundsWhere(
			slideshowArticle.get(),
			460,
			slideshowHeight,
			lookupFlags,
			[&](const MarkdownArticleHitTestResult &hit) {
				return (hit.segmentIndex == 0)
					&& (hit.mediaActivation.kind == MediaActivationKind::Document);
			});
		Check(
			slideshowMediaBounds.has_value(),
			slideshowLabel + u" slideshow active slide bounds"_q,
			ok);
		if (slideshowMediaBounds && slideshowBlockBounds) {
			const auto hit = slideshowArticle->hitTest(
				slideshowMediaBounds->center(),
				lookupFlags);
			Check(
				hit.mediaActivation.kind == MediaActivationKind::Document,
				slideshowLabel + u" active slide activation"_q,
				ok);
			if (hit.mediaActivation.document) {
				hit.mediaActivation.document->open(Qt::LeftButton);
			}
			Check(
				slideshowDocumentA->openedButtons.size() == 1,
				slideshowLabel + u" first slide open intent"_q,
				ok);
			const auto context = slideshowArticle->textForContext(hit);
			Check(
				context.expanded
					== (tr::lng_media_selected_video(tr::now, lt_count, 2)
						+ u"\n"_q
						+ slideshowFixture.caption),
				slideshowLabel + u" context export text"_q,
				ok);
			const auto &grouped = st::defaultMarkdown.groupedMedia;
			const auto navButtonSize = std::min({
				grouped.navButtonSize,
				std::max(slideshowBlockBounds->height(), 0),
				std::max(
					(slideshowBlockBounds->width() - 2 * grouped.navButtonSkip) / 2,
					0),
			});
			const auto nextPoint = QPoint(
				slideshowBlockBounds->x()
					+ slideshowBlockBounds->width()
					- grouped.navButtonSkip
					- (navButtonSize / 2),
				slideshowBlockBounds->center().y());
			const auto nextHit = slideshowArticle->hitTest(nextPoint, lookupFlags);
			Check(
				(nextHit.state.link != nullptr)
					&& (nextHit.mediaActivation.kind == MediaActivationKind::None),
				slideshowLabel + u" next nav uses click handler"_q,
				ok);
			if (nextHit.state.link) {
				nextHit.state.link->onClick(ClickContext{
					.button = Qt::LeftButton,
				});
			}
			Check(
				(slideshowRuntime->documentRequests.size() == 2)
					&& (slideshowRuntime->documentRequests.back() == 7005),
				slideshowLabel + u" next nav resolves second slide"_q,
				ok);
			const auto secondHit = slideshowArticle->hitTest(
				slideshowMediaBounds->center(),
				lookupFlags);
			Check(
				secondHit.mediaActivation.document.get() == slideshowDocumentB.get(),
				slideshowLabel + u" second slide becomes active"_q,
				ok);
			if (secondHit.mediaActivation.document) {
				secondHit.mediaActivation.document->open(Qt::LeftButton);
			}
			Check(
				slideshowDocumentB->openedButtons.size() == 1,
				slideshowLabel + u" second slide open intent"_q,
				ok);
			auto resizedHeight = slideshowArticle->resizeGetHeight(360);
			const auto resizedBounds = HitBoundsWhere(
				slideshowArticle.get(),
				360,
				resizedHeight,
				lookupFlags,
				[&](const MarkdownArticleHitTestResult &resizedHit) {
					return (resizedHit.segmentIndex == 0)
						&& (resizedHit.mediaActivation.kind
							== MediaActivationKind::Document);
				});
			Check(
				resizedBounds.has_value(),
				slideshowLabel + u" resized active slide bounds"_q,
				ok);
			if (resizedBounds) {
				const auto resizedHit = slideshowArticle->hitTest(
					resizedBounds->center(),
					lookupFlags);
				Check(
					resizedHit.mediaActivation.document.get()
						== slideshowDocumentB.get(),
					slideshowLabel + u" active slide persists after relayout"_q,
					ok);
			}
			Check(
				(slideshowRuntime->documentRequests.size() == 2)
					&& (slideshowRuntime->documentRequests.back() == 7005),
				slideshowLabel + u" relayout keeps active slide runtime"_q,
				ok);
			Check(
				slideshowDocumentA->thumbnailSizes.size() == 1
					&& slideshowDocumentA->fullSizes.size() == 1,
				slideshowLabel + u" relayout skips inactive slide images"_q,
				ok);
			Check(
				slideshowDocumentB->thumbnailSizes.size() == 2
					&& slideshowDocumentB->fullSizes.size() == 2
					&& (slideshowDocumentB->thumbnailSizes.front()
						!= slideshowDocumentB->thumbnailSizes.back())
					&& (slideshowDocumentB->fullSizes.front()
						!= slideshowDocumentB->fullSizes.back()),
				slideshowLabel + u" relayout refreshes active slide images"_q,
				ok);
			auto slideshowBlockHost = ArticleMediaBlockHost();
			slideshowArticle->setMediaBlockHost(&slideshowBlockHost);
			const auto slideshowMaxWidth = slideshowArticle->maxWidth();
			Check(
				slideshowMaxWidth > 0,
				slideshowLabel + u" max width computed"_q,
				ok);
			slideshowBlockHost.reset();
			slideshowDocumentB->fullImage->notify();
			Check(
				slideshowBlockHost.repaintRects.size() == 1,
				slideshowLabel + u" max width keeps active slide repaint"_q,
				ok);
			Check(
				slideshowDocumentB->thumbnailImage->subscriptionCount == 1
					&& slideshowDocumentB->fullImage->subscriptionCount == 1,
				slideshowLabel + u" max width avoids transient slide subscriptions"_q,
				ok);
		}
		const auto slideshowSelection = slideshowArticle->textForSelection({
			.from = { .segment = 0, .offset = 0 },
			.to = { .segment = 0, .offset = 1 },
		}, nullptr);
		Check(
			slideshowSelection.expanded
				== (tr::lng_media_selected_video(tr::now, lt_count, 2)
					+ u"\n"_q
					+ slideshowFixture.caption),
			slideshowLabel + u" selection export text"_q,
			ok);
		const auto slideshowCaptionSelection = slideshowArticle->textForSelection({
			.from = { .segment = 1, .offset = 0 },
			.to = {
				.segment = 1,
				.offset = slideshowArticle->segmentLength(1),
			},
		}, nullptr);
		Check(
			slideshowCaptionSelection.expanded == slideshowFixture.caption,
			slideshowLabel + u" caption selection export"_q,
			ok);
	}

	const auto largeSlideshowLabel = u"native-iv-large-slideshow-article"_q;
	const auto largeSlideshowFixture = NativeIvSlideshowFixture(44);
	auto largeSlideshowRuntime = std::make_shared<TestMediaRuntime>();
	auto largeSlideshowDocument = std::make_shared<TestDocumentRuntime>();
	largeSlideshowDocument->thumbnailImage = std::make_shared<TestDynamicImage>();
	largeSlideshowDocument->fullImage = std::make_shared<TestDynamicImage>();
	largeSlideshowRuntime->addDocumentRuntime(7004, largeSlideshowDocument);
	auto largeSlideshowSource = NativeIvSource(
		QVector<MTPPageBlock>{ largeSlideshowFixture.block },
		largeSlideshowFixture.photos,
		largeSlideshowFixture.documents);
	auto largeSlideshowPrepared = TryPrepareNativeInstantView({
		.source = &largeSlideshowSource,
		.mediaRuntime = largeSlideshowRuntime,
	});
	Check(
		largeSlideshowPrepared.supported(),
		largeSlideshowLabel + u" prepare supported"_q,
		ok);
	if (largeSlideshowPrepared.supported()) {
		auto largeSlideshowHeight = 0;
		auto largeSlideshowArticle = BuildArticleForTest(
			std::move(largeSlideshowPrepared.content),
			renderer,
			460,
			&largeSlideshowHeight);
		const auto largeSlideshowImage = PaintArticleForTest(
			largeSlideshowArticle.get(),
			460,
			largeSlideshowHeight);
		Check(
			HasPaintedPixels(largeSlideshowImage),
			largeSlideshowLabel + u" paint produced pixels"_q,
			ok);
		Check(
			(largeSlideshowRuntime->documentRequests.size() == 1)
				&& (largeSlideshowRuntime->documentRequests.front() == 7004),
			largeSlideshowLabel + u" skips grouped-grid bulk resolves"_q,
			ok);
	}
}

void CheckNativeInstantViewEmbedPostAvatarRegression(bool *ok) {
	const auto label = u"native-iv-embed-post-unresolved-author-photo"_q;
	const auto renderer = std::make_shared<MathRenderer>();
	auto runtime = std::make_shared<TestMediaRuntime>();
	auto source = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvEmbedPostBlock(
			NativeIvDefaultEmbedPostBlocks(),
			u"Embed post caption"_q,
			QString(),
			u"https://example.com/embed-post-unresolved"_q,
			u"Unresolved Author"_q,
			kNativeIvEmbedPostDate,
			kNativeIvEmbedPostAuthorPhotoId),
	});
	auto prepared = TryPrepareNativeInstantView({
		.source = &source,
		.mediaRuntime = runtime,
	});
	Check(
		prepared.supported(),
		label + u" prepare supported"_q,
		ok);
	if (!prepared.supported()) {
		return;
	}
	Check(
		!prepared.content.failure.failed(),
		label + u" prepare failure"_q,
		ok);
	if (prepared.content.failure.failed()) {
		return;
	}
	const auto articleWidth = 420;
	auto articleHeight = 0;
	auto article = BuildArticleForTest(
		std::move(prepared.content),
		renderer,
		articleWidth,
		&articleHeight);
	Check(
		runtime->photoRequests.size() == 1
			&& (runtime->photoRequests.front() == kNativeIvEmbedPostAuthorPhotoId),
		label + u" photo runtime resolve request"_q,
		ok);
	const auto authorBounds = SegmentHitBounds(
		article.get(),
		articleWidth,
		articleHeight,
		0);
	const auto dateBounds = SegmentHitBounds(
		article.get(),
		articleWidth,
		articleHeight,
		1);
	const auto bodyBounds = SegmentHitBounds(
		article.get(),
		articleWidth,
		articleHeight,
		2);
	Check(authorBounds.has_value(), label + u" author bounds"_q, ok);
	Check(dateBounds.has_value(), label + u" date bounds"_q, ok);
	Check(bodyBounds.has_value(), label + u" body bounds"_q, ok);
	if (!authorBounds || !dateBounds || !bodyBounds) {
		return;
	}
	const auto expectedTextLeft = st::defaultMarkdown.textPadding.left()
		+ st::defaultMarkdown.embedPost.accentWidth
		+ st::defaultMarkdown.embedPost.accentSkip
		+ st::defaultMarkdown.embedPost.padding.left();
	const auto maxTextLeft = expectedTextLeft
		+ st::defaultMarkdown.embedPost.headerGap;
	const auto authorLeftDelta = authorBounds->left() - bodyBounds->left();
	const auto dateLeftDelta = dateBounds->left() - bodyBounds->left();
	Check(
		(bodyBounds->left() >= expectedTextLeft)
			&& (bodyBounds->left() <= maxTextLeft),
		label + u" body starts at content left"_q,
		ok);
	Check(
		(authorBounds->left() >= expectedTextLeft)
			&& (authorBounds->left() <= maxTextLeft),
		label + u" author starts at content left"_q,
		ok);
	Check(
		(dateBounds->left() >= expectedTextLeft)
			&& (dateBounds->left() <= maxTextLeft),
		label + u" date starts at content left"_q,
		ok);
	Check(
		(authorLeftDelta >= -6) && (authorLeftDelta <= 6),
		label + u" author has no avatar gap"_q,
		ok);
	Check(
		(dateLeftDelta >= -6) && (dateLeftDelta <= 6),
		label + u" date has no avatar gap"_q,
		ok);
}

void CheckNativeInstantViewEmbedPostLoadingAvatarPlaceholderRegression(
		bool *ok) {
	const auto label = u"native-iv-embed-post-loading-avatar-placeholder"_q;
	const auto renderer = std::make_shared<MathRenderer>();
	auto runtime = std::make_shared<TestMediaRuntime>();
	auto photoRuntime = std::make_shared<TestPhotoRuntime>();
	photoRuntime->thumbnailImage = std::make_shared<TestDynamicImage>();
	runtime->addPhotoRuntime(kNativeIvEmbedPostAuthorPhotoId, photoRuntime);
	auto source = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvEmbedPostBlock(
			NativeIvDefaultEmbedPostBlocks(),
			u"Embed post caption"_q,
			QString(),
			u"https://example.com/embed-post-loading-avatar"_q,
			u"Loading Avatar Author"_q,
			kNativeIvEmbedPostDate,
			kNativeIvEmbedPostAuthorPhotoId),
	});
	auto prepared = TryPrepareNativeInstantView({
		.source = &source,
		.mediaRuntime = runtime,
	});
	Check(
		prepared.supported(),
		label + u" prepare supported"_q,
		ok);
	if (!prepared.supported()) {
		return;
	}
	Check(
		!prepared.content.failure.failed(),
		label + u" prepare failure"_q,
		ok);
	if (prepared.content.failure.failed()) {
		return;
	}
	const auto articleWidth = 420;
	auto articleHeight = 0;
	auto article = BuildArticleForTest(
		std::move(prepared.content),
		renderer,
		articleWidth,
		&articleHeight);
	const auto image = PaintArticleForTest(
		article.get(),
		articleWidth,
		articleHeight);
	const auto avatarRect = NativeIvEmbedPostAvatarRect();
	const auto centerRect = QRect(
		avatarRect.center() - QPoint(1, 1),
		QSize(3, 3));
	const auto cornerSize = QSize(4, 4);
	const auto topLeftRect = QRect(avatarRect.topLeft(), cornerSize);
	const auto topRightRect = QRect(
		QPoint(avatarRect.right() - cornerSize.width() + 1, avatarRect.top()),
		cornerSize);
	const auto bottomLeftRect = QRect(
		QPoint(avatarRect.left(), avatarRect.bottom() - cornerSize.height() + 1),
		cornerSize);
	const auto bottomRightRect = QRect(
		QPoint(
			avatarRect.right() - cornerSize.width() + 1,
			avatarRect.bottom() - cornerSize.height() + 1),
		cornerSize);
	Check(
		runtime->photoRequests.size() == 1
			&& (runtime->photoRequests.front() == kNativeIvEmbedPostAuthorPhotoId),
		label + u" photo runtime resolve request"_q,
		ok);
	Check(
		!photoRuntime->thumbnailSizes.empty(),
		label + u" thumbnail size request"_q,
		ok);
	Check(
		PaintedBoundsInRect(image, centerRect).has_value(),
		label + u" placeholder center painted"_q,
		ok);
	Check(
		!PaintedBoundsInRect(image, topLeftRect).has_value(),
		label + u" top-left corner stays circular"_q,
		ok);
	Check(
		!PaintedBoundsInRect(image, topRightRect).has_value(),
		label + u" top-right corner stays circular"_q,
		ok);
	Check(
		!PaintedBoundsInRect(image, bottomLeftRect).has_value(),
		label + u" bottom-left corner stays circular"_q,
		ok);
	Check(
		!PaintedBoundsInRect(image, bottomRightRect).has_value(),
		label + u" bottom-right corner stays circular"_q,
		ok);
}

void CheckNativeInstantViewPreviewOpenPageCoverage(bool *ok) {
	const auto renderer = std::make_shared<MathRenderer>();
	auto lookupFlags = Ui::Text::StateRequest::Flags();
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupLink;
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	const auto runPreviewOpenPageCase = [&](
			QString label,
			Iv::Source *source,
			std::shared_ptr<MediaRuntime> runtime,
			auto &&predicate,
			uint64 expectedPageId,
			QString expectedUrl) {
		const auto prepareRequest = NativeInstantViewPrepareRequest{
			.source = source,
			.mediaRuntime = runtime,
		};
		auto prepared = TryPrepareNativeInstantView(prepareRequest);
		Check(
			prepared.supported(),
			label + u" preview prepare supported"_q,
			ok);
		Check(
			!prepared.content.failure.failed(),
			label + u" preview prepare failure"_q,
			ok);
		if (!prepared.supported() || prepared.content.failure.failed()) {
			return;
		}
		auto window = Ui::RpWindow();
		window.setGeometry(QRect(0, 0, 460, 320));
		window.show();
		FlushPendingWidgetEvents();
		auto events = std::vector<Event>();
		auto preview = CreateMarkdownPreviewWidget(
			window.body(),
			std::move(prepared.content),
			renderer,
			[&](Event event) {
				events.push_back(std::move(event));
			});
		preview->setGeometry(QRect(QPoint(), window.body()->size()));
		preview->show();
		FlushPendingWidgetEvents();
		const auto body = FindChildObject<MarkdownDocumentWidget>(preview.get());
		Check(
			body != nullptr,
			label + u" preview body widget"_q,
			ok);
		if (!body) {
			return;
		}
		auto probePrepared = TryPrepareNativeInstantView(prepareRequest);
		Check(
			probePrepared.supported(),
			label + u" probe prepare supported"_q,
			ok);
		Check(
			!probePrepared.content.failure.failed(),
			label + u" probe prepare failure"_q,
			ok);
		if (!probePrepared.supported() || probePrepared.content.failure.failed()) {
			return;
		}
		auto probeHeight = 0;
		auto probeArticle = BuildArticleForTest(
			std::move(probePrepared.content),
			renderer,
			body->width(),
			&probeHeight);
		const auto clickBounds = HitBoundsWhere(
			probeArticle.get(),
			body->width(),
			probeHeight,
			lookupFlags,
			std::forward<decltype(predicate)>(predicate));
		Check(
			clickBounds.has_value(),
			label + u" click bounds"_q,
			ok);
		if (!clickBounds) {
			return;
		}
		SendMouseClick(body, clickBounds->center(), Qt::LeftButton);
		FlushPendingWidgetEvents();
		Check(
			events.size() == 1,
			label + u" open-page event count"_q,
			ok);
		if (events.size() == 1) {
			Check(
				events.front().type == Event::Type::OpenPage,
				label + u" open-page event type"_q,
				ok);
			Check(
				events.front().webpageId == expectedPageId,
				label + u" open-page id"_q,
				ok);
			Check(
				events.front().url == expectedUrl,
				label + u" open-page url"_q,
				ok);
		}
	};

	const auto inlineLabel = u"native-iv-preview-inline-open-page"_q;
	auto inlineSource = NativeIvSource(QVector<MTPPageBlock>{
		MTP_pageBlockParagraph(NativeIvConcat({
			NativeIvText(u"Open "_q),
			NativeIvTextUrl(
				u"native page"_q,
				u"https://example.com/inline-open#part"_q,
				99001),
			NativeIvText(u" now."_q),
		})),
	});
	runPreviewOpenPageCase(
		inlineLabel,
		&inlineSource,
		nullptr,
		[](const MarkdownArticleHitTestResult &hit) {
			return hit.preparedLink.has_value()
				&& hit.preparedLink->kind == PreparedLinkKind::InstantViewPage
				&& (hit.preparedLink->webpageId == 99001);
		},
		99001,
		UrlClickHandler::EncodeForOpening(
			u"https://example.com/inline-open#part"_q));

	const auto relatedLabel = u"native-iv-preview-related-open-page"_q;
	auto relatedSource = NativeIvSource(QVector<MTPPageBlock>{
		NativeIvRelatedArticlesBlock(
			u"Related articles"_q,
			{
				NativeIvRelatedArticle(
					u"https://example.com/related-open#jump"_q,
					99002,
					u"Related native article"_q,
					u"Preview row"_q),
			}),
	});
	runPreviewOpenPageCase(
		relatedLabel,
		&relatedSource,
		nullptr,
		[](const MarkdownArticleHitTestResult &hit) {
			return hit.preparedLink.has_value()
				&& hit.preparedLink->kind == PreparedLinkKind::InstantViewPage
				&& (hit.preparedLink->webpageId == 99002);
		},
		99002,
		UrlClickHandler::EncodeForOpening(
			u"https://example.com/related-open#jump"_q));
}

void CheckPrepareCoverage(
		const PreparedFixture &markdownFixture,
		const PreparedFixture &latexFixture,
		bool *ok) {
	const auto &markdown = markdownFixture.prepared;
	const auto &latex = latexFixture.prepared;

	auto markdownTables = std::vector<const PreparedBlock*>();
	const PreparedBlock *markdownDisplayMath = nullptr;
	const PreparedBlock *detailsBlock = nullptr;
	auto footnoteDefinitionListFound = false;
	auto footnoteDefinitionAnchorFound = false;
	const auto isFootnoteDefinitionAnchor = [](const QString &anchorId) {
		return anchorId == FromLatin1("fn-1")
			|| anchorId == FromLatin1("fn-2");
	};
	ForEachPreparedBlock(markdown.blocks.blocks, [&](const PreparedBlock &block) {
		if (block.kind == PreparedBlockKind::Table) {
			markdownTables.push_back(&block);
		}
		if (!markdownDisplayMath
			&& block.kind == PreparedBlockKind::DisplayMath
			&& block.formulaTex.trimmed()
				== FromLatin1("\\int_0^1 x^2\\,dx = \\frac{1}{3}")) {
			markdownDisplayMath = &block;
		}
		if (!detailsBlock && block.kind == PreparedBlockKind::Details) {
			detailsBlock = &block;
		}
		if (isFootnoteDefinitionAnchor(block.anchorId)) {
			footnoteDefinitionAnchorFound = true;
		}
		if (block.kind == PreparedBlockKind::List) {
			for (const auto &child : block.children) {
				if (isFootnoteDefinitionAnchor(child.anchorId)) {
					footnoteDefinitionListFound = true;
				}
			}
		}
	});
	Check(
		markdownDisplayMath != nullptr,
		FromLatin1("markdown-example.md prepared display math formulaTex"),
		ok);
	Check(
		markdownTables.size() >= 2,
		FromLatin1("markdown-example.md prepared table count"),
		ok);
	if (markdownTables.size() >= 2) {
		const auto &firstTable = *markdownTables[0];
		const auto &secondTable = *markdownTables[1];
		Check(
			firstTable.tableColumnCount == 3,
			FromLatin1("markdown-example.md prepared first table column count"),
			ok);
		Check(
			firstTable.tableRows.size() == 4,
			FromLatin1("markdown-example.md prepared first table row count"),
			ok);
		if (firstTable.tableRows.size() == 4) {
			Check(
				firstTable.tableRows[0].header,
				FromLatin1("markdown-example.md prepared first table header row"),
				ok);
			Check(
				firstTable.tableRows[0].cells.size() == 3
					&& firstTable.tableRows[1].cells.size() == 3,
				FromLatin1("markdown-example.md prepared first table cell shape"),
				ok);
		}
		Check(
			secondTable.tableAlignments.size() == 3
				&& secondTable.tableAlignments[0] == TableAlignment::Left
				&& secondTable.tableAlignments[1] == TableAlignment::Center
				&& secondTable.tableAlignments[2] == TableAlignment::Right,
			FromLatin1("markdown-example.md prepared second table alignments"),
			ok);
	}
	Check(
		detailsBlock != nullptr,
		FromLatin1("markdown-example.md prepared details block"),
		ok);
	if (detailsBlock) {
		Check(
			detailsBlock->collapsed,
			FromLatin1("markdown-example.md prepared details collapsed"),
			ok);
		Check(
			detailsBlock->anchorId.startsWith(FromLatin1("details-")),
			FromLatin1("markdown-example.md prepared details anchor id"),
			ok);
		Check(
			detailsBlock->text.text
				== FromLatin1("Click to expand details/summary block"),
			FromLatin1("markdown-example.md prepared details summary text"),
			ok);
		Check(
			detailsBlock->links.empty()
				&& !HasEntityType(
					detailsBlock->text.entities,
					EntityType::CustomUrl),
			FromLatin1("markdown-example.md prepared details summary toggle"),
			ok);
		Check(
			!detailsBlock->children.empty()
				&& detailsBlock->children[0].kind == PreparedBlockKind::Paragraph
				&& detailsBlock->children[0].text.text.contains(
					FromLatin1("Hidden content inside details.")),
			FromLatin1("markdown-example.md prepared details body"),
			ok);
	}
	Check(
		!footnoteDefinitionListFound,
		FromLatin1("markdown-example.md no prepared footnote list"),
		ok);
	Check(
		!footnoteDefinitionAnchorFound,
		FromLatin1("markdown-example.md no prepared footnote anchors"),
		ok);
	const auto footnoteOne = FindPreparedFootnote(markdown, FromLatin1("1"));
	const auto longFootnote = FindPreparedFootnote(
		markdown,
		FromLatin1("long-note"));
	Check(
		footnoteOne != nullptr
			&& footnoteOne->displayText == FromLatin1("[1]"),
		FromLatin1("markdown-example.md prepared footnote one display"),
		ok);
	Check(
		longFootnote != nullptr
			&& longFootnote->displayText == FromLatin1("[long-note]"),
		FromLatin1("markdown-example.md prepared long footnote display"),
		ok);

	auto normalFootnoteBacklinkFound = false;
	ForEachPreparedLink(markdown.blocks.blocks, [&](const PreparedLink &link) {
		if (link.kind == PreparedLinkKind::FootnoteBacklink) {
			normalFootnoteBacklinkFound = true;
		}
	});
	auto preparedFootnoteBacklinkFound = false;
	ForEachPreparedFootnoteLink(markdown, [&](const PreparedLink &link) {
		if (link.kind == PreparedLinkKind::FootnoteBacklink) {
			preparedFootnoteBacklinkFound = true;
		}
	});
	Check(
		!normalFootnoteBacklinkFound,
		FromLatin1("markdown-example.md no normal footnote backlink"),
		ok);
	Check(
		!preparedFootnoteBacklinkFound,
		FromLatin1("markdown-example.md no prepared footnote backlink"),
		ok);

	const auto firstReferenceText = FromLatin1("[1]");
	const auto firstReferenceParagraph = FindPreparedParagraphContaining(
		markdown,
		FromLatin1("Footnote reference one."));
	Check(
		firstReferenceParagraph != nullptr,
		FromLatin1("markdown-example.md first prepared footnote paragraph"),
		ok);
	if (firstReferenceParagraph) {
		const auto &text = firstReferenceParagraph->text;
		Check(
			text.text.contains(FromLatin1("Footnote reference one.[1]")),
			FromLatin1("markdown-example.md first footnote display text"),
			ok);
		const auto offset = text.text.indexOf(firstReferenceText);
		Check(
			offset >= 0,
			FromLatin1("markdown-example.md first footnote text range"),
			ok);
		if (offset >= 0) {
			const auto length = firstReferenceText.size();
			Check(
				HasEntityRange(text, EntityType::Superscript, offset, length),
				FromLatin1("markdown-example.md first footnote superscript"),
				ok);
			Check(
				HasEntityRange(text, EntityType::CustomUrl, offset, length),
				FromLatin1("markdown-example.md first footnote custom url"),
				ok);
			const auto link = FindPreparedLinkByCustomUrlRange(
				text,
				firstReferenceParagraph->links,
				offset,
				length);
			Check(
				link != nullptr
					&& link->kind == PreparedLinkKind::Footnote
					&& link->target == FromLatin1("1")
					&& link->copyText == FromLatin1("[1]"),
				FromLatin1("markdown-example.md first footnote prepared link"),
				ok);
		}
	}

	const auto secondReferenceText = FromLatin1("[long-note]");
	const auto secondReferenceParagraph = FindPreparedParagraphContaining(
		markdown,
		FromLatin1("Footnote reference two with more text."));
	Check(
		secondReferenceParagraph != nullptr,
		FromLatin1("markdown-example.md second prepared footnote paragraph"),
		ok);
	if (secondReferenceParagraph) {
		const auto &text = secondReferenceParagraph->text;
		Check(
			text.text.contains(secondReferenceText),
			FromLatin1("markdown-example.md second footnote display text"),
			ok);
		Check(
			!text.text.contains(FromLatin1("[2]")),
			FromLatin1("markdown-example.md second footnote no ordinal display"),
			ok);
		const auto offset = text.text.indexOf(secondReferenceText);
		Check(
			offset >= 0,
			FromLatin1("markdown-example.md second footnote text range"),
			ok);
		if (offset >= 0) {
			const auto length = secondReferenceText.size();
			Check(
				HasEntityRange(text, EntityType::Superscript, offset, length),
				FromLatin1("markdown-example.md second footnote superscript"),
				ok);
			Check(
				HasEntityRange(text, EntityType::CustomUrl, offset, length),
				FromLatin1("markdown-example.md second footnote custom url"),
				ok);
			const auto link = FindPreparedLinkByCustomUrlRange(
				text,
				secondReferenceParagraph->links,
				offset,
				length);
			Check(
				link != nullptr
					&& link->kind == PreparedLinkKind::Footnote
					&& link->target == FromLatin1("long-note")
					&& link->copyText == FromLatin1("[long-note]"),
				FromLatin1("markdown-example.md second footnote prepared link"),
				ok);
		}
	}

	if (longFootnote) {
		const auto &text = longFootnote->text;
		Check(
			text.text.contains(FromLatin1(
				"This is a longer footnote with formatting, code, and a link.")),
			FromLatin1("markdown-example.md long footnote text"),
			ok);
		Check(
			HasEntityType(text.entities, EntityType::Bold),
			FromLatin1("markdown-example.md long footnote bold entity"),
			ok);
		Check(
			HasEntityType(text.entities, EntityType::Code),
			FromLatin1("markdown-example.md long footnote code entity"),
			ok);
		Check(
			HasEntityType(text.entities, EntityType::CustomUrl),
			FromLatin1("markdown-example.md long footnote custom url entity"),
			ok);
		auto externalLinkFound = false;
		for (const auto &link : longFootnote->links) {
			if (link.kind == PreparedLinkKind::External
				&& (link.target.contains(FromLatin1("https://example.com"))
					|| link.copyText.contains(
						FromLatin1("https://example.com")))) {
				externalLinkFound = true;
			}
		}
		Check(
			externalLinkFound,
			FromLatin1("markdown-example.md long footnote external link"),
			ok);
	}

	auto latexTables = std::vector<const PreparedBlock*>();
	ForEachPreparedBlock(latex.blocks.blocks, [&](const PreparedBlock &block) {
		if (block.kind == PreparedBlockKind::Table) {
			latexTables.push_back(&block);
		}
	});
	Check(
		!latexTables.empty(),
		FromLatin1("latex-markdown-test.md prepared table count"),
		ok);
	if (!latexTables.empty()) {
		const auto &table = *latexTables[0];
		Check(
			table.tableColumnCount == 3,
			FromLatin1("latex-markdown-test.md prepared table column count"),
			ok);
		Check(
			table.tableRows.size() == 5,
			FromLatin1("latex-markdown-test.md prepared table row count"),
			ok);
		if (table.tableRows.size() == 5) {
			Check(
				table.tableRows[0].header,
				FromLatin1("latex-markdown-test.md prepared table header row"),
				ok);
			Check(
				table.tableRows[1].cells.size() == 3,
				FromLatin1("latex-markdown-test.md prepared table cell count"),
				ok);
		}
	}

	const auto &limits = PrepareTableRenderLimitsForIv();
	auto overflowTable = QByteArray("| A | B |\n| --- | --- |\n");
	for (auto i = 0; i != limits.maxRows; ++i) {
		overflowTable.append("| row ");
		overflowTable.append(QByteArray::number(i));
		overflowTable.append(" | value |\n");
	}
	const auto overflowLabel = FromLatin1("generated-overflow-table.md");
	const auto overflowParsed = ParseMarkdownForIv(
		overflowTable,
		ParseOptions{ overflowLabel });
	Check(
		overflowParsed.ok,
		overflowLabel + FromLatin1(" parse failed: ") + overflowParsed.error,
		ok);
	if (overflowParsed.ok) {
		const auto overflowPrepared = PrepareParsedDocumentForTest(
			overflowParsed.document,
			overflowLabel,
			std::make_shared<MathRenderer>());
		Check(
			!overflowPrepared.failure.failed(),
			overflowLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(overflowPrepared.failure),
			ok);
		auto overflowTableBlocks = 0;
		ForEachPreparedBlock(
			overflowPrepared.blocks.blocks,
			[&](const PreparedBlock &block) {
				if (block.kind == PreparedBlockKind::Table) {
					++overflowTableBlocks;
				}
			});
		Check(
			overflowPrepared.debug.prepareWarningCount > 0,
			overflowLabel + FromLatin1(" flatten warning count"),
			ok);
		Check(
			overflowTableBlocks == 0,
			overflowLabel + FromLatin1(" flattened table block removed"),
			ok);
		Check(
			!overflowPrepared.blocks.blocks.empty(),
			overflowLabel + FromLatin1(" flattened fallback blocks present"),
			ok);
	}
}

void CheckPrepareLinkClassification(
		const QString &sourcePath,
		bool *ok) {
	const auto label = FromLatin1("generated-relative-links.md");
	const auto parsed = ParseMarkdownForIv(
		QByteArray(
			"[Local](./docs/getting-started.md#section-1)\n"
			"[Rejected](../outside.md)\n"),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		sourcePath,
		std::make_shared<MathRenderer>());
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	const auto expectedLocalTarget = QDir(
		QFileInfo(sourcePath).absolutePath()).absoluteFilePath(
			FromLatin1("docs/getting-started.md"));
	auto foundLocal = false;
	auto foundRejected = false;
	ForEachPreparedLink(prepared.blocks.blocks, [&](const PreparedLink &link) {
		if (link.kind == PreparedLinkKind::LocalFile
			&& link.target == QDir::cleanPath(expectedLocalTarget)
			&& link.fragment == FromLatin1("section-1")
			&& link.copyText
				== FromLatin1("./docs/getting-started.md#section-1")) {
			foundLocal = true;
		}
		if (link.kind == PreparedLinkKind::RejectedRelative
			&& link.copyText == FromLatin1("../outside.md")) {
			foundRejected = true;
		}
	});
	Check(
		foundLocal,
		FromLatin1("generated-relative-links.md local markdown classification"),
		ok);
	Check(
		foundRejected,
		FromLatin1("generated-relative-links.md rejected relative classification"),
		ok);
}

void CheckPreparedExternalLinkCoverage(bool *ok) {
	const auto markdownLabel = FromLatin1("generated-prepared-external-links.md");
	const auto markdownParsed = ParseMarkdownForIv(
		QByteArray(
			"[https://visible.example.com/path?q=1](https://visible.example.com/path?q=1)\n"
			"[partial-visible.example.com/path?q=3](https://partial-visible.example.com/path?q=3)\n"
			"[Hidden label](https://hidden.example.com/path?q=2)\n"
			"[support@example.com](mailto:support@example.com?subject=Hello%20Telegram)\n"),
		ParseOptions{ markdownLabel });
	Check(
		markdownParsed.ok,
		markdownLabel + FromLatin1(" parse failed: ") + markdownParsed.error,
		ok);
	if (markdownParsed.ok) {
		const auto markdownPrepared = PrepareParsedDocumentForTest(
			markdownParsed.document,
			markdownLabel,
			std::make_shared<MathRenderer>());
		Check(
			!markdownPrepared.failure.failed(),
			markdownLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(markdownPrepared.failure),
			ok);
		if (!markdownPrepared.failure.failed()) {
			const auto visibleTarget = UrlClickHandler::EncodeForOpening(
				FromLatin1("https://visible.example.com/path?q=1"));
			const auto partialTarget = UrlClickHandler::EncodeForOpening(
				FromLatin1("https://partial-visible.example.com/path?q=3"));
			const auto hiddenTarget = UrlClickHandler::EncodeForOpening(
				FromLatin1("https://hidden.example.com/path?q=2"));
			const auto emailTarget = FromLatin1("support@example.com");
			const auto markdownVisible = FindPreparedLink(
				markdownPrepared.blocks.blocks,
				[&](const PreparedLink &link) {
					return link.kind == PreparedLinkKind::External
						&& link.target == visibleTarget;
				});
			const auto markdownHidden = FindPreparedLink(
				markdownPrepared.blocks.blocks,
				[&](const PreparedLink &link) {
					return link.kind == PreparedLinkKind::External
						&& link.target == hiddenTarget;
				});
			const auto markdownPartial = FindPreparedLink(
				markdownPrepared.blocks.blocks,
				[&](const PreparedLink &link) {
					return link.kind == PreparedLinkKind::External
						&& link.target == partialTarget;
				});
			const auto markdownEmail = FindPreparedLink(
				markdownPrepared.blocks.blocks,
				[&](const PreparedLink &link) {
					return link.kind == PreparedLinkKind::External
						&& link.target == emailTarget;
				});
			Check(
				markdownVisible != nullptr
					&& markdownVisible->entityType == EntityType::Url
					&& markdownVisible->copyText == visibleTarget
					&& markdownVisible->shown == EntityLinkShown::Full,
				markdownLabel + FromLatin1(" visible URL semantics"),
				ok);
			Check(
				markdownPartial != nullptr
					&& markdownPartial->entityType == EntityType::Url
					&& markdownPartial->copyText == partialTarget
					&& markdownPartial->shown == EntityLinkShown::Partial,
				markdownLabel + FromLatin1(" partial visible URL semantics"),
				ok);
			Check(
				markdownHidden != nullptr
					&& markdownHidden->entityType == EntityType::CustomUrl
					&& markdownHidden->copyText == hiddenTarget
					&& markdownHidden->shown == EntityLinkShown::Full,
				markdownLabel + FromLatin1(" hidden URL semantics"),
				ok);
			Check(
				markdownEmail != nullptr
					&& markdownEmail->entityType == EntityType::Email
					&& markdownEmail->copyText == emailTarget
					&& markdownEmail->target == emailTarget
					&& !markdownEmail->target.contains(QChar(':')),
				markdownLabel + FromLatin1(" email target normalization"),
				ok);
		}
	}

	const auto nativeLabel = u"native-iv-prepared-external-links"_q;
	auto nativeBlocks = QVector<MTPPageBlock>();
	nativeBlocks.push_back(MTP_pageBlockParagraph(NativeIvConcat({
		NativeIvTextUrl(
			u"https://native-visible.example/path?q=1"_q,
			u"https://native-visible.example/path?q=1"_q),
		NativeIvText(u" "_q),
		NativeIvTextUrl(
			u"native-partial.example/path?q=3"_q,
			u"https://native-partial.example/path?q=3"_q),
		NativeIvText(u" "_q),
		NativeIvTextUrl(
			u"Native hidden label"_q,
			u"https://native-hidden.example/path?q=2"_q),
		NativeIvText(u" "_q),
		NativeIvTextEmail(u"native@example.com"_q, u"native@example.com"_q),
	})));
	auto nativeSource = NativeIvSource(std::move(nativeBlocks));
	const auto nativePrepared = TryPrepareNativeInstantView({
		.source = &nativeSource,
	});
	Check(
		nativePrepared.supported(),
		nativeLabel + u" prepare supported"_q,
		ok);
	Check(
		!nativePrepared.content.failure.failed(),
		nativeLabel + u" prepare failure"_q,
		ok);
	if (nativePrepared.supported() && !nativePrepared.content.failure.failed()) {
		const auto visibleTarget = UrlClickHandler::EncodeForOpening(
			FromLatin1("https://native-visible.example/path?q=1"));
		const auto partialTarget = UrlClickHandler::EncodeForOpening(
			FromLatin1("https://native-partial.example/path?q=3"));
		const auto hiddenTarget = UrlClickHandler::EncodeForOpening(
			FromLatin1("https://native-hidden.example/path?q=2"));
		const auto emailTarget = FromLatin1("native@example.com");
		const auto nativeVisible = FindPreparedLink(
			nativePrepared.content.blocks.blocks,
			[&](const PreparedLink &link) {
				return link.kind == PreparedLinkKind::External
					&& link.target == visibleTarget;
			});
		const auto nativeHidden = FindPreparedLink(
			nativePrepared.content.blocks.blocks,
			[&](const PreparedLink &link) {
				return link.kind == PreparedLinkKind::External
					&& link.target == hiddenTarget;
			});
		const auto nativePartial = FindPreparedLink(
			nativePrepared.content.blocks.blocks,
			[&](const PreparedLink &link) {
				return link.kind == PreparedLinkKind::External
					&& link.target == partialTarget;
			});
		const auto nativeEmail = FindPreparedLink(
			nativePrepared.content.blocks.blocks,
			[&](const PreparedLink &link) {
				return link.kind == PreparedLinkKind::External
					&& link.target == emailTarget;
			});
		Check(
			nativeVisible != nullptr
				&& nativeVisible->entityType == EntityType::Url
				&& nativeVisible->copyText == visibleTarget
				&& nativeVisible->shown == EntityLinkShown::Full,
			nativeLabel + u" visible URL semantics"_q,
			ok);
		Check(
			nativePartial != nullptr
				&& nativePartial->entityType == EntityType::Url
				&& nativePartial->copyText == partialTarget
				&& nativePartial->shown == EntityLinkShown::Partial,
			nativeLabel + u" partial visible URL semantics"_q,
			ok);
		Check(
			nativeHidden != nullptr
				&& nativeHidden->entityType == EntityType::CustomUrl
				&& nativeHidden->copyText == hiddenTarget
				&& nativeHidden->shown == EntityLinkShown::Full,
			nativeLabel + u" hidden URL semantics"_q,
			ok);
		Check(
			nativeEmail != nullptr
				&& nativeEmail->entityType == EntityType::Email
				&& nativeEmail->copyText == emailTarget
				&& nativeEmail->target == emailTarget,
			nativeLabel + u" email semantics"_q,
			ok);
	}
}

void CheckDetailsSummaryHitCoverage(bool *ok) {
	const auto label = FromLatin1("generated-details-summary-hit.md");
	const auto parsed = ParseMarkdownForIv(
		QByteArray(
			"<details>\n"
			"<summary>Summary text for selection</summary>\n"
			"\n"
			"Hidden body.\n"
			"</details>\n"),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		std::make_shared<MathRenderer>());
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	auto height = 0;
	auto article = BuildArticleForTest(
		prepared,
		std::make_shared<MathRenderer>(),
		420,
		&height);
	const auto bounds = SegmentHitBounds(article.get(), 420, height, 0);
	Check(
		bounds.has_value(),
		label + FromLatin1(" details segment hit bounds"),
		ok);
	Check(
		article->segmentIsText(0),
		label + FromLatin1(" details segment remains text-selectable"),
		ok);
	if (!bounds || !article->segmentIsText(0)) {
		return;
	}
	auto flags = Ui::Text::StateRequest::Flags();
	flags |= Ui::Text::StateRequest::Flag::LookupLink;
	flags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto summaryTextHit = false;
	auto summaryPaddingHit = false;
	for (auto y = bounds->top();
		(y <= bounds->bottom()) && !(summaryTextHit && summaryPaddingHit);
		++y) {
		for (auto x = bounds->left();
			(x <= bounds->right()) && !(summaryTextHit && summaryPaddingHit);
			++x) {
			const auto hit = article->hitTest(QPoint(x, y), flags);
			if (!hit.valid()
				|| !hit.direct
				|| (hit.segmentIndex != 0)
				|| !hit.preparedLink
				|| (hit.preparedLink->kind != PreparedLinkKind::ToggleDetails)) {
				continue;
			}
			if (hit.state.link) {
				summaryPaddingHit = true;
			} else {
				summaryTextHit = true;
			}
		}
	}
	Check(
		summaryTextHit,
		label + FromLatin1(" summary text hit leaves link inactive"),
		ok);
	Check(
		summaryPaddingHit,
		label + FromLatin1(" summary padding hit keeps toggle link"),
		ok);
}

void CheckTableFormulaTextSizeCoverage(bool *ok) {
	const auto label = FromLatin1("generated-table-formula-text-sizes.md");
	const auto parsed = ParseMarkdownForIv(
		QByteArray(
			"| Header $x$ | Value |\n"
			"| --- | --- |\n"
			"| Body $y$ | Value |\n"),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	auto dimensions = CaptureMarkdownPrepareDimensions();
	dimensions.bodyTextSize = 11;
	dimensions.tableHeaderTextSize = 29;
	dimensions.tableBodyTextSize = 17;
	const auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		std::make_shared<MathRenderer>(),
		dimensions);
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	const auto headerFormula = FindPreparedFormulaSlot(
		prepared,
		FromLatin1("x"),
		MathKind::Inline);
	const auto bodyFormula = FindPreparedFormulaSlot(
		prepared,
		FromLatin1("y"),
		MathKind::Inline);
	Check(
		headerFormula != nullptr,
		label + FromLatin1(" header formula slot"),
		ok);
	Check(
		bodyFormula != nullptr,
		label + FromLatin1(" body formula slot"),
		ok);
	if (headerFormula && bodyFormula) {
		Check(
			headerFormula->textSize == dimensions.tableHeaderTextSize,
			label + FromLatin1(" header formula text size"),
			ok);
		Check(
			bodyFormula->textSize == dimensions.tableBodyTextSize,
			label + FromLatin1(" body formula text size"),
			ok);
		Check(
			headerFormula->textSize != bodyFormula->textSize,
			label + FromLatin1(" header/body formula size split"),
			ok);
		Check(
			headerFormula->textSize != dimensions.bodyTextSize
				&& bodyFormula->textSize != dimensions.bodyTextSize,
			label + FromLatin1(" table formulas avoid body text size"),
			ok);
	}
}

void CheckInlineTextObjectArticleCoverage(bool *ok) {
	const auto selectionLabel = FromLatin1("generated-inline-selection.md");
	const auto selectionParsed = ParseMarkdownForIv(
		QByteArray("Inline formula $\\frac{a}{b}$ export.\n"),
		ParseOptions{ selectionLabel });
	Check(
		selectionParsed.ok,
		selectionLabel + FromLatin1(" parse failed: ")
			+ selectionParsed.error,
		ok);
	if (selectionParsed.ok) {
		auto selectionRenderer = std::make_shared<MathRenderer>();
		auto selectionPrepared = PrepareParsedDocumentForTest(
			selectionParsed.document,
			selectionLabel,
			selectionRenderer);
		Check(
			!selectionPrepared.failure.failed(),
			selectionLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(selectionPrepared.failure),
			ok);
		if (!selectionPrepared.failure.failed()) {
			auto selectionObjectOffset = -1;
			ForEachPreparedBlock(
				selectionPrepared.blocks.blocks,
				[&](const PreparedBlock &block) {
					if (selectionObjectOffset >= 0) {
						return;
					}
					const auto matches = CollectInlineTextObjectMatches(block.text);
					if (!matches.empty()) {
						selectionObjectOffset = matches.front().entity.offset();
					}
				});
			auto selectionFormulaWidth = 0;
			for (const auto &slot : selectionPrepared.formulas) {
				if (slot.present
					&& (slot.kind == MathKind::Inline)
					&& slot.measured.success) {
					selectionFormulaWidth = std::max(
						slot.measured.logicalSize.width(),
						1);
					break;
				}
			}
			auto selectionArticleHeight = 0;
			auto selectionArticle = BuildArticleForTest(
				std::move(selectionPrepared),
				selectionRenderer,
				480,
				&selectionArticleHeight);
			const auto selectionImage = PaintArticleForTest(
				selectionArticle.get(),
				480,
				selectionArticleHeight);
			Check(
				HasPaintedPixels(selectionImage),
				selectionLabel + FromLatin1(" paint produced pixels"),
				ok);
			const auto selectionHitBounds = SymbolHitBounds(
				selectionArticle.get(),
				480,
				selectionArticleHeight,
				selectionObjectOffset);
			Check(
				selectionHitBounds.has_value(),
				selectionLabel + FromLatin1(" inline formula hit bounds"),
				ok);
			if (selectionHitBounds && (selectionFormulaWidth > 0)) {
				Check(
					selectionHitBounds->width() >= selectionFormulaWidth,
					selectionLabel + FromLatin1(
						" inline formula hit width uses object width"),
					ok);
			}
			Check(
				selectionArticle->segmentIsText(0),
				selectionLabel + FromLatin1(" first segment is text"),
				ok);
			if (selectionArticle->segmentIsText(0)) {
				const auto exported = selectionArticle->textForSelection({
					.from = { .segment = 0, .offset = 0 },
					.to = {
						.segment = 0,
						.offset = selectionArticle->segmentLength(0),
					},
				}, nullptr);
				const auto expected = FromLatin1(
					"Inline formula $\\frac{a}{b}$ export.");
				Check(
					exported.expanded == expected,
					selectionLabel + FromLatin1(" expanded export"),
					ok);
				Check(
					exported.rich.text == expected,
					selectionLabel + FromLatin1(" rich export text"),
					ok);
				Check(
					!HasEntityType(exported.rich.entities, EntityType::CustomEmoji),
					selectionLabel + FromLatin1(" export drops custom emoji entity"),
					ok);
			}
		}
	}

	const auto repeatedLabel = FromLatin1("generated-repeated-inline-formulas.md");
	const auto repeatedParsed = ParseMarkdownForIv(
		QByteArray("Repeated $x^2$ then $x^2$ then $x^2$.\n"),
		ParseOptions{ repeatedLabel });
	Check(
		repeatedParsed.ok,
		repeatedLabel + FromLatin1(" parse failed: ") + repeatedParsed.error,
		ok);
	if (!repeatedParsed.ok) {
		return;
	}
	auto repeatedRenderer = std::make_shared<MathRenderer>();
	auto repeatedPrepared = PrepareParsedDocumentForTest(
		repeatedParsed.document,
		repeatedLabel,
		repeatedRenderer);
	Check(
		!repeatedPrepared.failure.failed(),
		repeatedLabel + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(repeatedPrepared.failure),
		ok);
	if (repeatedPrepared.failure.failed()) {
		return;
	}
	const auto repeatedPointers = MeasuredFormulaPointers(repeatedPrepared);
	Check(
		repeatedPointers.size() == 3,
		repeatedLabel + FromLatin1(" measured pointer count"),
		ok);
	if (repeatedPointers.size() == 3) {
		Check(
			repeatedPointers[0] == repeatedPointers[1]
				&& repeatedPointers[1] == repeatedPointers[2],
			repeatedLabel + FromLatin1(" measured pointer reuse in one prepare"),
			ok);
	}
	auto repeatedArticleHeight = 0;
	auto repeatedArticle = BuildArticleForTest(
		std::move(repeatedPrepared),
		repeatedRenderer,
		480,
		&repeatedArticleHeight);
	const auto repeatedFirstImage = PaintArticleForTest(
		repeatedArticle.get(),
		480,
		repeatedArticleHeight);
	Check(
		HasPaintedPixels(repeatedFirstImage),
		repeatedLabel + FromLatin1(" first paint produced pixels"),
		ok);
	const auto repeatedFirstCounters = repeatedRenderer->debugCounters();
	Check(
		repeatedFirstCounters.misses >= 1,
		repeatedLabel + FromLatin1(" first paint raster misses"),
		ok);
	Check(
		repeatedFirstCounters.rendered >= 1,
		repeatedLabel + FromLatin1(" first paint raster rendered"),
		ok);
	const auto repeatedSecondImage = PaintArticleForTest(
		repeatedArticle.get(),
		480,
		repeatedArticleHeight);
	Check(
		HasPaintedPixels(repeatedSecondImage),
		repeatedLabel + FromLatin1(" second paint produced pixels"),
		ok);
	const auto repeatedSecondCounters = repeatedRenderer->debugCounters();
	Check(
		repeatedSecondCounters.hits == repeatedFirstCounters.hits
			&& repeatedSecondCounters.misses == repeatedFirstCounters.misses
			&& repeatedSecondCounters.rendered == repeatedFirstCounters.rendered,
		repeatedLabel + FromLatin1(" same article paint cache reuse"),
		ok);
	repeatedRenderer->resetDebugCounters();
	auto repeatedPreparedAgain = PrepareParsedDocumentForTest(
		repeatedParsed.document,
		repeatedLabel,
		repeatedRenderer);
	Check(
		!repeatedPreparedAgain.failure.failed(),
		repeatedLabel + FromLatin1(" second prepare failure: ")
			+ PrepareFailureReason(repeatedPreparedAgain.failure),
		ok);
	if (!repeatedPreparedAgain.failure.failed()) {
		auto repeatedArticleAgainHeight = 0;
		auto repeatedArticleAgain = BuildArticleForTest(
			std::move(repeatedPreparedAgain),
			repeatedRenderer,
			480,
			&repeatedArticleAgainHeight);
		const auto repeatedCachedImage = PaintArticleForTest(
			repeatedArticleAgain.get(),
			480,
			repeatedArticleAgainHeight);
		Check(
			HasPaintedPixels(repeatedCachedImage),
			repeatedLabel + FromLatin1(" cached paint produced pixels"),
			ok);
		const auto repeatedReuseCounters = repeatedRenderer->debugCounters();
		Check(
			repeatedReuseCounters.hits >= 1,
			repeatedLabel + FromLatin1(" renderer cache reuse hits"),
			ok);
		Check(
			repeatedReuseCounters.misses == 0,
			repeatedLabel + FromLatin1(" renderer cache reuse misses"),
			ok);
		Check(
			repeatedReuseCounters.rendered == 0,
			repeatedLabel + FromLatin1(" renderer cache reuse rendered"),
			ok);
	}
}

void CheckArticleRasterRegressionCoverage(bool *ok) {
	struct SegmentTextProbe {
		int segmentIndex = -1;
		int offset = -1;
		int length = 0;
		TextWithEntities segment;

		[[nodiscard]] bool valid() const {
			return (segmentIndex >= 0) && (offset >= 0) && (length > 0);
		}
	};
	const auto wideFormulaTex = [] {
		auto result = QByteArray();
		for (auto i = 0; i != 24; ++i) {
			if (!result.isEmpty()) {
				result.append(" + ");
			}
			result.append("x_");
			result.append(QByteArray::number(i));
		}
		return result;
	}();
	const auto forEachPreparedTextSegment = [](
			const MarkdownArticleContent &prepared,
			auto &&callback) {
		auto segmentIndex = 0;
		const auto collectTextSegment = [&](const TextWithEntities &text) {
			if (callback(segmentIndex, text)) {
				return true;
			}
			++segmentIndex;
			return false;
		};
		const auto visitBlock = [&](const auto &self, const PreparedBlock &block)
		-> bool {
			switch (block.kind) {
			case PreparedBlockKind::Paragraph:
			case PreparedBlockKind::Heading:
			case PreparedBlockKind::Details:
			case PreparedBlockKind::CodeBlock:
				if (collectTextSegment(block.text)) {
					return true;
				}
				break;
			case PreparedBlockKind::DisplayMath:
				++segmentIndex;
				break;
			case PreparedBlockKind::Table:
				if (!block.text.text.isEmpty()
					&& collectTextSegment(block.text)) {
					return true;
				}
				++segmentIndex;
				for (const auto &row : block.tableRows) {
					for (const auto &cell : row.cells) {
						if (collectTextSegment(cell.text)) {
							return true;
						}
					}
				}
				break;
			case PreparedBlockKind::Placeholder:
			case PreparedBlockKind::Photo:
				++segmentIndex;
				if (!block.text.text.isEmpty()
					&& collectTextSegment(block.text)) {
					return true;
				}
				break;
			case PreparedBlockKind::EmbedPost:
				if (!block.embedPost.author.isEmpty()
					&& collectTextSegment(
						TextWithEntities::Simple(block.embedPost.author))) {
					return true;
				}
				if (!block.embedPost.dateText.isEmpty()
					&& collectTextSegment(
						TextWithEntities::Simple(block.embedPost.dateText))) {
					return true;
				}
				for (const auto &child : block.children) {
					if (self(self, child)) {
						return true;
					}
				}
				if (!block.text.text.isEmpty()
					&& collectTextSegment(block.text)) {
					return true;
				}
				return false;
			case PreparedBlockKind::Rule:
			case PreparedBlockKind::List:
			case PreparedBlockKind::ListItem:
			case PreparedBlockKind::Quote:
				break;
			}
			for (const auto &child : block.children) {
				if (self(self, child)) {
					return true;
				}
			}
			return false;
		};
		for (const auto &block : prepared.blocks.blocks) {
			if (visitBlock(visitBlock, block)) {
				break;
			}
		}
	};
	const auto findInlineFormulaProbe = [&](
			const MarkdownArticleContent &prepared,
			const QString &segmentText) {
		auto result = SegmentTextProbe();
		forEachPreparedTextSegment(prepared, [&](
				int segmentIndex,
				const TextWithEntities &text) {
			if (!segmentText.isEmpty() && !text.text.contains(segmentText)) {
				return false;
			}
			for (const auto &match : CollectInlineTextObjectMatches(text)) {
				if (match.object.kind == InlineTextObjectKind::Formula) {
					result = {
						.segmentIndex = segmentIndex,
						.offset = match.entity.offset(),
						.length = match.entity.length(),
						.segment = text,
					};
					return true;
				}
			}
			return false;
		});
		return result;
	};
	const auto findFirstInlineFormulaProbe = [&](
			const MarkdownArticleContent &prepared) {
		return findInlineFormulaProbe(prepared, QString());
	};
	const auto findTextProbe = [&](
			const MarkdownArticleContent &prepared,
			const QString &segmentText,
			const QString &text,
			int requiredSegmentIndex = -1) {
		auto result = SegmentTextProbe();
		forEachPreparedTextSegment(prepared, [&](
				int segmentIndex,
				const TextWithEntities &segment) {
			if ((requiredSegmentIndex >= 0)
				&& (segmentIndex != requiredSegmentIndex)) {
				return false;
			}
			if (!segmentText.isEmpty() && !segment.text.contains(segmentText)) {
				return false;
			}
			const auto offset = int(segment.text.indexOf(text));
			if (offset < 0) {
				return false;
			}
			result = {
				.segmentIndex = segmentIndex,
				.offset = offset,
				.length = int(text.size()),
				.segment = segment,
			};
			return true;
		});
		return result;
	};
	const auto findTextOffset = [&](
			const MarkdownArticleContent &prepared,
			const QString &text) {
		return findTextProbe(prepared, QString(), text).offset;
	};
	const auto firstMeasuredInlineFormulaWidth = [](
			const MarkdownArticleContent &prepared) {
		for (const auto &slot : prepared.formulas) {
			if (slot.present
				&& (slot.kind == MathKind::Inline)
				&& slot.measured.success) {
				return slot.measured.logicalSize.width();
			}
		}
		return 0;
	};
	const auto logicalRectToImageRect = [](QRect rect, int devicePixelRatio) {
		return QRect(
			rect.x() * devicePixelRatio,
			rect.y() * devicePixelRatio,
			rect.width() * devicePixelRatio,
			rect.height() * devicePixelRatio);
	};
	const auto withinOneLogicalPixel = [](double left, double right) {
		const auto delta = left - right;
		return (delta <= 1.) && (delta >= -1.);
	};
	const auto sameVerticalBounds = [](
			const QRect &left,
			const QRect &right) {
		return (left.top() == right.top())
			&& (left.bottom() == right.bottom());
	};
	const auto markerSearchStripFromLineBounds = [](QRect lineBounds) {
		lineBounds = lineBounds.adjusted(0, -1, 0, 1);
		return QRect(
			0,
			lineBounds.y(),
			std::max(lineBounds.left(), 1),
			std::max(lineBounds.height(), 1));
	};
	const auto buildSegmentLeaf = [&](
			const MarkdownArticleContent &prepared,
			const SegmentTextProbe &probe,
			const std::shared_ptr<MathRenderer> &renderer,
			int width) {
		auto leaf = Ui::Text::String();
		auto cache = CreateInlineFormulaObjectCache(renderer);
		SetTextLeaf(
			&leaf,
			st::defaultMarkdown.body,
			probe.segment,
			&prepared.formulas,
			cache.get(),
			prepared.mediaRuntime,
			width);
		return leaf;
	};
	const auto resolveSegmentBaseline = [&](
			const QString &label,
			const MarkdownArticleContent &prepared,
			const SegmentTextProbe &probe,
			const std::shared_ptr<MathRenderer> &renderer,
			const QRect &lineBounds,
			int width) -> std::optional<int> {
		const auto leaf = buildSegmentLeaf(prepared, probe, renderer, width);
		const auto lines = leaf.countLinesGeometry(width, true);
		Check(
			!lines.empty(),
			label + FromLatin1(" control line geometry"),
			ok);
		return !lines.empty()
			? std::make_optional(lineBounds.top() + lines.front().baseline)
			: std::nullopt;
	};
	struct ArticleLayoutProbe {
		int width = 0;
		int height = 0;
	};
	const auto resolveArticleWidth = [&](
			const QString &label,
			const MarkdownArticleContent &prepared,
			const std::shared_ptr<MathRenderer> &renderer,
			const SegmentTextProbe &formulaProbe,
			const SegmentTextProbe &controlProbe) {
		auto result = ArticleLayoutProbe();
		for (auto width = 320; width <= 1280; width += 32) {
			auto height = 0;
			auto article = BuildArticleForTest(prepared, renderer, width, &height);
			const auto formulaBounds = SymbolHitBounds(
				article.get(),
				width,
				height,
				formulaProbe.offset,
				formulaProbe.segmentIndex);
			const auto controlBounds = SymbolRangeHitBounds(
				article.get(),
				width,
				height,
				controlProbe.offset,
				controlProbe.length,
				controlProbe.segmentIndex);
			if (!formulaBounds
				|| !controlBounds
				|| !sameVerticalBounds(*formulaBounds, *controlBounds)) {
				continue;
			}
			result = {
				.width = width,
				.height = height,
			};
			break;
		}
		Check(
			result.width > 0,
			label + FromLatin1(" derived article width"),
			ok);
		return result;
	};
	const auto checkListMarkerBaselineRegression = [&](
			const QString &label,
			const MarkdownArticleContent &prepared,
			const std::shared_ptr<MathRenderer> &renderer,
			const SegmentTextProbe &referenceControlProbe,
			const SegmentTextProbe &formulaProbe,
			const SegmentTextProbe &controlProbe) {
		Check(
			referenceControlProbe.valid(),
			label + FromLatin1(" plain-text reference probe"),
			ok);
		Check(
			formulaProbe.valid(),
			label + FromLatin1(" inline formula probe"),
			ok);
		Check(
			controlProbe.valid(),
			label + FromLatin1(" plain-text control probe"),
			ok);
		if (!referenceControlProbe.valid()
			|| !formulaProbe.valid()
			|| !controlProbe.valid()) {
			return;
		}
		Check(
			formulaProbe.segmentIndex == controlProbe.segmentIndex,
			label + FromLatin1(" formula and control share a segment"),
			ok);
		Check(
			referenceControlProbe.segmentIndex != controlProbe.segmentIndex,
			label + FromLatin1(" reference and formula use distinct segments"),
			ok);
		if ((formulaProbe.segmentIndex != controlProbe.segmentIndex)
			|| (referenceControlProbe.segmentIndex == controlProbe.segmentIndex)) {
			return;
		}
		const auto layout = resolveArticleWidth(
			label,
			prepared,
			renderer,
			formulaProbe,
			controlProbe);
		if (layout.width <= 0) {
			return;
		}
		auto articleHeight = 0;
		auto article = BuildArticleForTest(
			prepared,
			renderer,
			layout.width,
			&articleHeight);
		const auto image = PaintArticleForTest(
			article.get(),
			layout.width,
			articleHeight);
		Check(
			HasPaintedPixels(image),
			label + FromLatin1(" paint produced pixels"),
			ok);
		const auto referenceControlBounds = SymbolRangeHitBounds(
			article.get(),
			layout.width,
			articleHeight,
			referenceControlProbe.offset,
			referenceControlProbe.length,
			referenceControlProbe.segmentIndex);
		const auto formulaBounds = SymbolHitBounds(
			article.get(),
			layout.width,
			articleHeight,
			formulaProbe.offset,
			formulaProbe.segmentIndex);
		const auto controlBounds = SymbolRangeHitBounds(
			article.get(),
			layout.width,
			articleHeight,
			controlProbe.offset,
			controlProbe.length,
			controlProbe.segmentIndex);
		Check(
			referenceControlBounds.has_value(),
			label + FromLatin1(" reference control hit bounds"),
			ok);
		Check(
			formulaBounds.has_value(),
			label + FromLatin1(" inline formula hit bounds"),
			ok);
		Check(
			controlBounds.has_value(),
			label + FromLatin1(" plain-text control hit bounds"),
			ok);
		if (!referenceControlBounds || !formulaBounds || !controlBounds) {
			return;
		}
		Check(
			sameVerticalBounds(*formulaBounds, *controlBounds),
			label + FromLatin1(" formula stays on first rendered line"),
			ok);
		const auto referenceBaseline = resolveSegmentBaseline(
			label + FromLatin1(" reference"),
			prepared,
			referenceControlProbe,
			renderer,
			*referenceControlBounds,
			layout.width);
		const auto controlBaseline = resolveSegmentBaseline(
			label,
			prepared,
			controlProbe,
			renderer,
			*controlBounds,
			layout.width);
		Check(
			referenceBaseline.has_value(),
			label + FromLatin1(" reference baseline"),
			ok);
		Check(
			controlBaseline.has_value(),
			label + FromLatin1(" control baseline"),
			ok);
		if (!referenceBaseline || !controlBaseline) {
			return;
		}
		const auto referenceMarkerPainted = PaintedBoundsInRect(
			image,
			markerSearchStripFromLineBounds(*referenceControlBounds));
		const auto markerPainted = PaintedBoundsInRect(
			image,
			markerSearchStripFromLineBounds(*controlBounds));
		Check(
			referenceMarkerPainted.has_value(),
			label + FromLatin1(" reference gutter marker painted bounds"),
			ok);
		Check(
			markerPainted.has_value(),
			label + FromLatin1(" gutter marker painted bounds"),
			ok);
		if (!referenceMarkerPainted || !markerPainted) {
			return;
		}
		Check(
			referenceMarkerPainted->right() < referenceControlBounds->left(),
			label + FromLatin1(" reference gutter marker stays left of text"),
			ok);
		Check(
			markerPainted->right() < controlBounds->left(),
			label + FromLatin1(" gutter marker stays left of text"),
			ok);
		Check(
			withinOneLogicalPixel(
				double(markerPainted->center().y()
					- referenceMarkerPainted->center().y()),
				double(*controlBaseline - *referenceBaseline)),
			label + FromLatin1(" gutter marker follows baseline shift"),
			ok);
	};

	const auto wideInlineLabel = FromLatin1("generated-inline-wide-raster.md");
	const auto wideInlineParsed = ParseMarkdownForIv(
		QByteArray("$") + wideFormulaTex + QByteArray("$\n"),
		ParseOptions{ wideInlineLabel });
	Check(
		wideInlineParsed.ok,
		wideInlineLabel + FromLatin1(" parse failed: ")
			+ wideInlineParsed.error,
		ok);
	if (wideInlineParsed.ok) {
		const auto articleWidth = 180;
		auto renderer = std::make_shared<MathRenderer>();
		auto prepared = PrepareParsedDocumentForTest(
			wideInlineParsed.document,
			wideInlineLabel,
			renderer);
		Check(
			!prepared.failure.failed(),
			wideInlineLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(prepared.failure),
			ok);
		if (!prepared.failure.failed()) {
			const auto formulaWidth = firstMeasuredInlineFormulaWidth(prepared);
			const auto formulaProbe = findFirstInlineFormulaProbe(prepared);
			Check(
				formulaWidth > articleWidth,
				wideInlineLabel + FromLatin1(" formula is wider than article"),
				ok);
			Check(
				formulaProbe.valid(),
				wideInlineLabel + FromLatin1(" inline formula probe"),
				ok);
			auto articleHeight = 0;
			auto article = BuildArticleForTest(
				std::move(prepared),
				renderer,
				articleWidth,
				&articleHeight);
			const auto image = PaintArticleForTest(
				article.get(),
				articleWidth,
				articleHeight);
			Check(
				HasPaintedPixels(image),
				wideInlineLabel + FromLatin1(" paint produced pixels"),
				ok);
			const auto bounds = SymbolHitBounds(
				article.get(),
				articleWidth,
				articleHeight,
				formulaProbe.offset,
				formulaProbe.segmentIndex);
			Check(
				bounds.has_value(),
				wideInlineLabel + FromLatin1(" inline formula hit bounds"),
				ok);
			Check(
				bounds
					&& PaintedBoundsInRect(image, *bounds).has_value(),
				wideInlineLabel + FromLatin1(" inline formula paints at line start"),
				ok);
		}
	}

	const auto tableLabel = FromLatin1("generated-table-inline-wide-raster.md");
	const auto tableParsed = ParseMarkdownForIv(
		QByteArray("| Header | Notes | Tail |\n"
			"| --- | --- | --- |\n"
			"| $")
			+ wideFormulaTex
			+ QByteArray("$ | cell | tail |\n"),
		ParseOptions{ tableLabel });
	Check(
		tableParsed.ok,
		tableLabel + FromLatin1(" parse failed: ") + tableParsed.error,
		ok);
	if (tableParsed.ok) {
		const auto articleWidth = 240;
		auto renderer = std::make_shared<MathRenderer>();
		auto prepared = PrepareParsedDocumentForTest(
			tableParsed.document,
			tableLabel,
			renderer);
		Check(
			!prepared.failure.failed(),
			tableLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(prepared.failure),
			ok);
		if (!prepared.failure.failed()) {
			const auto formulaWidth = firstMeasuredInlineFormulaWidth(prepared);
			const auto formulaProbe = findFirstInlineFormulaProbe(prepared);
			Check(
				formulaWidth > (articleWidth / 3),
				tableLabel + FromLatin1(" formula is wider than cell budget"),
				ok);
			Check(
				formulaProbe.valid(),
				tableLabel + FromLatin1(" table formula probe"),
				ok);
			auto articleHeight = 0;
			auto article = BuildArticleForTest(
				std::move(prepared),
				renderer,
				articleWidth,
				&articleHeight);
			const auto image = PaintArticleForTest(
				article.get(),
				articleWidth,
				articleHeight);
			Check(
				HasPaintedPixels(image),
				tableLabel + FromLatin1(" paint produced pixels"),
				ok);
			const auto bounds = SymbolHitBounds(
				article.get(),
				articleWidth,
				articleHeight,
				formulaProbe.offset,
				formulaProbe.segmentIndex);
			Check(
				bounds.has_value(),
				tableLabel + FromLatin1(" table formula hit bounds"),
				ok);
			Check(
				bounds
					&& PaintedBoundsInRect(image, *bounds).has_value(),
				tableLabel + FromLatin1(" table formula paints in narrow cell"),
				ok);
		}
	}

	const auto baselineLabel = FromLatin1("generated-inline-baseline-dpr.md");
	const auto baselineParsed = ParseMarkdownForIv(
		QByteArray(
			"Multiple inline: We have $a = 1$, and plain text a = 1"
			" on the same line.\n"),
		ParseOptions{ baselineLabel });
	Check(
		baselineParsed.ok,
		baselineLabel + FromLatin1(" parse failed: ")
			+ baselineParsed.error,
		ok);
	if (baselineParsed.ok) {
		const auto articleWidth = 480;
		auto renderer = std::make_shared<MathRenderer>();
		auto prepared = PrepareParsedDocumentForTest(
			baselineParsed.document,
			baselineLabel,
			renderer);
		Check(
			!prepared.failure.failed(),
			baselineLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(prepared.failure),
			ok);
		if (!prepared.failure.failed()) {
			const auto formulaProbe = findFirstInlineFormulaProbe(prepared);
			const auto controlText = FromLatin1("a = 1");
			const auto controlOffset = findTextOffset(prepared, controlText);
			Check(
				formulaProbe.valid(),
				baselineLabel + FromLatin1(" inline formula probe"),
				ok);
			Check(
				controlOffset >= 0,
				baselineLabel + FromLatin1(" plain-text control offset"),
				ok);
			auto articleHeight = 0;
			auto article = BuildArticleForTest(
				std::move(prepared),
				renderer,
				articleWidth,
				&articleHeight);
			const auto dpr1Image = PaintArticleForTest(
				article.get(),
				articleWidth,
				articleHeight,
				1);
			const auto dpr2Image = PaintArticleForTest(
				article.get(),
				articleWidth,
				articleHeight,
				2);
			Check(
				HasPaintedPixels(dpr1Image),
				baselineLabel + FromLatin1(" DPR1 paint produced pixels"),
				ok);
			Check(
				HasPaintedPixels(dpr2Image),
				baselineLabel + FromLatin1(" DPR2 paint produced pixels"),
				ok);
			const auto bounds = SymbolHitBounds(
				article.get(),
				articleWidth,
				articleHeight,
				formulaProbe.offset,
				formulaProbe.segmentIndex);
			Check(
				bounds.has_value(),
				baselineLabel + FromLatin1(" inline formula hit bounds"),
				ok);
			const auto controlBounds = SymbolRangeHitBounds(
				article.get(),
				articleWidth,
				articleHeight,
				controlOffset,
				controlText.size());
			Check(
				controlBounds.has_value(),
				baselineLabel + FromLatin1(" plain-text control hit bounds"),
				ok);
			Check(
				article->segmentIsText(0),
				baselineLabel + FromLatin1(" first segment is text"),
				ok);
			if (bounds && controlBounds) {
				const auto dpr1Painted = PaintedBoundsInRect(dpr1Image, *bounds);
				const auto dpr1ControlPainted = PaintedBoundsInRect(
					dpr1Image,
					*controlBounds);
				const auto dpr2Painted = PaintedBoundsInRect(
					dpr2Image,
					logicalRectToImageRect(*bounds, 2));
				const auto dpr2ControlPainted = PaintedBoundsInRect(
					dpr2Image,
					logicalRectToImageRect(*controlBounds, 2));
				Check(
					dpr1Painted.has_value(),
					baselineLabel + FromLatin1(" DPR1 painted bounds"),
					ok);
				Check(
					dpr1ControlPainted.has_value(),
					baselineLabel + FromLatin1(" DPR1 plain-text control bounds"),
					ok);
				Check(
					dpr2Painted.has_value(),
					baselineLabel + FromLatin1(" DPR2 painted bounds"),
					ok);
				Check(
					dpr2ControlPainted.has_value(),
					baselineLabel + FromLatin1(" DPR2 plain-text control bounds"),
					ok);
				if (dpr1Painted
					&& dpr1ControlPainted
					&& dpr2Painted
					&& dpr2ControlPainted) {
					const auto formulaExtendsAboveControl
						= dpr1Painted->top() < dpr1ControlPainted->top();
					const auto formulaExtendsBelowControl
						= dpr1Painted->bottom() > dpr1ControlPainted->bottom();
					Check(
						withinOneLogicalPixel(
							double(dpr1Painted->bottom()),
							double(dpr1ControlPainted->bottom())),
						baselineLabel + FromLatin1(
							" DPR1 formula bottom matches plain-text control"),
						ok);
					Check(
						withinOneLogicalPixel(
							double(dpr2Painted->bottom()) / 2.,
							double(dpr2ControlPainted->bottom()) / 2.),
						baselineLabel + FromLatin1(
							" DPR2 formula bottom matches plain-text control"),
						ok);
					Check(
						withinOneLogicalPixel(
							double(dpr1Painted->top()),
							double(dpr2Painted->top()) / 2.),
						baselineLabel + FromLatin1(" DPR top alignment"),
						ok);
					Check(
						withinOneLogicalPixel(
							double(dpr1Painted->bottom()),
							double(dpr2Painted->bottom()) / 2.),
						baselineLabel + FromLatin1(" DPR bottom alignment"),
						ok);
					if (article->segmentIsText(0)) {
						const auto selectedImage = PaintArticleForTest(
							article.get(),
							articleWidth,
							articleHeight,
							1,
							MarkdownArticleSelection{
								.from = { .segment = 0, .offset = 0 },
								.to = {
									.segment = 0,
									.offset = article->segmentLength(0),
								},
							});
						const auto selectionFormulaBounds = ChangedBoundsInRect(
							dpr1Image,
							selectedImage,
							QRect(bounds->x(), 0, bounds->width(), articleHeight));
						const auto selectionControlBounds = ChangedBoundsInRect(
							dpr1Image,
							selectedImage,
							QRect(
								controlBounds->x(),
								0,
								controlBounds->width(),
								articleHeight));
						Check(
							selectionFormulaBounds.has_value(),
							baselineLabel + FromLatin1(
								" formula selection changed bounds"),
							ok);
						Check(
							selectionControlBounds.has_value(),
							baselineLabel + FromLatin1(
								" plain-text control selection changed bounds"),
							ok);
						if (selectionFormulaBounds && selectionControlBounds) {
							Check(
								selectionFormulaBounds->top()
									<= selectionControlBounds->top(),
								baselineLabel + FromLatin1(
									" formula selection reaches control top"),
								ok);
							Check(
								selectionFormulaBounds->bottom()
									>= selectionControlBounds->bottom(),
								baselineLabel + FromLatin1(
									" formula selection reaches control bottom"),
								ok);
							if (formulaExtendsAboveControl) {
								Check(
									selectionFormulaBounds->top()
										< selectionControlBounds->top(),
									baselineLabel + FromLatin1(
										" formula selection keeps extra top"),
									ok);
							}
							if (formulaExtendsBelowControl) {
								Check(
									selectionFormulaBounds->bottom()
										> selectionControlBounds->bottom(),
									baselineLabel + FromLatin1(
										" formula selection keeps extra bottom"),
									ok);
							}
						}
					}
				}
		}
	}

	const auto orderedListLabel = FromLatin1(
		"generated-ordered-list-marker-baseline.md");
	const auto orderedParsed = ParseMarkdownForIv(
		QByteArray(
			"3. Third with fraction: p / q\n"
			"\n"
			"3. Third with fraction: $\\frac{p}{q}$\n"),
		ParseOptions{ orderedListLabel });
	Check(
		orderedParsed.ok,
		orderedListLabel + FromLatin1(" parse failed: ")
			+ orderedParsed.error,
		ok);
	if (orderedParsed.ok) {
		auto orderedRenderer = std::make_shared<MathRenderer>();
		auto orderedPrepared = PrepareParsedDocumentForTest(
			orderedParsed.document,
			orderedListLabel,
			orderedRenderer);
		Check(
			!orderedPrepared.failure.failed(),
			orderedListLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(orderedPrepared.failure),
			ok);
		if (!orderedPrepared.failure.failed()) {
			const auto controlText = FromLatin1("Third with fraction:");
			const auto formulaProbe = findInlineFormulaProbe(
				orderedPrepared,
				controlText);
			const auto referenceControlProbe = findTextProbe(
				orderedPrepared,
				controlText,
				controlText);
			const auto controlProbe = findTextProbe(
				orderedPrepared,
				controlText,
				controlText,
				formulaProbe.segmentIndex);
			checkListMarkerBaselineRegression(
				orderedListLabel,
				orderedPrepared,
				orderedRenderer,
				referenceControlProbe,
				formulaProbe,
				controlProbe);
		}
	}

	const auto bulletListLabel = FromLatin1(
		"generated-bullet-list-marker-baseline.md");
	const auto bulletParsed = ParseMarkdownForIv(
		QByteArray(
			"- Bullet with fraction: p / q\n"
			"\n"
			"- Bullet with fraction: $\\frac{p}{q}$\n"),
		ParseOptions{ bulletListLabel });
	Check(
		bulletParsed.ok,
		bulletListLabel + FromLatin1(" parse failed: ")
			+ bulletParsed.error,
		ok);
	if (bulletParsed.ok) {
		auto bulletRenderer = std::make_shared<MathRenderer>();
		auto bulletPrepared = PrepareParsedDocumentForTest(
			bulletParsed.document,
			bulletListLabel,
			bulletRenderer);
		Check(
			!bulletPrepared.failure.failed(),
			bulletListLabel + FromLatin1(" prepare failure: ")
				+ PrepareFailureReason(bulletPrepared.failure),
			ok);
		if (!bulletPrepared.failure.failed()) {
			const auto controlText = FromLatin1("Bullet with fraction:");
			const auto formulaProbe = findInlineFormulaProbe(
				bulletPrepared,
				controlText);
			const auto referenceControlProbe = findTextProbe(
				bulletPrepared,
				controlText,
				controlText);
			const auto controlProbe = findTextProbe(
				bulletPrepared,
				controlText,
				controlText,
				formulaProbe.segmentIndex);
			checkListMarkerBaselineRegression(
				bulletListLabel,
				bulletPrepared,
				bulletRenderer,
				referenceControlProbe,
				formulaProbe,
				controlProbe);
		}
	}
}
}

void CheckArticleRenderSmoke(
		const PreparedFixture &markdownFixture,
		const PreparedFixture &latexFixture,
		bool *ok) {
	Check(
		MicrotexBackendLinked(),
		FromLatin1("microtex backend should be linked"),
		ok);
	auto marginRenderer = MathRenderer();
	const auto dimensions = CaptureMarkdownPrepareDimensions();
	const auto checkFormulaRasterMargins = [&](
			const QString &label,
			const QString &tex,
			MathKind kind,
			int textSize) {
		const auto rendered = marginRenderer.renderFormula({
			.trimmedTex = tex,
			.kind = kind,
			.textSize = textSize,
			.renderWidthCap = dimensions.displayMathMaxRenderWidth,
			.renderHeightCap = dimensions.displayMathMaxRenderHeight,
			.devicePixelRatio = 2,
		});
		Check(
			rendered.success,
			label + FromLatin1(" formula raster success"),
			ok);
		if (rendered.success) {
			Check(
				!PaintTouchesBottomOrRightImageEdge(rendered.image),
				label + FromLatin1(" formula raster margin"),
				ok);
		}
	};
	checkFormulaRasterMargins(
		FromLatin1("inline-x"),
		FromLatin1("x"),
		MathKind::Inline,
		dimensions.bodyTextSize);
	checkFormulaRasterMargins(
		FromLatin1("inline-y"),
		FromLatin1("y"),
		MathKind::Inline,
		dimensions.bodyTextSize);
	checkFormulaRasterMargins(
		FromLatin1("inline-alpha"),
		FromLatin1("\\alpha"),
		MathKind::Inline,
		dimensions.bodyTextSize);
	checkFormulaRasterMargins(
		FromLatin1("inline-beta"),
		FromLatin1("\\beta"),
		MathKind::Inline,
		dimensions.bodyTextSize);
	checkFormulaRasterMargins(
		FromLatin1("display-quadratic"),
		FromLatin1("x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}"),
		MathKind::Display,
		dimensions.displayMathTextSize);
	auto renderer = std::make_shared<MathRenderer>();
	auto firstMarkdown = PrepareParsedDocumentForTest(
		markdownFixture.parsed,
		markdownFixture.path,
		renderer);
	auto firstLatex = PrepareParsedDocumentForTest(
		latexFixture.parsed,
		latexFixture.path,
		renderer);
	Check(
		!firstMarkdown.failure.failed() && !firstLatex.failure.failed(),
		FromLatin1("article prepare smoke first pass failed"),
		ok);
	Check(
		renderer->cacheUsageBytes() == 0,
		FromLatin1("article prepare smoke keeps renderer cache empty"),
		ok);
	const auto prepareCounters = renderer->debugCounters();
	Check(
		prepareCounters.hits == 0
			&& prepareCounters.misses == 0
			&& prepareCounters.rendered == 0,
		FromLatin1("article prepare smoke has no raster work"),
		ok);
	auto prepareSmokeLine = FromLatin1("prepare-lazy-smoke");
	prepareSmokeLine.append(FromLatin1(" measured_slots="));
	prepareSmokeLine.append(QString::number(
		CountMeasuredFormulaSlots(firstMarkdown)
			+ CountMeasuredFormulaSlots(firstLatex)));
	prepareSmokeLine.append(FromLatin1(" hits="));
	prepareSmokeLine.append(QString::number(prepareCounters.hits));
	prepareSmokeLine.append(FromLatin1(" misses="));
	prepareSmokeLine.append(QString::number(prepareCounters.misses));
	prepareSmokeLine.append(FromLatin1(" rendered="));
	prepareSmokeLine.append(QString::number(prepareCounters.rendered));
	PrintLine(prepareSmokeLine);

	const auto articleWidth = 640;
	const auto expectedArticleFormulaHits = CountMeasuredFormulaSlots(
		firstMarkdown);
	auto firstArticleHeight = 0;
	auto firstArticle = BuildArticleForTest(
		std::move(firstMarkdown),
		renderer,
		articleWidth,
		&firstArticleHeight);
	const auto firstImage = PaintArticleForTest(
		firstArticle.get(),
		articleWidth,
		firstArticleHeight);
	Check(
		HasPaintedPixels(firstImage),
		FromLatin1("article first paint produced pixels"),
		ok);
	const auto firstPaintCounters = renderer->debugCounters();
	Check(
		firstPaintCounters.misses >= expectedArticleFormulaHits,
		FromLatin1("article first paint lazy raster misses"),
		ok);
	Check(
		firstPaintCounters.rendered >= expectedArticleFormulaHits,
		FromLatin1("article first paint lazy raster rendered"),
		ok);
	Check(
		renderer->cacheUsageBytes() > 0,
		FromLatin1("article first paint populated renderer cache"),
		ok);
	const auto secondImage = PaintArticleForTest(
		firstArticle.get(),
		articleWidth,
		firstArticleHeight);
	Check(
		HasPaintedPixels(secondImage),
		FromLatin1("article second paint produced pixels"),
		ok);
	const auto secondPaintCounters = renderer->debugCounters();
	Check(
		secondPaintCounters.hits == firstPaintCounters.hits
			&& secondPaintCounters.misses == firstPaintCounters.misses
			&& secondPaintCounters.rendered == firstPaintCounters.rendered,
		FromLatin1("article same-instance raster cache reuse"),
		ok);
	auto articleSmokeLine = FromLatin1("article-lazy-smoke");
	articleSmokeLine.append(FromLatin1(" first_hits="));
	articleSmokeLine.append(QString::number(firstPaintCounters.hits));
	articleSmokeLine.append(FromLatin1(" first_misses="));
	articleSmokeLine.append(QString::number(firstPaintCounters.misses));
	articleSmokeLine.append(FromLatin1(" first_rendered="));
	articleSmokeLine.append(QString::number(firstPaintCounters.rendered));
	articleSmokeLine.append(FromLatin1(" second_hits="));
	articleSmokeLine.append(QString::number(secondPaintCounters.hits));
	articleSmokeLine.append(FromLatin1(" second_misses="));
	articleSmokeLine.append(QString::number(secondPaintCounters.misses));
	PrintLine(articleSmokeLine);

	renderer->resetDebugCounters();
	auto secondMarkdown = PrepareParsedDocumentForTest(
		markdownFixture.parsed,
		markdownFixture.path,
		renderer);
	Check(
		!secondMarkdown.failure.failed(),
		FromLatin1("article renderer cache smoke second prepare failed"),
		ok);
	auto secondArticleHeight = 0;
	auto secondArticle = BuildArticleForTest(
		std::move(secondMarkdown),
		renderer,
		articleWidth,
		&secondArticleHeight);
	const auto cachedImage = PaintArticleForTest(
		secondArticle.get(),
		articleWidth,
		secondArticleHeight);
	Check(
		HasPaintedPixels(cachedImage),
		FromLatin1("article renderer cache paint produced pixels"),
		ok);
	const auto rendererReuseCounters = renderer->debugCounters();
	Check(
		rendererReuseCounters.hits >= expectedArticleFormulaHits,
		FromLatin1("article renderer cache reuse hits"),
		ok);
	Check(
		rendererReuseCounters.misses == 0,
		FromLatin1("article renderer cache reuse misses stay zero"),
		ok);
	Check(
		rendererReuseCounters.rendered == 0,
		FromLatin1("article renderer cache reuse rendered stays zero"),
		ok);
	auto rendererSmokeLine = FromLatin1("renderer-cache-smoke");
	rendererSmokeLine.append(FromLatin1(" hits="));
	rendererSmokeLine.append(QString::number(rendererReuseCounters.hits));
	rendererSmokeLine.append(FromLatin1(" misses="));
	rendererSmokeLine.append(QString::number(rendererReuseCounters.misses));
	rendererSmokeLine.append(FromLatin1(" rendered="));
	rendererSmokeLine.append(QString::number(rendererReuseCounters.rendered));
	PrintLine(rendererSmokeLine);

	const auto sharedMarkdown = std::make_shared<const PreparedDocument>(
		markdownFixture.parsed);
	auto measurementFirst = PrepareParsedDocumentForTest(
		sharedMarkdown,
		markdownFixture.path,
		std::make_shared<MathRenderer>());
	Check(
		!measurementFirst.failure.failed(),
		FromLatin1("document measurement cache smoke first pass failed"),
		ok);
	auto measurementSecond = PrepareParsedDocumentForTest(
		sharedMarkdown,
		markdownFixture.path,
		std::make_shared<MathRenderer>());
	Check(
		!measurementSecond.failure.failed(),
		FromLatin1("document measurement cache smoke second pass failed"),
		ok);
	const auto firstMeasurements = MeasuredFormulaPointers(measurementFirst);
	const auto secondMeasurements = MeasuredFormulaPointers(measurementSecond);
	Check(
		!firstMeasurements.empty()
			&& (firstMeasurements.size() == secondMeasurements.size()),
		FromLatin1("document measurement cache smoke pointer shape"),
		ok);
	for (auto i = 0, count = int(firstMeasurements.size()); i != count; ++i) {
		Check(
			firstMeasurements[i] == secondMeasurements[i],
			FromLatin1("document measurement cache smoke pointer reuse"),
			ok);
	}
	auto documentSmokeLine = FromLatin1("document-cache-smoke");
	documentSmokeLine.append(FromLatin1(" slots="));
	documentSmokeLine.append(QString::number(firstMeasurements.size()));
	documentSmokeLine.append(FromLatin1(" reused="));
	documentSmokeLine.append(YesNo(firstMeasurements == secondMeasurements));
	PrintLine(documentSmokeLine);

	const auto failureLabel = FromLatin1("generated-formula-cap.md");
	const auto failureParsed = ParseMarkdownForIv(
		QByteArray("$$\nE = mc^2\n$$\n"),
		ParseOptions{ failureLabel });
	Check(
		failureParsed.ok,
		failureLabel + FromLatin1(" parse failed: ") + failureParsed.error,
		ok);
	if (!failureParsed.ok) {
		return;
	}
	auto failureDimensions = CaptureMarkdownPrepareDimensions();
	failureDimensions.displayMathMaxRenderWidth = 1;
	auto failureRenderer = std::make_shared<MathRenderer>();
	auto failurePrepared = PrepareParsedDocumentForTest(
		failureParsed.document,
		failureLabel,
		failureRenderer,
		std::move(failureDimensions));
	Check(
		!failurePrepared.failure.failed(),
		failureLabel + FromLatin1(" terminal prepare failure"),
		ok);
	Check(
		failurePrepared.debug.formulaWarningCount > 0,
		failureLabel + FromLatin1(" formula warning count"),
		ok);
	auto failedFormulaFound = false;
	for (const auto &slot : failurePrepared.formulas) {
		if (!slot.present) {
			continue;
		}
		if (!slot.measured.success
			&& (slot.measured.tooLarge || slot.measured.overflow)
			&& !slot.measured.fallbackText.isEmpty()) {
			failedFormulaFound = true;
		}
	}
	Check(
		failedFormulaFound,
		failureLabel + FromLatin1(" formula cap fallback result"),
		ok);
	auto fallbackArticleHeight = 0;
	auto fallbackArticle = BuildArticleForTest(
		std::move(failurePrepared),
		failureRenderer,
		articleWidth,
		&fallbackArticleHeight);
	const auto fallbackImage = PaintArticleForTest(
		fallbackArticle.get(),
		articleWidth,
		fallbackArticleHeight);
	Check(
		HasPaintedPixels(fallbackImage),
		failureLabel + FromLatin1(" fallback paint produced pixels"),
		ok);
	const auto fallbackCounters = failureRenderer->debugCounters();
	Check(
		fallbackCounters.hits == 0
			&& fallbackCounters.misses == 0
			&& fallbackCounters.rendered == 0,
		failureLabel + FromLatin1(" fallback paint avoided raster renderer"),
		ok);

	const auto inlineFailureLabel = FromLatin1("generated-inline-formula-cap.md");
	const auto inlineFailureParsed = ParseMarkdownForIv(
		QByteArray("Inline fallback $\\frac{a}{b}$ text.\n"),
		ParseOptions{ inlineFailureLabel });
	Check(
		inlineFailureParsed.ok,
		inlineFailureLabel + FromLatin1(" parse failed: ")
			+ inlineFailureParsed.error,
		ok);
	if (!inlineFailureParsed.ok) {
		return;
	}
	auto inlineFailureDimensions = CaptureMarkdownPrepareDimensions();
	inlineFailureDimensions.displayMathMaxRenderWidth = 1;
	auto inlineFailureRenderer = std::make_shared<MathRenderer>();
	auto inlineFailurePrepared = PrepareParsedDocumentForTest(
		inlineFailureParsed.document,
		inlineFailureLabel,
		inlineFailureRenderer,
		std::move(inlineFailureDimensions));
	Check(
		!inlineFailurePrepared.failure.failed(),
		inlineFailureLabel + FromLatin1(" terminal prepare failure"),
		ok);
	auto inlineMeasuredFallbackText = QString();
	for (const auto &slot : inlineFailurePrepared.formulas) {
		if (!slot.present || (slot.kind != MathKind::Inline)) {
			continue;
		}
		if (!slot.measured.success
			&& (slot.measured.tooLarge || slot.measured.overflow)
			&& (slot.measured.logicalSize.width() > 0)
			&& !slot.measured.fallbackText.isEmpty()) {
			inlineMeasuredFallbackText = slot.measured.fallbackText;
			break;
		}
	}
	auto inlineDisplayedFallbackText = QString();
	auto inlineFailureObjectOffset = -1;
	ForEachPreparedBlock(
		inlineFailurePrepared.blocks.blocks,
		[&](const PreparedBlock &block) {
			if (!inlineDisplayedFallbackText.isEmpty()) {
				return;
			}
			for (const auto &match : CollectInlineTextObjectMatches(block.text)) {
				if (match.object.kind != InlineTextObjectKind::Formula) {
					continue;
				}
				const auto formula = std::get_if<InlineTextObjectFormulaData>(
					&match.object.data);
				if (!formula || formula->copySource.isEmpty()) {
					continue;
				}
				inlineDisplayedFallbackText = formula->copySource;
				inlineFailureObjectOffset = match.entity.offset();
				break;
			}
		});
	Check(
		!inlineMeasuredFallbackText.isEmpty(),
		inlineFailureLabel + FromLatin1(" inline fallback result"),
		ok);
	Check(
		!inlineDisplayedFallbackText.isEmpty(),
		inlineFailureLabel + FromLatin1(" inline displayed fallback text"),
		ok);
	Check(
		inlineFailureObjectOffset >= 0,
		inlineFailureLabel + FromLatin1(" inline object offset"),
		ok);
	Check(
		inlineDisplayedFallbackText != inlineMeasuredFallbackText,
		inlineFailureLabel + FromLatin1(
			" displayed fallback differs from trimmed fallback"),
		ok);
	if (!inlineFailurePrepared.failure.failed()
		&& !inlineDisplayedFallbackText.isEmpty()
		&& (inlineFailureObjectOffset >= 0)) {
		auto inlineFailureArticleHeight = 0;
		auto inlineFailureArticle = BuildArticleForTest(
			std::move(inlineFailurePrepared),
			inlineFailureRenderer,
			articleWidth,
			&inlineFailureArticleHeight);
		const auto inlineFailureImage = PaintArticleForTest(
			inlineFailureArticle.get(),
			articleWidth,
			inlineFailureArticleHeight);
		Check(
			HasPaintedPixels(inlineFailureImage),
			inlineFailureLabel + FromLatin1(" fallback paint produced pixels"),
			ok);
		const auto inlineFailureCounters = inlineFailureRenderer->debugCounters();
		Check(
			inlineFailureCounters.hits == 0
				&& inlineFailureCounters.misses == 0
				&& inlineFailureCounters.rendered == 0,
			inlineFailureLabel
				+ FromLatin1(" fallback paint avoided raster renderer"),
			ok);
		const auto inlineFailureHitBounds = SymbolHitBounds(
			inlineFailureArticle.get(),
			articleWidth,
			inlineFailureArticleHeight,
			inlineFailureObjectOffset);
		Check(
			inlineFailureHitBounds.has_value(),
			inlineFailureLabel + FromLatin1(" inline hit bounds"),
			ok);
		const auto inlineFailurePaintStrip = inlineFailureHitBounds
			? QRect(
				inlineFailureHitBounds->x(),
				0,
				inlineFailureHitBounds->width(),
				inlineFailureImage.height())
			: QRect();
		const auto inlineFailurePaintedBounds = inlineFailureHitBounds
			? PaintedBoundsInRect(inlineFailureImage, inlineFailurePaintStrip)
			: std::nullopt;
		Check(
			inlineFailurePaintedBounds.has_value(),
			inlineFailureLabel + FromLatin1(" fallback paint strip bounds"),
			ok);
		if (inlineFailureHitBounds && inlineFailurePaintedBounds) {
			Check(
				inlineFailurePaintedBounds->top()
					>= inlineFailureHitBounds->top()
					&& inlineFailurePaintedBounds->bottom()
						<= inlineFailureHitBounds->bottom(),
				inlineFailureLabel
					+ FromLatin1(" fallback paint stays within line bounds"),
				ok);
		}

		const auto inlinePlainLabel = FromLatin1(
			"generated-inline-formula-cap-plain.md");
		auto inlinePlainText = inlineDisplayedFallbackText;
		inlinePlainText.replace(u"$"_q, u"\\$"_q);
		const auto inlinePlainSource = QByteArray("Inline fallback ")
			+ inlinePlainText.toUtf8()
			+ QByteArray(" text.\n");
		const auto inlinePlainParsed = ParseMarkdownForIv(
			inlinePlainSource,
			ParseOptions{ inlinePlainLabel });
		Check(
			inlinePlainParsed.ok,
			inlinePlainLabel + FromLatin1(" parse failed: ")
				+ inlinePlainParsed.error,
			ok);
		if (inlinePlainParsed.ok) {
			auto inlinePlainRenderer = std::make_shared<MathRenderer>();
			auto inlinePlainPrepared = PrepareParsedDocumentForTest(
				inlinePlainParsed.document,
				inlinePlainLabel,
				inlinePlainRenderer);
			Check(
				!inlinePlainPrepared.failure.failed(),
				inlinePlainLabel + FromLatin1(" prepare failure: ")
					+ PrepareFailureReason(inlinePlainPrepared.failure),
				ok);
			if (!inlinePlainPrepared.failure.failed()) {
				auto inlinePlainArticleHeight = 0;
				auto inlinePlainArticle = BuildArticleForTest(
					std::move(inlinePlainPrepared),
					inlinePlainRenderer,
					articleWidth,
					&inlinePlainArticleHeight);
				const auto inlinePlainImage = PaintArticleForTest(
					inlinePlainArticle.get(),
					articleWidth,
					inlinePlainArticleHeight);
				Check(
					inlineFailureArticle->maxWidth()
						== inlinePlainArticle->maxWidth(),
					inlineFailureLabel
						+ FromLatin1(" fallback max width matches plain text"),
					ok);
				Check(
					inlineFailureArticleHeight == inlinePlainArticleHeight,
					inlineFailureLabel
						+ FromLatin1(" fallback height matches plain text"),
					ok);
				const auto inlinePlainPaintedBounds = inlineFailureHitBounds
					? PaintedBoundsInRect(inlinePlainImage, inlineFailurePaintStrip)
					: std::nullopt;
				Check(
					inlinePlainPaintedBounds.has_value(),
					inlinePlainLabel + FromLatin1(" fallback paint strip bounds"),
					ok);
				if (inlineFailurePaintedBounds && inlinePlainPaintedBounds) {
					Check(
						inlineFailurePaintedBounds->top()
							== inlinePlainPaintedBounds->top()
							&& inlineFailurePaintedBounds->bottom()
								== inlinePlainPaintedBounds->bottom(),
						inlineFailureLabel
							+ FromLatin1(
								" fallback paint vertical bounds match plain text"),
						ok);
				}
			}
		}
	}

	const auto &prepareLimits = PrepareLimitsForIv();
	auto blockLimitSource = QByteArray();
	for (auto i = 0; i != (prepareLimits.maxPreparedBlocks + 1); ++i) {
		blockLimitSource.append("Paragraph ");
		blockLimitSource.append(QByteArray::number(i));
		blockLimitSource.append("\n\n");
	}
	const auto blockLimitLabel = FromLatin1("generated-prepare-block-limit.md");
	const auto blockLimitParsed = ParseMarkdownForIv(
		blockLimitSource,
		ParseOptions{ blockLimitLabel });
	Check(
		blockLimitParsed.ok,
		blockLimitLabel + FromLatin1(" parse failed: ") + blockLimitParsed.error,
		ok);
	if (blockLimitParsed.ok) {
		const auto blockLimitPrepared = PrepareParsedDocumentForTest(
			blockLimitParsed.document,
			blockLimitLabel,
			std::make_shared<MathRenderer>());
		Check(
			blockLimitPrepared.failure.failed(),
			blockLimitLabel + FromLatin1(
				" missing real terminal prepare failure"),
			ok);
		Check(
			blockLimitPrepared.failure.terminal
				== PrepareTerminalFailure::DocumentTooLarge,
			blockLimitLabel + FromLatin1(
				" real terminal prepare failure kind"),
			ok);
		Check(
			PrepareFailureReason(blockLimitPrepared.failure)
				== FromLatin1("prepared-block-limit"),
			blockLimitLabel + FromLatin1(
				" real terminal prepare failure reason"),
			ok);
		Check(
			blockLimitPrepared.blocks.blocks.empty(),
			blockLimitLabel + FromLatin1(
				" real terminal prepare clears blocks"),
			ok);
	}
}

void CheckAnchorScrollAlignmentCoverage(bool *ok) {
	const auto label = FromLatin1("markdown-example.md");
	auto renderer = std::make_shared<MathRenderer>();
	auto fixture = PreparedFixture();
	if (!PrepareFixture(
			DefaultFixturePath(label),
			label,
			renderer,
			&fixture)) {
		Check(false, label + FromLatin1(" fixture preparation"), ok);
		return;
	}
	auto window = Ui::RpWindow();
	window.setGeometry(QRect(0, 0, 520, 240));
	window.show();
	FlushPendingWidgetEvents();
	auto preview = CreateMarkdownPreviewWidget(
		window.body(),
		std::move(fixture.prepared),
		renderer,
		[](Event) {
		});
	preview->setGeometry(QRect(QPoint(), window.body()->size()));
	preview->show();
	FlushPendingWidgetEvents();
	const auto scroll = FindChildObject<Ui::ScrollArea>(preview.get());
	const auto body = FindChildObject<MarkdownDocumentWidget>(preview.get());
	Check(
		scroll != nullptr,
		label + FromLatin1(" preview scroll area"),
		ok);
	Check(
		body != nullptr,
		label + FromLatin1(" preview body widget"),
		ok);
	if (!scroll || !body) {
		return;
	}
	const auto initialTop = scroll->scrollTop();
	const auto anchorId = u"headings"_q;
	Check(
		scroll->scrollTopMax() > 0,
		label + FromLatin1(" preview can scroll"),
		ok);
	Check(
		body->anchorTop(anchorId) > initialTop,
		label + FromLatin1(" headings anchor starts below viewport top"),
		ok);
	if (!(scroll->scrollTopMax() > 0)
		|| !(body->anchorTop(anchorId) > initialTop)) {
		return;
	}
	Check(
		ScrollMarkdownPreviewToAnchor(preview.get(), anchorId),
		label + FromLatin1(" anchor scroll request"),
		ok);
	FlushPendingWidgetEvents();
	Check(
		scroll->scrollTop()
			== std::min(body->anchorTop(anchorId), scroll->scrollTopMax()),
		label + FromLatin1(" anchor scroll aligns to viewport top"),
		ok);
}

void CheckArticlePageWidthCoverage(bool *ok) {
	const auto label = FromLatin1("native-iv-page-width-bands");
	const auto parsed = ParseMarkdownForIv(
		QByteArray("Simple preview body."),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	auto renderer = std::make_shared<MathRenderer>();
	auto previewPrepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		renderer);
	Check(
		!previewPrepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(previewPrepared.failure),
		ok);
	if (previewPrepared.failure.failed()) {
		return;
	}
	auto window = Ui::RpWindow();
	const auto wideWidth = st::defaultMarkdown.pageMaxWidth + 300;
	window.setGeometry(QRect(0, 0, wideWidth, 260));
	window.show();
	FlushPendingWidgetEvents();
	auto preview = CreateMarkdownPreviewWidget(
		window.body(),
		std::move(previewPrepared),
		renderer,
		[](Event) {
		});
	preview->setGeometry(QRect(QPoint(), window.body()->size()));
	preview->show();
	FlushPendingWidgetEvents();
	const auto scroll = FindChildObject<Ui::ScrollArea>(preview.get());
	const auto body = FindChildObject<MarkdownDocumentWidget>(preview.get());
	Check(scroll != nullptr, label + FromLatin1(" preview scroll area"), ok);
	Check(body != nullptr, label + FromLatin1(" preview body widget"), ok);
	if (scroll && body) {
		Check(
			body->width() == body->maxWidth(),
			label + FromLatin1(" preview clamps body to max width"),
			ok);
		Check(
			body->x()
				== std::max((scroll->width() - body->width()) / 2, 0),
			label + FromLatin1(" preview centers clamped body"),
			ok);
	}

	const auto photoId = uint64(9601);
	const auto largePhotoId = uint64(9602);
	auto runtime = std::make_shared<TestMediaRuntime>();
	runtime->hostedFactory = std::make_shared<TestHostedMediaBlockFactory>();
	auto source = NativeIvSource(
		QVector<MTPPageBlock>{
			NativeIvPhotoBlock(photoId, u"Small hosted photo caption"_q),
			NativeIvPhotoBlock(largePhotoId),
		},
		QVector<MTPPhoto>{
			NativeIvPhoto(photoId, 160, 90),
			NativeIvPhoto(largePhotoId, 1000, 500),
		});
	auto prepared = TryPrepareNativeInstantView({
		.source = &source,
		.mediaRuntime = runtime,
	});
	Check(prepared.supported(), label + FromLatin1(" prepare supported"), ok);
	if (!prepared.supported()) {
		return;
	}
	const auto articleWidth = 620;
	auto articleHeight = 0;
	auto article = BuildArticleForTest(
		std::move(prepared.content),
		renderer,
		articleWidth,
		&articleHeight);
	Check(
		runtime->hostedPhotoRequests() == 2,
		label + FromLatin1(" hosted photo request"),
		ok);
	const auto &blocks = runtime->hostedFactory->photoBlocks;
	Check(blocks.size() == 2, label + FromLatin1(" hosted photo block"), ok);
	if (blocks.size() < 2) {
		return;
	}
	const auto expectedMediaWidth = 320;
	const auto mediaLimitedAndCentered = std::any_of(
		blocks.front()->requestedGeometries.begin(),
		blocks.front()->requestedGeometries.end(),
		[=](const QRect &geometry) {
			return (geometry.width() == expectedMediaWidth)
				&& (geometry.x() > 0);
		});
	Check(
		mediaLimitedAndCentered,
		label + FromLatin1(" media width limited and centered"),
		ok);
	const auto mediaFullWidth = std::any_of(
		blocks[1]->requestedGeometries.begin(),
		blocks[1]->requestedGeometries.end(),
		[=](const QRect &geometry) {
			return (geometry.x() == 0)
				&& (geometry.width() == articleWidth);
		});
	Check(
		mediaFullWidth,
		label + FromLatin1(" wide media uses full article band"),
		ok);
	const auto captionBounds = SegmentHitBounds(
		article.get(),
		articleWidth,
		articleHeight,
		1);
	Check(
		captionBounds.has_value(),
		label + FromLatin1(" caption bounds"),
		ok);
	if (captionBounds) {
		Check(
			captionBounds->left() >= st::defaultMarkdown.textPadding.left(),
			label + FromLatin1(" caption uses text left padding"),
			ok);
		Check(
			captionBounds->right()
				< articleWidth - st::defaultMarkdown.textPadding.right(),
			label + FromLatin1(" caption uses text right padding"),
			ok);
	}

	const auto channelId = uint64(9603);
	auto channelRuntime = std::make_shared<TestMediaRuntime>();
	channelRuntime->addChannelRuntime(
		channelId,
		u"nativeivpagewidth"_q,
		std::make_shared<TestChannelRuntime>());
	auto channelSource = NativeIvSource(QVector<MTPPageBlock>{
		MTP_pageBlockChannel(NativeIvChannelChat(
			channelId,
			u"Native IV Page Width Channel"_q,
			u"nativeivpagewidth"_q)),
		MTP_pageBlockParagraph(NativeIvText(u"Body text keeps margins."_q)),
	});
	auto channelPrepared = TryPrepareNativeInstantView({
		.source = &channelSource,
		.mediaRuntime = channelRuntime,
	});
	Check(
		channelPrepared.supported(),
		label + FromLatin1(" channel prepare supported"),
		ok);
	if (!channelPrepared.supported()) {
		return;
	}
	auto channelHeight = 0;
	auto channelArticle = BuildArticleForTest(
		std::move(channelPrepared.content),
		renderer,
		articleWidth,
		&channelHeight);
	const auto channelBounds = SegmentHitBounds(
		channelArticle.get(),
		articleWidth,
		channelHeight,
		0);
	Check(
		channelBounds.has_value(),
		label + FromLatin1(" channel bounds"),
		ok);
	if (channelBounds) {
		const auto maxChannelHeight
			= st::defaultMarkdown.channel.titleStyle.lineHeight
			+ st::defaultMarkdown.channel.padding.top()
			+ st::defaultMarkdown.channel.padding.bottom();
		Check(
			channelBounds->left() == 0
				&& channelBounds->right() == articleWidth - 1,
			label + FromLatin1(" channel uses full article band"),
			ok);
		Check(
			channelBounds->height() <= maxChannelHeight,
			label + FromLatin1(" channel bar stays compact"),
			ok);
	}
	const auto bodyBounds = SegmentHitBounds(
		channelArticle.get(),
		articleWidth,
		channelHeight,
		1);
	Check(
		bodyBounds.has_value(),
		label + FromLatin1(" body bounds"),
		ok);
	if (bodyBounds) {
		Check(
			bodyBounds->left() >= st::defaultMarkdown.textPadding.left(),
			label + FromLatin1(" body uses text left padding"),
			ok);
	}
}

void CheckArticleHorizontalRelayoutRegression(bool *ok) {
	const auto label = FromLatin1("generated-horizontal-relayout-regression.md");
	const auto parsed = ParseMarkdownForIv(
		QByteArray(R"(# Horizontal resize regression heading that wraps when narrowed

This body paragraph also needs to reflow after a horizontal resize so the article must rebuild later block offsets instead of leaving them behind.

$$
\int_0^1 x^2 \, dx = \frac{1}{3}
$$

ThisIsALongUnbrokenStringToTestWrappingBehavior_ABCD1234EFGH5678IJKL
)"),
		ParseOptions{ label });
	Check(
		parsed.ok,
		label + FromLatin1(" parse failed: ") + parsed.error,
		ok);
	if (!parsed.ok) {
		return;
	}
	const auto &topLevelBlocks = parsed.document.document.children;
	Check(
		topLevelBlocks.size() == 4,
		label + FromLatin1(" parse top-level block count"),
		ok);
	if (topLevelBlocks.size() != 4) {
		return;
	}
	Check(
		topLevelBlocks[0].kind == NodeKind::Heading,
		label + FromLatin1(" parse heading block kind"),
		ok);
	Check(
		topLevelBlocks[1].kind == NodeKind::Paragraph,
		label + FromLatin1(" parse body paragraph block kind"),
		ok);
	Check(
		topLevelBlocks[2].kind == NodeKind::DisplayMath,
		label + FromLatin1(" parse display math block kind"),
		ok);
	Check(
		topLevelBlocks[3].kind == NodeKind::Paragraph,
		label + FromLatin1(" parse trailing paragraph block kind"),
		ok);
	auto renderer = std::make_shared<MathRenderer>();
	auto prepared = PrepareParsedDocumentForTest(
		parsed.document,
		label,
		renderer);
	Check(
		!prepared.failure.failed(),
		label + FromLatin1(" prepare failure: ")
			+ PrepareFailureReason(prepared.failure),
		ok);
	if (prepared.failure.failed()) {
		return;
	}
	Check(
		prepared.blocks.blocks.size() == 4,
		label + FromLatin1(" prepared top-level block count"),
		ok);
	if (prepared.blocks.blocks.size() != 4) {
		return;
	}
	const auto wideWidth = 640;
	const auto narrowWidth = 280;
	auto wideHeight = 0;
	auto article = BuildArticleForTest(
		std::move(prepared),
		renderer,
		wideWidth,
		&wideHeight);
	const auto wideImage = PaintArticleForTest(
		article.get(),
		wideWidth,
		wideHeight);
	Check(
		HasPaintedPixels(wideImage),
		label + FromLatin1(" wide paint produced pixels"),
		ok);
	const auto wideFinalBounds = SegmentHitBounds(
		article.get(),
		wideWidth,
		wideHeight,
		3);
	Check(
		wideFinalBounds.has_value(),
		label + FromLatin1(" wide final segment hit bounds"),
		ok);
	const auto narrowHeight = article->resizeGetHeight(narrowWidth);
	const auto narrowImage = PaintArticleForTest(
		article.get(),
		narrowWidth,
		narrowHeight);
	Check(
		HasPaintedPixels(narrowImage),
		label + FromLatin1(" narrow paint produced pixels"),
		ok);
	Check(
		narrowHeight > wideHeight,
		label + FromLatin1(" narrow relayout height grows"),
		ok);
	auto segmentBounds = std::vector<std::optional<QRect>>();
	segmentBounds.reserve(4);
	for (auto segmentIndex = 0; segmentIndex != 4; ++segmentIndex) {
		segmentBounds.push_back(SegmentHitBounds(
			article.get(),
			narrowWidth,
			narrowHeight,
			segmentIndex));
		Check(
			segmentBounds.back().has_value(),
			label + FromLatin1(" segment hit bounds ")
				+ QString::number(segmentIndex),
			ok);
	}
	auto haveAllSegmentBounds = true;
	for (const auto &bounds : segmentBounds) {
		if (!bounds.has_value()) {
			haveAllSegmentBounds = false;
			break;
		}
	}
	if (!haveAllSegmentBounds) {
		return;
	}
	for (auto segmentIndex = 1; segmentIndex != 4; ++segmentIndex) {
		const auto &previousBounds = *segmentBounds[segmentIndex - 1];
		const auto &currentBounds = *segmentBounds[segmentIndex];
		Check(
			currentBounds.top() > previousBounds.top(),
			label + FromLatin1(" segment document order ")
				+ QString::number(segmentIndex),
			ok);
		Check(
			currentBounds.top() > previousBounds.bottom(),
			label + FromLatin1(" segment vertical separation ")
				+ QString::number(segmentIndex),
			ok);
	}
	const auto &finalBounds = *segmentBounds.back();
	if (wideFinalBounds) {
		Check(
			finalBounds.height() > wideFinalBounds->height(),
			label + FromLatin1(" long final segment wraps when narrowed"),
			ok);
	}
	Check(
		finalBounds.height() > st::defaultMarkdown.body.lineHeight,
		label + FromLatin1(" long final segment spans multiple lines"),
		ok);
	auto lookupFlags = Ui::Text::StateRequest::Flags();
	lookupFlags |= Ui::Text::StateRequest::Flag::LookupSymbol;
	auto finalProbePoint = std::optional<QPoint>();
	for (auto y = finalBounds.top();
		(y <= finalBounds.bottom()) && !finalProbePoint;
		++y) {
		for (auto x = finalBounds.left(); x <= finalBounds.right(); ++x) {
			const auto hit = article->hitTest(QPoint(x, y), lookupFlags);
			if (hit.valid() && hit.direct && (hit.segmentIndex == 3)) {
				finalProbePoint = QPoint(x, y);
				break;
			}
		}
	}
	Check(
		finalProbePoint.has_value(),
		label + FromLatin1(" final segment direct probe point"),
		ok);
	if (!finalProbePoint) {
		return;
	}
	const auto finalHit = article->hitTest(*finalProbePoint, lookupFlags);
	Check(
		finalBounds.contains(*finalProbePoint),
		label + FromLatin1(" final probe point inside final segment bounds"),
		ok);
	Check(
		finalHit.valid()
			&& finalHit.direct
			&& (finalHit.segmentIndex == 3),
		label + FromLatin1(" final segment hit after relayout"),
		ok);
}

[[nodiscard]] int RunTests(int argc, char **argv) {
	auto args = ParseArgs(argc, argv);
	if (!args.ok) {
		PrintError(args.error);
		return 1;
	}
	if (args.inlineHtml) {
		auto ok = true;
		CheckInlineHtmlCoverage(args.dump, &ok);
		CheckInlineHtmlPrepareCoverage(&ok);
		return ok ? 0 : 1;
	}
	if (args.markdownPath.isEmpty()) {
		args.markdownPath = DefaultFixturePath(FromLatin1("markdown-example.md"));
	}
	if (args.latexMarkdownPath.isEmpty()) {
		args.latexMarkdownPath = DefaultFixturePath(
			FromLatin1("latex-markdown-test.md"));
	}

	auto fixtureRenderer = std::make_shared<MathRenderer>();
	auto markdownFixture = PreparedFixture();
	if (!PrepareFixture(
			args.markdownPath,
			FromLatin1("markdown-example.md"),
			fixtureRenderer,
			&markdownFixture)) {
		return 1;
	}
	if (args.dump) {
		PrintLine(DumpForDebug(markdownFixture.parsed));
	}

	auto latexFixture = PreparedFixture();
	if (!PrepareFixture(
			args.latexMarkdownPath,
			FromLatin1("latex-markdown-test.md"),
			fixtureRenderer,
			&latexFixture)) {
		return 1;
	}
	if (args.dump) {
		PrintLine(DumpForDebug(latexFixture.parsed));
	}

	const auto &markdown = markdownFixture.parsed;
	const auto &latex = latexFixture.parsed;
	auto ok = true;
	Check(
		markdown.stats.cmarkNodeCount == 562,
		FromLatin1("markdown-example.md cmark node count"),
		&ok);
	Check(
		CountFormulas(markdown, MathKind::Inline) == 1,
		FromLatin1("markdown-example.md inline formula count"),
		&ok);
	Check(
		CountFormulas(markdown, MathKind::Display) == 1,
		FromLatin1("markdown-example.md display formula count"),
		&ok);
	Check(
		CountDisplayMathNodes(markdown.document) == 1,
		FromLatin1("markdown-example.md display math node count"),
		&ok);
	Check(
		FindNodeByKindAndRange(
			markdown.document,
			NodeKind::DisplayMath,
			281,
			1,
			283,
			2) != nullptr,
		FromLatin1("markdown-example.md display formula range"),
		&ok);
	Check(
		CountNodesByKindAndRange(
			markdown.document,
			NodeKind::Paragraph,
			281,
			1,
			283,
			2) == 0,
		FromLatin1("markdown-example.md duplicate display paragraph removed"),
		&ok);
	Check(
		HasKind(markdown.document, NodeKind::Table),
		FromLatin1("markdown-example.md table coverage"),
		&ok);
	Check(
		HasKind(markdown.document, NodeKind::Strike),
		FromLatin1("markdown-example.md strikethrough coverage"),
		&ok);
	Check(
		HasTaskState(markdown.document, TaskState::Checked),
		FromLatin1("markdown-example.md checked task"),
		&ok);
	Check(
		HasTaskState(markdown.document, TaskState::Unchecked),
		FromLatin1("markdown-example.md unchecked task"),
		&ok);
	auto markdownTables = std::vector<const MarkdownNode*>();
	CollectTables(markdown.document, &markdownTables);
	Check(
		int(markdownTables.size()) >= 2,
		FromLatin1("markdown-example.md table count"),
		&ok);
	if (markdownTables.size() >= 2) {
		const auto &firstTable = *markdownTables[0];
		const auto &secondTable = *markdownTables[1];
		Check(
			HasTableHeaderRow(firstTable),
			FromLatin1("markdown-example.md first table header row"),
			&ok);
		Check(
			TableHeaderRowCount(firstTable) == 1,
			FromLatin1("markdown-example.md first table header row count"),
			&ok);
		Check(
			TableColumnCount(firstTable) == 3,
			FromLatin1("markdown-example.md first table column count"),
			&ok);
		Check(
			HasSequentialTableColumns(firstTable),
			FromLatin1("markdown-example.md first table column order"),
			&ok);
		Check(
			HasTableAlignments(
				secondTable,
				{
					TableAlignment::Left,
					TableAlignment::Center,
					TableAlignment::Right,
				}),
			FromLatin1("markdown-example.md second table alignments"),
			&ok);
	}
	CheckFixtureSemanticCoverage(markdown, args.markdownPath, &ok);
	Check(
		latex.stats.cmarkNodeCount == 532,
		FromLatin1("latex-markdown-test.md cmark node count"),
		&ok);
	Check(
		CountFormulas(latex, MathKind::Inline) == 100,
		FromLatin1("latex-markdown-test.md inline formula count"),
		&ok);
	Check(
		CountFormulas(latex, MathKind::Display) == 31,
		FromLatin1("latex-markdown-test.md display formula count"),
		&ok);
	Check(
		CountDisplayMathNodes(latex.document) == 31,
		FromLatin1("latex-markdown-test.md display math node count"),
		&ok);
	Check(
		FindNodeByPathAndRange(
			latex.document,
			{
				NodeKind::Document,
				NodeKind::List,
				NodeKind::ListItem,
				NodeKind::DisplayMath,
			},
			299,
			4,
			301,
			5) != nullptr,
		FromLatin1("latex-markdown-test.md list display formula nested"),
		&ok);
	Check(
		FindNodeByPathAndRange(
			latex.document,
			{
				NodeKind::Document,
				NodeKind::DisplayMath,
			},
			299,
			4,
			301,
			5) == nullptr,
		FromLatin1("latex-markdown-test.md list display formula not hoisted"),
		&ok);
	Check(
		FindNodeByPathAndRange(
			latex.document,
			{
				NodeKind::Document,
				NodeKind::Blockquote,
				NodeKind::DisplayMath,
			},
			307,
			3,
			309,
			4) != nullptr,
		FromLatin1("latex-markdown-test.md blockquote display formula nested"),
		&ok);
	Check(
		FindNodeByPathAndRange(
			latex.document,
			{
				NodeKind::Document,
				NodeKind::DisplayMath,
			},
			307,
			3,
			309,
			4) == nullptr,
		FromLatin1("latex-markdown-test.md blockquote display formula not hoisted"),
		&ok);
	Check(
		HasKind(latex.document, NodeKind::Table),
		FromLatin1("latex-markdown-test.md table coverage"),
		&ok);
	auto latexTables = std::vector<const MarkdownNode*>();
	CollectTables(latex.document, &latexTables);
	Check(
		!latexTables.empty(),
		FromLatin1("latex-markdown-test.md table count"),
		&ok);
	if (!latexTables.empty()) {
		const auto &table = *latexTables[0];
		Check(
			HasTableHeaderRow(table),
			FromLatin1("latex-markdown-test.md table header row"),
			&ok);
		Check(
			TableColumnCount(table) == 3,
			FromLatin1("latex-markdown-test.md table column count"),
			&ok);
		Check(
			HasSequentialTableColumns(table),
			FromLatin1("latex-markdown-test.md table column order"),
			&ok);
	}
	const auto tableMath = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::Table,
		313,
		318);
	Check(
		tableMath != nullptr,
		FromLatin1("latex-markdown-test.md table range"),
		&ok);
	if (tableMath) {
		Check(
			!HasKind(*tableMath, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md table display math stays literal"),
			&ok);
		Check(
			!HasKind(*tableMath, NodeKind::InlineMath),
			FromLatin1("latex-markdown-test.md table inline math stays literal"),
			&ok);
	}
	Check(
		CountFormulasInLineRange(latex, MathKind::Inline, 315, 318) == 12,
		FromLatin1("latex-markdown-test.md table inline formulas preserved"),
		&ok);
	Check(
		CountFormulasInLineRange(latex, MathKind::Display, 315, 318) == 0,
		FromLatin1("latex-markdown-test.md table display formulas absent"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 71, FromLatin1("\\int_0^1 x \\, dx")),
		FromLatin1("latex-markdown-test.md line 71 inline formula"),
		&ok);
	Check(
		HasFormulaOnLine(
			latex,
			259,
			FromLatin1(
				"\\mathbb{E}[X] = \\int_{-\\infty}^{\\infty} x f(x) \\, dx")),
		FromLatin1("latex-markdown-test.md line 259 inline formula"),
		&ok);
	const auto headerMath = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::Heading,
		322,
		322);
	Check(
		headerMath != nullptr,
		FromLatin1("latex-markdown-test.md heading range"),
		&ok);
	if (headerMath) {
		Check(
			!HasKind(*headerMath, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md heading display math stays inline"),
			&ok);
	}
	Check(
		HasFormulaOnLine(latex, 322, FromLatin1("ax^2 + bx + c = 0")),
		FromLatin1("latex-markdown-test.md line 322 inline formula"),
		&ok);
	const auto strongMath = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::Strong,
		326,
		326);
	Check(
		strongMath != nullptr,
		FromLatin1("latex-markdown-test.md strong range"),
		&ok);
	if (strongMath) {
		Check(
			!HasKind(*strongMath, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md strong display math stays inline"),
			&ok);
		Check(
			!HasKind(*strongMath, NodeKind::InlineMath),
			FromLatin1("latex-markdown-test.md strong inline math stays literal"),
			&ok);
	}
	Check(
		HasFormulaOnLine(latex, 326, FromLatin1("E = mc^2")),
		FromLatin1("latex-markdown-test.md strong inline formula preserved"),
		&ok);
	const auto emphasisMath = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::Emphasis,
		328,
		328);
	Check(
		emphasisMath != nullptr,
		FromLatin1("latex-markdown-test.md emphasis range"),
		&ok);
	if (emphasisMath) {
		Check(
			!HasKind(*emphasisMath, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md emphasis display math stays inline"),
			&ok);
		Check(
			!HasKind(*emphasisMath, NodeKind::InlineMath),
			FromLatin1("latex-markdown-test.md emphasis inline math stays literal"),
			&ok);
	}
	Check(
		HasFormulaOnLine(latex, 328, FromLatin1("\\pi \\approx 3.14")),
		FromLatin1("latex-markdown-test.md emphasis inline formula preserved"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 381, FromLatin1("a\\,b")),
		FromLatin1("latex-markdown-test.md line 381 inline formula"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 383, FromLatin1("a\\:b")),
		FromLatin1("latex-markdown-test.md line 383 inline formula"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 385, FromLatin1("a\\;b")),
		FromLatin1("latex-markdown-test.md line 385 inline formula"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 391, FromLatin1("a\\!b")),
		FromLatin1("latex-markdown-test.md line 391 inline formula"),
		&ok);
	const auto fencedCode = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::CodeBlock,
		332,
		334);
	Check(
		fencedCode != nullptr,
		FromLatin1("latex-markdown-test.md fenced code range"),
		&ok);
	if (fencedCode) {
		Check(
			!HasKind(*fencedCode, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md fenced code display math stays literal"),
			&ok);
		Check(
			!HasKind(*fencedCode, NodeKind::InlineMath),
			FromLatin1("latex-markdown-test.md fenced code inline math stays literal"),
			&ok);
	}
	const auto inlineCode = FindNodeByKindAndLineRange(
		latex.document,
		NodeKind::InlineCode,
		336,
		336);
	Check(
		inlineCode != nullptr,
		FromLatin1("latex-markdown-test.md inline code range"),
		&ok);
	if (inlineCode) {
		Check(
			!HasKind(*inlineCode, NodeKind::DisplayMath),
			FromLatin1("latex-markdown-test.md inline code display math stays literal"),
			&ok);
		Check(
			!HasKind(*inlineCode, NodeKind::InlineMath),
			FromLatin1("latex-markdown-test.md inline code math stays literal"),
			&ok);
	}
	Check(
		CountFormulasInLineRange(latex, MathKind::Inline, 332, 336) == 0,
		FromLatin1("latex-markdown-test.md code inline formulas excluded"),
		&ok);
	Check(
		CountFormulasInLineRange(latex, MathKind::Display, 332, 336) == 0,
		FromLatin1("latex-markdown-test.md code display formulas excluded"),
		&ok);
	Check(
		!HasFormulaInLineRange(latex, 281, 281),
		FromLatin1("latex-markdown-test.md line 281 exclusion"),
		&ok);
	Check(
		HasFormulaOnLine(latex, 285, FromLatin1("5x + 3")),
		FromLatin1("latex-markdown-test.md line 285 formula"),
		&ok);
	Check(
		!HasFormulaInLineRange(latex, 332, 340),
		FromLatin1("latex-markdown-test.md lines 332-340 exclusions"),
		&ok);

	CheckInlineTextObjectPrepareCoverage(&ok);
	CheckNativeInstantViewPrepareCoverage(&ok);
	CheckCodeBlockTrailingNewlineTrim(&ok);
	CheckCodeBlockSelectionExportCoverage(&ok);
	CheckCodeBlockAsyncSyntaxHighlightCoverage(&ok);
	CheckPrepareCoverage(markdownFixture, latexFixture, &ok);
	CheckPrepareLinkClassification(markdownFixture.path, &ok);
	CheckPreparedExternalLinkCoverage(&ok);
	CheckDetailsSummaryHitCoverage(&ok);
	CheckTableFormulaTextSizeCoverage(&ok);
	CheckArticleRenderSmoke(markdownFixture, latexFixture, &ok);
	CheckInlineTextObjectArticleCoverage(&ok);
	CheckArticleRasterRegressionCoverage(&ok);
	CheckAnchorScrollAlignmentCoverage(&ok);
	CheckArticlePageWidthCoverage(&ok);
	CheckArticleHorizontalRelayoutRegression(&ok);
	CheckNativeInstantViewArticleCoverage(&ok);
	CheckNativeInstantViewEmbedPostAvatarRegression(&ok);
	CheckNativeInstantViewEmbedPostLoadingAvatarPlaceholderRegression(&ok);
	CheckNativeInstantViewPreviewOpenPageCoverage(&ok);
	CheckNativeInstantViewHostedMediaCoverage(&ok);
	CheckNativeInstantViewHostedMediaFallbackCoverage(&ok);
	CheckInlineHtmlCoverage(args.dump, &ok);
	CheckInlineHtmlPrepareCoverage(&ok);
	CheckValidationEdges(&ok);

	return ok ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
	QCoreApplication::setAttribute(Qt::AA_Use96Dpi);
	auto application = QApplication(argc, argv);
	(void)application;

	style::SetDevicePixelRatio(1);
	style::StartManager(style::kScaleDefault);
	const auto result = RunTests(argc, argv);
	style::StopManager();
	return result;
}

namespace crl {

rpl::producer<> on_main_update_requests() {
	return rpl::never<>();
}

} // namespace crl
