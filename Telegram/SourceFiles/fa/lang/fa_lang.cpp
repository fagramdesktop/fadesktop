/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL
*/

#include "fa/lang/fa_lang.h"

#include "base/parse_helper.h"
#include "lang/lang_tag.h"

#include <QHash>
#include <QMutex>

namespace FAlang {
namespace {

const auto kDefaultLanguage = qsl("en");

QString langCode = kDefaultLanguage;

QHash<QString, QString> translationsCache;
QMutex cacheMutex;
bool cacheLoaded = false;

void LoadTranslationsCache() {
    if (cacheLoaded) {
        return;
    }

    auto loadFromFile = [](const QString &path) -> QJsonObject {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QJsonObject();
        }
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
        if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
            return QJsonObject();
        }
        return jsonDoc.object();
    };

    QJsonObject jsonObj = loadFromFile(qsl(":/fa_lang/%1.json").arg(langCode));

    if (jsonObj.isEmpty() && langCode != kDefaultLanguage) {
        jsonObj = loadFromFile(qsl(":/fa_lang/en.json"));
    }

    for (auto it = jsonObj.constBegin(); it != jsonObj.constEnd(); ++it) {
        if (it.value().isString()) {
            translationsCache.insert(it.key(), it.value().toString());
        }
    }

    cacheLoaded = true;
}

void InvalidateCache() {
    QMutexLocker locker(&cacheMutex);
    translationsCache.clear();
    cacheLoaded = false;
}

} // namespace

rpl::producer<QString> RplTranslate(const QString &key) {
    return rpl::single(Translate(key));
}

QString Translate(const QString &key) {
    QMutexLocker locker(&cacheMutex);

    if (!cacheLoaded) {
        LoadTranslationsCache();
    }

    auto it = translationsCache.constFind(key);
    if (it != translationsCache.constEnd() && !it.value().isEmpty()) {
        return it.value();
    }

    return key;
}

void Load(const QString &baseLangCode, const QString &lang_code) {
    QString mutableBaseLangCode = baseLangCode;
    QString mutableLangCode = lang_code;

    mutableBaseLangCode = mutableBaseLangCode.replace("-raw", "");
    mutableLangCode = mutableLangCode.replace("-raw", "");

    QString newLangCode = mutableLangCode.isEmpty()
        ? mutableBaseLangCode
        : mutableLangCode;

    if (newLangCode != langCode) {
        langCode = newLangCode;
        InvalidateCache();
    }
}

}