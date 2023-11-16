#include "login-user-info.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QVariant>
#include "login-common-struct.hpp"
#include "json-data-handler.hpp"
#include "obs-app.hpp"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "libutils-api.h"

using namespace common;
PLSLoginUserInfo::PLSLoginUserInfo() {}

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
	if (isSuccess) {
		m_userLoginInfo.email = m_userObj.value(LOGIN_USERINFO_EMAIL).toString();
		m_userLoginInfo.nickName = m_userObj.value(LOGIN_USERINFO_NICKNAME).toString();
		m_userLoginInfo.userCode = m_userObj.value(LOGIN_USERINFO_USER_CODE).toString();
		m_userLoginInfo.profileThumbnailUrl = m_userObj.value(LOGIN_USERINFO_PROFILEURL).toString();
		m_userLoginInfo.authType = m_userObj.value(LOGIN_USERINFO_AUTHTYPE).toString();
		m_userLoginInfo.token = m_userObj.value(LOGIN_USERINFO_TOKEN).toString();
		m_userLoginInfo.authStatusCode = m_userObj.value(LOGIN_USERINFO_AUThSTATUS_CODE).toString();
		m_userLoginInfo.hashUserCode = m_userObj.value(LOGIN_USERINFO_HASHUSERCODE).toString();
		m_userLoginInfo.prismCookie = m_userObj.value(COOKIE_NEO_SES).toString();
		m_userLoginInfo.prismSessionCookie = m_userObj.value(COOKIE_NEO_CHK).toString();
		m_userLoginInfo.gcc = m_userObj.value("gcc").toString();
	}
}

bool PLSLoginUserInfo::isPrismLogined() const
{
	return QFile::exists(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
}

void PLSLoginUserInfo::clearPrismLoginInfo() const
{
	bool isSuccess = QFile::remove(pls_get_user_path(CONFIGS_USER_CONFIG_PATH));
	PLS_INFO("loginUserInfo", "delete user cache file is %s", isSuccess ? "success" : "failed");
}

QString PLSLoginUserInfo::getUserCode() const
{
	return QString::fromUtf8(m_userLoginInfo.userCode.toByteArray());
}
QString PLSLoginUserInfo::getUserCodeWithEncode() const
{
	return QString::fromUtf8(m_userLoginInfo.hashUserCode.toByteArray());
}
QString PLSLoginUserInfo::getEmail() const
{
	return QString::fromUtf8(m_userLoginInfo.email.toByteArray());
}

QString PLSLoginUserInfo::getToken() const
{
	if (m_userLoginInfo.token.has_value()) {
		return m_userLoginInfo.token.value().toString();
	}
	return QString();
}

QString PLSLoginUserInfo::getAuthType() const
{
	return QString::fromUtf8(m_userLoginInfo.authType.toByteArray());
}

QString PLSLoginUserInfo::getUsercode() const
{
	return QString::fromUtf8(m_userLoginInfo.userCode.toByteArray());
}

QString PLSLoginUserInfo::getAuthStatusCode() const
{
	return QString::fromUtf8(m_userLoginInfo.authStatusCode.toByteArray());
}

QString PLSLoginUserInfo::getprofileThumbanilUrl() const
{
	return QString::fromUtf8(m_userLoginInfo.profileThumbnailUrl.toByteArray());
}

void PLSLoginUserInfo::clearToken()
{
	m_userLoginInfo.token.reset();
}

QString PLSLoginUserInfo::getNickname() const
{
	return QString::fromUtf8(m_userLoginInfo.nickName.toByteArray());
}

QByteArray PLSLoginUserInfo::getPrismCookie() const
{
	return m_userObj.value(COOKIE_NEO_SES).toString().toUtf8();
}

QByteArray PLSLoginUserInfo::getSessionCookie() const
{
	return m_userObj.value(COOKIE_NEO_CHK).toString().toUtf8();
}
QString PLSLoginUserInfo::getGcc() const
{
	return m_userLoginInfo.gcc.toString();
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
