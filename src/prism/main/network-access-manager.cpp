#include "network-access-manager.hpp"
#include "free-network-reply-guard.hpp"
#include "json-data-handler.hpp"
#include "pls-net-url.hpp"
#include "frontend-api.h"
#include "log/log.h"
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QUrl>
#include <QUrlQuery>
#include "pls-common-define.hpp"
#include <QTimer>

Q_GLOBAL_STATIC(PLSNetworkAccessManager, netWorkAccessManger);

PLSNetworkAccessManager::PLSNetworkAccessManager(QObject *parent) : QNetworkAccessManager(parent), m_multiPart(nullptr) {}

PLSNetworkAccessManager::~PLSNetworkAccessManager() {}

PLSNetworkAccessManager *PLSNetworkAccessManager::getInstance()
{
	auto instance = netWorkAccessManger();
	return instance;
}

QNetworkReply *PLSNetworkAccessManager::createHttpRequest(QNetworkAccessManager::Operation op, const QString &url, bool isEncode, const QVariantMap &headData, const QVariantMap &sendData, bool isGcc,
							  bool isPrintLog, const QString &maskUrl)
{
	QString operation;
	QNetworkReply *reply = nullptr;
	QNetworkRequest httpRequest;
	httpRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	QUrl httpUrl(url);
	requestUrlHeadHandler(headData, httpRequest, isGcc);

	if (networkAccessible() == QNetworkAccessManager::NotAccessible) {
		setNetworkAccessible(QNetworkAccessManager::Accessible);
	}

	switch (op) {
	case Operation::GetOperation:
		operation = "Get";
		reply = httpGetRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::PostOperation:
		operation = "Post";
		reply = httpPostRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::PutOperation:
		operation = "Put";
		reply = httpPutRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	case Operation::DeleteOperation:
		operation = "Delete";
		reply = httpDeleteRequest(sendData, isEncode, httpRequest, httpUrl);
		break;
	default:
		break;
	}
	auto httpRequestUrlMask = maskUrl.isEmpty() ? url : maskUrl;

	PLS_INFO(HTTP_REQUEST, "http request start:%s, url = %s.", operation.toUtf8().data(), httpRequestUrlMask.toUtf8().constData());
	if (!maskUrl.isEmpty()) {
		PLS_INFO_KR(HTTP_REQUEST, "http request start:%s, url = %s.", operation.toUtf8().data(), url.toUtf8().constData());
	}

	if (url.contains("/auth/session")) {
		QVariantMap headMap;
		pls_http_request_head(headMap);
		if (pls_is_dev_server()) {
			headMap.insert("X-prism-usercode", QVariant());
		}
		QString headStr;
		for (auto firstHead = headMap.begin(); firstHead != headMap.end(); ++firstHead) {
			if (firstHead.key().contains("X-prism-usercode") || firstHead.key().contains("X-prism-ip") || firstHead.key().contains("X-prism-cc")) {
				continue;
			}
			headStr += QString("\n         %1 = %2").arg(firstHead.key()).arg(firstHead.value().toString());
		}
		PLS_INFO(HTTP_REQUEST, QString("http requeset head:%1").arg(headStr).toUtf8().data());
	}
	httpResponseHandler(reply, url, isPrintLog, !maskUrl.isEmpty(), httpRequestUrlMask);
	return reply;
}

QList<QNetworkCookie> PLSNetworkAccessManager::getCookieForUrl(const QString &url)
{
	return m_cookies.value(url);
}

void PLSNetworkAccessManager::setCookieToHttpHeader(const QString &key, const QString &value, QVariantMap &headerMap)
{
	QList<QNetworkCookie> cookieList;
	setCookieToList(key, value, cookieList);
	headerMap[HTTP_COOKIE] = QVariant::fromValue(cookieList);
}

