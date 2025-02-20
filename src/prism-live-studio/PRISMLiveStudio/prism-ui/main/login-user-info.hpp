/*
 * @fine      PrismLiveStudio
 * @brief     prism user info manager
 * @date      2019-09-26
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_LOGINUSERINFO_H
#define LOGIN_LOGINUSERINFO_H

#include <QSettings>
#include <QString>
#include <qnetworkcookie.h>
#include "login-common-struct.hpp"
#include <qjsonobject.h>
#include <qbytearray.h>

// prism login type
enum class LoginInfoType { PrismLoginInfo, ChannelInfo, TokenInfo, PrismLogoutInfo, PrismSignoutInfo, PrismGCCInfo, PrismUserThumbnail, PrismNoticeInfo };

class PLSLoginUserInfo {
public:
	static PLSLoginUserInfo *getInstance();
	/**
     * @brief getUserLoginInfo get info from configs
     */
	void getUserLoginInfo();
	/**
     * @brief isPrismLogined
     * @return true login state fasle :logout/other state
     */
	bool isPrismLogined() const;
	/**
     * @brief clearPrismLoginInfo clear all prism login infomation
     */
	void clearPrismLoginInfo();
	/**
     * @brief getUserCode: get user code
     * @return
     */
	QString getUserCode() const;
	QString getUserCodeWithEncode() const;

	/**
     * @brief getEmail: get email
     * @return
     */
	QString getEmail() const;
	/**
     * @brief getToken:get login token info
     * @return
     */
	QString getToken() const;
	/**
     * @brief getAuthType user login type
     * @return login type
     */
	QString getAuthType() const;
	/**
     * @brief getAuthStatusCode : get user status code
     * @return
     */
	QString getAuthStatusCode() const;
	/**
     * @brief getprofileThumbanilUrl  get user profile picture url
     * @return
     */
	QString getprofileThumbanilUrl() const;
	/**
     * @brief getNickname get user nick name
     * @return
     */
	void clearToken();
	QString getNickname() const;
	QByteArray getPrismCookie() const;
	QByteArray getSessionCookie() const;
	void setPrismCookie(const QVariant &neo_sesCookies);
	void setSessionTokenAndCookie(const QJsonObject &token, const QVariant &cookies);
	QJsonObject &getDataObj();
	void selfFlag(bool isSelf);
	bool isSelf() const;
	QString getNCPPlatformToken();
	QString getNCPPlatformRefreshToken();
	qint64 getNCPPlatformExpiresTime();
	QString getNCPPlatformServiceName();
	QString getNCPPlatformServiceId();
	QString getLoginPlatformName();
	QString getNCPPlatformServiceAuthUrl();
	void updateNCB2BTokenInfo(const QString &token, const QString &refreshToken, const qint64 &expiresTime);

private:
	explicit PLSLoginUserInfo();
	~PLSLoginUserInfo() = default;
	void getUserInfoFromOldVersion(const QString &path);
	QByteArray m_prismCookies;
	QByteArray m_sessionCookies;
	QJsonObject m_userObj;
	bool m_isSelf = true;
};
#define PLSLOGINUSERINFO PLSLoginUserInfo::getInstance()
#endif // PLSLoginUserInfo_H
