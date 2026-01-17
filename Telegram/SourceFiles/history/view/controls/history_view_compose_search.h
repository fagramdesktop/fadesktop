/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/
#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {
class RpWidget;
} // namespace Ui

class History;

namespace HistoryView {

class ComposeSearch final {
public:
	ComposeSearch(
		not_null<Ui::RpWidget*> parent,
		not_null<Window::SessionController*> window,
		not_null<History*> history,
		PeerData *from = nullptr,
		const QString &query = QString());
	~ComposeSearch();

	void hideAnimated();
	void setInnerFocus();
	void setQuery(const QString &query);

	void setTopMsgId(MsgId topMsgId);

	struct Activation {
		not_null<HistoryItem*> item;
		QString query;
	};
	[[nodiscard]] rpl::producer<Activation> activations() const;
	[[nodiscard]] rpl::producer<> destroyRequests() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	class Inner;
	const std::unique_ptr<Inner> _inner;

};

} // namespace HistoryView
