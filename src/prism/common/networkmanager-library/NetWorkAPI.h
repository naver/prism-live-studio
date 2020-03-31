#pragma once
#include <string>
#include <QVariantMap>
#include "NetWorkAccessManager_global.h"

class NetworkAccessManager;
class QHostInfo;
class QString;
class QNetworkCookie;
class QUrlQuery;
class QNetworkRequest;
class QNetworkReply;

class NETWORKACCESSMANAGER_EXPORT NetWorkAPI {
public:
	virtual ~NetWorkAPI() {}
	virtual QVariantMap &createHttpRequest(int op, const QString &url, bool isEncode = false, const QVariantMap &headData = QVariantMap(), const QVariantMap &sendData = QVariantMap(),
					       bool isSync = false) = 0;

	virtual void encryptUrl(QUrl &url, const QString &hMacKey) = 0;

	virtual QList<QNetworkCookie> getCookieForUrl(const QString &url) = 0;

	virtual QVariantMap takeTaskMap(const QString &ID) = 0;

	virtual QVariantMap &getTaskMap(const QString &ID) = 0;

	virtual void removeTaskMap(const QString &ID) = 0;

	virtual void removeTaskMap(const QVariantMap &last) = 0;

	virtual void abortAll() = 0;
};
NETWORKACCESSMANAGER_EXPORT void deleteAPI(NetWorkAPI *API);
NETWORKACCESSMANAGER_EXPORT NetWorkAPI *createNetWorkAPI();
