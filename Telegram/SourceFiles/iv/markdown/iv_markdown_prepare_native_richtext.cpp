/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "iv/markdown/iv_markdown_prepare_native_richtext.h"

struct GeoPointLocation;

#include "data/data_location.h"
#include "iv/markdown/iv_markdown_prepare_links.h"
#include "ui/basic_click_handlers.h"
#include "history/history_location_manager.h"

#include <limits>
#include <utility>

namespace Iv::Markdown {
namespace {

[[nodiscard]] uint64 GeneratePreparedBlockIdValue(
		NativeIvPrepareState *state) {
	return uint64(++state->nextGeneratedId);
}

[[nodiscard]] PreparedMediaBlockId GeneratePreparedMediaBlockId(
		NativeIvPrepareState *state) {
	return { .value = GeneratePreparedBlockIdValue(state) };
}

[[nodiscard]] PreparedPlaceholderBlockId GeneratePreparedPlaceholderBlockId(
		NativeIvPrepareState *state) {
	return { .value = GeneratePreparedBlockIdValue(state) };
}

[[nodiscard]] const NativeIvPhotoInfo *FindNativeIvPhoto(
		uint64 photoId,
		const NativeIvPrepareState &state) {
	for (const auto &photo : state.photos) {
		if (photo.id == photoId) {
			return &photo;
		}
	}
	return nullptr;
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

void MergeNativeIvDocumentInfo(
		NativeIvDocumentInfo *existing,
		NativeIvDocumentInfo info) {
	if (!existing || (existing->id != info.id)) {
		return;
	}
	if (existing->width <= 0 && info.width > 0) {
		existing->width = info.width;
	}
	if (existing->height <= 0 && info.height > 0) {
		existing->height = info.height;
	}
	if (existing->fileName.isEmpty() && !info.fileName.isEmpty()) {
		existing->fileName = std::move(info.fileName);
	}
	if (existing->title.isEmpty() && !info.title.isEmpty()) {
		existing->title = std::move(info.title);
	}
	if (existing->performer.isEmpty() && !info.performer.isEmpty()) {
		existing->performer = std::move(info.performer);
	}
	if (existing->duration <= 0 && info.duration > 0) {
		existing->duration = info.duration;
	}
	if (!existing->isVideoFile && info.isVideoFile) {
		existing->isVideoFile = true;
	}
	if (!existing->isAnimation && info.isAnimation) {
		existing->isAnimation = true;
	}
}

[[nodiscard]] bool IsNativeIvVideoDocument(
		const NativeIvDocumentInfo &info) {
	return info.isVideoFile || info.isAnimation;
}

[[nodiscard]] bool PrepareNativeIvGroupedMediaItem(
		const MTPPageBlock &item,
		PreparedGroupedMediaItemData *result,
		const NativeIvPrepareState &state) {
	return item.match([&](const MTPDpageBlockPhoto &data) {
		const auto info = FindNativeIvPhoto(uint64(data.vphoto_id().v), state);
		if (!info || info->width <= 0 || info->height <= 0) {
			return false;
		}
		result->media.kind = PreparedMediaItemKind::Photo;
		result->media.id = info->id;
		result->media.width = info->width;
		result->media.height = info->height;
		return true;
	}, [&](const MTPDpageBlockVideo &data) {
		const auto info = FindNativeIvDocument(uint64(data.vvideo_id().v), state);
		if (!info
			|| !IsNativeIvVideoDocument(*info)
			|| info->width <= 0
			|| info->height <= 0) {
			return false;
		}
		result->media.kind = PreparedMediaItemKind::Document;
		result->media.id = info->id;
		result->media.width = info->width;
		result->media.height = info->height;
		return true;
	}, [](const auto &) {
		return false;
	});
}

void SortPreparedIvRichText(PreparedIvRichText *text) {
	SortEntities(&text->text);
}

[[nodiscard]] const QString &NativeIvLinkColorizedEntityData() {
	static const auto result = [] {
		auto value = QString();
		value.reserve(2);
		value.push_back(QChar(kNativeIvLinkSpecialColorIndex));
		value.push_back(QChar(kNativeIvLinkSpecialColorIndex));
		return value;
	}();
	return result;
}

[[nodiscard]] bool AddNativeIvPreparedLink(
		TextWithEntities *text,
		std::vector<PreparedLink> *links,
		int from,
		int length,
		QString target,
		uint64 webpageId = 0) {
	if (!length || target.isEmpty()) {
		return true;
	}
	const auto index = links->size() + 1;
	if (index > std::numeric_limits<uint16>::max()) {
		return true;
	}
	auto prepared = PreparedLink();
	if (webpageId) {
		prepared.index = uint16(index);
		prepared.kind = PreparedLinkKind::InstantViewPage;
		prepared.webpageId = webpageId;
		NormalizePreparedUrlLink(&prepared, target);
	} else {
		prepared = ClassifiedLink(uint16(index), target, nullptr);
	}
	if (prepared.kind == PreparedLinkKind::RejectedRelative
		|| prepared.kind == PreparedLinkKind::LocalFile) {
		return true;
	}
	FinalizePreparedUrlLink(&prepared, QStringView(text->text).mid(from, length));
	if (prepared.kind == PreparedLinkKind::InstantViewPage) {
		text->entities.push_back(EntityInText(
			EntityType::Colorized,
			from,
			length,
			NativeIvLinkColorizedEntityData()));
	}
	text->entities.push_back(EntityInText(
		EntityType::CustomUrl,
		from,
		length,
		InternalLinkData(uint16(index))));
	links->push_back(std::move(prepared));
	return true;
}

[[nodiscard]] bool AddNativeIvEntity(
		TextWithEntities *text,
		int from,
		EntityType type) {
	const auto length = text->text.size() - from;
	if (length <= 0) {
		return true;
	}
	text->entities.push_back(EntityInText(type, from, length));
	return true;
}

[[nodiscard]] bool AppendNativeIvRichText(
		const MTPRichText &text,
		TextWithEntities *result,
		std::vector<PreparedLink> *links,
		QString *blockAnchorId,
		NativeIvPrepareState *state) {
	if (state->blocked()) {
		return false;
	}
	return text.match([&](const MTPDtextEmpty &) {
		return true;
	}, [&](const MTPDtextPlain &data) {
		result->append(qs(data.vtext()));
		return true;
	}, [&](const MTPDtextConcat &data) {
		for (const auto &part : data.vtexts().v) {
			if (!AppendNativeIvRichText(
					part,
					result,
					links,
					blockAnchorId,
					state)) {
				return false;
			}
		}
		return true;
	}, [&](const MTPDtextImage &data) {
		const auto replacementText = u"[image]"_q;
		if (!data.vdocument_id().v || data.vw().v <= 0 || data.vh().v <= 0) {
			result->append(replacementText);
			return true;
		}
		const auto entityData = SerializeInlineTextObjectEntity({
			.kind = InlineTextObjectKind::IvImage,
			.data = InlineTextObjectIvImageData{
				.documentId = uint64(data.vdocument_id().v),
				.width = data.vw().v,
				.height = data.vh().v,
				.replacementText = replacementText,
			},
		});
		if (entityData.isEmpty()) {
			result->append(replacementText);
			return true;
		}
		const auto from = result->text.size();
		result->append(QChar::ObjectReplacementCharacter);
		result->entities.push_back(EntityInText(
			EntityType::CustomEmoji,
			from,
			1,
			entityData));
		return true;
	}, [&](const MTPDtextBold &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Bold);
	}, [&](const MTPDtextItalic &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Italic);
	}, [&](const MTPDtextUnderline &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Underline);
	}, [&](const MTPDtextStrike &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::StrikeOut);
	}, [&](const MTPDtextFixed &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Code);
	}, [&](const MTPDtextUrl &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		if (result->text.size() == from) {
			result->append(qs(data.vurl()));
		}
		const auto webpageId = uint64(data.vwebpage_id().v);
		return AddNativeIvPreparedLink(
			result,
			links,
			from,
			result->text.size() - from,
			qs(data.vurl()),
			webpageId);
	}, [&](const MTPDtextEmail &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		if (result->text.size() == from) {
			result->append(qs(data.vemail()));
		}
		return AddNativeIvPreparedLink(
			result,
			links,
			from,
			result->text.size() - from,
			u"mailto:"_q + qs(data.vemail()));
	}, [&](const MTPDtextSubscript &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Subscript);
	}, [&](const MTPDtextSuperscript &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Superscript);
	}, [&](const MTPDtextMarked &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		return AddNativeIvEntity(result, from, EntityType::Marked);
	}, [&](const MTPDtextPhone &data) {
		const auto from = result->text.size();
		if (!AppendNativeIvRichText(
				data.vtext(),
				result,
				links,
				blockAnchorId,
				state)) {
			return false;
		}
		if (result->text.size() == from) {
			result->append(qs(data.vphone()));
		}
		return AddNativeIvPreparedLink(
			result,
			links,
			from,
			result->text.size() - from,
			u"tel:"_q + qs(data.vphone()));
	}, [&](const MTPDtextAnchor &data) {
		if (blockAnchorId && blockAnchorId->isEmpty()) {
			*blockAnchorId = NormalizeFragmentId(qs(data.vname()));
		}
		return AppendNativeIvRichText(
			data.vtext(),
			result,
			links,
			blockAnchorId,
			state);
	});
}

