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

PLSPlatformPrism::PLSPlatformPrism() : m_timerHeartbeat(this)
{
	PLS_INFO(MODULE_PlatformService, "%p %s", this, __FUNCTION__);
	m_timerHeartbeat.setInterval(10000);
	connect(&m_timerHeartbeat, &QTimer::timeout, this, [this]() {
		pls::http::Requests requests;

		requests.add(requestHeartbeat(m_iVideoSeq[DualOutputType::Horizontal]));
		if (auto iVideoSeq = m_iVideoSeq[DualOutputType::Horizontal]; iVideoSeq > 0) {
			requests.add(requestHeartbeat(iVideoSeq));
		}

		pls::http::requests(requests //
					    .receiver(pls_get_main_view())
					    .workInMainThread());
	});
}

string PLSPlatformPrism::getStreamKey(DualOutputType outputType) const
{
	if (!m_strPublishUrl[outputType].empty()) {
		auto i = m_strPublishUrl[outputType].rfind('/');
		if (string::npos != i) {
			return m_strPublishUrl[outputType].substr(i + 1);
		}
	}

	return string();
}

string PLSPlatformPrism::getStreamServer(DualOutputType outputType) const
{
	if (!m_strPublishUrl[outputType].empty()) {
		auto i = m_strPublishUrl[outputType].rfind('/');
		if (string::npos != i) {
			return m_strPublishUrl[outputType].substr(0, i);
		}
	}

	return string();
}

void PLSPlatformPrism::sendAction(const QString &body) const
{
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)
				   .hmacUrl(PRISM_API_ACTION.arg(PRISM_SSL), PLS_PC_HMAC_KEY.toUtf8())
				   .jsonContentType()
				   .timeout(PRISM_NET_REQUEST_TIMEOUT)
				   .cookie(pls_get_prism_cookie())
				   .cookie(QString::fromUtf8(PLSLoginUserInfo::getInstance()->getSessionCookie()))
				   .body(body.toUtf8())
				   .withLog()
				   .failResult([](const pls::http::Reply &reply) { PLS_ERROR(MAIN_ACTION_LOG, "send action log failed: %d-%d", reply.statusCode(), reply.error()); }));
}

pls::http::Request PLSPlatformPrism::getUploadStatusRequest(const QString &apiPath) const
{
	return pls::http::Request();
}
void PLSPlatformPrism::uploadStatus(const QString &apiPath, const QByteArray &body, bool isPrintLog) const
{
	auto request = getUploadStatusRequest(apiPath);
	if (isPrintLog) {
		request.withLog();
		PLS_INFO(apiPath.toUtf8().constData(), "Analog msg content = %s", body.constData());
	}
	request.body(body);
	runtime_stats(PLS_RUNTIME_STATS_TYPE_STATUS, std::chrono::steady_clock::now(), request.request());
	pls::http::request(request);
}

void PLSPlatformPrism::onInactive(PLSPlatformBase *platform, bool value)
{
	auto bDualOutput = pls_is_dual_output_on();

	if (!PLSCHANNELS_API->isLiving()) {
		deactivateCallback(platform, value);
		return;
	}

	if (!PLS_PLATFORM_API->isPrismLive()) {
		deactivateCallback(platform, value);

		if (!bDualOutput) {
			return;
		}
	}

	if (bDualOutput) {
		auto bVerticalOutput = platform->isVerticalOutput();

		PLSBasic::instance()->StopStreaming(bVerticalOutput ? DualOutputType::Vertical : DualOutputType::Horizontal);

		auto &iVideoSeq = m_iVideoSeq[bVerticalOutput ? DualOutputType::Vertical : DualOutputType::Horizontal];
		if (iVideoSeq > 0) {
			auto request = requestStopSimulcastLive(false, iVideoSeq);
			iVideoSeq = 0;

			pls::http::request(request //
						   .receiver(pls_get_main_view())
						   .workInMainThread());
		}

		PLS_PLATFORM_API->stopMqtt(bVerticalOutput ? DualOutputType::Vertical : DualOutputType::Horizontal);
	} else {
		requestStopSingleLive(platform);
	}
}