void PLSNetworkAccessManager::setCookiesToHttpHeader(const QMap<QString, QString> cookiesMap, QVariantMap &headerMap)
{
	QList<QNetworkCookie> cookieList;
	for (QMap<QString, QString>::const_iterator iter = cookiesMap.begin(); iter != cookiesMap.end(); ++iter) {
		setCookieToList(iter.key(), iter.value(), cookieList);
	}
	headerMap[HTTP_COOKIE] = QVariant::fromValue(cookieList);
}

void PLSNetworkAccessManager::setPostMultiPart(QHttpMultiPart *multi_part_)
{
	m_multiPart = multi_part_;
}

void PLSNetworkAccessManager::setQueryData(QUrlQuery &query, const QVariantMap &data)
{
	QVariantMap::const_iterator it;
	for (it = data.constBegin(); it != data.constEnd(); ++it) {
		query.addQueryItem(it.key(), it.value().toString());
	}
}

void PLSNetworkAccessManager::requestUrlHeadHandler(const QVariantMap &headData, QNetworkRequest &httpRequest, bool isGcc)
{
	//TODO:
	QVariantMap headMaps(headData);
	pls_http_request_head(headMaps, isGcc);
	QVariantMap::const_iterator it;
	for (it = headMaps.constBegin(); it != headMaps.constEnd(); ++it) {
		if (it.key() == HTTP_COOKIE) {
			httpRequest.setHeader(QNetworkRequest::CookieHeader, it.value());
		} else {
			httpRequest.setRawHeader(it.key().toUtf8(), it.value().toByteArray());
		}
	}
}

void PLSNetworkAccessManager::setHmacUrlHandler(bool isEncode, QUrl &url)
{
	if (isEncode) {
		if (url.toString().contains(HTTP_HMAC_STR)) {
			pls_get_encrypt_url(url, QString::fromUtf8(url.toEncoded(QUrl::FullyDecoded)), PLS_PC_HMAC_KEY);
		}
	}
}

QNetworkReply *PLSNetworkAccessManager::httpGetRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
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

QNetworkReply *PLSNetworkAccessManager::httpDeleteRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
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

QNetworkReply *PLSNetworkAccessManager::httpPostRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	setHmacUrlHandler(isEncode, httpUrl);
	httpRequest.setUrl(httpUrl);
	if (m_multiPart) {
		QNetworkReply *replay = post(httpRequest, m_multiPart);
		m_multiPart->setParent(replay);
		return replay;
	} else if (httpRequest.header(QNetworkRequest::ContentTypeHeader).toString() == HTTP_CONTENT_TYPE_VALUE) {
		// json
		return post(httpRequest, PLSJsonDataHandler::getJsonByteArrayFromMap(sendData));
	} else {
		return post(httpRequest, getBodyfromMap(sendData));
	}
}

QNetworkReply *PLSNetworkAccessManager::httpPutRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl)
{
	setHmacUrlHandler(isEncode, httpUrl);

	httpRequest.setUrl(httpUrl);

	if (httpRequest.header(QNetworkRequest::ContentTypeHeader).toString() == HTTP_CONTENT_TYPE_VALUE) {
		// json
		return put(httpRequest, PLSJsonDataHandler::getJsonByteArrayFromMap(sendData));
	} else {
		return put(httpRequest, getBodyfromMap(sendData));
	}
}

