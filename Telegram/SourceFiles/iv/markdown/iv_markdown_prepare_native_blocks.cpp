/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_prepare_native_blocks.h"
#include "base/openssl_help.h"
#include "base/unixtime.h"
#include "iv/iv_data.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "lang/lang_keys.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace Iv::Markdown {
namespace {

[[nodiscard]] QString NativeIvDateText(TimeId date) {
	return langDateTimeFull(base::unixtime::parse(date));
}

[[nodiscard]] QString NativeIvRelatedArticleFooterText(
		const MTPDpageRelatedArticle &data) {
	const auto author = qs(data.vauthor().value_or_empty()).trimmed();
	const auto published = data.vpublished_date();
	if (published && !author.isEmpty()) {
		return author + u", "_q + NativeIvDateText(published->v);
	} else if (published) {
		return NativeIvDateText(published->v);
	}
	return author;
}

[[nodiscard]] PreparedLink PrepareNativeIvRelatedArticleLink(
		QString url,
		uint64 webpageId,
		QStringView renderedText) {
	auto result = PreparedLink();
	if (webpageId) {
		result.kind = PreparedLinkKind::InstantViewPage;
		result.webpageId = webpageId;
		NormalizePreparedUrlLink(&result, url);
	} else {
		result = ClassifiedLink(0, url, nullptr);
		if (result.kind == PreparedLinkKind::RejectedRelative
			|| result.kind == PreparedLinkKind::LocalFile) {
			result.kind = PreparedLinkKind::External;
			NormalizePreparedUrlLink(&result, url);
		}
	}
	FinalizePreparedUrlLink(&result, renderedText);
	return result;
}

[[nodiscard]] QString NativeIvDetailsAnchorId(NativeIvPrepareState *state) {
	return u"details-"_q + QString::number(++state->nextGeneratedId);
}

void SortPreparedIvRichText(PreparedIvRichText *text) {
	SortEntities(&text->text);
}

[[nodiscard]] uint16 InternalPreparedLinkIndex(const QString &data) {
	const auto prefix = u"internal:index"_q;
	return (data.size() == prefix.size() + 1 && data.startsWith(prefix))
		? uint16(data.back().unicode())
		: uint16(0);
}

[[nodiscard]] uint16 RemappedPreparedLinkIndex(
		const std::vector<std::pair<uint16, uint16>> &remapped,
		uint16 index) {
	for (const auto &[from, to] : remapped) {
		if (from == index) {
			return to;
		}
	}
	return 0;
}

void AppendPreparedIvRichText(
		PreparedIvRichText *result,
		PreparedIvRichText prepared) {
	auto remapped = std::vector<std::pair<uint16, uint16>>();
	remapped.reserve(prepared.links.size());
	result->links.reserve(result->links.size() + prepared.links.size());
	for (const auto &link : prepared.links) {
		const auto index = result->links.size() + 1;
		if (index > std::numeric_limits<uint16>::max()) {
			continue;
		}
		auto copy = link;
		copy.index = uint16(index);
		remapped.push_back({ link.index, copy.index });
		result->links.push_back(std::move(copy));
	}

	const auto shift = result->text.text.size();
	result->text.text.append(prepared.text.text);
	result->text.entities.reserve(
		result->text.entities.size() + prepared.text.entities.size());
	for (const auto &entity : prepared.text.entities) {
		auto data = entity.data();
		if (entity.type() == EntityType::CustomUrl) {
			if (const auto from = InternalPreparedLinkIndex(data)) {
				if (const auto to = RemappedPreparedLinkIndex(remapped, from)) {
					data = InternalLinkData(to);
				}
			}
		}
		result->text.entities.push_back(EntityInText(
			entity.type(),
			entity.offset() + shift,
			entity.length(),
			data));
	}
}

[[nodiscard]] bool PrepareNativeIvCaption(
		const MTPPageCaption &caption,
		PreparedIvRichText *result,
		QString *blockAnchorId,
		NativeIvPrepareState *state) {
	auto text = PreparedIvRichText();
	if (!PrepareNativeIvRichText(
			caption.data().vtext(),
			&text,
			blockAnchorId,
			state)) {
		return false;
	}
	AppendPreparedIvRichText(result, std::move(text));

	auto credit = PreparedIvRichText();
	if (!PrepareNativeIvRichText(
			caption.data().vcredit(),
			&credit,
			blockAnchorId,
			state)) {
		return false;
	}
	if (!credit.text.text.isEmpty()) {
		if (!result->text.text.isEmpty()) {
			result->text.append(QChar('\n'));
		}
		AppendPreparedIvRichText(result, std::move(credit));
	}
	return true;
}

struct NativeIvHtmlAttribute {
	QByteArray name;
	QByteArray value;
};

using NativeIvHtmlAttributes = std::vector<NativeIvHtmlAttribute>;

[[nodiscard]] QByteArray NativeIvHtmlEscape(QString text) {
	return text.toHtmlEscaped().toUtf8();
}

[[nodiscard]] QByteArray NativeIvRichText(QString text) {
	auto result = NativeIvHtmlEscape(std::move(text));
	result.replace("\xE2\x81\xA6", "<span dir=\"ltr\">");
	result.replace("\xE2\x81\xA7", "<span dir=\"rtl\">");
	result.replace("\xE2\x81\xA8", "<span dir=\"auto\">");
	result.replace("\xE2\x81\xA9", "</span>");
	return result;
}

[[nodiscard]] QByteArray NativeIvHtmlTag(
		QByteArray name,
		NativeIvHtmlAttributes attributes = {},
		QByteArray body = {}) {
	auto serialized = QByteArray();
	for (const auto &[attributeName, value] : attributes) {
		if (attributeName.isEmpty()) {
			continue;
		}
		serialized += ' ';
		serialized += attributeName;
		if (!value.isEmpty()) {
			serialized += "=\"";
			serialized += value;
			serialized += '"';
		}
	}
	return QByteArray("<")
		+ name
		+ serialized
		+ '>'
		+ body
		+ "</"
		+ name
		+ '>';
}

[[nodiscard]] QByteArray NativeIvHashBytes(const QByteArray &bytes) {
	auto binary = std::array<uchar, SHA256_DIGEST_LENGTH>{};
	SHA256(
		reinterpret_cast<const unsigned char*>(bytes.constData()),
		bytes.size(),
		binary.data());
	auto result = QByteArray();
	result.reserve(binary.size() * 2);
	for (const auto byte : binary) {
		const auto hex = [](uchar value) -> char {
			return (value >= 10) ? ('a' + (value - 10)) : ('0' + value);
		};
		result.push_back(hex(byte / 16));
		result.push_back(hex(byte % 16));
	}
	return result;
}

[[nodiscard]] QByteArray StoreNativeIvEmbedHtml(
		QByteArray html,
		NativeIvPrepareState *state) {
	const auto resourceId = QByteArray("native-iv-embed/")
		+ NativeIvHashBytes(html)
		+ ".html";
	state->result.embedHtmlResources.emplace(resourceId, std::move(html));
	return resourceId;
}

[[nodiscard]] QByteArray NativeIvResourceUrl(QByteArray resourceId) {
	return QByteArray("/") + resourceId;
}

[[nodiscard]] QByteArray NativeIvDocumentUrl(uint64 documentId) {
	return NativeIvResourceUrl(
		QByteArray("document/") + QByteArray::number(documentId));
}

[[nodiscard]] QByteArray WrapNativeIvEmbedHtml(QByteArray body) {
	return QByteArray(R"NativeIvHtml(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<script>
window.TelegramWebviewProxy = {
	postEvent: function(eventType, eventData) {
		if (window.external && typeof window.external.invoke === 'function') {
			try {
				window.external.invoke(JSON.stringify([eventType, eventData]));
			} catch (e) {
			}
		}
	}
};
</script>
<style>
html,
body {
	margin: 0;
	padding: 0;
	background: #fff;
	color: #000;
}

body {
	font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
}

figure {
	display: block;
	margin: 0;
	max-width: 100%;
}

img,
video {
	display: block;
	max-width: 100%;
	height: auto;
}

.iframe-wrap {
	display: block;
	margin: 0 auto;
	max-width: 100%;
	overflow: hidden;
}

iframe {
	display: block;
	width: 100%;
	max-width: 100%;
	border: 0;
}
</style>
</head>
<body>)NativeIvHtml")
		+ body
		+ QByteArray(R"NativeIvHtml(<script>
(function() {
	var iframeWindows = new Map();
	var lastPreferredWidth = 0;
	var lastPreferredHeight = 0;
	var measureScheduled = false;

	function clamp(value, max) {
		value = Number(value);
		if (!isFinite(value)) {
			return 0;
		}
		value = Math.round(value);
		if (value < 1) {
			return 0;
		}
		return Math.min(value, max || 100000);
	}

	function resourceId() {
		var path = '';
		try {
			path = String(window.location.pathname || '');
		} catch (e) {
		}
		while (path.charAt(0) === '/') {
			path = path.slice(1);
		}
		return path;
	}

	function documentHeight(doc) {
		if (!doc) {
			return 0;
		}
		var body = doc.body;
		var root = doc.documentElement;
		var height = 0;
		if (body) {
			var bodyRect = body.getBoundingClientRect();
			height = Math.max(
				height,
				body.scrollHeight,
				body.offsetHeight,
				body.clientHeight,
				bodyRect.height);
		}
		if (root) {
			var rootRect = root.getBoundingClientRect();
			height = Math.max(
				height,
				root.scrollHeight,
				root.offsetHeight,
				root.clientHeight,
				rootRect.height);
		}
		return clamp(height, 100000);
	}

	function iframeDocumentHeight(iframe) {
		try {
			return documentHeight(
				iframe.contentDocument
				|| (iframe.contentWindow && iframe.contentWindow.document));
		} catch (e) {
			return 0;
		}
	}

	function findWrap(iframe) {
		var node = iframe;
		while (node && node !== document.body) {
			if (node.classList && node.classList.contains('iframe-wrap')) {
				return node;
			}
			node = node.parentNode;
		}
		return null;
	}

	function rememberIframe(iframe) {
		try {
			if (iframe.contentWindow) {
				iframeWindows.set(iframe.contentWindow, iframe);
			}
		} catch (e) {
		}
	}

	function seedIframeHeight(iframe) {
		if (iframe.getAttribute('data-native-iv-resized') === '1') {
			return;
		}
		var measured = iframeDocumentHeight(iframe);
		if (measured) {
			iframe.setAttribute('data-native-iv-measured', '1');
			iframe.style.height = measured + 'px';
			return;
		}
		var wrap = findWrap(iframe);
		if (!wrap) {
			return;
		}
		var fixed = clamp(wrap.getAttribute('data-height'), 100000);
		if (fixed) {
			iframe.style.height = fixed + 'px';
			return;
		}
		var ratio = Number(wrap.getAttribute('data-aspect-ratio'));
		if (!isFinite(ratio) || ratio <= 0) {
			return;
		}
		var width = clamp(Math.ceil(
			wrap.getBoundingClientRect().width
			|| iframe.getBoundingClientRect().width
			|| 0), 100000);
		if (!width) {
			return;
		}
		var height = clamp(width * ratio, 100000);
		if (height) {
			iframe.style.height = height + 'px';
		}
	}

	function syncIframe(iframe) {
		rememberIframe(iframe);
		seedIframeHeight(iframe);
		if (iframe.getAttribute('data-native-iv-registered') === '1') {
			return;
		}
		iframe.setAttribute('data-native-iv-registered', '1');
		iframe.addEventListener('load', function() {
			rememberIframe(iframe);
			seedIframeHeight(iframe);
			scheduleMeasure();
		}, false);
	}

	function syncIframes() {
		var iframes = document.getElementsByTagName('iframe');
		for (var i = 0; i < iframes.length; i++) {
			syncIframe(iframes[i]);
		}
	}

	function measurePreferredSize() {
		var width = 0;
		var height = 0;
		var viewportWidth = clamp(
			Math.max(
				window.innerWidth || 0,
				document.documentElement
					? document.documentElement.clientWidth
					: 0),
			100000);
		var body = document.body;
		if (body) {
			var bodyRect = body.getBoundingClientRect();
			height = Math.max(
				height,
				body.scrollHeight,
				body.offsetHeight,
				bodyRect.height);
			var children = body.children;
			for (var i = 0; i < children.length; i++) {
				var child = children[i];
				var rect = child.getBoundingClientRect();
				width = Math.max(
					width,
					child.scrollWidth,
					child.offsetWidth,
					rect.width);
				height = Math.max(
					height,
					child.scrollHeight,
					child.offsetHeight,
					rect.bottom - bodyRect.top);
			}
			if (!width) {
				width = Math.max(
					body.scrollWidth,
					body.offsetWidth,
					bodyRect.width);
			}
		}
		var doc = document.documentElement;
		if (doc && (!width || !height)) {
			var docRect = doc.getBoundingClientRect();
			if (!width) {
				width = Math.max(
					doc.scrollWidth,
					doc.offsetWidth,
					doc.clientWidth,
					docRect.width);
			}
			if (!height) {
				height = Math.max(
					doc.scrollHeight,
					doc.offsetHeight,
					doc.clientHeight,
					docRect.height);
			}
		}
		return {
			width: clamp(width, 100000),
			height: clamp(height, 100000),
			viewportWidth: viewportWidth
		};
	}

	function reportPreferredSize() {
		var size = measurePreferredSize();
		if (!size.width || !size.height) {
			return;
		}
		if (size.width === lastPreferredWidth
			&& size.height === lastPreferredHeight) {
			return;
		}
		lastPreferredWidth = size.width;
		lastPreferredHeight = size.height;
		if (window.external && typeof window.external.invoke === 'function') {
			try {
				window.external.invoke(JSON.stringify({
					event: 'preferred_size',
					resourceId: resourceId(),
					width: size.width,
					height: size.height,
					viewportWidth: size.viewportWidth,
					devicePixelRatio: window.devicePixelRatio || 1
				}));
			} catch (e) {
			}
		}
	}

	function scheduleMeasure() {
		if (measureScheduled) {
			return;
		}
		measureScheduled = true;
		var callback = function() {
			measureScheduled = false;
			reportPreferredSize();
		};
		if (window.requestAnimationFrame) {
			window.requestAnimationFrame(callback);
		} else {
			setTimeout(callback, 0);
		}
	}

	window.addEventListener('message', function(event) {
		var iframe = iframeWindows.get(event.source);
		if (!iframe) {
			return;
		}
		var data = event.data;
		if (typeof data === 'string') {
			try {
				data = JSON.parse(data);
			} catch (e) {
				return;
			}
		}
		if (!data || data.eventType !== 'resize_frame' || !data.eventData) {
			return;
		}
		var height = clamp(data.eventData.height, 100000);
		if (!height) {
			return;
		}
		iframe.setAttribute('data-native-iv-resized', '1');
		iframe.style.height = height + 'px';
		scheduleMeasure();
	}, false);

	document.addEventListener('DOMContentLoaded', function() {
		syncIframes();
		scheduleMeasure();
	}, false);

	window.addEventListener('load', function() {
		syncIframes();
		scheduleMeasure();
	}, false);

	window.addEventListener('resize', function() {
		syncIframes();
		scheduleMeasure();
	}, false);

	if (window.ResizeObserver) {
		var observer = new ResizeObserver(function() {
			scheduleMeasure();
		});
		if (document.documentElement) {
			observer.observe(document.documentElement);
		}
		if (document.body) {
			observer.observe(document.body);
		}
	}

	setInterval(function() {
		syncIframes();
		scheduleMeasure();
	}, 250);

	syncIframes();
	scheduleMeasure();
})();
</script>
</body>
</html>)NativeIvHtml");
}

[[nodiscard]] const NativeIvDocumentInfo *FindNativeIvDocument(
		uint64 documentId,
		const NativeIvPrepareState &state) {
	for (const auto &document : state.documents) {
		if (document.id == documentId) {
			return &document;
		}
	}
	return nullptr;
}

[[nodiscard]] QByteArray RenderNativeIvRichTextHtml(
		const MTPRichText &text,
		NativeIvPrepareState *state) {
	return text.match([&](const MTPDtextEmpty &) {
		return QByteArray();
	}, [&](const MTPDtextPlain &data) {
		return NativeIvRichText(qs(data.vtext()));
	}, [&](const MTPDtextConcat &data) {
		auto result = QByteArray();
		for (const auto &part : data.vtexts().v) {
			result += RenderNativeIvRichTextHtml(part, state);
		}
		return result;
	}, [&](const MTPDtextImage &data) {
		const auto documentId = uint64(data.vdocument_id().v);
		if (!FindNativeIvDocument(documentId, *state)) {
			return NativeIvRichText(u"Image not found."_q);
		}
		auto attributes = NativeIvHtmlAttributes{
			{ "class", "pic" },
			{ "src", NativeIvHtmlEscape(
				QString::fromUtf8(NativeIvDocumentUrl(documentId))) },
		};
		if (const auto width = data.vw().v) {
			attributes.push_back({ "width", QByteArray::number(width) });
		}
		if (const auto height = data.vh().v) {
			attributes.push_back({ "height", QByteArray::number(height) });
		}
		return NativeIvHtmlTag("img", attributes);
	}, [&](const MTPDtextBold &data) {
		return NativeIvHtmlTag("b", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextItalic &data) {
		return NativeIvHtmlTag("i", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextUnderline &data) {
		return NativeIvHtmlTag("u", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextStrike &data) {
		return NativeIvHtmlTag("s", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextFixed &data) {
		return NativeIvHtmlTag("code", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextUrl &data) {
		auto attributes = NativeIvHtmlAttributes{
			{ "href", NativeIvHtmlEscape(qs(data.vurl())) },
		};
		return NativeIvHtmlTag(
			"a",
			attributes,
			RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextEmail &data) {
		return NativeIvHtmlTag(
			"a",
			{ { "href", "mailto:" + NativeIvHtmlEscape(qs(data.vemail())) } },
			RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextSubscript &data) {
		return NativeIvHtmlTag("sub", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextSuperscript &data) {
		return NativeIvHtmlTag("sup", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextMarked &data) {
		return NativeIvHtmlTag("mark", {}, RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextPhone &data) {
		return NativeIvHtmlTag(
			"a",
			{ { "href", "tel:" + NativeIvHtmlEscape(qs(data.vphone())) } },
			RenderNativeIvRichTextHtml(data.vtext(), state));
	}, [&](const MTPDtextAnchor &data) {
		auto inner = RenderNativeIvRichTextHtml(data.vtext(), state);
		const auto name = NativeIvHtmlEscape(qs(data.vname()));
		auto anchor = NativeIvHtmlTag("a", { { "name", name } });
		if (inner.isEmpty()) {
			return anchor;
		}
		anchor += inner;
		return NativeIvHtmlTag("span", { { "class", "reference" } }, anchor);
	});
}

[[nodiscard]] QByteArray NativeIvCaptionHtml(
		const MTPPageCaption &caption,
		NativeIvPrepareState *state) {
	auto text = RenderNativeIvRichTextHtml(caption.data().vtext(), state);
	const auto credit = RenderNativeIvRichTextHtml(caption.data().vcredit(), state);
	if (!credit.isEmpty()) {
		text += NativeIvHtmlTag("cite", { { "dir", "auto" } }, credit);
	} else if (text.isEmpty()) {
		return QByteArray();
	}
	return NativeIvHtmlTag("figcaption", { { "dir", "auto" } }, text);
}

[[nodiscard]] QString StripOneTrailingNewline(QString text);
[[nodiscard]] PreparedBlock PrepareNativeIvEmbedPostFallbackParagraph(
	QString url);

[[nodiscard]] QByteArray RenderNativeIvEmbedHtml(
		const MTPDpageBlockEmbed &data,
		NativeIvPrepareState *state,
		bool includeCaption) {
	auto iframeWidth = QByteArray();
	auto iframeHeight = QByteArray();
	auto width = QByteArray();
	auto height = QByteArray();
	auto aspectRatio = QByteArray();
	auto iframeAttributes = NativeIvHtmlAttributes();
	const auto autosize = !data.vw();
	const auto fullWidth = data.is_full_width() || (data.vw() && !data.vw()->v);
	if (autosize) {
		iframeWidth = "100%";
	} else if (fullWidth) {
		width = "100%";
		if (data.vh()) {
			if (data.vw()->v) {
				aspectRatio = QByteArray::number(
					double(data.vh()->v) / std::max(data.vw()->v, 1),
					'g',
					16);
			} else {
				height = QByteArray::number(data.vh()->v) + "px";
			}
		}
		iframeWidth = "100%";
		iframeHeight = height;
	} else {
		width = QByteArray::number(data.vw()->v) + "px";
		if (data.vh()) {
			aspectRatio = QByteArray::number(
				double(data.vh()->v) / std::max(data.vw()->v, 1),
				'g',
				16);
		}
	}
	if (!iframeWidth.isEmpty()) {
		iframeAttributes.push_back({ "width", iframeWidth });
	}
	if (!iframeHeight.isEmpty()) {
		iframeAttributes.push_back({ "height", iframeHeight });
	}
	if (const auto url = data.vurl()) {
		if (autosize) {
			iframeAttributes.push_back({
				"srcdoc",
				NativeIvHtmlEscape(qs(*url)),
			});
		} else {
			iframeAttributes.push_back({
				"src",
				NativeIvHtmlEscape(qs(*url)),
			});
		}
	} else if (const auto html = data.vhtml()) {
		const auto resourceId = StoreNativeIvEmbedHtml(html->v, state);
		iframeAttributes.push_back({
			"src",
			NativeIvHtmlEscape(QString::fromUtf8(NativeIvResourceUrl(resourceId))),
		});
	} else {
		return QByteArray();
	}
	if (!data.is_allow_scrolling()) {
		iframeAttributes.push_back({ "scrolling", "no" });
	}
	iframeAttributes.push_back({ "frameborder", "0" });
	iframeAttributes.push_back({ "allowtransparency", "true" });
	iframeAttributes.push_back({ "allowfullscreen", "true" });
	auto content = NativeIvHtmlTag("iframe", iframeAttributes);
	if (!autosize) {
		auto wrapAttributes = NativeIvHtmlAttributes{
			{ "class", "iframe-wrap" },
		};
		auto style = QByteArray();
		if (!width.isEmpty()) {
			style += QByteArray("width:") + width + ';';
		}
		if (!style.isEmpty()) {
			wrapAttributes.push_back({ "style", style });
		}
		if (!height.isEmpty()) {
			wrapAttributes.push_back({
				"data-height",
				QByteArray::number(data.vh()->v),
			});
		} else if (!aspectRatio.isEmpty()) {
			wrapAttributes.push_back({
				"data-aspect-ratio",
				aspectRatio,
			});
		}
		content = NativeIvHtmlTag("div", wrapAttributes, content);
	}
	if (includeCaption) {
		content += NativeIvCaptionHtml(data.vcaption(), state);
	}
	auto figureAttributes = NativeIvHtmlAttributes();
	if (!fullWidth) {
		figureAttributes.push_back({ "class", "nowide" });
	}
	return NativeIvHtmlTag("figure", figureAttributes, content);
}

[[nodiscard]] bool PrepareNativeIvEmbedBlock(
		const MTPDpageBlockEmbed &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	const auto label = tr::lng_iv_click_to_view(tr::now);
	const auto html = RenderNativeIvEmbedHtml(data, state, false);
	if (html.isEmpty()) {
		return PrepareNativeIvPlaceholderBlock(
			label,
			data.vcaption(),
			result,
			state);
	}
	auto request = EmbedRequest{
		.resourceId = StoreNativeIvEmbedHtml(
			WrapNativeIvEmbedHtml(html),
			state),
		.fallbackUrl = (!data.vw() || !data.vurl())
			? QString()
			: qs(*data.vurl()),
		.width = data.vw() ? data.vw()->v : 0,
		.height = data.vh() ? data.vh()->v : 0,
		.fullWidth = data.is_full_width() || (data.vw() && !data.vw()->v),
		.fixedHeight = (data.vh() != nullptr),
		.allowScrolling = data.is_allow_scrolling(),
	};
	return PrepareNativeIvPlaceholderBlock(
		label,
		data.vcaption(),
		result,
		state,
		std::move(request));
}

[[nodiscard]] bool PrepareNativeIvEmbedPostBlock(
		const MTPDpageBlockEmbedPost &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto caption = PreparedIvRichText();
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::EmbedPost;
	block.embedPost.url = qs(data.vurl()).trimmed();
	block.embedPost.authorPhotoId = uint64(data.vauthor_photo_id().v);
	block.embedPost.author = qs(data.vauthor()).trimmed();
	if (const auto date = data.vdate().v) {
		block.embedPost.dateText = NativeIvDateText(date);
	}
	if (!PrepareNativeIvCaption(
			data.vcaption(),
			&caption,
			&block.anchorId,
			state)) {
		return false;
	}
	SortPreparedIvRichText(&caption);
	block.text = std::move(caption.text);
	block.links = std::move(caption.links);
	block.supplementary = true;
	if (data.vblocks().v.isEmpty()) {
		block.children.push_back(
			PrepareNativeIvEmbedPostFallbackParagraph(block.embedPost.url));
	} else if (!PrepareNativeIvBlocks(
			data.vblocks().v,
			&block.children,
			state)) {
		return false;
	}
	result->push_back(std::move(block));
	return true;
}

[[nodiscard]] QString StripOneTrailingNewline(QString text) {
	if (text.endsWith(u"\r\n"_q)) {
		text.chop(2);
	} else if (!text.isEmpty()) {
		const auto last = text.back();
		if ((last == QChar(u'\n')) || (last == QChar(u'\r'))) {
			text.chop(1);
		}
	}
	return text;
}

[[nodiscard]] PreparedBlock EmptyParagraphBlock() {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Paragraph;
	return block;
}

[[nodiscard]] PreparedBlock PrepareNativeIvEmbedPostFallbackParagraph(
		QString url) {
	auto block = EmptyParagraphBlock();
	if (url.isEmpty()) {
		return block;
	}
	auto link = PreparedLink();
	link.index = 1;
	link.kind = PreparedLinkKind::External;
	NormalizePreparedUrlLink(&link, url);
	FinalizePreparedUrlLink(&link, url);
	block.text.text = std::move(url);
	block.text.entities.push_back(EntityInText(
		EntityType::CustomUrl,
		0,
		block.text.text.size(),
		InternalLinkData(link.index)));
	block.links.push_back(std::move(link));
	return block;
}

[[nodiscard]] PreparedBlock PrepareRuleBlock() {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Rule;
	return block;
}

[[nodiscard]] bool AppendNativeIvFlowBlock(
		std::vector<PreparedBlock> *result,
		PreparedBlockKind kind,
		int headingLevel,
		const MTPRichText &text,
		NativeIvPrepareState *state,
		bool allowEmpty = false) {
	auto prepared = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvRichText(text, &prepared, &anchorId, state)) {
		return false;
	}
	return AppendPreparedIvRichBlock(
		result,
		kind,
		headingLevel,
		std::move(prepared),
		std::move(anchorId),
		allowEmpty);
}

[[nodiscard]] bool PrepareNativeIvQuoteBlock(
		const MTPRichText &text,
		const MTPRichText &caption,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Quote;
	auto body = PreparedIvRichText();
	if (!PrepareNativeIvRichText(text, &body, &block.anchorId, state)) {
		return false;
	}
	if (!AppendPreparedIvRichBlock(
		&block.children,
		PreparedBlockKind::Paragraph,
		0,
		std::move(body))) {
		return false;
	}
	auto cite = PreparedIvRichText();
	if (!PrepareNativeIvRichText(caption, &cite, &block.anchorId, state)) {
		return false;
	}
	if (!AppendPreparedIvRichBlock(
		&block.children,
		PreparedBlockKind::Paragraph,
		0,
		std::move(cite),
		QString(),
		false,
		true)) {
		return false;
	}
	if (block.children.empty()) {
		block.children.push_back(EmptyParagraphBlock());
	}
	result->push_back(std::move(block));
	return true;
}

[[nodiscard]] bool PrepareNativeIvList(
		const QVector<MTPPageListItem> &items,
		PreparedBlock *result,
		NativeIvPrepareState *state) {
	for (const auto &item : items) {
		auto block = PreparedBlock();
		block.kind = PreparedBlockKind::ListItem;
		block.listKind = result->listKind;
		block.listDelimiter = result->listDelimiter;
		const auto ok = item.match([&](const MTPDpageListItemText &data) {
			auto prepared = PreparedIvRichText();
			if (!PrepareNativeIvRichText(
					data.vtext(),
					&prepared,
					&block.anchorId,
					state)) {
				return false;
			}
			return AppendPreparedIvRichBlock(
				&block.children,
				PreparedBlockKind::Paragraph,
				0,
				std::move(prepared));
		}, [&](const MTPDpageListItemBlocks &data) {
			return PrepareNativeIvBlocks(data.vblocks().v, &block.children, state);
		});
		if (!ok) {
			return false;
		}
		if (block.children.empty()) {
			block.children.push_back(EmptyParagraphBlock());
		}
		result->children.push_back(std::move(block));
	}
	return true;
}

[[nodiscard]] bool ParseOrderedNumber(
		const QString &value,
		int *result) {
	auto ok = false;
	const auto parsed = value.toInt(&ok);
	if (!ok) {
		return false;
	}
	*result = parsed;
	return true;
}

[[nodiscard]] int NextNativeIvOrderedNumber(const PreparedBlock &result) {
	return result.children.empty()
		? result.startNumber
		: (result.children.back().orderedNumber + 1);
}

[[nodiscard]] bool PrepareNativeIvOrderedList(
		const QVector<MTPPageListOrderedItem> &items,
		PreparedBlock *result,
		NativeIvPrepareState *state) {
	auto firstNumber = true;
	for (const auto &item : items) {
		auto block = PreparedBlock();
		block.kind = PreparedBlockKind::ListItem;
		block.listKind = result->listKind;
		block.listDelimiter = result->listDelimiter;
		const auto ok = item.match([&](const MTPDpageListOrderedItemText &data) {
			auto orderedNumber = 0;
			if (!ParseOrderedNumber(qs(data.vnum()), &orderedNumber)) {
				orderedNumber = NextNativeIvOrderedNumber(*result);
			}
			block.orderedNumber = orderedNumber;
			auto prepared = PreparedIvRichText();
			if (!PrepareNativeIvRichText(
					data.vtext(),
					&prepared,
					&block.anchorId,
					state)) {
				return false;
			}
			return AppendPreparedIvRichBlock(
				&block.children,
				PreparedBlockKind::Paragraph,
				0,
				std::move(prepared));
		}, [&](const MTPDpageListOrderedItemBlocks &data) {
			auto orderedNumber = 0;
			if (!ParseOrderedNumber(qs(data.vnum()), &orderedNumber)) {
				orderedNumber = NextNativeIvOrderedNumber(*result);
			}
			block.orderedNumber = orderedNumber;
			return PrepareNativeIvBlocks(data.vblocks().v, &block.children, state);
		});
		if (!ok) {
			return false;
		}
		if (firstNumber) {
			result->startNumber = block.orderedNumber;
			firstNumber = false;
		}
		if (block.children.empty()) {
			block.children.push_back(EmptyParagraphBlock());
		}
		result->children.push_back(std::move(block));
	}
	return true;
}

[[nodiscard]] TableAlignment NativeIvTableAlignment(
		const MTPDpageTableCell &cell) {
	if (cell.is_align_right()) {
		return TableAlignment::Right;
	} else if (cell.is_align_center()) {
		return TableAlignment::Center;
	}
	return TableAlignment::Left;
}

[[nodiscard]] PreparedTableCellVerticalAlignment NativeIvTableVerticalAlignment(
		const MTPDpageTableCell &cell) {
	if (cell.is_valign_bottom()) {
		return PreparedTableCellVerticalAlignment::Bottom;
	} else if (cell.is_valign_middle()) {
		return PreparedTableCellVerticalAlignment::Middle;
	}
	return PreparedTableCellVerticalAlignment::Top;
}

using NativeIvTableOccupancyRow = std::vector<char>;
using NativeIvTableOccupancyGrid = std::vector<NativeIvTableOccupancyRow>;

[[nodiscard]] int NormalizeNativeIvTableSpan(int span) {
	return std::max(span, 1);
}

[[nodiscard]] int ClampNativeIvTableRowspan(
		int rawRowspan,
		int row,
		int rowCount) {
	if ((row < 0) || (row >= rowCount) || (rowCount <= 0)) {
		return 0;
	}
	const auto remainingRows = int64(rowCount) - row;
	return int(std::min<int64>(
		NormalizeNativeIvTableSpan(rawRowspan),
		remainingRows));
}

[[nodiscard]] int ClampNativeIvTableColspan(
		int rawColspan,
		int column,
		int maxColumns) {
	if ((column < 0) || (column >= maxColumns) || (maxColumns <= 0)) {
		return 0;
	}
	const auto remainingColumns = int64(maxColumns) - column;
	return int(std::min<int64>(
		NormalizeNativeIvTableSpan(rawColspan),
		remainingColumns));
}

[[nodiscard]] bool CanOccupyNativeIvTableSlots(
		const NativeIvTableOccupancyGrid &occupancy,
		int row,
		int column,
		int rowspan,
		int colspan) {
	if ((row < 0)
		|| (column < 0)
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (row >= int(occupancy.size()))) {
		return false;
	}
	const auto rowLimit = int(std::min<int64>(
		int64(row) + rowspan,
		occupancy.size()));
	const auto columnLimit64 = int64(column) + colspan;
	if (columnLimit64 <= column) {
		return false;
	}
	const auto columnLimit = int(std::min<int64>(
		columnLimit64,
		std::numeric_limits<int>::max()));
	for (auto currentRow = row; currentRow < rowLimit; ++currentRow) {
		const auto &occupied = occupancy[currentRow];
		const auto occupiedLimit = std::min(columnLimit, int(occupied.size()));
		for (auto currentColumn = column;
			currentColumn < occupiedLimit;
			++currentColumn) {
			if (occupied[currentColumn]) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] int FirstAvailableNativeIvTableColumn(
		const NativeIvTableOccupancyGrid &occupancy,
		int row,
		int rowspan,
		int colspan,
		int maxColumns) {
	if ((row < 0)
		|| (row >= int(occupancy.size()))
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (maxColumns <= 0)) {
		return -1;
	}
	for (auto column = 0; column < maxColumns; ++column) {
		const auto effectiveColspan = ClampNativeIvTableColspan(
			colspan,
			column,
			maxColumns);
		if (effectiveColspan <= 0) {
			continue;
		}
		if (CanOccupyNativeIvTableSlots(
				occupancy,
				row,
				column,
				rowspan,
				effectiveColspan)) {
			return column;
		}
	}
	return -1;
}

void MarkNativeIvTableSlots(
		NativeIvTableOccupancyGrid *occupancy,
		int row,
		int column,
		int rowspan,
		int colspan) {
	if ((row < 0)
		|| (column < 0)
		|| (rowspan <= 0)
		|| (colspan <= 0)
		|| (row >= int(occupancy->size()))) {
		return;
	}
	const auto rowLimit = int(std::min<int64>(
		int64(row) + rowspan,
		occupancy->size()));
	const auto columnLimit64 = int64(column) + colspan;
	if (columnLimit64 <= column) {
		return;
	}
	const auto columnLimit = int(std::min<int64>(
		columnLimit64,
		std::numeric_limits<int>::max()));
	for (auto currentRow = row; currentRow < rowLimit; ++currentRow) {
		auto &occupied = (*occupancy)[currentRow];
		if (columnLimit > int(occupied.size())) {
			occupied.resize(columnLimit, false);
		}
		for (auto currentColumn = column;
			currentColumn < columnLimit;
			++currentColumn) {
			occupied[currentColumn] = true;
		}
	}
}

[[nodiscard]] int NativeIvTableColumnCount(
		const NativeIvTableOccupancyGrid &occupancy) {
	auto result = 0;
	for (const auto &row : occupancy) {
		result = std::max(result, int(row.size()));
	}
	return result;
}

[[nodiscard]] bool PrepareNativeIvTableBlock(
		const MTPDpageBlockTable &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Table;
	block.tableBordered = data.is_bordered();
	block.tableStriped = data.is_striped();

	auto title = PreparedIvRichText();
	if (!PrepareNativeIvRichText(
			data.vtitle(),
			&title,
			&block.anchorId,
			state)) {
		return false;
	}
	SortPreparedIvRichText(&title);
	block.text = std::move(title.text);
	block.links = std::move(title.links);

	const auto &limits = PrepareTableRenderLimitsForIv();
	const auto rowCount = std::min(int(data.vrows().v.size()), limits.maxRows);

	auto occupancy = NativeIvTableOccupancyGrid(rowCount);
	block.tableRows.reserve(rowCount);
	auto occupiedSlotCountSoFar = int64(0);

	auto rowIndex = 0;
	for (const auto &row : data.vrows().v) {
		if (rowIndex >= rowCount) {
			break;
		}
		auto preparedRow = PreparedTableRow();
		const auto ok = row.match([&](const MTPDpageTableRow &rowData) {
			preparedRow.cells.reserve(std::min(
				int(rowData.vcells().v.size()),
				limits.maxColumns));
			for (const auto &cell : rowData.vcells().v) {
				auto preparedCell = PreparedTableCell();
				const auto cellOk = cell.match([&](const MTPDpageTableCell &cellData) {
					const auto rawColspan = cellData.vcolspan()
						? cellData.vcolspan()->v
						: 1;
					const auto rawRowspan = cellData.vrowspan()
						? cellData.vrowspan()->v
						: 1;
					const auto normalizedColspan = NormalizeNativeIvTableSpan(
						rawColspan);
					const auto rowspan = ClampNativeIvTableRowspan(
						rawRowspan,
						rowIndex,
						rowCount);
					if (rowspan <= 0) {
						return true;
					}
					const auto column = FirstAvailableNativeIvTableColumn(
						occupancy,
						rowIndex,
						rowspan,
						normalizedColspan,
						limits.maxColumns);
					if (column < 0) {
						return true;
					}
					const auto colspan = ClampNativeIvTableColspan(
						normalizedColspan,
						column,
						limits.maxColumns);
					if (colspan <= 0) {
						return true;
					}
					const auto occupiedSlotGrowth = int64(rowspan) * colspan;
					if (occupiedSlotGrowth > limits.maxCells
						|| (occupiedSlotCountSoFar + occupiedSlotGrowth)
							> limits.maxCells) {
						return true;
					}
					preparedCell.column = column;
					preparedCell.alignment = NativeIvTableAlignment(cellData);
					preparedCell.header = cellData.is_header();
					preparedCell.verticalAlignment
						= NativeIvTableVerticalAlignment(cellData);
					preparedCell.colspan = colspan;
					preparedCell.rowspan = rowspan;
					if (cellData.vtext()) {
						auto rich = PreparedIvRichText();
						if (!PrepareNativeIvRichText(
								*cellData.vtext(),
								&rich,
								nullptr,
								state)) {
							return false;
						}
						SortPreparedIvRichText(&rich);
						preparedCell.text = std::move(rich.text);
						preparedCell.links = std::move(rich.links);
					}
					MarkNativeIvTableSlots(
						&occupancy,
						rowIndex,
						column,
						rowspan,
						colspan);
					occupiedSlotCountSoFar += occupiedSlotGrowth;
					preparedRow.cells.push_back(std::move(preparedCell));
					return true;
				});
				if (!cellOk) {
					return false;
				}
			}
			preparedRow.header = !preparedRow.cells.empty()
				&& std::all_of(
					preparedRow.cells.begin(),
					preparedRow.cells.end(),
					[](const PreparedTableCell &cell) {
						return cell.header;
					});
			return true;
		});
		if (!ok) {
			return false;
		}
		block.tableRows.push_back(std::move(preparedRow));
		++rowIndex;
	}

	block.tableColumnCount = NativeIvTableColumnCount(occupancy);

	block.tableAlignments.resize(block.tableColumnCount, TableAlignment::Left);
	for (const auto &preparedRow : block.tableRows) {
		for (const auto &preparedCell : preparedRow.cells) {
			const auto from = std::max(preparedCell.column, 0);
			const auto to = std::min(
				preparedCell.column + preparedCell.colspan,
				block.tableColumnCount);
			for (auto column = from; column != to; ++column) {
				block.tableAlignments[column] = preparedCell.alignment;
			}
		}
	}
	result->push_back(std::move(block));
	return true;
}

[[nodiscard]] bool PrepareNativeIvDetailsBlock(
		const MTPDpageBlockDetails &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto summary = PreparedIvRichText();
	auto anchorId = NativeIvDetailsAnchorId(state);
	if (!PrepareNativeIvRichText(
			data.vtitle(),
			&summary,
			&anchorId,
			state)) {
		return false;
	}
	if (!summary.links.empty()) {
		const auto isLink = [](const EntityInText &entity) {
			return entity.type() == EntityType::CustomUrl;
		};
		const auto from = std::remove_if(
			summary.text.entities.begin(),
			summary.text.entities.end(),
			isLink);
		summary.text.entities.erase(from, summary.text.entities.end());
		summary.links.clear();
	}
	SortPreparedIvRichText(&summary);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Details;
	block.anchorId = std::move(anchorId);
	block.collapsed = !data.is_open();
	block.text = std::move(summary.text);
	block.links = std::move(summary.links);
	if (!PrepareNativeIvBlocks(data.vblocks().v, &block.children, state)) {
		return false;
	}
	result->push_back(std::move(block));
	return true;
}

[[nodiscard]] bool PrepareNativeIvRelatedArticlesBlock(
		const MTPDpageBlockRelatedArticles &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto related = std::vector<PreparedBlock>();
	related.reserve(data.varticles().v.size());
	for (const auto &article : data.varticles().v) {
		auto prepared = PreparedBlock();
		prepared.kind = PreparedBlockKind::RelatedArticle;
		const auto &row = article.data();
		prepared.relatedArticle.title = qs(
			row.vtitle().value_or_empty()).trimmed();
		prepared.relatedArticle.description = qs(
			row.vdescription().value_or_empty()).trimmed();
		prepared.relatedArticle.footer = NativeIvRelatedArticleFooterText(row);
		prepared.relatedArticle.photoId = row.vphoto_id().value_or_empty();
		if (prepared.relatedArticle.title.isEmpty()
			&& prepared.relatedArticle.description.isEmpty()
			&& prepared.relatedArticle.footer.isEmpty()) {
			prepared.relatedArticle.title = qs(row.vurl()).trimmed();
		}
		const auto linkText = !prepared.relatedArticle.title.isEmpty()
			? prepared.relatedArticle.title
			: !prepared.relatedArticle.description.isEmpty()
			? prepared.relatedArticle.description
			: prepared.relatedArticle.footer;
		prepared.relatedArticle.link = PrepareNativeIvRelatedArticleLink(
			qs(row.vurl()),
			uint64(row.vwebpage_id().v),
			linkText);
		const auto appendLine = [&](QString *copyText, const QString &line) {
			if (line.isEmpty()) {
				return;
			} else if (!copyText->isEmpty()) {
				copyText->append(QChar('\n'));
			}
			copyText->append(line);
		};
		appendLine(&prepared.relatedArticle.copyText, prepared.relatedArticle.title);
		appendLine(
			&prepared.relatedArticle.copyText,
			prepared.relatedArticle.description);
		appendLine(&prepared.relatedArticle.copyText, prepared.relatedArticle.footer);
		if (prepared.relatedArticle.copyText.isEmpty()) {
			prepared.relatedArticle.copyText = prepared.relatedArticle.link.target;
		}
		related.push_back(std::move(prepared));
	}
	if (related.empty()) {
		return true;
	}

	auto title = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvRichText(
			data.vtitle(),
			&title,
			&anchorId,
			state)) {
		return false;
	}
	SortPreparedIvRichText(&title);
	if (!title.text.text.isEmpty()) {
		if (!AppendPreparedIvRichBlock(
				result,
				PreparedBlockKind::Heading,
				4,
				std::move(title),
				std::move(anchorId))) {
			return false;
		}
	} else if (!anchorId.isEmpty() && related.front().anchorId.isEmpty()) {
		related.front().anchorId = std::move(anchorId);
	}
	result->insert(
		result->end(),
		std::make_move_iterator(related.begin()),
		std::make_move_iterator(related.end()));
	return true;
}

[[nodiscard]] bool PrepareNativeIvBlock(
		const MTPPageBlock &block,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvBlock(
		const MTPPageBlock &block,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	if (state->blocked()) {
		return false;
	}
	return block.match([&](const MTPDpageBlockUnsupported &) {
		return PrepareNativeIvPlainPlaceholderBlock(
			u"Unsupported Content"_q,
			result);
	}, [&](const MTPDpageBlockTitle &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Heading,
			1,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockSubtitle &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Heading,
			2,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockAuthorDate &data) {
		auto prepared = PreparedIvRichText();
		auto anchorId = QString();
		if (!PrepareNativeIvRichText(
				data.vauthor(),
				&prepared,
				&anchorId,
				state)) {
			return false;
		}
		if (const auto date = data.vpublished_date().v) {
			if (!prepared.text.text.isEmpty()) {
				prepared.text.append(u" \u2022 "_q);
			}
			prepared.text.append(NativeIvDateText(date));
		}
		return AppendPreparedIvRichBlock(
			result,
			PreparedBlockKind::Paragraph,
			0,
			std::move(prepared),
			std::move(anchorId),
			false,
			true);
	}, [&](const MTPDpageBlockHeader &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Heading,
			3,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockSubheader &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Heading,
			4,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockParagraph &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Paragraph,
			0,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockPreformatted &data) {
		auto prepared = PreparedIvRichText();
		auto anchorId = QString();
		if (!PrepareNativeIvRichText(
				data.vtext(),
				&prepared,
				&anchorId,
				state)) {
			return false;
		}
		auto block = PreparedBlock();
		block.kind = PreparedBlockKind::CodeBlock;
		block.anchorId = std::move(anchorId);
		block.codeLanguage = qs(data.vlanguage()).trimmed();
		block.text.text = StripOneTrailingNewline(prepared.text.text);
		result->push_back(std::move(block));
		return true;
	}, [&](const MTPDpageBlockFooter &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Paragraph,
			0,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockDivider &) {
		result->push_back(PrepareRuleBlock());
		return true;
	}, [&](const MTPDpageBlockAnchor &data) {
		const auto anchorId = NormalizeFragmentId(qs(data.vname()));
		if (anchorId.isEmpty()) {
			return true;
		}
		auto prepared = PreparedIvRichText();
		return AppendPreparedIvRichBlock(
			result,
			PreparedBlockKind::Paragraph,
			0,
			std::move(prepared),
			anchorId,
			true);
	}, [&](const MTPDpageBlockList &data) {
		auto prepared = PreparedBlock();
		prepared.kind = PreparedBlockKind::List;
		prepared.listKind = ListKind::Bullet;
		return PrepareNativeIvList(data.vitems().v, &prepared, state)
			? (result->push_back(std::move(prepared)), true)
			: false;
	}, [&](const MTPDpageBlockBlockquote &data) {
		return PrepareNativeIvQuoteBlock(
			data.vtext(),
			data.vcaption(),
			result,
			state);
	}, [&](const MTPDpageBlockPullquote &data) {
		return PrepareNativeIvQuoteBlock(
			data.vtext(),
			data.vcaption(),
			result,
			state);
	}, [&](const MTPDpageBlockPhoto &data) {
		return PrepareNativeIvPhotoBlock(data, result, state);
	}, [&](const MTPDpageBlockVideo &data) {
		return PrepareNativeIvVideoBlock(data, result, state);
	}, [&](const MTPDpageBlockCover &data) {
		return PrepareNativeIvBlock(data.vcover(), result, state);
	}, [&](const MTPDpageBlockEmbed &data) {
		return PrepareNativeIvEmbedBlock(data, result, state);
	}, [&](const MTPDpageBlockEmbedPost &data) {
		return PrepareNativeIvEmbedPostBlock(data, result, state);
	}, [&](const MTPDpageBlockCollage &data) {
		return PrepareNativeIvGroupedMediaBlock(
			data.vitems().v,
			data.vcaption(),
			PreparedGroupedMediaIntent::Collage,
			u"Collage placeholder"_q,
			result,
			state);
	}, [&](const MTPDpageBlockSlideshow &data) {
		return PrepareNativeIvGroupedMediaBlock(
			data.vitems().v,
			data.vcaption(),
			PreparedGroupedMediaIntent::Slideshow,
			u"Grouped Media Placeholder"_q,
			result,
			state);
	}, [&](const MTPDpageBlockChannel &data) {
		return PrepareNativeIvChannelBlock(data, result, state);
	}, [&](const MTPDpageBlockAudio &data) {
		return PrepareNativeIvAudioBlock(data, result, state);
	}, [&](const MTPDpageBlockKicker &data) {
		return AppendNativeIvFlowBlock(
			result,
			PreparedBlockKind::Heading,
			5,
			data.vtext(),
			state);
	}, [&](const MTPDpageBlockTable &data) {
		return PrepareNativeIvTableBlock(data, result, state);
	}, [&](const MTPDpageBlockOrderedList &data) {
		auto prepared = PreparedBlock();
		prepared.kind = PreparedBlockKind::List;
		prepared.listKind = ListKind::Ordered;
		prepared.listDelimiter = ListDelimiter::Period;
		prepared.startNumber = 1;
		return PrepareNativeIvOrderedList(data.vitems().v, &prepared, state)
			? (result->push_back(std::move(prepared)), true)
			: false;
	}, [&](const MTPDpageBlockDetails &data) {
		return PrepareNativeIvDetailsBlock(data, result, state);
	}, [&](const MTPDpageBlockRelatedArticles &data) {
		return PrepareNativeIvRelatedArticlesBlock(data, result, state);
	}, [&](const MTPDpageBlockMap &data) {
		return PrepareNativeIvMapBlock(data, result, state);
		});
}

} // namespace

bool PrepareNativeIvBlocks(
		const QVector<MTPPageBlock> &blocks,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	for (const auto &block : blocks) {
		if (!PrepareNativeIvBlock(block, result, state)) {
			if (state->result.failure.failed()) {
				return false;
			}
			(void)PrepareNativeIvPlainPlaceholderBlock(
				u"Unsupported Block"_q,
				result);
		}
	}
	return !state->result.failure.failed();
}

} // namespace Iv::Markdown