void PLSPlatformPrism::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	m_listApiError.clear();

	PLS_PLATFORM_API->setPlatformUrl();

	pls::http::Requests requests;

	if (pls_is_dual_output_on()) {
		auto platformListH = PLS_PLATFORM_HORIZONTAL;
		auto platformListV = PLS_PLATFORM_VERTICAL;

		if (platformListH.empty() || platformListV.empty()) {
			prepareLiveCallback(value);
			return;
		}

		requests.add(requestStartSimulcastLive(PLS_PLATFORM_API->isPrismLive(), platformListH, DualOutputType::Horizontal));
		requests.add(requestStartSimulcastLive(PLS_PLATFORM_API->isPrismLive(), platformListV, DualOutputType::Vertical));
	} else {
		auto platformList = PLS_PLATFORM_ACTIVIED;
		if (platformList.empty()) {
			prepareLiveCallback(value);
			return;
		}
		requests.add(requestStartSimulcastLive(PLS_PLATFORM_API->isPrismLive(), platformList, DualOutputType::Horizontal));
	}

	pls::http::requests(requests //
				    .receiver(pls_get_main_view())
				    .workInMainThread()
				    .results([this](const pls::http::Replies &replies) {
					    auto bSuccessful = false;
					    if (pls_is_dual_output_on()) {
						    bSuccessful = 0 != m_iVideoSeq[DualOutputType::Horizontal] || 0 != m_iVideoSeq[DualOutputType::Vertical];
					    } else {
						    bSuccessful = 0 != m_iVideoSeq[DualOutputType::Horizontal];
					    }
					    prepareLiveCallback(bSuccessful);

					    if (bSuccessful && !PLS_PLATFORM_API->isPrismLive()) {
						    m_timerHeartbeat.start();
					    }
				    }));
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
	if (m_timerHeartbeat.isActive()) {
		m_timerHeartbeat.stop();
	}

	if (0 == m_iVideoSeq[DualOutputType::Horizontal] && 0 == m_iVideoSeq[DualOutputType::Vertical]) {
		pls_async_call_mt(this, [this] { liveEndedCallback(); });
		return;
	}

	pls::http::Requests requests;

	if (0 != m_iVideoSeq[DualOutputType::Horizontal]) {
		requests.add(requestStopSimulcastLive(PLS_PLATFORM_API->isPrismLive(), m_iVideoSeq[DualOutputType::Horizontal]));
		m_iVideoSeq[DualOutputType::Horizontal] = 0;
	}
	if (0 != m_iVideoSeq[DualOutputType::Vertical]) {
		requests.add(requestStopSimulcastLive(PLS_PLATFORM_API->isPrismLive(), m_iVideoSeq[DualOutputType::Vertical]));
		m_iVideoSeq[DualOutputType::Vertical] = 0;
	}

	pls::http::requests(requests //
				    .receiver(pls_get_main_view())
				    .workInMainThread()
				    .results([this](const pls::http::Replies &replies) {
					    pls_check_app_exiting();
					    liveEndedCallback();
				    }));
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

QString PLSPlatformPrism::getPublishingTitle(list<PLSPlatformBase *> platforms) const
{
	platforms.sort([](const PLSPlatformBase *lValue, const PLSPlatformBase *rValue) {
		int lType = lValue->getServiceType() == PLSServiceType::ST_CUSTOM ? 1 : 0;
		int rType = rValue->getServiceType() == PLSServiceType::ST_CUSTOM ? 1 : 0;
		char lName = lValue->getNameForLiveStart()[0];
		char rName = rValue->getNameForLiveStart()[0];

		if (lType != rType) {
			return lType < rType;
		} else {
			return lName < rName;
		}
	});

	if (!platforms.empty()) {
		for (auto item : platforms) {
			auto title = item->getTitle();
			if (!title.empty()) {
				return QString::fromStdString(title);
			}
		}
	}

	return "PRISM Live";
}

