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
	void clearPrismLoginInfo() const;
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
     * @brief getUsercode
     * @return
     */
	QString getUsercode() const;
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
	QString getGcc() const;
	void setPrismCookie(const QVariant &neo_sesCookies);
	QJsonObject &getDataObj();
	void selfFlag(bool isSelf);
	bool isSelf() const;

private:
	explicit PLSLoginUserInfo();
	~PLSLoginUserInfo() = default;

	UserInfo m_userLoginInfo;
	QByteArray m_prismCookies;
	QByteArray m_sessionCookies;
	QJsonObject m_userObj;
	bool m_isSelf = true;
};

#endif // PLSLoginUserInfo_H
