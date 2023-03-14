#include "PLSNetworkReplyBuilder.h"

#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QNetworkCookie>

#include "PLSHttpHelper.h"
#include "log.h"

using namespace std;

PLSNetworkReplyBuilder::PLSNetworkReplyBuilder() {}

PLSNetworkReplyBuilder::PLSNetworkReplyBuilder(const QString &value) : url(value) {}

PLSNetworkReplyBuilder::PLSNetworkReplyBuilder(const QUrl &value) : url(value) {}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setUrl(const QString &value)
{
	url = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setUrl(const QUrl &value)
{
	url = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addKnownHeader(const QNetworkRequest::KnownHeaders &key, const QVariant &value)
{
	knownheaders.insert(key, value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setKnownHeaders(const QMap<QNetworkRequest::KnownHeaders, QVariant> &headers)
{
	knownheaders = headers;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setCookie(const QVariant &value)
{
	auto cookieVariant = QVariant();

	try {
		if (QVariant::String == value.type()) {
			cookieVariant.setValue(QNetworkCookie::parseCookies(value.toString().toUtf8()));
		} else if (QVariant::ByteArray == value.type()) {
			cookieVariant.setValue(QNetworkCookie::parseCookies(value.toByteArray()));
		} else if (QVariant::List == value.type()) {
			cookieVariant.setValue(value);
		}
	} catch (...) {
		PLS_ERROR(MODULE_PLSHttpHelper, __FUNCTION__ ".error: %s", value.toString().toUtf8().constData());
	}

	if (!cookieVariant.isNull()) {
		addKnownHeader(QNetworkRequest::CookieHeader, cookieVariant);
	}

	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setCookie(const QList<QVariant> &values)
{
	auto cookieVariant = QVariant();
	QList<QNetworkCookie> cookies;
	for (auto value : values) {

		try {
			if (QVariant::String == value.type()) {
				cookies += QNetworkCookie::parseCookies(value.toString().toUtf8());
			} else if (QVariant::ByteArray == value.type()) {
				cookies += QNetworkCookie::parseCookies(value.toByteArray());
			}
		} catch (...) {
			PLS_ERROR(MODULE_PLSHttpHelper, __FUNCTION__ ".error: %s", value.toString().toUtf8().constData());
		}
	}
	QNetworkCookie uploadCookie;
	if (cookies.size() > 0) {
		cookieVariant = QVariant::fromValue(cookies);
	}
	addKnownHeader(QNetworkRequest::CookieHeader, cookieVariant);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setContentType(const QString &value)
{
	addKnownHeader(QNetworkRequest::ContentTypeHeader, value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setDefaultHeaders(const QVariantMap &headers)
{
	defaultHeaders = headers;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addRawHeader(const QString &key, const QVariant &value)
{
	rawHeaders.insert(key, value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setRawHeaders(const QVariantMap &headers)
{
	rawHeaders = headers;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addQuery(const QString &key, const QVariant &value)
{
	querys.insert(key, value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setQuerys(const QVariantMap &value)
{
	querys = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setBody(const QByteArray &value)
{
	body = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addField(const QString &key, const QVariant &value)
{
	fields.insert(key, value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setFields(const QVariantMap &value)
{
	fields = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addJsonObject(const QString &key, const QVariant &value)
{
	jsonObject.insert(key, QJsonValue::fromVariant(value));
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setJsonObject(const QVariantMap &value)
{
	jsonObject = QJsonObject::fromVariantMap(value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setJsonObject(const QJsonObject &value)
{
	jsonObject = value;
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::addJsonArray(const QVariant &value)
{
	jsonArray.append(QJsonValue::fromVariant(value));
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setJsonArray(const QVariantList &value)
{
	jsonArray = QJsonArray::fromVariantList(value);
	return *this;
}

PLSNetworkReplyBuilder &PLSNetworkReplyBuilder::setJsonArray(const QJsonArray &value)
{
	jsonArray = value;
	return *this;
}

QNetworkReply *PLSNetworkReplyBuilder::get(QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}

	return networkAccessManager->get(buildRequest());
}

QNetworkReply *PLSNetworkReplyBuilder::post(QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}
	return networkAccessManager->post(buildRequest(), buildBody());
}

QNetworkReply *PLSNetworkReplyBuilder::post(QHttpMultiPart *multiPart, QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}
	return networkAccessManager->post(buildRequest(), multiPart);
}

QNetworkReply *PLSNetworkReplyBuilder::put(QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}

	return networkAccessManager->put(buildRequest(), buildBody());
}

QNetworkReply *PLSNetworkReplyBuilder::del(QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}

	return networkAccessManager->deleteResource(buildRequest());
}

QNetworkReply *PLSNetworkReplyBuilder::patch(QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}

	return networkAccessManager->sendCustomRequest(buildRequest(), "PATCH", buildBody());
}

QNetworkReply *PLSNetworkReplyBuilder::custom(const char *method, const QByteArray &data, QNetworkAccessManager *networkAccessManager)
{
	if (nullptr == networkAccessManager) {
		networkAccessManager = PLS_HTTP_HELPER->getNetworkAccessManager();
	}

	return networkAccessManager->sendCustomRequest(buildRequest(), method, data);
}

QUrl PLSNetworkReplyBuilder::buildUrl(const QUrl &url)
{
	return url;
}

QNetworkRequest *PLSNetworkReplyBuilder::buildHeader(QNetworkRequest *request)
{
	if (!defaultHeaders.isEmpty()) {
		for (auto iter = defaultHeaders.begin(); iter != defaultHeaders.end(); ++iter) {
			request->setRawHeader(iter.key().toUtf8(), iter.value().toString().toUtf8());
		}
	}

	if (!rawHeaders.isEmpty()) {
		for (auto iter = rawHeaders.begin(); iter != rawHeaders.end(); ++iter) {
			request->setRawHeader(iter.key().toUtf8(), iter.value().toString().toUtf8());
		}
	}

	if (!knownheaders.isEmpty()) {
		for (auto iter = knownheaders.begin(); iter != knownheaders.end(); ++iter) {
			request->setHeader(iter.key(), iter.value());
		}
	}

	return request;
}

QUrlQuery PLSNetworkReplyBuilder::buildQuery(const QVariantMap &value)
{
	QUrlQuery urlQuery;

	for (auto iter = value.cbegin(); iter != value.cend(); ++iter) {
		urlQuery.addQueryItem(iter.key(), QUrl::toPercentEncoding(iter.value().toString()));
	}

	return urlQuery;
}

QByteArray PLSNetworkReplyBuilder::buildBody()
{
	QByteArray buffer;

	if (!body.isEmpty()) {
		buffer = body;
	} else if (!fields.isEmpty()) {
		buffer = buildQuery(fields).toString(QUrl::FullyEncoded).toUtf8();
	} else if (!jsonObject.isEmpty()) {
		buffer = QJsonDocument(jsonObject).toJson();
	} else if (!jsonArray.isEmpty()) {
		buffer = QJsonDocument(jsonArray).toJson();
	}

	return buffer;
}

QNetworkRequest PLSNetworkReplyBuilder::buildRequest()
{
	if (!querys.isEmpty()) {
		url.setQuery(buildQuery(querys));
	}

	QNetworkRequest request(buildUrl(url));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
	request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
	buildHeader(&request);

	return request;
}