pls::http::Request PLSPlatformPrism::requestStartSimulcastLive(bool bPrism, list<PLSPlatformBase *> platformList, DualOutputType outputType)
{
	uint64_t out_cx = config_get_uint(PLSBasic::Get()->Config(), "Video", DualOutputType::Horizontal == outputType ? "OutputCX" : "OutputCXV");
	uint64_t out_cy = config_get_uint(PLSBasic::Get()->Config(), "Video", DualOutputType::Horizontal == outputType ? "OutputCY" : "OutputCYV");

	QJsonObject jsonObjectRoot;
	jsonObjectRoot["title"] = getPublishingTitle(platformList);
	jsonObjectRoot["screenOrientation"] = out_cx >= out_cy ? "HORIZONTAL" : "VERTICAL";
	uint32_t fpsNum = 30;
	uint32_t fpsDen = 1;
	PLSBasic::Get()->GetConfigFPS(fpsNum, fpsDen);
	auto iFramerate = (int)(fpsNum / fpsDen);

	QString strPreset("simul_");
	strPreset.append(QString::number(min(out_cx, out_cy)));
	strPreset.append("p");
	strPreset.append(QString::number(iFramerate));
	strPreset.append("fps");

	jsonObjectRoot["preset"] = strPreset;
	jsonObjectRoot["resolution"] = QString("%1x%2").arg(out_cx).arg(out_cy);
	jsonObjectRoot["framerate"] = iFramerate;

	jsonObjectRoot["abpFlag"] = isAbpFlag();

	//Remove platforms that do not require streaming again
	PrismErrorPlatformType platformType = PrismErrorPlatformType::NonePlatform;
	QString url = urlForPath(bPrism ? "/live/start" : "/live/direct/v2/start");

	QStringList uploadStreamUrlList;
	QJsonArray jsonArrayPlatforms;
	for (auto item : platformList) {
		PLS_LIVE_INFO(MODULE_PlatformService, "request %s api , platform name is %s set stream url is: %s", url.toUtf8().constData(), item->getNameForLiveStart(),
			      item->getStreamServer().c_str());
		PLS_INFO_KR(MODULE_PlatformService, "request %s api , platform name is %s set stream url is: %s , stream key is: %s", url.toUtf8().constData(), item->getNameForLiveStart(),
			    item->getStreamServer().c_str(), item->getStreamKey().c_str());
		if (!item->getIsAllowPushStream()) {
			PLS_LIVE_INFO(MODULE_PlatformService, "request %s api , %s push streaming is not allowed so it is not uploaded to PRISM Server", url.toUtf8().constData(),
				      item->getNameForLiveStart());
			continue;
		}
		QString streamURL = item->getStreamUrl();
		if (uploadStreamUrlList.contains(streamURL)) {
			PLS_LIVE_INFO(MODULE_PlatformService, "request %s api , %s  push address is duplicated so it is not uploaded to PRISM Server.", url.toUtf8().constData(),
				      item->getNameForLiveStart());
			continue;
		}
		if (item->getServiceType() == PLSServiceType::ST_NCB2B) {
			platformType = PrismErrorPlatformType::NCPPlatform;
		}
		QJsonObject jsonObjecPlatform;

		jsonObjecPlatform["platform"] = item->getNameForLiveStart();
		jsonObjecPlatform["streamUrl"] = item->getStreamServer().c_str();
		jsonObjecPlatform["streamKey"] = item->getStreamKey().c_str();

		if (auto serviceLiveLink = item->getServiceLiveLink(); !serviceLiveLink.isEmpty()) {
			jsonObjecPlatform["serviceLiveLink"] = serviceLiveLink;
		}
		auto params = item->getLiveStartParams();
		for (auto iter = params.begin(); iter != params.end(); ++iter) {
			jsonObjecPlatform.insert(iter.key(), iter.value());
		}
		if (!bPrism) {
			QString directRtmlUrl = QString("%1/%2").arg(item->getStreamServer().c_str()).arg(QString::fromStdString(item->getStreamKey().c_str())).toUtf8().constData();
			jsonObjectRoot["publishUrl"] = directRtmlUrl.toStdString().c_str();
		}
		jsonArrayPlatforms.push_back(jsonObjecPlatform);
		uploadStreamUrlList.append(streamURL);
	}

	if (bPrism) {
		jsonObjectRoot["livePlatformList"] = jsonArrayPlatforms;
	} else {
		jsonObjectRoot["livePlatform"] = jsonArrayPlatforms[0];
	}

	auto request = pls::http::Request() //
			       .method(pls::http::Method::Post)
			       .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			       .withLog()
			       .body(jsonObjectRoot)
			       .jsonContentType()
			       .cookie(pls_get_prism_cookie())
			       .cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
			       .receiver(this)
			       .okResult([this, bPrism, url, platformList, outputType](const pls::http::Reply &reply) {
				       auto statusCode = reply.statusCode();
				       auto doc = QJsonDocument::fromJson(reply.data());
				       if (!doc.isObject()) {

					       // get live start api fail reason
					       LiveAbortStage stage = bPrism ? LiveAbortStage::LiveStartRequestFailed : LiveAbortStage::LiveDirectStartRequestFailed;
					       QString reason = PLS_PLATFORM_API->getLiveAbortReason(stage);

					       // get live start api detail reason
					       QVariantMap info;
					       info.insert(LIVE_ABORT_STATUS_CODE_KEY, statusCode);
					       info.insert(LIVE_ABORT_REQUEST_URL_KEY, url);
					       QString detailReason = PLS_PLATFORM_API->getLiveAbortDetailReason(LiveAbortDetailStage::LiveStartRequestNotJsonObject, info);

					       // send live abort info ,analog, platform abort living message
					       int analogFailType = bPrism ? ANALOG_LIVE_START_DATA_NOT_OBJECT : ANALOG_LIVE_DIRECT_START_DATA_NOT_OBJECT;
					       PLS_PLATFORM_API->sendLiveAbortOperation(reason, detailReason, analogFailType);

					       action::SendActionLog(action::ActionInfo(EVENT_APP, EVENT_APP_LIVING, EVENT_APP_INIT_RESULT_FAIL, ""));

					       if (pls_is_dual_output_on()) {
						       m_listApiError.push_back({platformList, PLSErrorHandler::getAlertStringByCustomErrName(customErrorFailedToStartLive(), getTableName())});
					       } else {
						       pls_async_call_mt(this, [] { PLSErrorHandler::showAlertByCustomErrName(customErrorFailedToStartLive(), getTableName()); });
					       }

					       PLS_PLATFORM_API->addFailedPlatform(platformList);
				       } else {
					       requestStartSimulcastLiveSuccess(doc, bPrism, url, statusCode, outputType, platformList);
				       }
			       })
			       .failResult([this, url, bPrism, platformType, platformList, outputType](const pls::http::Reply &reply) {
				       int statusCode = reply.statusCode();
				       auto data = reply.data();
				       bool isTimeout = reply.isTimeout();

				       //  send analog request
				       action::SendActionLog(action::ActionInfo(EVENT_APP, EVENT_APP_LIVING, EVENT_APP_INIT_RESULT_FAIL, ""));

				       // get live start api fail reason
				       LiveAbortStage stage = bPrism ? LiveAbortStage::LiveStartRequestFailed : LiveAbortStage::LiveDirectStartRequestFailed;
				       QString reason = PLS_PLATFORM_API->getLiveAbortReason(stage);

				       // get live start api detail reason
				       QVariantMap info;
				       info.insert(LIVE_ABORT_STATUS_CODE_KEY, statusCode);
				       info.insert(LIVE_ABORT_REQUEST_URL_KEY, url);

				       PLS_PLATFORM_API->addFailedPlatform(platformList);

				       //live start or live direct api request timeout
				       if (isTimeout) {

					       // get live start api timeout stage info
					       QString detailReason = PLS_PLATFORM_API->getLiveAbortDetailReason(LiveAbortDetailStage::LiveStartRequestTimeout, info);

					       // send live abort info ,analog, platform abort living message
					       int analogFailType = bPrism ? ANALOG_LIVE_START_TIME_OUT_30S : ANALOG_LIVE_DIRECT_START_TIME_OUT_30S;
					       PLS_PLATFORM_API->sendLiveAbortOperation(reason, detailReason, analogFailType);

					       if (pls_is_dual_output_on()) {
						       m_listApiError.push_back({platformList, PLSErrorHandler::getAlertStringByCustomErrName(customErrorTimeoutTryAgain(), getTableName())});
					       } else {
						       pls_async_call_mt(this, [] { PLSErrorHandler::showAlertByCustomErrName(customErrorTimeoutTryAgain(), getTableName()); });
					       }
					       return;
				       }

				       // live start or live direct api request data is not json
				       QJsonDocument jsonDocument = QJsonDocument::fromJson(data);
				       if (!jsonDocument.isObject()) {

					       // get live start api timeout stage info
					       QString detailReason = PLS_PLATFORM_API->getLiveAbortDetailReason(LiveAbortDetailStage::LiveStartRequestNotJsonObject, info);

					       // send live abort info ,analog, platform abort living message
					       int analogFailType = bPrism ? ANALOG_LIVE_START_DATA_NOT_OBJECT : ANALOG_LIVE_DIRECT_START_DATA_NOT_OBJECT;
					       PLS_PLATFORM_API->sendLiveAbortOperation(reason, detailReason, analogFailType);

					       if (pls_is_dual_output_on()) {
						       m_listApiError.push_back({platformList, PLSErrorHandler::getAlertStringByCustomErrName(customErrorFailedToStartLive(), getTableName())});
					       } else {
						       pls_async_call_mt(this, [] { PLSErrorHandler::showAlertByCustomErrName(customErrorFailedToStartLive(), getTableName()); });
					       }
					       return;
				       }

				       // get live start failed code info
				       auto root = jsonDocument.object();
				       auto prismCode = root["code"].toInt();

				       // get live start api timeout stage info
				       info.insert(LIVE_ABORT_JSON_TEXT_KEY, data);
				       QString detailReason = PLS_PLATFORM_API->getLiveAbortDetailReason(LiveAbortDetailStage::LiveStartRequestFailedWithJson, info);

				       // send live abort info ,analog, platform abort living message
				       PLS_PLATFORM_API->sendLiveAbortOperation(reason, detailReason, prismCode);

				       // b2b live start api fail special case
				       PLSErrorHandler::ExtraData exData;
				       exData.isShowUnknownError = false;
				       if (platformType == PrismErrorPlatformType::NCPPlatform) {
					       auto retData = PLSErrorHandler::getAlertString({statusCode, reply.error(), data}, NCB2B, QString(), exData);
					       if (retData.isMatched) {
						       if (pls_is_dual_output_on()) {
							       m_listApiError.push_back({platformList, retData});
						       } else {
							       pls_async_call_mt(this, [retData] {
								       auto data = retData;
								       PLSErrorHandler::directShowAlert(data, nullptr);
							       });
						       }
						       return;
					       }
				       }

				       auto retData = PLSErrorHandler::getAlertString({statusCode, reply.error(), data}, getTableName(), customErrorServerErrorTryAgain());
				       if (PLSErrorHandler::PRISM_API_TOKEN_EXPIRED == retData.prismCode) {
					       QMetaObject::invokeMethod(
						       this, [this, retData] { onTokenExpired(retData); }, Qt::QueuedConnection);
					       return;
				       } else {
					       if (pls_is_dual_output_on()) {
						       m_listApiError.push_back({platformList, retData});
					       } else {
						       pls_async_call_mt(this, [retData]() {
							       auto data = retData;
							       PLSErrorHandler::directShowAlert(data, nullptr);
						       });
					       }
				       }
			       });

	PLS_LIVE_INFO(MODULE_PlatformService, "start living url:%s", url.toStdString().c_str());

	if (pls_is_dev_server()) {
		PLSBasic::Get()->SysTrayNotify(url, QSystemTrayIcon::Information);
	}

	return request;
}

