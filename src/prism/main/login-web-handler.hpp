/*
 * @fine      PrismLiveStudio
 * @brief     third paty login prism platform by https;
 *            request/response handler and send finished signal to reciver
 * @date      2019-09-26
 * @author    Bing Cheng
 * @attention

 * @version   v1.0
 * @modify
 */

#ifndef LOGIN_LOGINWEBHANDLER_H
#define LOGIN_LOGINWEBHANDLER_H

#include <QObject>
#include <QNetworkCookie>
#include <qeventloop.h>
#include "login-user-info.hpp"
#include <QNetworkAccessManager>
#include <qlist.h>

class PLSNetworkAccessManager;
enum LoginInfoType { PrismLoginInfo, ChannelInfo, TokenInfo, PrismLogoutInfo, PrismSignoutInfo, PrismGCCInfo, PrismUserThumbnail, PrismNoticeInfo };
class LoginWebHandler : public QObject {
	Q_OBJECT
public:
	explicit LoginWebHandler(QObject *parent = nullptr);
	explicit LoginWebHandler(QJsonObject &result, QObject *parent = nullptr);
	~LoginWebHandler();
	/**
     * @brief initialize init https handler data
     * @param cookie cookie value
     * @param url https request url
     */
	void initialize(const QList<QNetworkCookie> &cookies, const QString &url, LoginInfoType infoType);

	void initialize(const QString &url, LoginInfoType infoType);
	/**
     * @brief requesetHttp send server https request
     */
	void requesetHttp(const QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation);

	void requestUserThubanail(LoginInfoType type);

	void requesetHttpForNotice(const QNetworkAccessManager::Operation operation = QNetworkAccessManager::GetOperation);

	/**
     * @brief unInitialize clear data
     */
	void unInitialize();
	bool isSuccessLogin();
	QString getErrorInfo();

private:
	void setConnect();
	QString getCookieStr();
	void getPrismLoginInfoHandler(int statusCode, const QByteArray &array);
	void getTokenInfoHandler(int statusCode, const QByteArray &array);
	void getPrismLogoutInfoHandler(int statusCode, const QByteArray &array);
	void getPrismSignoutInfoHandler(int statusCode, const QByteArray &array);

	void getPrismUserThubnail(int statusCode, const QByteArray &array);
	void getPrismNoitceInfoHandler(int statusCode, const QByteArray &array);
	void requesetHttpPrivacy(const QString &url, const QByteArray &body);
	bool saveThumbnail(const QPixmap &pixmap);

private slots:
	/**
     * @brief replyResultDataHandler response handler
     * @param statusCode response status code
     * @param url is requeset url is to match with who send http request
     * @param array response data include code and body
     */
	void replyResultDataHandler(int statusCode, const QString &url, const QByteArray array);
	/**
     * @brief replyErrorHandler response error info
     * @param url is requeset url is to match with who send http request
     * @param errorStr response error info
     */
	void replyErrorHandler(int statusCode, const QString &url, const QString &body, const QString &errorStr);

signals:
	/**
     * @brief finished notify response result
     * @param info
     */
	void finished(const QString &info);

private:
	QString m_url;
	QString m_errorInfo;
	QList<QNetworkCookie> m_cookies;
	PLSNetworkAccessManager *m_networkAccessManager;
	bool m_isSuccess;
	QJsonObject &m_result;
	LoginInfoType m_infoType;
};

#endif // LOGINWEBHANDLER_H
