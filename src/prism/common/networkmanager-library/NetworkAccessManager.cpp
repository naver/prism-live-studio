#include "NetworkAccessManager.h"
#include "FreeNetworkReplyGuard.h"
#include "JsonDataHandler.h"
#include "NetBaseInfo.h"
#include "CommonDefine.h"
#include "IMACManager.h"
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QUrl>
#include <QUrlQuery>
#include <string>
#include <QUuid>
#include "NetWorkCommonDefines.h"
#include "NetWorkCommonFunctions.h"
#include <QCoreApplication>
#include <QNetworkConfigurationManager>
#include <QTimer>
#include <QPointer>
using namespace std;

NetworkAccessManager::NetworkAccessManager()
{
	QNetworkConfigurationManager manager;
	auto defaultConfig = manager.defaultConfiguration();
	defaultConfig.setConnectTimeout(PRISM_NET_REQUEST_TIMEOUT);
	this->setConfiguration(defaultConfig);
	connect(qApp, &QCoreApplication::aboutToQuit, this, &QNetworkAccessManager::deleteLater);
}

NetworkAccessManager::~NetworkAccessManager() {}

void NetworkAccessManager::abortAll()
{
	auto abort = [](QVariantMap &src) {
		auto reply = src.value(REPLY_POINTER).value<ReplyPtrs>();
		if (reply && reply->isRunning()) {
			reply->abort();
		}
	};
	std::for_each(mTasksMap.begin(), mTasksMap.end(), abort);
}

QVariantMap &NetworkAccessManager::createHttpRequest(int op, const QString &url, bool isEncode, const QVariantMap &headData, const QVariantMap &sendData, bool isSync)
{
	QNetworkReply *reply = nullptr;
	QNetworkRequest httpRequest;
	QUrl httpUrl(url);
	requestUrlHeadHandler(headData, httpRequest);

	switch (op) {
	case Operation::GetOperation:
		reply = httpGetRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::PostOperation:
		reply = httpPostRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::PutOperation:
		reply = httpPutRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::DeleteOperation:
		reply = httpDeleteRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	default:
		break;
	}

	return httpResponseHandler(reply, url, isSync);
}

void NetworkAccessManager::encryptUrl(QUrl &url, const QString &hMacKey)
{
	return ;
}

QList<QNetworkCookie> NetworkAccessManager::getCookieForUrl(const QString &url)
{
	QNetworkCookieJar *cookieJar = this->cookieJar();
	return cookieJar->cookiesForUrl(url);
}

QVariantMap NetworkAccessManager::takeTaskMap(const QString &ID)
{
	return mTasksMap.take(ID);
}

QVariantMap &NetworkAccessManager::getTaskMap(const QString &ID)
{
	return mTasksMap[ID];
}

void NetworkAccessManager::removeTaskMap(const QString &ID)
{
	mTasksMap.remove(ID);
}

void NetworkAccessManager::removeTaskMap(const QVariantMap &last)
{
	auto taskId = last.value(UUID).toString();
	removeTaskMap(taskId);
}

void NetworkAccessManager::setQueryData(QUrlQuery &query, const QVariantMap &data)
{
	QVariantMap::const_iterator it;
	for (it = data.constBegin(); it != data.constEnd(); ++it) {
		query.addQueryItem(it.key(), it.value().toString());
	}
}

void NetworkAccessManager::requestUrlHeadHandler(const QVariantMap &headData, QNetworkRequest &httpRequest)
{
	QVariantMap::const_iterator it;
	for (it = headData.constBegin(); it != headData.constEnd(); ++it) {
		if (it.key() == HTTP_COOKIE) {
			httpRequest.setHeader(QNetworkRequest::CookieHeader, it.value());
		} else {
			httpRequest.setRawHeader(it.key().toLocal8Bit(), it.value().toByteArray());
		}
	}
}