[[nodiscard]] bool PrepareNativeIvCaption(
		const MTPPageCaption &caption,
		PreparedIvRichText *result,
		QString *blockAnchorId,
		NativeIvPrepareState *state) {
	if (!AppendNativeIvRichText(
			caption.data().vtext(),
			&result->text,
			&result->links,
			blockAnchorId,
			state)) {
		return false;
	}
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
		if (!AppendNativeIvRichText(
				caption.data().vcredit(),
				&result->text,
				&result->links,
				blockAnchorId,
				state)) {
			return false;
		}
	}
	return true;
}

} // namespace

void RememberNativeIvPhoto(
		NativeIvPrepareState *state,
		const MTPPhoto &photo) {
	auto info = NativeIvPhotoInfo{
		.id = photo.match([](const auto &data) {
			return data.vid().v;
		}),
	};
	photo.match([](const MTPDphotoEmpty &) {
	}, [&](const MTPDphoto &data) {
		auto width = 0;
		auto height = 0;
		const auto assign = [&](int w, int h) {
			if (w > 0 && h > 0) {
				width = w;
				height = h;
			}
		};
		for (const auto &size : data.vsizes().v) {
			size.match([&](const MTPDphotoSizeEmpty &) {
			}, [&](const MTPDphotoSize &data) {
				if (data.vtype().v == u"y"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"x"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"w"_q) {
					assign(data.vw().v, data.vh().v);
				}
			}, [&](const MTPDphotoCachedSize &data) {
				if (data.vtype().v == u"y"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"x"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"w"_q) {
					assign(data.vw().v, data.vh().v);
				}
			}, [&](const MTPDphotoStrippedSize &) {
			}, [&](const MTPDphotoSizeProgressive &data) {
				if (data.vtype().v == u"y"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"x"_q) {
					assign(data.vw().v, data.vh().v);
				} else if (!width && data.vtype().v == u"w"_q) {
					assign(data.vw().v, data.vh().v);
				}
			}, [&](const MTPDphotoPathSize &) {
			});
		}
		info.width = width;
		info.height = height;
	});
	if (!info.id) {
		return;
	}
	for (auto &existing : state->photos) {
		if (existing.id == info.id) {
			existing = info;
			return;
		}
	}
	state->photos.push_back(info);
}

