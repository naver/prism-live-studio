

#ifndef PLSNETWORKACCESSMANAGER_H
#define PLSNETWORKACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "NetWorkCommonDefines.h"
#include "NetWorkAccessManager_global.h"
#include "NetWorkAPI.h"

class NetworkAccessManager : public QNetworkAccessManager, public NetWorkAPI {
	Q_OBJECT
public:
	NetworkAccessManager();
	virtual ~NetworkAccessManager();

	void abortAll() override;
	/**
     * @brief createHttpRequest
     * @param op:request type get;put; post etc;
     * @param url:request url
     * @param isEncode:is or not hamc
     * @param headData: head params
     * @param sendData: data params
     * @param context, custom context if need
     */

	QVariantMap &createHttpRequest(int op, const QString &url, bool isEncode = false, const QVariantMap &headData = QVariantMap(), const QVariantMap &sendData = QVariantMap(),
				       bool isSync = false) override;

	/**
     * @brief encryptUrl url mac64 convert
     * @param url
     * @param hMacKey
     */
	void encryptUrl(QUrl &url, const QString &hMacKey) override;
	/**
     * @brief getCookieForUrl: get cookies with url
     * @param url
     * @return :all cookies
     */
	QList<QNetworkCookie> getCookieForUrl(const QString &url) override;

	/**
     * @brief set multiple cookies in http header
     * @param cookiesMap
     */

	/* take a finished task */
	QVariantMap takeTaskMap(const QString &ID) override;

	QVariantMap &getTaskMap(const QString &ID) override;

	void removeTaskMap(const QString &ID) override;

	void removeTaskMap(const QVariantMap &last) override;

signals:
	/**
     * @brief replyResultData notify response data
     * @param statusCode response status code
     * @param url source url
     * @param array response data
     * @param context, custom context if need
     */
	void replyResultData(const QString &taskID);

	void replySyncData(const QString &taskID);

private:
	/**
     * @brief setQueryData
     * @param query httpRequest query object
     * @param data query param
     */
	void setQueryData(QUrlQuery &query, const QVariantMap &data);
	/**
     * @brief requestUrlHeadHandler set httprequest head params
     * @param isEncode is or not hmac
     * @param headData head params
     * @param httpRequest
     * @param url
     */
	void requestUrlHeadHandler(const QVariantMap &headData, QNetworkRequest &httpRequest);
	/**
     * @brief setHmacUrlHandler
     * @param isEncode
     * @param url
     */
	void setHmacUrlHandler(bool isEncode, QUrl &url);
	/**
     * @brief httpGetRequest: get request handler
     * @param sendData
     * @param isEncode
     * @param httpRequest
     * @param url
     * @return
     */

	QNetworkReply *httpGetRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &url);

	/**
     * @brief httpDeleteRequest: delete request handler
     * @param sendData
     * @param isEncode
     * @param httpRequest
     * @param url
     * @return
     */
	QNetworkReply *httpDeleteRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl);

	/**
     * @brief httpPostRequest: post request handler
     * @param sendData
     * @param httpRequest
     * @param httpUrl
     * @return
     */
	QNetworkReply *httpPostRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl);

	/**
     * @brief httpPostRequest: post request handler
     * @param sendData
     */
	QNetworkReply *httpPutRequest(const QVariantMap &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl);

	/**
     * @param sendData, encode to json array style data
     * @param httpRequest
     * @param httpUrl
     * @return
     */
	QNetworkReply *httpPostRequest(const QVariantList &sendData, bool isEncode, QNetworkRequest &httpRequest, QUrl &httpUrl);

	/**
     * @brief httpResponseHandler http response handler
     * @param reply
     * @param requestHttpUrl
     * @param context, custom context if need
     */
	QVariantMap &httpResponseHandler(QNetworkReply *reply, const QString &requestHttpUrl, bool isSync = false);
	/**
     * @brief getBodyfromMap get body value
     * @param bodyMap
     * @return
     */
	QByteArray getBodyfromMap(const QVariantMap &bodyMap);

private:
	TasksMap mTasksMap; //all the request task
};

#endif // PLSNETWORKACCESSMANAGER_H

/**********************network data map **************************/
/*
TasksMap{
	taskid{
	id:546714313413;
	url:www.baidu.com;
	reply:replyptrs;
	replycode:404;
	status:1;
	channel:twitch;
	......

	}
	taskid{
	id:546714313413;
	reply:replyptrs;
	......

	}
	taskid{
	id:546714313413;
	reply:replyptrs;
	......

	}
	taskid{
	id:546714313413;
	reply:replyptrs;
	......

	}
}
*/
/**********************network data map **************************/