void PLSPlatformPrism::requestStartSimulcastLiveSuccess(const QJsonDocument &doc, bool bPrism, const QString &url, const int &statusCode, DualOutputType outputType,
							list<PLSPlatformBase *> platformList)
{
	auto root = doc.object();
	auto errorCode = root.value(name2str(error_code)).toString().toInt();
	if (errorCode == 0) {
		m_iVideoSeq[outputType] = root["videoSeq"].toInt();
		PLS_LIVE_INFO(MODULE_PlatformService, "request url:%s success", url.toStdString().c_str());
		PLS_INFO_KR(MODULE_PlatformService, "request url:%s success, videoSeq is %d  ", url.toStdString().c_str(), m_iVideoSeq);
		if (bPrism) {
			m_strPublishUrl[outputType] = root["publishUrl"].toString().toStdString();
			setPrismPlatformChannelLiveId(root, outputType);
			PLS_PLATFORM_API->saveStreamSettings("Prism", getStreamServer(outputType), getStreamKey(outputType), DualOutputType::Vertical == outputType);
		} else {
			auto simulcastSeq = root["simulcastSeq"].toInt();
			for (auto service : platformList) {
				service->setChannelLiveSeq(simulcastSeq);
			}
		}
	} else {

		LiveAbortStage stage = bPrism ? LiveAbortStage::LiveStartRequestFailed : LiveAbortStage::LiveDirectStartRequestFailed;
		QString reason = PLS_PLATFORM_API->getLiveAbortReason(stage);
		QVariantMap info;
		info.insert(LIVE_ABORT_STATUS_CODE_KEY, statusCode);
		info.insert(LIVE_ABORT_STATUS_CODE_KEY, errorCode);
		info.insert(LIVE_ABORT_REQUEST_URL_KEY, url);

		QString detailReason = PLS_PLATFORM_API->getLiveAbortDetailReason(LiveAbortDetailStage::LiveStartRequestFailed, info);
		int analogFailType = bPrism ? ANALOG_LIVE_START_REQUEST_FAILED : ANALOG_LIVE_DIRECT_START_REQUEST_FAILED;
		PLS_PLATFORM_API->sendLiveAbortOperation(reason, detailReason, analogFailType);

		PLS_PLATFORM_API->addFailedPlatform(platformList);

		if (pls_is_dual_output_on()) {
			m_listApiError.push_back({platformList, PLSErrorHandler::getAlertStringByCustomErrName(customErrorFailedToStartLive(), getTableName())});
		} else {
			pls_async_call_mt(this, [outputType] { PLSErrorHandler::showAlertByCustomErrName(customErrorFailedToStartLive(), getTableName()); });
		}
	}
}