void RememberNativeIvDocument(
		NativeIvPrepareState *state,
		const MTPDocument &document) {
	auto info = NativeIvDocumentInfo{
		.id = document.match([](const auto &data) {
			return data.vid().v;
		}),
	};
	document.match([](const MTPDdocumentEmpty &) {
	}, [&](const MTPDdocument &data) {
		const auto assignDimensions = [&](int width, int height, bool force) {
			if (width <= 0 || height <= 0) {
				return;
			}
			if (force || info.width <= 0 || info.height <= 0) {
				info.width = width;
				info.height = height;
			}
		};
		for (const auto &attribute : data.vattributes().v) {
			attribute.match([&](const MTPDdocumentAttributeAudio &data) {
				info.duration = data.vduration().v;
				info.title = qs(data.vtitle().value_or_empty());
				info.performer = qs(data.vperformer().value_or_empty());
			}, [&](const MTPDdocumentAttributeFilename &data) {
				info.fileName = qs(data.vfile_name());
			}, [&](const MTPDdocumentAttributeImageSize &data) {
				assignDimensions(data.vw().v, data.vh().v, false);
			}, [&](const MTPDdocumentAttributeAnimated &) {
				info.isAnimation = true;
			}, [&](const MTPDdocumentAttributeVideo &data) {
				info.isVideoFile = true;
				assignDimensions(data.vw().v, data.vh().v, true);
			}, [&](const auto &) {});
		}
	});
	if (!info.id) {
		return;
	}
	if (const auto existing = FindNativeIvDocument(info.id, *state)) {
		const auto index = existing - state->documents.data();
		MergeNativeIvDocumentInfo(&state->documents[index], std::move(info));
		return;
	}
	state->documents.push_back(std::move(info));
}

