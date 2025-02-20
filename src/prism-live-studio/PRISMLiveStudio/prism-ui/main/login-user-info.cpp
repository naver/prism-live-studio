#include "login-user-info.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QVariant>
#include "obs-app.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "libutils-api.h"

using namespace common;

PLSLoginUserInfo::PLSLoginUserInfo()
{
	auto prePath = pls_get_user_path("PRISMLiveStudio/user/config.ini");

	if (!QFile::exists(prePath)) {
		PLS_INFO("UserInfo", "old version have not login prism");
	} else {
		PLS_INFO("UserInfo", "start parse pre version user info");
		getUserInfoFromOldVersion(prePath);
		bool isSuccess = QFile::remove(prePath);
		PLS_INFO("UserInfo", "del pre version  user config file %s", isSuccess ? "Success" : "Failed");
	}
}

// Singleton mode
PLSLoginUserInfo *PLSLoginUserInfo::getInstance()
{
	static PLSLoginUserInfo info;
	if (info.getToken().isEmpty()) {
		// initialize user info
		info.getUserLoginInfo();
	}

	return &info;
}
QJsonObject &PLSLoginUserInfo::getDataObj()
{
	return m_userObj;
}
void PLSLoginUserInfo::selfFlag(bool isSelf)
{
	m_isSelf = isSelf;
}
bool PLSLoginUserInfo::isSelf() const
{
	bool isSelf = m_isSelf && !m_userObj.value("isCam").toBool();
	return isSelf;
}

QString PLSLoginUserInfo::getNCPPlatformToken()
{
	return m_userObj.value("NCP_access_token").toString();
}
QString PLSLoginUserInfo::getNCPPlatformRefreshToken()
{
	return m_userObj.value("NCP_refresh_token").toString();
}
qint64 PLSLoginUserInfo::getNCPPlatformExpiresTime()
{
	return m_userObj.value("NCP_expires_in").toInteger();
}
QString PLSLoginUserInfo::getNCPPlatformServiceName()
{
	return m_userObj.value("NCP_service_name").toString();
}
QString PLSLoginUserInfo::getNCPPlatformServiceId()
{
	return m_userObj.value("NCP_service_id").toString();
}
QString PLSLoginUserInfo::getLoginPlatformName()
{
	return m_userObj.value("login_name").toString();
}
QString PLSLoginUserInfo::getNCPPlatformServiceAuthUrl()
{
	return m_userObj.value("NCP_service_auth_url").toString();
}
void PLSLoginUserInfo::updateNCB2BTokenInfo(const QString &token, const QString &refreshToken, const qint64 &expiresTime)
{
	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	m_userObj.insert("NCP_access_token", token);
	m_userObj.insert("NCP_refresh_token", refreshToken);
	m_userObj.insert("NCP_expires_in", expiresTime);
	bool isSuccess = pls_write_json_cbor(userPath, m_userObj);

	PLS_INFO("UserInfo", "save ncb2b token is %s.", isSuccess ? "success" : "failed");
}
void PLSLoginUserInfo::getUserLoginInfo()
{
	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	auto camUsePath = pls_get_user_path(CONFIGS_CAM_USER_CONFIG_PATH);
	bool isSuccess = pls_read_json_cbor(m_userObj, userPath) && !m_userObj.value(LOGIN_USERINFO_TOKEN).isNull();
	if (!isSuccess) {
		PLS_INFO("UserInfo", "get user info from lens");
		isSuccess = pls_read_json_cbor(m_userObj, camUsePath);
		m_userObj.insert("isCam", true);
		m_isSelf = false;
	}

	PLS_INFO("UserInfo", "get user info is %s.", isSuccess ? "success" : "failed");
}

