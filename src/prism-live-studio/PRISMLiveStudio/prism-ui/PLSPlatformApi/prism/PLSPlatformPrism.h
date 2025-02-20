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
#include "PLSDualOutputConst.h"

enum class PrismErrorPlatformType {
	NonePlatform,
	NCPPlatform,
};

using ApiErrorList = std::list<std::pair<std::list<PLSPlatformBase*>, PLSErrorHandler::RetData>>;

class PLSPlatformPrism : public QObject {
	Q_OBJECT
public:
	static PLSPlatformPrism *instance();
	PLSPlatformPrism();

	void sendAction(const QString &body) const;
	pls::http::Request getUploadStatusRequest(const QString &apiPath) const;
	void uploadStatus(const QString &apiPath, const QByteArray &body, bool isPrintLog = true) const;

	void onInactive(PLSPlatformBase *, bool);

	void onPrepareLive(bool value);
	void onPrepareFinish() const;
	void onLiveStopped() const;
	void onLiveEnded();

	int getVideoSeq(DualOutputType outputType) const {return m_iVideoSeq[outputType]; }
	std::string getCharVideoSeq() const;

	void mqttRequestRefreshToken(PLSPlatformBase *, const std::function<void(bool)> &) const;

	void bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const;

	void requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate = false) const;

	static std::string formatDateTime(time_t now = 0);

	void onTokenExpired(const PLSErrorHandler::RetData &retData) const;
	//prism refresh one accesstoken
	void requestRefreshAccessToken(const PLSPlatformBase *platform, const std::function<void(bool)> &onNext, bool isForceRefresh = true, int retryCount = 1) const;

	ApiErrorList &getApiErrorList() { return m_listApiError; }

	static QString getTableName() { return QStringLiteral("PRISM"); }
	static QString customErrorFailedToStartLive() { return QStringLiteral("FailedToStartLive"); }
	static QString customErrorTimeoutTryAgain() { return QStringLiteral("TimeoutTryAgain"); }
	static QString customErrorServerErrorTryAgain() { return QStringLiteral("ServerErrorTryAgain"); }
	static QString customErrorTempErrorTryAgain() { return QStringLiteral("TempErrorTryAgain"); }

private:
	//The address and StreamKey pushed to the Prism server
	std::string getStreamKey(DualOutputType outputType) const;
	std::string getStreamServer(DualOutputType outputType) const;

	void deactivateCallback(const PLSPlatformBase *, bool) const;

	void prepareLiveCallback(bool value);
	void prepareFinishCallback() const;
	void liveStoppedCallback() const;
	void liveEndedCallback() const;
	QString urlForPath(const QString &path) const;

	QString getPublishingTitle(std::list<PLSPlatformBase *>) const;

	pls::http::Request requestStartSimulcastLive(bool, std::list<PLSPlatformBase*>, DualOutputType);
	void requestStartSimulcastLiveSuccess(const QJsonDocument &doc, bool bPrism, const QString &url, const int &code, DualOutputType, std::list<PLSPlatformBase *>);
	void setPrismPlatformChannelLiveId(const QJsonObject &root, DualOutputType) const;
	pls::http::Request requestStopSimulcastLive(bool, int iVideoSeq);
	pls::http::Request requestHeartbeat(int) const;
	void requestFailedCallback(const QString &url, int code, QByteArray data) const;

	void requestStopSingleLive(PLSPlatformBase *platform) const;

	void printStartLog() const;
	bool isAbpFlag() const;

	std::array<int, DualOutputType::All> m_iVideoSeq{};
	std::array<std::string, DualOutputType::All> m_strPublishUrl{};

	QTimer m_timerHeartbeat;

	ApiErrorList m_listApiError;
};

#define PLS_PLATFORM_PRSIM PLSPlatformPrism::instance()
