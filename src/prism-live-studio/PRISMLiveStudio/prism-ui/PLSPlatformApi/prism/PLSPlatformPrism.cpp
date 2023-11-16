
#include "PLSPlatformPrism.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QApplication>

#include "log/log.h"
#include "PLSAction.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#include <PLSChannelDataHandler.h>
#include "PLSAlertView.h"
#include <vector>
#include <cassert>
#include "pls-net-url.hpp"
#include "PLSServerStreamHandler.hpp"
#include "band/PLSPlatformBand.h"
#include "../youtube/PLSAPIYoutube.h"
//#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "log/module_names.h"
#include "utils-api.h"
#include "PLSAPIYoutube.h"
#include "login-user-info.hpp"
#include "pls-gpop-data-struct.hpp"
#include "PLSChannelsVirualAPI.h"
#include "PLSCommonConst.h"

using namespace std;
using namespace common;
using namespace action;

extern QString translatePlatformName(const QString &platformName);
extern OBSData GetDataFromJsonFile(const char *jsonFile);

PLSPlatformPrism *PLSPlatformPrism::instance()
{
	static PLSPlatformPrism *_instance = nullptr;

	if (nullptr == _instance) {
		_instance = pls_new<PLSPlatformPrism>();
		_instance->moveToThread(qApp->thread()); //zhangdewen force move to main thread
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { pls_delete(_instance, nullptr); });
		QObject::connect(
			PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, _instance,
			[](bool isSucceed) {
				if (isSucceed) {
					_instance->printStartLog();
					PLSPlatformYoutube::showAutoStartFalseAlertIfNeeded();
				}
			},
			Qt::QueuedConnection);
	}

	return _instance;
}

PLSPlatformPrism::PLSPlatformPrism() : m_timerHeartbeat(this)
{
	
}

string PLSPlatformPrism::getStreamKey() const
{
	
	return string();
}

string PLSPlatformPrism::getStreamServer() const
{
	return string();
}

void PLSPlatformPrism::sendAction(const QString &body) const
{
	
}

pls::http::Request PLSPlatformPrism::getUploadStatusRequest(const QString &apiPath) const
{
	return pls::http::Request();
}

void PLSPlatformPrism::uploadStatus(const QString &apiPath, const QString &body, bool isPrintLog) const
{
	
}

void PLSPlatformPrism::onInactive(PLSPlatformBase *platform, bool value) const
{
	deactivateCallback(platform, value);
}

void PLSPlatformPrism::onPrepareLive(bool value)
{
	prepareLiveCallback(true);
}

void PLSPlatformPrism::onPrepareFinish() const
{
	prepareFinishCallback();
}

void PLSPlatformPrism::onLiveStopped() const
{
	liveStoppedCallback();
}

void PLSPlatformPrism::onLiveEnded()
{
	liveEndedCallback();
}

std::string PLSPlatformPrism::getCharVideoSeq() const
{
	return std::string();
}

void PLSPlatformPrism::deactivateCallback(const PLSPlatformBase *platform, bool value) const
{
	PLS_PLATFORM_API->deactivateCallback(platform, value);
}

void PLSPlatformPrism::prepareLiveCallback(bool value) const
{
	if (LiveStatus::PrepareLive != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	PLS_PLATFORM_API->prepareLiveCallback(value);
}

void PLSPlatformPrism::prepareFinishCallback() const
{
	if (LiveStatus::PrepareFinish != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	PLS_PLATFORM_API->prepareFinishCallback();
}

void PLSPlatformPrism::liveStoppedCallback() const
{
	if (LiveStatus::LiveStoped != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	PLS_PLATFORM_API->liveStoppedCallback();
}

void PLSPlatformPrism::liveEndedCallback() const
{
	if (LiveStatus::LiveEnded != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}
	PLS_PLATFORM_API->liveEndedCallback();
}

QString PLSPlatformPrism::urlForPath(const QString &path) const
{
	return QString("%1%2").arg(PRISM_API_BASE.arg(PRISM_SSL)).arg(path);
}

string PLSPlatformPrism::formatDateTime(time_t now)
{
	if (0 == now) {
		time(&now);
	}
	string buf;
	tm tmNow;
#if defined(Q_OS_WIN)
#define os_gmtime_s(tm, time) gmtime_s(tm, time)
#else
#define os_gmtime_s(tm, time) gmtime_r(time, tm)
#endif
	if (0 == os_gmtime_s(&tmNow, &now)) {
		buf.resize(32);
		buf.resize(strftime(&buf.front(), buf.size(), "%FT%T+00:00", &tmNow));
	}

	return buf;
}

string PLSPlatformPrism::getPublishingTitle() const
{
	return "PRISM Live";
}

static void addLiveAbortSreFiled(bool bPrism, const char *abortReason)
{
	
}

void PLSPlatformPrism::requestStartSimulcastLive(bool bPrism)
{
	
}

void PLSPlatformPrism::requestStartSimulcastLiveFail(QNetworkReply::NetworkError error, const QByteArray &data, const int &code)
{
	
}

void PLSPlatformPrism::requestStartSimulcastLiveSuccess(const QJsonDocument &doc, bool bPrism, const QString &url, const int &code)
{
	
}

void PLSPlatformPrism::setPrismPlatformChannelLiveId(const QJsonObject &root) const
{
	
}

void PLSPlatformPrism::requestHeartbeat() const
{
	
}

void PLSPlatformPrism::requestFailedCallback(const QString &url, int code, QByteArray data) const
{
	
}

void PLSPlatformPrism::requestStopSimulcastLive(bool bPrism)
{
	liveEndedCallback();
}

void PLSPlatformPrism::requestLiveDirectStart()
{
	prepareLiveCallback(true);
}

void PLSPlatformPrism::requestLiveDirectEnd()
{
	liveEndedCallback();
}

void PLSPlatformPrism::requestStopSingleLive(PLSPlatformBase *platform) const
{
	
}

void PLSPlatformPrism::requestRefrshAccessToken(const PLSPlatformBase *platform, const std::function<void(bool)> &onNext, bool isForceRefresh) const
{
	
}

void PLSPlatformPrism::showWarningAlertWithMsg(const QString &msg) const
{
	auto alertParent = PLSBasic::Get();
	PLSAlertView::warning(alertParent, QTStr("Alert.Title"), msg);
}

void PLSPlatformPrism::mqttRequestRefreshToken(PLSPlatformBase *platform, const std::function<void(bool)> &callback) const
{
	callback(true);
}

void PLSPlatformPrism::bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const
{
	
}

void PLSPlatformPrism::mqttRequestYoutubeRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid) const
{
	
}

void PLSPlatformPrism::requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate) const
{
	
}

void PLSPlatformPrism::getSendThumAPIJson(QJsonObject &parameter) const
{
	
}

void PLSPlatformPrism::printStartLog() const
{
	
}

bool PLSPlatformPrism::isAbpFlag() const
{
	return true;
}

void PLSPlatformPrism::onTokenExpired() const
{
	reloginPrismExpired();
}