void PLSNetworkAccessManager::httpResponseHandler(QNetworkReply *reply, const QString &requestHttpUrl, bool isPrintLog, bool isMasking, const QString &maskUrl)
{
	QTimer *t = new QTimer();
	connect(t, &QTimer::timeout, this, [=]() { reply->abort(); });
	t->start(PRISM_NET_REQUEST_TIMEOUT);
	if (reply) {
		connect(reply, &QNetworkReply::finished, this, [=]() {
			t->stop();
			FreeNetworkReplyGuard guard(reply, t, m_multiPart);
			int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			QVariant code;
			QByteArray body = reply->readAll();
			PLSJsonDataHandler::getValueFromByteArray(body, "code", code);
			if (reply->error() != QNetworkReply::NoError) {
				if (isFilterResponseErrorUrls(requestHttpUrl)) {
					//notice,token response is error but the response is not error,it is another response.eg:no notice.
					PLS_INFO(HTTP_REQUEST, "http response success, url = %s, networkReplyError = %d statusCode = %d.", maskUrl.toUtf8().data(), reply->error(), statusCode);
					PLS_INFO_KR(HTTP_REQUEST, "http response success, url = %s networkReplyError = %d statusCode = %d errorMessage = %s.", requestHttpUrl.toUtf8().data(),
						    reply->error(), statusCode, body.constData());
				} else {
					PLS_ERROR(HTTP_REQUEST, "http response error! url = %s networkReplyError = %d statusCode = %d.", maskUrl.toUtf8().data(), reply->error(), statusCode);

					PLS_ERROR_KR(HTTP_REQUEST, "http response error! url = %s networkReplyError = %d statusCode = %d errorMessage = %s.", requestHttpUrl.toUtf8().data(),
						     reply->error(), statusCode, body.constData());
				}
				if (HTTP_STATUS_CODE_401 == statusCode && HTTP_TOKEN_INVAILD_CODE_3000 == code.toInt() && (!isFilterUrls(requestHttpUrl))) {
					emit sessionExpired();
				} else {
					emit replyErrorDataWithSatusCode(statusCode, requestHttpUrl, body, reply->errorString());
					emit replyErrorData(requestHttpUrl, reply->errorString());
				}

			} else {
				if (isPrintLog) {
					auto contentTypeStr = reply->header(QNetworkRequest::ContentTypeHeader).toString();

					PLS_INFO(HTTP_REQUEST, "http response success, url = %s, ", maskUrl.toUtf8().constData());

					PLS_INFO_KR(HTTP_REQUEST, "http response success, url = %s, response data = %s", requestHttpUrl.toUtf8().constData(),
						    contentTypeStr.contains("image") ? "is image res." : body.constData());
				}
				m_cookies.insert(requestHttpUrl, reply->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie>>());
				emit replyResultData(statusCode, requestHttpUrl, body);
			}
		});
	}
}

QByteArray PLSNetworkAccessManager::getBodyfromMap(const QVariantMap &bodyMap)
{
	QString bodyStr;
	QVariantMap::const_iterator it;
	for (it = bodyMap.constBegin(); it != bodyMap.constEnd(); ++it) {
		bodyStr += QString("%1=%2&").arg(it.key()).arg(it.value().toString());
	}
	bodyStr.chop(1); // 1:delete last char "&"
	return bodyStr.toUtf8();
}

void PLSNetworkAccessManager::setCookieToList(const QString &key, const QString &value, QList<QNetworkCookie> &cookieList)
{
	QNetworkCookie cookie;
	cookie.setName(QByteArray::fromStdString(key.toStdString()));
	cookie.setValue(QByteArray::fromStdString(value.toStdString()));
	cookieList.push_back(cookie);
}

bool PLSNetworkAccessManager::isFilterUrls(const QString &url)
{
	QStringList filterUrls = {PLS_TOKEN_SESSION_URL.arg(PRISM_SSL), PLS_LOGOUT_URL.arg(PRISM_SSL), PLS_SIGNOUT_URL.arg(PRISM_SSL)};
	for (auto filterUrl : filterUrls) {
		if (0 == url.compare(filterUrl)) {
			return true;
		}
	}
	return false;
}

bool PLSNetworkAccessManager::isFilterResponseErrorUrls(const QString &url)
{
	QStringList filterUrls = {PLS_TOKEN_SESSION_URL.arg(PRISM_SSL), PLS_NOTICE_URL.arg(PRISM_SSL)};
	for (auto filterUrl : filterUrls) {
		if (0 == url.compare(filterUrl)) {
			return true;
		}
	}
	return false;
}