void PLSPlatformPrism::setPrismPlatformChannelLiveId(const QJsonObject &root, DualOutputType outputType) const
{
	auto livePlatformList = root["livePlatformList"].toArray();
	for (auto item : livePlatformList) {
		auto streamUrl = item.toObject()["streamUrl"].toString();
		auto streamKey = item.toObject()["streamKey"].toString();
		auto simulcastSeq = item.toObject()["simulcastSeq"].toInt();
		for (auto service : DualOutputType::Vertical == outputType ? PLS_PLATFORM_VERTICAL : PLS_PLATFORM_HORIZONTAL) {
			if (QString::fromStdString(service->getStreamServer()).startsWith(streamUrl) && service->getStreamKey() == streamKey.toStdString()) {
				service->setChannelLiveSeq(simulcastSeq);
				break;
			}
		}
	}
}

pls::http::Request PLSPlatformPrism::requestHeartbeat(int iVideoSeq) const
{
	QString url = urlForPath(QString("/live/direct/v2/%1/heartbeat").arg(iVideoSeq));
	return pls::http::Request() //
		.method(pls::http::Method::Post)
		.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
		.jsonContentType()
		.cookie(pls_get_prism_cookie())
		.withLog()
		.cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
		.receiver(this)
		.failResult([this, url](const pls::http::Reply &reply) {
			pls_async_call_mt(this, [this, url, code = reply.statusCode(), data = reply.data()]() {
				requestFailedCallback(url, code, data); //
			});
		});
}

void PLSPlatformPrism::requestFailedCallback(const QString &url, int code, QByteArray data) const
{
	PLS_LIVE_ERROR(MODULE_PlatformService, "request url:%s error: %d-%s", url.toStdString().c_str(), code, QString(data).toStdString().c_str());
}

