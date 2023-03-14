#include "login-web-handler.hpp"

#include "json-data-handler.hpp"
#include "network-access-manager.hpp"
#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "log/log.h"
#include "frontend-api.h"
#include "pls-notice-handler.hpp"
#include "ui-config.h"
#include "window-basic-main.hpp"
#include "alert-view.hpp"

static QJsonObject _result;

LoginWebHandler::LoginWebHandler(QObject *parent) : QObject(parent), m_networkAccessManager(PLSNetworkAccessManager::getInstance()), m_isSuccess(false), m_result(_result)
{
	setConnect();
}
LoginWebHandler::LoginWebHandler(QJsonObject &result, QObject *parent) : QObject(parent), m_networkAccessManager(PLSNetworkAccessManager::getInstance()), m_isSuccess(false), m_result(result)
{
	setConnect();
}

LoginWebHandler::~LoginWebHandler() {}

void LoginWebHandler::initialize(const QList<QNetworkCookie> &cookies, const QString &url, LoginInfoType infoType)
{
	m_cookies = cookies;
	initialize(url, infoType);
}

void LoginWebHandler::initialize(const QString &url, LoginInfoType infoType)
{
	m_url = url;
	m_infoType = infoType;
}

void LoginWebHandler::requesetHttp(const QNetworkAccessManager::Operation op)
{
	return;
}
void LoginWebHandler::requesetHttpForNotice(const QNetworkAccessManager::Operation operation)
{
	return;
}

void LoginWebHandler::replyResultDataHandler(int statusCode, const QString &url, const QByteArray array)
{
	return;
}

void LoginWebHandler::replyErrorHandler(int statusCode, const QString &url, const QString &body, const QString &errorStr)
{
	emit finished(m_errorInfo);

}

void LoginWebHandler::unInitialize()
{
	m_url.clear();
	m_errorInfo.clear();
}

QString LoginWebHandler::getErrorInfo()
{
	return m_errorInfo;
}

bool LoginWebHandler::isSuccessLogin()
{
	return m_isSuccess;
}

void LoginWebHandler::setConnect()
{
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyResultData, this, &LoginWebHandler::replyResultDataHandler);
	connect(m_networkAccessManager, &PLSNetworkAccessManager::replyErrorDataWithSatusCode, this, &LoginWebHandler::replyErrorHandler);
}

QString LoginWebHandler::getCookieStr()
{
	for (auto cookie : m_cookies) {
		if (cookie.name() == NEO_SESKEY) {
			return QString("%1=%2;").arg(cookie.name().data()).arg(cookie.value().data());
		}
	}
	return QString();
}

void LoginWebHandler::getTokenInfoHandler(int statusCode, const QByteArray &array)
{
	return;
}

void LoginWebHandler::getPrismLogoutInfoHandler(int statusCode, const QByteArray &array)
{
	return;
}

void LoginWebHandler::getPrismSignoutInfoHandler(int statusCode, const QByteArray &array)
{
	return;
}
bool LoginWebHandler::saveThumbnail(const QPixmap &pixmap)
{
	return true;
}

bool getPrismUserThumbnail(QPixmap &pixmap)
{
	return false;
}

void LoginWebHandler::getPrismUserThubnail(int statusCode, const QByteArray &array)
{
	return;
}

void LoginWebHandler::getPrismNoitceInfoHandler(int statusCode, const QByteArray &array)
{
	return;
}

void LoginWebHandler::requesetHttpPrivacy(const QString &, const QByteArray &body)
{
	return;
}

void LoginWebHandler::requestUserThubanail(LoginInfoType type)
{
	return;
}

void LoginWebHandler::getPrismLoginInfoHandler(int statusCode, const QByteArray &array)
{
	return;
}
