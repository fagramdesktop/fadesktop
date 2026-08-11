/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/utils/fa_profile_values.h"

#include "lang/lang_keys.h"
#include <QtCore/QLocale>
#include <QtCore/QDateTime>
#include <algorithm>

namespace {

constexpr auto kMaxChannelId = -1000000000000LL;

struct RegDataPoint {
	int64 id;
	int64 timestamp;
};

// Sorted calibration dataset mapping Telegram User ID -> Approximate Registration Unix Timestamp
constexpr RegDataPoint kRegistrationData[] = {
	{ 1000000LL,    1376438400LL }, // 2013-08
	{ 2768409LL,    1383264000LL }, // 2013-11
	{ 7679610LL,    1388448000LL }, // 2013-12
	{ 11538514LL,   1391212000LL }, // 2014-02
	{ 15835244LL,   1392940000LL }, // 2014-02
	{ 23646077LL,   1393459000LL }, // 2014-02
	{ 38015510LL,   1393632000LL }, // 2014-03
	{ 44634663LL,   1399334000LL }, // 2014-05
	{ 46145305LL,   1400198000LL }, // 2014-05
	{ 54845238LL,   1411257000LL }, // 2014-09
	{ 63263518LL,   1414454000LL }, // 2014-10
	{ 101260938LL,  1425600000LL }, // 2015-03
	{ 101323197LL,  1426204000LL }, // 2015-03
	{ 103151531LL,  1433376000LL }, // 2015-06
	{ 109393468LL,  1439078000LL }, // 2015-08
	{ 125828524LL,  1444003000LL }, // 2015-10
	{ 143445125LL,  1448928000LL }, // 2015-12
	{ 148670295LL,  1452211000LL }, // 2016-01
	{ 152079341LL,  1453420000LL }, // 2016-01
	{ 171295414LL,  1457481000LL }, // 2016-03
	{ 181783990LL,  1460246000LL }, // 2016-04
	{ 222021233LL,  1465344000LL }, // 2016-06
	{ 278941742LL,  1473465000LL }, // 2016-09
	{ 285253072LL,  1476835000LL }, // 2016-10
	{ 294851037LL,  1479600000LL }, // 2016-11
	{ 328594461LL,  1482969000LL }, // 2016-12
	{ 337808429LL,  1487707000LL }, // 2017-02
	{ 369669043LL,  1490918000LL }, // 2017-03
	{ 400169472LL,  1501459000LL }, // 2017-07
	{ 616816630LL,  1529625600LL }, // 2018-06
	{ 681896077LL,  1532821500LL }, // 2018-07
	{ 727572658LL,  1543708800LL }, // 2018-12
	{ 925078064LL,  1563290000LL }, // 2019-07
	{ 1054883348LL, 1585674420LL }, // 2020-03
	{ 1145856008LL, 1586342040LL }, // 2020-04
	{ 1227964864LL, 1596127860LL }, // 2020-07
	{ 1382531194LL, 1600188120LL }, // 2020-09
	{ 1658586909LL, 1613148540LL }, // 2021-02
	{ 1719536397LL, 1619293500LL }, // 2021-04
	{ 1807942741LL, 1625520300LL }, // 2021-07
	{ 1972424006LL, 1631669400LL }, // 2021-09
	{ 2104178931LL, 1638353220LL }, // 2021-12
	{ 5162494923LL, 1652449800LL }, // 2022-05
	{ 5304951856LL, 1656718440LL }, // 2022-07
	{ 5387234031LL, 1662137700LL }, // 2022-09
	{ 5802242180LL, 1671821040LL }, // 2022-12
	{ 5853442730LL, 1674866100LL }, // 2023-01
	{ 6020888206LL, 1675534800LL }, // 2023-02
	{ 6132325730LL, 1692033840LL }, // 2023-08
	{ 6338817029LL, 1705536000LL }, // 2024-01
	{ 6739267230LL, 1704067200LL }, // 2024-01
	{ 6957108444LL, 1713312000LL }, // 2024-04
	{ 7100000000LL, 1720224000LL }, // 2024-07
	{ 7229898489LL, 1723075200LL }, // 2024-08
	{ 7600158321LL, 1733356800LL }, // 2024-12
	{ 7851389063LL, 1733097600LL }, // 2024-12
	{ 7857659678LL, 1727222400LL }, // 2024-09
	{ 7884373548LL, 1732233600LL }, // 2024-11
	{ 8060910775LL, 1736294400LL }, // 2025-01
	{ 8089817806LL, 1736899200LL }, // 2025-01
	{ 8454563873LL, 1764979200LL }, // 2025-12
	{ 8461412540LL, 1755628800LL }, // 2025-08
};

} // namespace

QString IDString(not_null<PeerData*> peer) {
	auto resultId = QString::number(peerIsUser(peer->id)
		? peerToUser(peer->id).bare
		: peerIsChat(peer->id)
		? peerToChat(peer->id).bare
		: peerIsChannel(peer->id)
		? peerToChannel(peer->id).bare
		: peer->id.value);
	const bool showBotApi = FASettings::FASettings::getInstance().showIdBotapi();
	if (showBotApi) {
		if (peer->isChannel()) {
			resultId = QString::number(peerToChannel(peer->id).bare - kMaxChannelId).prepend(u"-"_q);
		} else if (peer->isChat()) {
			resultId = resultId.prepend(u"-"_q);
		}
	}
	return resultId;
}

rpl::producer<TextWithEntities> IDValue(not_null<PeerData*> peer) {
	return rpl::single(IDString(peer)) | rpl::map(tr::marked);
}

QString parseRegistrationTime(const QString &prefix, int64 regTime) {
	const auto date = QDateTime::fromSecsSinceEpoch(regTime).date();
	const auto monthYear = QLocale::system().toString(date, u"MMMM yyyy"_q);
	return prefix + monthYear;
}

QString findRegistrationTime(int64 userId) {
	if (userId <= 0) {
		return QString();
	}

	constexpr auto count = std::size(kRegistrationData);
	if (userId < kRegistrationData[0].id) {
		return parseRegistrationTime(u"< "_q, kRegistrationData[0].timestamp);
	}
	if (userId >= kRegistrationData[count - 1].id) {
		return parseRegistrationTime(u"> "_q, kRegistrationData[count - 1].timestamp);
	}

	const auto it = std::lower_bound(
		std::begin(kRegistrationData),
		std::end(kRegistrationData),
		userId,
		[](const RegDataPoint &point, int64 value) {
			return point.id < value;
		});

	if (it->id == userId) {
		return parseRegistrationTime(QString(), it->timestamp);
	}

	const auto &prev = *(it - 1);
	const auto &next = *it;

	const double ratio = static_cast<double>(userId - prev.id)
		/ static_cast<double>(next.id - prev.id);
	const int64 estimatedTimestamp = prev.timestamp
		+ static_cast<int64>(ratio * static_cast<double>(next.timestamp - prev.timestamp));

	return parseRegistrationTime(u"~ "_q, estimatedTimestamp);
}

rpl::producer<TextWithEntities> RegistrationValue(not_null<PeerData*> peer) {
	if (!peer->isUser()) {
		return rpl::single(TextWithEntities());
	}
	const auto userId = peerToUser(peer->id).bare;
	return rpl::single(findRegistrationTime(userId)) | rpl::map(tr::marked);
}