pls::http::Request PLSPlatformPrism::requestStopSimulcastLive(bool bPrism, int iVideoSeq)
{
	QString path = QString("/live/direct/v2/%1/end").arg(iVideoSeq);
	if (bPrism) {
		path = QString("/live/%1/end").arg(iVideoSeq);
	}

	QJsonObject jsonObjectRoot;
	jsonObjectRoot["reason"] = PLS_PLATFORM_API->getLiveEndReason();

	QString url = urlForPath(path);
	auto request = pls::http::Request() //
			       .method(pls::http::Method::Post)
			       .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			       .jsonContentType()
			       .cookie(pls_get_prism_cookie())
			       .cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
			       .withLog()
			       .body(jsonObjectRoot)
			       .allowAbort(false)
			       .okResult([this, url](const pls::http::Reply &reply) {
				       auto data = reply.data();
				       if (auto doc = QJsonDocument::fromJson(data); !doc.isObject()) {
					       pls_check_app_exiting();
					       requestFailedCallback(url, reply.statusCode(), data);
				       } else {
					       PLS_LIVE_INFO(MODULE_PlatformService, "request url:%s success", url.toStdString().c_str());
				       }
			       })
			       .failResult([this, url](const pls::http::Reply &reply) {
				       pls_check_app_exiting();
				       requestFailedCallback(url, reply.statusCode(), reply.data());
			       });

	PLS_LIVE_INFO(MODULE_PlatformService, "stop living url:%s", url.toStdString().c_str());

	if (pls_is_dev_server()) {
		PLSBasic::Get()->SysTrayNotify(url, QSystemTrayIcon::Information);
	}

	return request;
}

void PLSPlatformPrism::requestStopSingleLive(PLSPlatformBase *platform) const
{
	auto url = urlForPath(
		QString("/live/%1/simulcast").arg((pls_is_dual_output_on() && platform->isVerticalOutput()) ? m_iVideoSeq[DualOutputType::Vertical] : m_iVideoSeq[DualOutputType::Horizontal]));

	auto simulcastSeq = platform->getChannelLiveSeq();
	if (0 == simulcastSeq) {
		deactivateCallback(platform, true);
		return;
	}

	QJsonObject jsonObjectRoot;
	jsonObjectRoot["simulcastSeq"] = simulcastSeq;
	jsonObjectRoot["livePlatform"] = platform->getNameForLiveStart();

	pls::http::request(pls::http::Request() //
				   .method(pls::http::Method::Post)
				   .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
				   .jsonContentType()
				   .cookie(pls_get_prism_cookie())
				   .cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
				   .body(jsonObjectRoot)
				   .withLog()
				   .receiver(pls_get_main_view())
				   .workInMainThread()
				   .allowAbort(false)
				   .okResult([this, platform, url](const pls::http::Reply &) {
					   PLS_LIVE_INFO(MODULE_PlatformService, "request url:%s success", url.toStdString().c_str());
					   pls_check_app_exiting();
					   platform->onLiveStopped();
					   deactivateCallback(platform, true);
				   })
				   .failResult([this, platform, url](const pls::http::Reply &reply) {
					   pls_check_app_exiting();
					   requestFailedCallback(url, reply.statusCode(), reply.data());
					   int statusCode = reply.statusCode();
					   auto data = reply.data();
					   PLSErrorHandler::showAlert({statusCode, reply.error(), data}, getTableName(), customErrorTempErrorTryAgain());
					   PLSCHANNELS_API->setChannelUserStatus(platform->getChannelUUID(), ChannelData::Enabled);
					   deactivateCallback(platform, false);
				   }));

	PLS_LIVE_INFO(MODULE_PlatformService, "stop single live request url:%s", url.toStdString().c_str());
}

