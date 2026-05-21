/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "iv/markdown/iv_markdown_parse.h"
#include "iv/markdown/iv_markdown_prepare.h"
#include "iv/iv_prepare.h"

#include <QtCore/QByteArray>

#include <memory>
#include <vector>

namespace Iv::Markdown {

struct PrepareContext {
	int listDepth = 0;
	int quoteDepth = 0;
};

struct FootnoteDefinitionEntry {
	const MarkdownNode *node = nullptr;
};

struct PrepareState {
	const PrepareRequest *request = nullptr;
	MarkdownArticleContent result;
	QByteArray sourceUtf8;
	std::vector<FootnoteDefinitionEntry> footnoteDefinitions;
	int nextGeneratedId = 0;

	void rememberFormula(
		int index,
		MathKind kind,
		QString formulaTex,
		int textSize,
		int renderWidthCap,
		int renderHeightCap);
	void rememberFormula(const PreparedBlock &block);
	void addPrepareWarning();
	void addFormulaWarning();
	void addPrepareWarnings(int count);
	void addFormulaWarnings(int count);
	void setTerminalFailure(
		PrepareTerminalFailure terminal,
		QString debugReason);
	[[nodiscard]] QString formulaSourceText(int index) const;
};

struct NativeIvPhotoInfo {
	uint64 id = 0;
	int width = 0;
	int height = 0;
};

struct NativeIvDocumentInfo {
	uint64 id = 0;
	int width = 0;
	int height = 0;
	QString fileName;
	QString title;
	QString performer;
	int duration = 0;
	bool isVideoFile = false;
	bool isAnimation = false;
};

struct NativeIvPrepareState {
	MarkdownArticleContent result;
	std::vector<NativeIvPhotoInfo> photos;
	std::vector<NativeIvDocumentInfo> documents;
	int nextGeneratedId = 0;

	void setFailure(
		PrepareTerminalFailure terminal,
		QString debugReason);
	[[nodiscard]] bool blocked() const;
};

[[nodiscard]] QString InvalidStyleReason(
	const MarkdownPrepareDimensions &dimensions);
void ClearPreparedOutput(MarkdownArticleContent *result);

} // namespace Iv::Markdown
