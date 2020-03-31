/*
* @file		PLSHttpHelper.h
* @brief	A helper class for http request
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "PLSNetworkReplyBuilder.h"
#include "PLSHmacNetworkReplyBuilder.h"

#define MODULE_PLSHttpHelper "PLSHttpHelper"

using namespace std;

class PLSHttpHelper {
public:
	static PLSHttpHelper *instance();

public:
	QNetworkAccessManager *getNetworkAccessManager() { return &networkAccessManager; }

	static void connectFinished(QNetworkReply *networkReplay, const QObject *receiver = nullptr,
				    function<void(QNetworkReply *networkReplay, int code, QByteArray data, void *context)> onSucceed = nullptr,
				    function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context)> onFailed = nullptr,
				    function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context)> onFinished = nullptr,
				    void *const context = nullptr);

	static void deleteNetworkReplyLater(QNetworkReply *networkReply);

	static QString getLocalIPAddr();
	static QString getUserAgent();

private:
	QNetworkAccessManager networkAccessManager;
};

#define PLS_HTTP_HELPER PLSHttpHelper::instance()