void PLSPlatformPrism::requestRefreshAccessToken(const PLSPlatformBase *platform, const std::function<void(bool)> &onNext, bool isForceRefresh, int retryCount) const
{
	const char *platName = platform->getNameForChannelType();
	auto _onSucceed = [onNext, platName](QByteArray) {
		PLS_INFO(MODULE_PlatformService, "refresh %s token success", platName);
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [onNext, platName, this, isForceRefresh, retryCount, platform](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_LIVE_ERROR(MODULE_PlatformService, "refresh %s token error: %d-%d, retryCount:%d", platName, code, error, retryCount);
		if (retryCount > 0) {
			requestRefreshAccessToken(platform, onNext, isForceRefresh, (retryCount - 1));
			return;
		}
		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto _getNetworkReply = [this, platform, _onSucceed, _onFail] {
		QJsonObject jsonObjectRoot;
		jsonObjectRoot["simulcastSeq"] = platform->getChannelLiveSeq();
		jsonObjectRoot["livePlatform"] = platform->getNameForLiveStart();
		jsonObjectRoot["accessToken"] = platform->getChannelToken();
		auto url = urlForPath(
			QString("/live/%1/token").arg(pls_is_dual_output_on() && platform->isVerticalOutput() ? m_iVideoSeq[DualOutputType::Vertical] : m_iVideoSeq[DualOutputType::Horizontal]));
		auto _request = pls::http::Request();
		_request.method(pls::http::Method::Post) //
			.contentType(HTTP_CONTENT_TYPE_VALUE)
			.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
			.body(jsonObjectRoot)
			.workInMainThread()
			.withLog()
			.timeout(PRISM_NET_REQUEST_TIMEOUT)
			.receiver(platform)
			.okResult([_onSucceed](const pls::http::Reply &reply) { _onSucceed(reply.data()); })
			.failResult([_onFail](const pls::http::Reply &reply) { _onFail(reply.statusCode(), reply.data(), reply.error()); });
		_request.cookie(pls_get_prism_cookie()).cookie(PLSLoginUserInfo::getInstance()->getSessionCookie());
		pls::http::request(_request);
	};
	PLS_INFO(MODULE_PlatformService, "start refresh %s token", platName);
	PLSAPICommon::RefreshType refreshType = isForceRefresh ? PLSAPICommon::RefreshType::ForceRefresh : PLSAPICommon::RefreshType::NotRefresh;
	if (platform->getServiceType() == PLSServiceType::ST_YOUTUBE) {
		PLSAPIYoutube::refreshYoutubeTokenBeforeRequest(refreshType, _getNetworkReply, platform, _onSucceed, _onFail);
	} else if (platform->getServiceType() == PLSServiceType::ST_NCB2B) {
		PLSAPINCB2B::refreshTokenBeforeRequest(pls_dynamic_cast<PLSPlatformNCB2B>(platform), refreshType, _getNetworkReply, platform, _onSucceed, _onFail);
	} else {
		assert(false);
		if (nullptr != onNext) {
			onNext(false);
		}
	}
}

void PLSPlatformPrism::mqttRequestRefreshToken(PLSPlatformBase *platform, const std::function<void(bool)> &callback) const
{
	if (PLSTokenRequestStatus::PLS_BAD == platform->getTokenRequestStatus()) {
		if (callback) {
			callback(false);
		}
		return;
	}

	auto uuid = platform->getChannelUUID();
	if (platform->getServiceType() == PLSServiceType::ST_YOUTUBE || platform->getServiceType() == PLSServiceType::ST_NCB2B) {
		if (platform->getTokenRequestStatus() == PLSTokenRequestStatus::PLS_ING) {
			return;
		}
		platform->setTokenRequestStatus(PLSTokenRequestStatus::PLS_ING);
		requestRefreshAccessToken(platform, [platform, uuid, callback](bool isSucceed) {
			platform->setTokenRequestStatus(isSucceed ? PLSTokenRequestStatus::PLS_GOOD : PLSTokenRequestStatus::PLS_BAD);
			if (!isSucceed) {
				PLSCHANNELS_API->setChannelStatus(uuid, ChannelData::Expired);
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Token.Expired").arg(platform->getChannelName()));
			}
			if (nullptr != callback) {
				callback(isSucceed);
			}
		});
	} else if (platform->getServiceType() == PLSServiceType::ST_BAND) {
		if (platform->getTokenRequestStatus() == PLSTokenRequestStatus::PLS_ING) {
			return;
		}
		platform->setTokenRequestStatus(PLSTokenRequestStatus::PLS_ING);

		requestBandRefreshToken(callback, platform, uuid, true);
	} else {
		//except band and youtube, receive request_access_token message will show toast "Failed to update"
		if (platform->getTokenRequestStatus() == PLSTokenRequestStatus::PLS_GOOD) {
			platform->setTokenRequestStatus(PLSTokenRequestStatus::PLS_BAD);
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Token.Expired").arg(translatePlatformName(platform->getChannelName())));
			PLSCHANNELS_API->setChannelStatus(uuid, ChannelData::Expired);
		}
		if (callback) {
			callback(false);
		}
		return;
	}
}

void PLSPlatformPrism::bandRefreshTokenFinished(bool isRefresh, const PLSPlatformBase *platform, const QString &uuid, const std::function<void(bool)> &callback) const
{
	bool _isSucceed = false;
	if (!isRefresh) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("Live.Check.LiveInfo.Refresh.Band.Expired").arg(platform->getChannelName()));
		PLSCHANNELS_API->setChannelStatus(uuid, ChannelData::Expired);
		PLS_INFO(MODULE_PlatformService, "refresh band token expired");
	} else {
		PLS_INFO(MODULE_PlatformService, "refresh band token success");
		QJsonObject jsonObjectRoot;
		jsonObjectRoot["simulcastSeq"] = platform->getChannelLiveSeq();
		jsonObjectRoot["livePlatform"] = platform->getNameForLiveStart();

		jsonObjectRoot["accessToken"] = platform->getChannelToken();
		auto url = urlForPath(
			QString("/live/%1/token").arg(pls_is_dual_output_on() && platform->isVerticalOutput() ? m_iVideoSeq[DualOutputType::Vertical] : m_iVideoSeq[DualOutputType::Horizontal]));
		pls::http::request(pls::http::Request() //
					   .method(pls::http::Method::Post)
					   .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
					   .withLog()
					   .jsonContentType()
					   .cookie(pls_get_prism_cookie())
					   .cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
					   .body(jsonObjectRoot)
					   .receiver(this));

		_isSucceed = true;
	}
	if (nullptr != callback) {
		callback(_isSucceed);
	}
}

