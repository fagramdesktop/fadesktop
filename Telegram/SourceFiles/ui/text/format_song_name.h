/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Ui::Text {

class FormatSongName final {
public:
	struct ComposedName {
		QString title;
		QString performer;
	};

	FormatSongName(
		const QString &filename,
		const QString &songTitle,
		const QString &songPerformer);

	[[nodiscard]] ComposedName composedName() const;
	[[nodiscard]] QString string() const;
	[[nodiscard]] TextWithEntities textWithEntities(
		bool boldOnlyPerformer = false) const;

	void setNoDash(bool noDash);

private:
	const ComposedName _composedName;
	bool _noDash = false;

};

} // namespace Ui::Text