bool PLSLoginUserInfo::isPrismLogined() const
{
	return QFile::exists(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
}

void PLSLoginUserInfo::clearPrismLoginInfo()
{
	m_userObj = {};
	bool isSuccess = QFile::remove(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
	PLS_INFO("loginUserInfo", "delete user cache file is %s", isSuccess ? "success" : "failed");
}

QString PLSLoginUserInfo::getUserCode() const
{
	return m_userObj.value(LOGIN_USERINFO_USER_CODE).toString();
}
QString PLSLoginUserInfo::getUserCodeWithEncode() const
{
	return m_userObj.value(LOGIN_USERINFO_HASHUSERCODE).toString();
}
QString PLSLoginUserInfo::getEmail() const
{
	return m_userObj.value(LOGIN_USERINFO_EMAIL).toString();
}

QString PLSLoginUserInfo::getToken() const
{
	return m_userObj.value(LOGIN_USERINFO_TOKEN).toString();
}

QString PLSLoginUserInfo::getAuthType() const
{
	return m_userObj.value(LOGIN_USERINFO_AUTHTYPE).toString();
}

QString PLSLoginUserInfo::getAuthStatusCode() const
{
	return m_userObj.value(LOGIN_USERINFO_AUThSTATUS_CODE).toString();
}

QString PLSLoginUserInfo::getprofileThumbanilUrl() const
{
	return m_userObj.value(LOGIN_USERINFO_PROFILEURL).toString();
}

void PLSLoginUserInfo::clearToken()
{
	m_userObj.insert(LOGIN_USERINFO_TOKEN, "");
}

QString PLSLoginUserInfo::getNickname() const
{
	return m_userObj.value(LOGIN_USERINFO_NICKNAME).toString();
}

QByteArray PLSLoginUserInfo::getPrismCookie() const
{
	return m_userObj.value(COOKIE_NEO_SES).toString().toUtf8();
}

QByteArray PLSLoginUserInfo::getSessionCookie() const
{
	return m_userObj.value(COOKIE_NEO_CHK).toString().toUtf8();
}

void PLSLoginUserInfo::setPrismCookie(const QVariant &neo_sesCookies)
{
	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	for (auto cookie : neo_sesCookies.value<QList<QNetworkCookie>>()) {
		if ("NEO_SES" == cookie.name()) {
			m_userObj.insert("NEO_SES", cookie.toRawForm(QNetworkCookie::NameAndValueOnly).constData());
			break;
		}
	}
	pls_write_json_cbor(userPath, m_userObj);
}

void PLSLoginUserInfo::setSessionTokenAndCookie(const QJsonObject &token, const QVariant &cookies)
{
	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	auto tokenInfo = token.value("token");
	for (auto cookie : cookies.value<QList<QNetworkCookie>>()) {
		if ("NEO_CHK" == cookie.name()) {
			m_userObj.insert("NEO_CHK", cookie.toRawForm(QNetworkCookie::NameAndValueOnly).constData());
			break;
		}
	}
	m_userObj.insert("token", tokenInfo);
	pls_write_json_cbor(userPath, m_userObj);
}

static QString getXorEncryptDecrypt(const QString &str, const char &key = '~')
{
	QByteArray bs = str.toUtf8();

	for (int i = 0; i < bs.size(); i++) {
		bs[i] = bs[i] ^ key;
	}

	return QString(bs);
}
void PLSLoginUserInfo::getUserInfoFromOldVersion(const QString &path)
{

	/*
     * For compatibility with user information from versions prior to 3.0. parse userinfo and save new cache file.
     */
	PLS_INFO("loginUserInfo", "getUserInfoFromOldVersion");
	QSettings setings(path, QSettings::IniFormat);
	setings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_LOGIN));
	m_userObj.insert(LOGIN_USERINFO_EMAIL, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_EMAIL)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_NICKNAME, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_NICKNAME)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_USER_CODE, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_USER_CODE)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_PROFILEURL, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_PROFILEURL)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_AUTHTYPE, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_AUTHTYPE)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_TOKEN, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_TOKEN)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_AUThSTATUS_CODE, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_AUThSTATUS_CODE)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_HASHUSERCODE, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_HASHUSERCODE)).toByteArray()).constData());
	m_userObj.insert(LOGIN_USERINFO_USER_CODE, QByteArray::fromBase64(setings.value(getXorEncryptDecrypt(LOGIN_USERINFO_USER_CODE)).toByteArray()).constData());
	setings.endGroup();
	//init prism cookies
	setings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_COOKIE));

	m_userObj.insert(COOKIE_NEO_SES, setings.value(getXorEncryptDecrypt(COOKIE_NEO_SES)).toByteArray().constData());
	m_userObj.insert(COOKIE_NEO_CHK, setings.value(getXorEncryptDecrypt(COOKIE_NEO_CHK)).toByteArray().constData());

	setings.endGroup();
	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	bool isSuccess = pls_write_json_cbor(userPath, m_userObj);
	PLS_INFO("loginUserInfo", "save old version login info %s", isSuccess ? "success" : "failed");
}