void PLSPlatformPrism::requestBandRefreshToken(const std::function<void(bool)> &callback, PLSPlatformBase *platform, const QString &uuid, bool isForceUpdate) const
{
	auto callbackFun = [platform, uuid, callback, this](bool isRefresh) {
		platform->setTokenRequestStatus(isRefresh ? PLSTokenRequestStatus::PLS_GOOD : PLSTokenRequestStatus::PLS_BAD);
		this->bandRefreshTokenFinished(isRefresh, platform, uuid, callback);
	};
	auto bandPlatform = static_cast<PLSPlatformBand *>(platform);
	if (bandPlatform) {
		PLS_INFO(MODULE_PlatformService, "start refresh band token request");
		bandPlatform->getBandRefreshTokenInfo(callbackFun, isForceUpdate);
	}
}

void PLSPlatformPrism::printStartLog() const
{
#define BOOL_To_STR(x) (x) ? "true" : "false"
	QString log = QString("PRISM Start Live Succeed isToPrism: %1, videoSeq: %2.\n").arg(BOOL_To_STR(PLS_PLATFORM_API->isPrismLive())).arg(QString::fromStdString(getCharVideoSeq()));
	for (auto item : PLS_PLATFORM_ACTIVIED) {
		const auto &channel = PLSCHANNELS_API->getChanelInfoRef(item->getChannelUUID());
		bool isScheduled = item->isScheduleLive();
		bool isRehearsal = PLSChannelDataAPI::getInstance()->isRehearsaling();
		auto dataType = channel.value(ChannelData::g_data_type, ChannelData::RTMPType);

		log = log.append("platformName: %1").arg(channel.value(ChannelData::g_channelName, "").toString());
		log = log.append("\tisRTMP: %1").arg(BOOL_To_STR(dataType == ChannelData::RTMPType));
		log = log.append("\ttitle: %1").arg(pls_masking_person_info(QString::fromStdString(item->getTitle())).toUtf8().constData());
		log = log.append("\twatchUrl: %1").arg(item->getShareUrlEnc());
		log = log.append("\tVODUrl: %1").arg(item->getServiceLiveLinkEnc());
		log = log.append("\tisScheduled: %1").arg(BOOL_To_STR(isScheduled));
		log = log.append("\tisRehearsal: %1").arg(BOOL_To_STR(isRehearsal));
		log = log.append("\tsimulcastSeq: %1").arg(item->getChannelLiveSeq());
		log = log.append("\tuuid: %1.\n").arg(item->getChannelUUID());
		log = log.append("\tstream url: %1.\n").arg(item->getStreamServer().c_str());
	}
	PLS_LIVE_INFO(MODULE_PlatformService, "%s", log.toStdString().c_str());

	QString log_kr = QString("PRISM Start Live Succeed isToPrism: %1, videoSeq: %2.\n").arg(BOOL_To_STR(PLS_PLATFORM_API->isPrismLive())).arg(QString::fromStdString(getCharVideoSeq()));
	for (auto item : PLS_PLATFORM_ACTIVIED) {
		const auto &channel = PLSCHANNELS_API->getChanelInfoRef(item->getChannelUUID());
		bool isScheduled = item->isScheduleLive();
		bool isRehearsal = PLSChannelDataAPI::getInstance()->isRehearsaling();
		auto dataType = channel.value(ChannelData::g_data_type, ChannelData::RTMPType);

		log_kr = log_kr.append("platformName: %1").arg(channel.value(ChannelData::g_channelName, "").toString());
		log_kr = log_kr.append("\tisRTMP: %1").arg(BOOL_To_STR(dataType == ChannelData::RTMPType));
		log_kr = log_kr.append("\ttitle: %1").arg(item->getTitle().c_str());
		log_kr = log_kr.append("\twatchUrl: %1").arg(item->getShareUrl());
		log_kr = log_kr.append("\tVODUrl: %1").arg(item->getServiceLiveLink());
		log_kr = log_kr.append("\tisScheduled: %1").arg(BOOL_To_STR(isScheduled));
		log_kr = log_kr.append("\tisRehearsal: %1").arg(BOOL_To_STR(isRehearsal));
		log_kr = log_kr.append("\tsimulcastSeq: %1").arg(item->getChannelLiveSeq());
		log_kr = log_kr.append("\tuuid: %1.\n").arg(item->getChannelUUID());
		log_kr = log_kr.append("\tstream url: %1.\n").arg(item->getStreamServer().c_str());
		log_kr = log_kr.append("\tstream key: %1.\n").arg(item->getStreamKey().c_str());
	}
	PLS_LIVE_INFO_KR(MODULE_PlatformService, "%s", log_kr.toStdString().c_str());
}

bool PLSPlatformPrism::isAbpFlag() const
{
	OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
	const char *rate_control = obs_data_get_string(streamEncSettings, "rate_control");
	if (!rate_control)
		rate_control = "";
	if (astrcmpi(rate_control, "ABR") == 0) {
		return true;
	}
	return false;
}

void PLSPlatformPrism::onTokenExpired(const PLSErrorHandler::RetData &retData) const
{
	reloginPrismExpired(retData);
}
