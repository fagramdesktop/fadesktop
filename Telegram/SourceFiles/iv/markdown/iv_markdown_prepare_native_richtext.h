/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_prepare_state.h"

namespace Iv::Markdown {

inline constexpr auto kNativeIvLinkSpecialColorIndex = 9;

struct PreparedIvRichText {
	TextWithEntities text;
	std::vector<PreparedLink> links;
};

void RememberNativeIvPhoto(
	NativeIvPrepareState *state,
	const MTPPhoto &photo);
void RememberNativeIvDocument(
	NativeIvPrepareState *state,
	const MTPDocument &document);
[[nodiscard]] bool PrepareNativeIvPlainPlaceholderBlock(
	QString label,
	std::vector<PreparedBlock> *result);
[[nodiscard]] bool PrepareNativeIvPlaceholderBlock(
	QString label,
	const MTPPageCaption &caption,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state,
	std::optional<EmbedRequest> embed = std::nullopt);
[[nodiscard]] bool PrepareNativeIvPhotoBlock(
	const MTPDpageBlockPhoto &data,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvVideoBlock(
	const MTPDpageBlockVideo &data,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvAudioBlock(
	const MTPDpageBlockAudio &data,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvMapBlock(
	const MTPDpageBlockMap &data,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvChannelBlock(
	const MTPDpageBlockChannel &data,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvGroupedMediaBlock(
	const QVector<MTPPageBlock> &items,
	const MTPPageCaption &caption,
	PreparedGroupedMediaIntent intent,
	QString placeholderLabel,
	std::vector<PreparedBlock> *result,
	NativeIvPrepareState *state);
[[nodiscard]] bool PrepareNativeIvRichText(
	const MTPRichText &text,
	PreparedIvRichText *result,
	QString *blockAnchorId,
	NativeIvPrepareState *state);
[[nodiscard]] bool AppendPreparedIvRichBlock(
	std::vector<PreparedBlock> *result,
	PreparedBlockKind kind,
	int headingLevel,
	PreparedIvRichText prepared,
	QString anchorId = QString(),
	bool allowEmpty = false,
	bool supplementary = false);

} // namespace Iv::Markdown
