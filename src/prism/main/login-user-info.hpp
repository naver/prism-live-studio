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

#include "login-common-struct.hpp"

#include <QSettings>
#include <QString>
#include <qnetworkcookie.h>
#include "json-data-handler.hpp"

// prism login type

class PLSLoginUserInfo {
public:
	static PLSLoginUserInfo *getInstance();
	/**
     * @brief setUserLoginInfo:set
     * @param jsonHandler:handler json data
     */
	void setUserLoginInfo(const QByteArray &arrayData);
	void setTokenInfo(const QByteArray &arrayData);
	void setPrismCookieConfigInfo(const QList<QNetworkCookie> &cookies);
	void setSessionCookieInfo(const QList<QNetworkCookie> &cookies);
	void setUserCodeWithEncode(const QString &hashUserCode);
	/**
     * @brief isPrismLogined
     * @return true login state fasle :logout/other state
     */
	bool isPrismLogined();
	/**
     * @brief clearPrismLoginInfo clear all prism login infomation
     */
	void clearPrismLoginInfo();
	/**
     * @brief getUserCode: get user code
     * @return
     */
	QString getUserCode();
	QString getUserCodeWithEncode();

	/**
     * @brief getEmail: get email
     * @return
     */
	QString getEmail();
	/**
     * @brief getToken:get login token info
     * @return
     */
	QString getToken();
	/**
     * @brief getAuthType user login type
     * @return login type
     */
	QString getAuthType();
	/**
     * @brief getUsercode
     * @return
     */
	QString getUsercode();
	/**
     * @brief getAuthStatusCode : get user status code
     * @return
     */
	QString getAuthStatusCode();
	/**
     * @brief getprofileThumbanilUrl  get user profile picture url
     * @return
     */
	QString getprofileThumbanilUrl();
	/**
     * @brief getNickname get user nick name
     * @return
     */
	QString getNickname();
	const QByteArray &getPrismCookie() const;
	const QByteArray &getSessionCookie() const;

private:
	explicit PLSLoginUserInfo();
	~PLSLoginUserInfo();
	/**
     * @brief getUserLoginInfo get info from configs
     */
	void getUserLoginInfo();
	/**
     * @brief setUserConfigInfo save user info to configs
     * @param userInfo
     */
	void setUserConfigInfo(const UserInfo &userInfo);

private:
	UserInfo m_userLoginInfo;
	QSettings m_userSettings;
	QByteArray m_prismCookies;
	QByteArray m_sessionCookies;
};

#endif // PLSLoginUserInfo_H
