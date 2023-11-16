/*
* @file		PrismApi.h
* @brief	All api should send to Prism server
* @author	wu.longyue@navercorp.com
* @date		2020-01-08
*/

#pragma once

#include <string>
#include <list>
#include <memory>
#include <qobject.h>
#include "../PLSPlatformBase.hpp"
#include "libhttp-client.h"

class PLSPlatformPrism : public QObject {
	Q_OBJECT
public:
	static PLSPlatformPrism *instance();
	PLSPlatformPrism();

	void sendAction(const QString &body) const;
	pls::http::Request getUploadStatusRequest(const QString &apiPath) const;
	void uploadStatus(const QString &apiPath, const QString &body, bool isPrintLog = true) const;

	void onInactive(PLSPlatformBase *, bool) const;

	void onPrepareLive(bool value);
	void onPrepareFinish() const;
	void onLiveStopped() const;
	void onLiveEnded();

	int getVideoSeq() const { return m_iVideoSeq; }
	std::string getCharVideoSeq() const;
	void setVideoSeq(int seq) { m_iVideoSeq = seq; }

	void mqttRequestRefreshToken(PLSPlatformBase *, const std::function<void(bool)> &) const;

	void bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const;

	void mqttRequestYoutubeRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid) const;
	void requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate = false) const;

	static std::string formatDateTime(time_t now = 0);

	void getSendThumAPIJson(QJsonObject &jsonData) const;

	void onTokenExpired() const;
	//prism refresh one accesstoken
	void requestRefrshAccessToken(const PLSPlatformBase *platform, const std::function<void(bool)> &onNext, bool isForceRefresh = true) const;

private:
	//The address and StreamKey pushed to the Prism server
	std::string getStreamKey() const;
	std::string getStreamServer() const;

	void deactivateCallback(const PLSPlatformBase *, bool) const;

	void prepareLiveCallback(bool value) const;
	void prepareFinishCallback() const;
	void liveStoppedCallback() const;
	void liveEndedCallback() const;
	QString urlForPath(const QString &path) const;

	std::string getPublishingTitle() const;

	void requestStartSimulcastLive(bool);
	void requestStartSimulcastLiveFail(QNetworkReply::NetworkError error, const QByteArray &data, const int &code);
	void requestStartSimulcastLiveSuccess(const QJsonDocument &doc, bool bPrism, const QString &url, const int &code);
	void setPrismPlatformChannelLiveId(const QJsonObject &root) const;
	void requestStopSimulcastLive(bool);
	void requestHeartbeat() const;
	void requestFailedCallback(const QString &url, int code, QByteArray data) const;

	void requestLiveDirectStart();
	void requestLiveDirectEnd();

	void requestStopSingleLive(PLSPlatformBase *platform) const;

	void showWarningAlertWithMsg(const QString &msg) const;

	void printStartLog() const;
	bool isAbpFlag() const;

	int m_iVideoSeq{0};
	std::string m_strPublishUrl;

	int m_iHistorySeq{0};
	QTimer m_timerHeartbeat;
};

#define PLS_PLATFORM_PRSIM PLSPlatformPrism::instance()
