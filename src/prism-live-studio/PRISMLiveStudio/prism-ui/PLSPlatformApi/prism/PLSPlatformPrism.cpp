#include "PLSPlatformPrism.h"

#include <sstream>

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
#include "../ncb2b/PLSAPINCB2B.h"
#include "PLSErrorHandler.h"
#include "pls/pls-dual-output.h"

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

PLSPlatformPrism::PLSPlatformPrism()
{
	PLS_INFO(MODULE_PlatformService, "%p %s", this, __FUNCTION__);
}

string PLSPlatformPrism::getStreamKey(DualOutputType outputType) const
{
	return string();
}

string PLSPlatformPrism::getStreamServer(DualOutputType outputType) const
{
	return string();
}

void PLSPlatformPrism::onInactive(PLSPlatformBase *platform, bool value)
{
	auto bDualOutput = pls_is_dual_output_on();

	if (!PLSCHANNELS_API->isLiving()) {
		deactivateCallback(platform, value);
		return;
	}

	if (!PLS_PLATFORM_API->isPrismLive(platform)) {
		deactivateCallback(platform, value);

		if (!bDualOutput) {
			return;
		}
	}

	if (bDualOutput) {
		auto bVerticalOutput = platform->getVerticalOutput();

		PLSBasic::instance()->StopStreaming(bVerticalOutput ? DualOutputType::Vertical : DualOutputType::Horizontal);
	}
	stopLiveAndMqtt(platform);
}

void PLSPlatformPrism::stopLiveAndMqtt(PLSPlatformBase *platform)
{
	platform->onLiveStopped();
}

void PLSPlatformPrism::onPrepareLive(bool value)
{
	prepareLiveCallback(value);
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
	if (0 == m_iVideoSeq[DualOutputType::Horizontal] && 0 == m_iVideoSeq[DualOutputType::Vertical]) {
		pls_async_call_mt(this, [this] { liveEndedCallback(); });
		return;
	}
}

std::string PLSPlatformPrism::getCharVideoSeq() const
{
	ostringstream oss;

	oss << m_iVideoSeq[DualOutputType::Horizontal];
	if (pls_is_dual_output_on()) {
		oss << ", " << m_iVideoSeq[DualOutputType::Vertical];
	}

	return oss.str();
}

void PLSPlatformPrism::deactivateCallback(const PLSPlatformBase *platform, bool value) const
{
	PLS_PLATFORM_API->deactivateCallback(platform, value);
}

void PLSPlatformPrism::prepareLiveCallback(bool value)
{
	if (LiveStatus::PrepareLive != PLS_PLATFORM_API->getLiveStatus()) {
		return;
	}

	if (!value && !m_listApiError.empty()) {
		for (auto &item : m_listApiError) {
			pls_async_call_mt(this, [item = item]() {
				auto retData = item.second;
				PLSErrorHandler::directShowAlert(retData);
			});
		}
	}

	PLS_INFO(MODULE_PlatformService, "%s %s value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
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
	if (auto liveStatus = PLS_PLATFORM_API->getLiveStatus(); LiveStatus::LiveEnded != liveStatus && LiveStatus::Normal != liveStatus) {
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

QString PLSPlatformPrism::getPublishingTitle(list<PLSPlatformBase *> platforms) const
{
	return "PRISM Live";
}

void PLSPlatformPrism::requestFailedCallback(const QString &url, int code, QByteArray data) const
{
	PLS_LIVE_ERROR(MODULE_PlatformService, "request url:%s error: %d-%s", url.toStdString().c_str(), code, QString(data).toStdString().c_str());
}

void PLSPlatformPrism::bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const {}

void PLSPlatformPrism::requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate) const {}

void PLSPlatformPrism::printStartLog() const {}

bool PLSPlatformPrism::isAbpFlag() const
{
	return false;
}

void PLSPlatformPrism::onTokenExpired(const PLSErrorHandler::RetData &retData) const
{
	reloginPrismExpired(retData);
}