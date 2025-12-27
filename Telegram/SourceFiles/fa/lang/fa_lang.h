/*
This file is part of FAgram Desktop,
the unofficial desktop client based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/fajox1/fagramdesktop/blob/master/LEGAL
*/

#pragma once

#include <rpl/producer.h>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonParseError>
#include <QtCore/QDir>
#include <QtCore/QFile>

namespace FAlang {
    rpl::producer<QString> RplTranslate(const QString &key);
    QString Translate(const QString &key);

    void Load(const QString &baseLangCode, const QString &lang_code);
}