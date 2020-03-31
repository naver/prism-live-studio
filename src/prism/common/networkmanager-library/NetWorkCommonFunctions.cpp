#include "NetWorkCommonFunctions.h"
#include "CommonDefine.h"
#include <QNetworkCookie>

void setCookieToHttpHeader(const QString &key, const QString &value, QVariantMap &headerMap)
{
	QList<QNetworkCookie> cookieList;
	setCookieToList(key, value, cookieList);
	headerMap[HTTP_COOKIE] = QVariant::fromValue(cookieList);
}
void setCookieToList(const QString &key, const QString &value, QList<QNetworkCookie> &cookieList)
{
	QNetworkCookie cookie;
	cookie.setName(QByteArray::fromStdString(key.toStdString()));
	cookie.setValue(QByteArray::fromStdString(value.toStdString()));
	cookieList.push_back(cookie);
}

void setCookiesToHttpHeader(const QMap<QString, QString> cookiesMap, QVariantMap &headerMap)
{
	QList<QNetworkCookie> cookieList;
	for (QMap<QString, QString>::const_iterator iter = cookiesMap.begin(); iter != cookiesMap.end(); ++iter) {
		setCookieToList(iter.key(), iter.value(), cookieList);
	}
	headerMap[HTTP_COOKIE] = QVariant::fromValue(cookieList);
}
