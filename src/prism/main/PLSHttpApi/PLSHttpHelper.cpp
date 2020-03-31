#include <Windows.h>
#include "PLSHttpHelper.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkConfiguration>
#include <QNetworkInterface>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

#include <util/windows/win-version.h>

#include "log.h"

#include "ui-config.h"
#include"pls-net-url.hpp"
using namespace std;

PLSHttpHelper *PLSHttpHelper::instance()
{
	static PLSHttpHelper *_instance = nullptr;

	if (_instance == nullptr) {
		_instance = new PLSHttpHelper();

		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { delete _instance; });
	}

	return _instance;
}

void PLSHttpHelper::connectFinished(QNetworkReply *networkReplay, const QObject *receiver, function<void(QNetworkReply *networkReplay, int code, QByteArray data, void *const context)> onSucceed,
				    function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *const context)> onFailed,
				    function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *const context)> onFinished, void *const context)
{
	auto _onFinished = [=] {
		auto statusCode = networkReplay->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		auto data = networkReplay->readAll();
		auto error = networkReplay->error();
		int code = 0;

		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			const char *KEY_CODE = "code";
			auto root = doc.object();
			if (root.contains(KEY_CODE)) {
				code = root[KEY_CODE].toInt();
			}
		}

		if (QNetworkReply::NoError == networkReplay->error()) {
			PLS_INFO(MODULE_PLSHttpHelper, "http response success! url = %s statusCode = %d code = %d.", networkReplay->url().toString().toStdString().c_str(), statusCode, code);
			if (nullptr != onSucceed) {
				onSucceed(networkReplay, statusCode, data, context);
			}
		} else {
			PLS_INFO(MODULE_PLSHttpHelper, "http response error! url = %s statusCode = %d code = %d.", networkReplay->url().toString().toStdString().c_str(), statusCode, code);
			if (onFailed != nullptr) {
				onFailed(networkReplay, statusCode, data, error, context);
			}
		}

		if (onFinished != nullptr) {
			onFinished(networkReplay, statusCode, data, error, context);
		}
	};

	assert(nullptr != receiver || (nullptr == onSucceed && nullptr == onFailed && nullptr == onFinished));
	if (nullptr != onSucceed || nullptr == onFailed || nullptr == onFinished) {
		QObject::connect(networkReplay, &QNetworkReply::finished, receiver, _onFinished);
	}

	QTimer::singleShot(PRISM_NET_REQUEST_TIMEOUT, networkReplay, &QNetworkReply::abort);

	deleteNetworkReplyLater(networkReplay);
}

void PLSHttpHelper::deleteNetworkReplyLater(QNetworkReply *reply)
{
	QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

QString PLSHttpHelper::getLocalIPAddr()
{
	for (auto &addr : QNetworkInterface::allAddresses()) {
		if (!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol)) {
			return addr.toString();
		}
	}

	return QString();
}

QString PLSHttpHelper::getUserAgent()
{
	win_version_info wvi = {0};
	get_win_ver(&wvi);

	LANGID langId = GetUserDefaultUILanguage();
#ifdef Q_OS_WIN64
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x64 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#else
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x86 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#endif
}