bool PrepareNativeIvRichText(
		const MTPRichText &text,
		PreparedIvRichText *result,
		QString *blockAnchorId,
		NativeIvPrepareState *state) {
	return AppendNativeIvRichText(
		text,
		&result->text,
		&result->links,
		blockAnchorId,
		state);
}

bool AppendPreparedIvRichBlock(
		std::vector<PreparedBlock> *result,
		PreparedBlockKind kind,
		int headingLevel,
		PreparedIvRichText prepared,
		QString anchorId,
		bool allowEmpty,
		bool supplementary) {
	SortPreparedIvRichText(&prepared);
	if (prepared.text.text.isEmpty() && !allowEmpty) {
		return true;
	}
	auto block = PreparedBlock();
	block.kind = kind;
	block.headingLevel = headingLevel;
	block.text = std::move(prepared.text);
	block.links = std::move(prepared.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = supplementary;
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvPlainPlaceholderBlock(
		QString label,
		std::vector<PreparedBlock> *result) {
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Placeholder;
	block.placeholder.label = std::move(label);
	block.placeholder.copyText = block.placeholder.label;
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvPhotoBlock(
		const MTPDpageBlockPhoto &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	const auto info = FindNativeIvPhoto(uint64(data.vphoto_id().v), *state);
	if (!info || info->width <= 0 || info->height <= 0) {
		return true;
	}
	auto caption = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvCaption(data.vcaption(), &caption, &anchorId, state)) {
		return false;
	}
	SortPreparedIvRichText(&caption);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Photo;
	block.text = std::move(caption.text);
	block.links = std::move(caption.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = true;
	block.photo.id = GeneratePreparedMediaBlockId(state);
	block.photo.photoId = info->id;
	block.photo.width = info->width;
	block.photo.height = info->height;
	block.photo.urlOverride = data.vurl() ? qs(*data.vurl()) : QString();
	block.photo.viewerOpen = true;
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvVideoBlock(
		const MTPDpageBlockVideo &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	const auto info = FindNativeIvDocument(uint64(data.vvideo_id().v), *state);
	if (!info
		|| !IsNativeIvVideoDocument(*info)
		|| info->width <= 0
		|| info->height <= 0) {
		return true;
	}
	auto caption = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvCaption(data.vcaption(), &caption, &anchorId, state)) {
		return false;
	}
	SortPreparedIvRichText(&caption);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Video;
	block.text = std::move(caption.text);
	block.links = std::move(caption.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = true;
	block.video.id = GeneratePreparedMediaBlockId(state);
	block.video.media.kind = PreparedMediaItemKind::Document;
	block.video.media.id = info->id;
	block.video.media.width = info->width;
	block.video.media.height = info->height;
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvAudioBlock(
		const MTPDpageBlockAudio &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	const auto info = FindNativeIvDocument(uint64(data.vaudio_id().v), *state);
	if (!info) {
		return true;
	}
	auto caption = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvCaption(data.vcaption(), &caption, &anchorId, state)) {
		return false;
	}
	SortPreparedIvRichText(&caption);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Audio;
	block.text = std::move(caption.text);
	block.links = std::move(caption.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = true;
	block.audio.id = GeneratePreparedMediaBlockId(state);
	block.audio.documentId = info->id;
	block.audio.title = info->title;
	block.audio.performer = info->performer;
	block.audio.fileName = info->fileName;
	block.audio.duration = info->duration;
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvMapBlock(
		const MTPDpageBlockMap &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto prepared = PreparedMapBlockData();
	const auto supported = data.vgeo().match([&](const MTPDgeoPoint &geo) {
		if (!geo.vaccess_hash().v || data.vw().v <= 0 || data.vh().v <= 0) {
			return false;
		}
		prepared.latitude = geo.vlat().v;
		prepared.longitude = geo.vlong().v;
		prepared.accessHash = geo.vaccess_hash().v;
		prepared.width = data.vw().v;
		prepared.height = data.vh().v;
		prepared.zoom = data.vzoom().v;
		prepared.url = LocationClickHandler::Url(Data::LocationPoint(geo));
		return true;
	}, [](const auto &) {
		return false;
	});
	if (!supported) {
		return true;
	}
	auto caption = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvCaption(data.vcaption(), &caption, &anchorId, state)) {
		return false;
	}
	SortPreparedIvRichText(&caption);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Map;
	block.text = std::move(caption.text);
	block.links = std::move(caption.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = true;
	prepared.id = GeneratePreparedMediaBlockId(state);
	block.map = std::move(prepared);
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvChannelBlock(
		const MTPDpageBlockChannel &data,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	auto prepared = PreparedChannelBlockData();
	const auto supported = data.vchannel().match([&](const MTPDchannel &channel) {
		prepared.channelId = channel.vid().v;
		prepared.title = qs(channel.vtitle());
		prepared.username = qs(channel.vusername().value_or_empty());
		return true;
	}, [&](const MTPDchannelForbidden &channel) {
		prepared.channelId = channel.vid().v;
		prepared.title = qs(channel.vtitle());
		return true;
	}, [&](const MTPDchat &channel) {
		prepared.channelId = channel.vid().v;
		prepared.title = qs(channel.vtitle());
		return true;
	}, [&](const MTPDchatForbidden &channel) {
		prepared.channelId = channel.vid().v;
		prepared.title = qs(channel.vtitle());
		return true;
	}, [](const auto &) {
		return false;
	});
	if (!supported || !prepared.channelId || prepared.title.isEmpty()) {
		return true;
	}
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Channel;
	prepared.id = GeneratePreparedMediaBlockId(state);
	block.channel = std::move(prepared);
	result->push_back(std::move(block));
	return true;
}

bool PrepareNativeIvGroupedMediaBlock(
		const QVector<MTPPageBlock> &items,
		const MTPPageCaption &caption,
		PreparedGroupedMediaIntent intent,
		QString placeholderLabel,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state) {
	Q_UNUSED(placeholderLabel);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::GroupedMedia;
	block.groupedMedia.id = GeneratePreparedMediaBlockId(state);
	block.groupedMedia.intent = intent;
	block.groupedMedia.items.reserve(items.size());
	for (const auto &item : items) {
		auto prepared = PreparedGroupedMediaItemData();
		prepared.id = GeneratePreparedMediaBlockId(state);
		if (!PrepareNativeIvGroupedMediaItem(item, &prepared, *state)) {
			continue;
		}
		block.groupedMedia.items.push_back(std::move(prepared));
	}
	if (block.groupedMedia.items.empty()) {
		return true;
	}
	auto preparedCaption = PreparedIvRichText();
	if (!PrepareNativeIvCaption(
			caption,
			&preparedCaption,
			&block.anchorId,
			state)) {
		return false;
	}
	SortPreparedIvRichText(&preparedCaption);
	block.text = std::move(preparedCaption.text);
	block.links = std::move(preparedCaption.links);
	block.supplementary = true;
	result->push_back(std::move(block));
	return true;
}

namespace {

[[nodiscard]] QString NativeIvPlaceholderCopyText(
		const QString &label,
		const TextWithEntities &caption) {
	return caption.text.isEmpty()
		? label
		: (label + u"\n"_q + caption.text);
}

} // namespace

bool PrepareNativeIvPlaceholderBlock(
		QString label,
		const MTPPageCaption &caption,
		std::vector<PreparedBlock> *result,
		NativeIvPrepareState *state,
		std::optional<EmbedRequest> embed) {
	auto prepared = PreparedIvRichText();
	auto anchorId = QString();
	if (!PrepareNativeIvCaption(caption, &prepared, &anchorId, state)) {
		return state->result.failure.failed()
			? false
			: PrepareNativeIvPlainPlaceholderBlock(std::move(label), result);
	}
	SortPreparedIvRichText(&prepared);
	auto block = PreparedBlock();
	block.kind = PreparedBlockKind::Placeholder;
	block.text = std::move(prepared.text);
	block.links = std::move(prepared.links);
	block.anchorId = std::move(anchorId);
	block.supplementary = true;
	block.placeholder.label = std::move(label);
	block.placeholder.embed = std::move(embed);
	if (block.placeholder.embed && *block.placeholder.embed) {
		block.placeholder.id = GeneratePreparedPlaceholderBlockId(state);
	}
	block.placeholder.copyText = NativeIvPlaceholderCopyText(
		block.placeholder.label,
		block.text);
	result->push_back(std::move(block));
	return true;
}

} // namespace Iv::Markdown