void NetworkAccessManager::setHmacUrlHandler(bool isEncode, QUrl &url)
{
	if (isEncode) {
		if (url.toString().contains(HTTP_HMAC_STR)) {
			encryptUrl(url, PLS_HMAC_KEY);
		} 
	}
}

QNetworkReply *NetworkAccessManager::httpGetRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	QUrlQuery params;
	if (!sendData.isEmpty()) {
		setQueryData(params, sendData);
		httpUrl.setQuery(params.toString());
	}
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);
	return get(httpRequest);
}

QNetworkReply *NetworkAccessManager::httpDeleteRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	QUrlQuery params;
	if (!sendData.isEmpty()) {
		setQueryData(params, sendData);
		httpUrl.setQuery(params.toString());
	}
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);
	return deleteResource(httpRequest);
}

QNetworkReply *NetworkAccessManager::httpPostRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);

	if (httpRequest.header(QNetworkRequest::ContentTypeHeader).toString() == HTTP_CONTENT_TYPE_VALUE) {
		// json
		auto doc = QJsonDocument::fromVariant(sendData);
		return post(httpRequest, doc.toJson());
	} else {
		return post(httpRequest, getBodyfromMap(sendData));
	}
}

QNetworkReply *NetworkAccessManager::httpPutRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);

	if (httpRequest.header(QNetworkRequest::ContentTypeHeader).toString() == HTTP_CONTENT_TYPE_VALUE) {
		// json
		auto doc = QJsonDocument::fromVariant(sendData);
		return put(httpRequest, doc.toJson());
	} else {
		return put(httpRequest, getBodyfromMap(sendData));
	}
}

QNetworkReply *NetworkAccessManager::httpPostRequest(const QVariantList &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);
	return post(httpRequest, JsonDataHandler::getJsonByteArrayFromList(sendData));
}

QVariantMap &NetworkAccessManager::httpResponseHandler(QNetworkReply *reply, const QString &requestHttpUrl, bool isSync)
{
	if (reply) {
		auto uuid = QUuid::createUuid().toString();
		auto finishedHandler = [=]() {
			auto data = reply->readAll();
			auto &taskMap = this->mTasksMap[uuid];
			taskMap.insert(BODY_DATA, QVariant::fromValue(data));
			int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			taskMap.insert(STATUS_CODE, statusCode);
			if (isSync) {
				emit replySyncData(uuid);
			} else {
				emit replyResultData(uuid);
			}
		};
		this->setNetworkAccessible(QNetworkAccessManager::Accessible);
		connect(reply, &QNetworkReply::finished, this, finishedHandler, Qt::QueuedConnection);
		connect(this, &QNetworkAccessManager::networkAccessibleChanged, reply, [=]() {
			if (this->networkAccessible() != QNetworkAccessManager::Accessible && reply->isRunning()) {
				reply->abort();
			}
		});
		QTimer::singleShot(PRISM_NET_REQUEST_TIMEOUT, reply, [=] {
			if (this->mTasksMap.contains(uuid) && reply->isRunning()) {
				reply->abort();
			}
		});

		QVariantMap ret;
		ret.insert(UUID, uuid);
		ret.insert(URL, requestHttpUrl);
		ReplyPtrs replyGuard(reply, deleteQtObject<QNetworkReply>);
		ret.insert(REPLY_POINTER, QVariant::fromValue(replyGuard));
		reply->setProperty(UUID_C, uuid);
		mTasksMap.insert(uuid, ret);
		return mTasksMap[uuid];
	}
	static QVariantMap tmp;
	tmp.clear();
	return tmp;
}

QByteArray NetworkAccessManager::getBodyfromMap(const QVariantMap &bodyMap)
{
	QString bodyStr;
	QVariantMap::const_iterator it;
	for (it = bodyMap.constBegin(); it != bodyMap.constEnd(); ++it) {
		bodyStr += QString("%1=%2&").arg(it.key()).arg(it.value().toString());
	}
	bodyStr.chop(1); // 1:delete last char "&"
	return bodyStr.toLocal8Bit();
}
