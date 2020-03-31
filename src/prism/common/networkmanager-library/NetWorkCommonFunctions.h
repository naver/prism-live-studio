#ifndef NETWORKCOMMONFUNCTIONS_H
#define NETWORKCOMMONFUNCTIONS_H
#include <QString>
#include <QNetworkCookie>
#include <QVariantMap>
#include <QMap>
#include "NetWorkAccessManager_global.h"

NETWORKACCESSMANAGER_EXPORT void setCookieToHttpHeader(const QString &key, const QString &value, QVariantMap &headerMap);

NETWORKACCESSMANAGER_EXPORT void setCookieToList(const QString &key, const QString &value, QList<QNetworkCookie> &cookieList);

NETWORKACCESSMANAGER_EXPORT void setCookiesToHttpHeader(const QMap<QString, QString> cookiesMap, QVariantMap &headerMap);

#endif // !NETWORKCOMMONFUNCTIONS_H
