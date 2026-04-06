/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/translate_mtproto_provider.h"

#include "api/api_text_entities.h"
#include "boxes/GoogleAppTranslator.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "spellcheck/platform/platform_language.h"
#include "ui/text/text_utilities.h"

namespace Ui {
namespace {

class MTProtoTranslateProvider final : public TranslateProvider {
public:
	explicit MTProtoTranslateProvider(not_null<Main::Session*> session)
	: _session(session)
	, _api(&session->mtp()) {
	}

	[[nodiscard]] bool supportsMessageId() const override {
		return true;
	}

	void request(
			TranslateProviderRequest request,
			LanguageId to,
			Fn<void(TranslateProviderResult)> done) override {
		requestBatch(
			{ std::move(request) },
			to,
			[done = std::move(done)](
					int,
					TranslateProviderResult result) {
				done(std::move(result));
			},
			[] {});
	}

	void requestBatch(
			std::vector<TranslateProviderRequest> requests,
			const LanguageId &to,
			Fn<void(int, TranslateProviderResult)> doneOne,
			Fn<void()> doneAll) override {
		if (requests.empty()) {
			doneAll();
			return;
		}

		const auto failAll = [=] {
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(i, TranslateProviderResult{
					.error = TranslateProviderError::Unknown,
				});
			}
			doneAll();
		};
		const auto doneFromList = [=, session = _session](
				const QVector<MTPTextWithEntities> &list) {
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(
					i,
					(i < list.size())
						? TranslateProviderResult{
							.text = Api::ParseTextWithEntities(
								session,
								list[i]),
						}
						: TranslateProviderResult{
							.error = TranslateProviderError::Unknown,
						});
			}
			doneAll();
		};

		const auto firstPeer = PeerId(requests.front().peerId);
		const auto allWithIds = ranges::all_of(
			requests,
			[&](const TranslateProviderRequest &request) {
				return (PeerId(request.peerId) == firstPeer)
					&& (request.msgId != 0);
			});
		if (allWithIds) {
			try {
				auto result = GoogleAppTranslator::instance()->translate(
					requests[0].text.text,
					"auto",
					to.twoLetterCode());
				auto text = QVector<MTPTextWithEntities>();
				text.push_back(MTP_textWithEntities(
					MTP_string(result.translation),
					Api::EntitiesToMTP(
						_session,
						TextWithEntities().entities,
						Api::ConvertOption::SkipLocal)));
				doneFromList(text);
			} catch (...) {
				failAll();
			}
			return;
		}

		const auto allWithText = ranges::all_of(
			requests,
			[](const TranslateProviderRequest &request) {
				return !request.text.text.isEmpty();
			});
		if (!allWithText) {
			TranslateProvider::requestBatch(
				std::move(requests),
				to,
				std::move(doneOne),
				std::move(doneAll));
			return;
		}

		try {
			auto result = GoogleAppTranslator::instance()->translate(
				requests[0].text.text,
				"auto",
				to.twoLetterCode());
			auto text = QVector<MTPTextWithEntities>();
			text.push_back(MTP_textWithEntities(
				MTP_string(result.translation),
				Api::EntitiesToMTP(
					_session,
					TextWithEntities().entities,
					Api::ConvertOption::SkipLocal)));
			doneFromList(text);
		} catch (...) {
			failAll();
		}
	}

private:
	const not_null<Main::Session*> _session;
	MTP::Sender _api;

};

} // namespace

std::unique_ptr<TranslateProvider> CreateMTProtoTranslateProvider(
		not_null<Main::Session*> session) {
	return std::make_unique<MTProtoTranslateProvider>(session);
}

} // namespace Ui
