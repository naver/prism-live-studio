#include "login-user-info.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QVariant>
#include "pls-common-define.hpp"
#include "login-common-struct.hpp"
#include "json-data-handler.hpp"
#include "pls-app.hpp"

QString getXorEncryptDecrypt(const QString &str, const char &key = '~')
{
	QByteArray bs = str.toUtf8();

	for (int i = 0; i < bs.size(); i++) {
		bs[i] = bs[i] ^ key;
	}

	return QString(bs);
}

PLSLoginUserInfo::PLSLoginUserInfo() : m_userSettings(pls_get_user_path(CONFIGS_USER_CONFIG_PATH), QSettings::IniFormat)
{
	// initialize user info
	getUserLoginInfo();
}

PLSLoginUserInfo::~PLSLoginUserInfo() {}
// Singleton mode
PLSLoginUserInfo *PLSLoginUserInfo::getInstance()
{
	static PLSLoginUserInfo info;
	return &info;
}

void PLSLoginUserInfo::getUserLoginInfo()
{
	m_userSettings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_LOGIN));
	m_userLoginInfo.email = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_EMAIL)).toByteArray());
	m_userLoginInfo.nickName = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_NICKNAME)).toByteArray());
	m_userLoginInfo.userCode = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_USER_CODE)).toByteArray());
	m_userLoginInfo.profileThumbnailUrl = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_PROFILEURL)).toByteArray());
	m_userLoginInfo.authType = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_AUTHTYPE)).toByteArray());
	m_userLoginInfo.token = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_TOKEN)).toByteArray());
	m_userLoginInfo.authStatusCode = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_AUThSTATUS_CODE)).toByteArray());
	m_userLoginInfo.hashUserCode = QByteArray::fromBase64(m_userSettings.value(getXorEncryptDecrypt(LOGIN_USERINFO_HASHUSERCODE)).toByteArray());

	m_userSettings.endGroup();
	//init prism cookies
	m_userSettings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_COOKIE));
	m_prismCookies = m_userSettings.value(getXorEncryptDecrypt(COOKIE_NEO_SES)).toByteArray();
	m_sessionCookies = m_userSettings.value(getXorEncryptDecrypt(COOKIE_NEO_CHK)).toByteArray();
	m_userSettings.endGroup();
}

void PLSLoginUserInfo::setUserConfigInfo(const UserInfo &userInfo)
{
	m_userSettings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_LOGIN));
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_EMAIL), userInfo.email.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_NICKNAME), userInfo.nickName.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_USER_CODE), userInfo.userCode.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_PROFILEURL), userInfo.profileThumbnailUrl.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_AUTHTYPE), userInfo.authType.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_TOKEN), userInfo.token.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_AUThSTATUS_CODE), userInfo.authStatusCode.toByteArray().toBase64());
	m_userSettings.setValue(getXorEncryptDecrypt(LOGIN_USERINFO_HASHUSERCODE), userInfo.hashUserCode.toByteArray().toBase64());

	m_userSettings.endGroup();
	m_userSettings.sync();
}

void PLSLoginUserInfo::setPrismCookieConfigInfo(const QList<QNetworkCookie> &cookies)
{
	m_userSettings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_COOKIE));
	for (auto cookie : cookies) {
		if (COOKIE_NEO_SES == cookie.name()) {
			m_prismCookies = cookie.toRawForm();
			m_userSettings.setValue(getXorEncryptDecrypt(COOKIE_NEO_SES), QString(m_prismCookies));
		}
	}
	m_userSettings.endGroup();
}

void PLSLoginUserInfo::setSessionCookieInfo(const QList<QNetworkCookie> &cookies)
{
	m_userSettings.beginGroup(getXorEncryptDecrypt(CONFIGS_GROUP_COOKIE));

	for (auto cookie : cookies) {
		if (COOKIE_NEO_CHK == cookie.name()) {
			m_sessionCookies = cookie.toRawForm();
			m_userSettings.setValue(getXorEncryptDecrypt(COOKIE_NEO_CHK), QString(m_sessionCookies));
		}
	}
	m_userSettings.endGroup();
}
void PLSLoginUserInfo::setUserCodeWithEncode(const QString &hashUserCode)
{
	if (!hashUserCode.isEmpty()) {
		m_userLoginInfo.hashUserCode = hashUserCode;
		setUserConfigInfo(m_userLoginInfo);
	}
}

void PLSLoginUserInfo::setUserLoginInfo(const QByteArray &arrayData)
{
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_EMAIL, m_userLoginInfo.email);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_TOKEN, m_userLoginInfo.token);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_AUTHTYPE, m_userLoginInfo.authType);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_NICKNAME, m_userLoginInfo.nickName);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_USER_CODE, m_userLoginInfo.userCode);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_AUThSTATUS_CODE, m_userLoginInfo.authStatusCode);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_PROFILEURL, m_userLoginInfo.profileThumbnailUrl);
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_HASHUSERCODE, m_userLoginInfo.hashUserCode);

	setUserConfigInfo(m_userLoginInfo);
}

void PLSLoginUserInfo::setTokenInfo(const QByteArray &arrayData)
{
	PLSJsonDataHandler::getValueFromByteArray(arrayData, LOGIN_USERINFO_TOKEN, m_userLoginInfo.token);
	setUserConfigInfo(m_userLoginInfo);
}

bool PLSLoginUserInfo::isPrismLogined()
{
	return QFile::exists(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
}

void PLSLoginUserInfo::clearPrismLoginInfo()
{
	QFile::remove(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
}

QString PLSLoginUserInfo::getUserCode()
{
	return QString::fromUtf8(m_userLoginInfo.userCode.toByteArray());
}
QString PLSLoginUserInfo::getUserCodeWithEncode()
{
	return QString::fromUtf8(m_userLoginInfo.hashUserCode.toByteArray());
}
QString PLSLoginUserInfo::getEmail()
{
	return QString::fromUtf8(m_userLoginInfo.email.toByteArray());
}

QString PLSLoginUserInfo::getToken()
{
	return QString::fromUtf8(m_userLoginInfo.token.toByteArray());
}

QString PLSLoginUserInfo::getAuthType()
{
	return QString::fromUtf8(m_userLoginInfo.authType.toByteArray());
}

QString PLSLoginUserInfo::getUsercode()
{
	return QString::fromUtf8(m_userLoginInfo.userCode.toByteArray());
}

QString PLSLoginUserInfo::getAuthStatusCode()
{
	return QString::fromUtf8(m_userLoginInfo.authStatusCode.toByteArray());
}

QString PLSLoginUserInfo::getprofileThumbanilUrl()
{
	return QString::fromUtf8(m_userLoginInfo.profileThumbnailUrl.toByteArray());
}

QString PLSLoginUserInfo::getNickname()
{
	return QString::fromUtf8(m_userLoginInfo.nickName.toByteArray());
}

const QByteArray &PLSLoginUserInfo::getPrismCookie() const
{
	return m_prismCookies;
}

const QByteArray &PLSLoginUserInfo::getSessionCookie() const
{
	return m_sessionCookies;
}
