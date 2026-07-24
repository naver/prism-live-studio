#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif
#include "PLSLoginDataHandler.h"
#include "libhttp-client.h"
#include <qeventloop.h>
#include <qmetaobject.h>
#include <QUrlQuery>
#include <qapplication.h>
#include <qdir.h>
#include "liblog.h"
#include "PLSAlertView.h"
#include <qbuffer.h>
#include "ui/login-terms-of-agree-view.hpp"
#include <qimagereader.h>
#include "pls-shared-values.h"
#include "pls-shared-functions.h"
#include "libutils-api.h"
#include <QDir>
#include "pls-common-define.hpp"
#include <qsettings.h>
#include "ui/PLSLoginMainView.h"
#include <limits.h>
#include "pls-gpop-data.hpp"
#include "login-user-info.hpp"
#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "pls-gpop-data.hpp"
#include <QVersionNumber>
#include "obs-app.hpp"
#include <log/log.h>
#include "ChannelCommonFunctions.h"
#include "PLSBasic.h"
#include "PLSNoticeUpdateRepository.hpp"
#include "ui/login-select-platform-view.hpp"
#include "PLSRecentLoginStore.hpp"
#include "pls-performance.h"

constexpr const int s_maxReopenCount = 1;

#define MAC_PRISM_APP_NAME "PRISMLiveStudio.app"
#define PARAM_PLATFORM_TYPE QStringLiteral("platformType")
#define PARAM_VERSION QStringLiteral("version")
#define PARAM_APP_TYPE_KEY QStringLiteral("appType")
#define PARAM_APP_TYPE_VALUE QStringLiteral("LIVE_STUDIO")
#ifdef Q_OS_WIN64
#define PLATFORM_TYPE QStringLiteral("WIN64")
#else
#define PLATFORM_TYPE QStringLiteral("MAC")
#endif

using namespace common;
QString getUpdateFromRegister()
{

#if defined(Q_OS_WIN)
	QSettings setting("NAVER Corporation", "Prism Live Studio");
	return setting.value("UpdateSpecifyApi").toString();
#elif defined(Q_OS_MACOS)
	QSettings settings("prismlive", "prismlivestudio");
	return settings.value("UpdateSpecifyApi").toString();
#endif
}

PLSLoginDataHandler *PLSLoginDataHandler::instance()
{
	static PLSLoginDataHandler dataHandler;
	return &dataHandler;
}

PLSLoginDataHandler::PLSLoginDataHandler(QObject *parent) : QObject(parent)
{

	QString userID = PLSLoginUserInfo::getInstance()->getUserCode();
	GlobalVars::logUserID = userID.toUtf8().constData();
	auto hashCode = PLSLoginUserInfo::getInstance()->getUserCodeWithEncode();
	GlobalVars::maskingLogUserID = hashCode.toUtf8().constData();

	pls_set_user_id(pls_is_empty(GlobalVars::logUserID.c_str()) ? "prismDefaultUser" : GlobalVars::logUserID.c_str(), PLS_SET_TAG_KR);
	pls_set_user_id(pls_is_empty(GlobalVars::maskingLogUserID.c_str()) ? "Hr_prismDefaultUser" : GlobalVars::maskingLogUserID.c_str(), PLS_SET_TAG_CN);
	pls_set_user_id(userID.isEmpty() ? QStringLiteral("prismDefaultUser") : userID);
	pls_set_hash_user_id(hashCode.isEmpty() ? QStringLiteral("Hr_prismDefaultUser") : hashCode);

	QObject::connect(&m_plsCancel, &PLSCancel::cancelSignal, this, [this](bool isCancel) {
		if (isCancel) {
			downloadPackageRequest.abort();
		}
	});
	QJsonObject chatObj = {{"offNormal", QJsonValue("images/chat/btn-tab-%1-off-normal.svg")}, {"offHover", QJsonValue("images/chat/btn-tab-%1-off-over.svg")},
			       {"offClick", QJsonValue("images/chat/btn-tab-%1-off-click.svg")},   {"offDisable", QJsonValue("images/chat/btn-tab-%1-off-disable.svg")},
			       {"onNormal", QJsonValue("images/chat/btn-tab-%1-on-normal.svg")},   {"onHover", QJsonValue("images/chat/btn-tab-%1-on-over.svg")},
			       {"onClick", QJsonValue("images/chat/btn-tab-%1-on-click.svg")},     {"webIcon", QJsonValue("images/chat/web-%1.svg")}};

	m_serviceResLocalObj = {{"plaftorm", QJsonValue()},
				{"name", QJsonValue()},
				{"serviceName", QJsonValue()},
				{"tagIcon", "images/B2B_%1_tagIcon.png"},
				{"dashboardButtonIcon", "images/B2B_%1_addch_logo.svg"},
				{"addChannelButtonIcon", "images/B2B_%1_addch_logo_off.svg"},
				{"addChannelButtonConnectedIcon", "images/B2B_%1_addch_logo_on.svg"},
				{"channelSettingBigIcon", "images/B2B_%1_addch_logo_large.svg"},
				{"chatIcon", chatObj}};
}

void PLSLoginDataHandler::resetLoginResultCommitState()
{
	m_loginResultCommitted = false;
	m_committedLoginPlatform.clear();
}

QVariantMap PLSLoginDataHandler::getRequestApiDefaultHeader(bool hasGcc) const
{
	QVariantMap headMap;
	if (hasGcc) {
		headMap[pls_launcher_const::HEADER_PRISM_GCC] = pls_get_gcc();
	}
#if defined(Q_OS_WIN)
	pls_win_ver_t ver = pls_get_win_ver();
#elif defined(Q_OS_MACOS)
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
#endif
	headMap[pls_launcher_const::HEADER_PRISM_LANGUAGE] = QString(pls_get_locale());
	headMap[pls_launcher_const::HEADER_PRISM_APPVERSION] = PLS_VERSION;
#if defined(Q_OS_WIN)
	headMap[pls_launcher_const::HEADER_PRISM_DEVICE] = QStringLiteral("Windows OS");
#elif defined(Q_OS_MACOS)
	headMap[pls_launcher_const::HEADER_PRISM_DEVICE] = QStringLiteral("Mac OS");
#endif
	headMap[pls_launcher_const::HEADER_PRISM_IP] = pls_get_local_ip();
#ifdef Q_OS_WIN64
	headMap[pls_launcher_const::HEADER_USER_AGENT_KEY] =
		QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x64 Language %3)").arg(ver.major).arg(ver.build).arg(GetUserDefaultUILanguage());
#else
	headMap[pls_launcher_const::HEADER_USER_AGENT_KEY] =
		QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x86 Language %3)").arg(ver.major).arg(ver.buildNum.c_str()).arg(pls_get_system_identifier());
#endif
	headMap[pls_launcher_const::HEADER_PRISM_USERCODE] = PLSLoginUserInfo::getInstance()->getUserCode();
#if defined(Q_OS_WIN)
	headMap[pls_launcher_const::HEADER_PRISM_OS] = QString("Windows %1.%2.%3.%4").arg(ver.major).arg(ver.minor).arg(ver.build).arg(ver.revis);
#elif defined(Q_OS_MACOS)
	headMap[pls_launcher_const::HEADER_PRISM_OS] = QString("MacOS %1.%2.%3.%4").arg(ver.major).arg(ver.minor).arg(ver.patch).arg(ver.buildNum.c_str());
#endif

	return headMap;
}
QMap<QString, QString> PLSLoginDataHandler::getBrowserDefaultHeader() const
{
	QMap<QString, QString> head;
	auto headMap = getRequestApiDefaultHeader();
	for (auto inter = headMap.constBegin(); inter != headMap.constEnd(); ++inter) {
		head.insert(inter.key(), inter.value().toString());
	}
	return head;
}

void PLSLoginDataHandler::requestUpdateNoticeDetail(const QString &appVersion, bool forTargetVersion)
{
	PLSNoticeUpdateRepository::instance()->fetchCurrentNoticeFromApiAsync(
		this, appVersion,
		[this, appVersion, forTargetVersion](const PLSNoticeUpdateItem &item, const QString &error) {
			if (!error.isEmpty()) {
				PLS_ERROR(UPDATE_MODULE, "Current update notice request failed. version=%s, target=%s, error=%s", appVersion.toUtf8().constData(),
					  forTargetVersion ? "true" : "false", error.toUtf8().constData());
			} else {
				PLS_INFO(UPDATE_MODULE, "Current update notice request finished. version=%s, target=%s, detailUrl=%s", appVersion.toUtf8().constData(),
					 forTargetVersion ? "true" : "false", item.contentDetailUrl.toUtf8().constData());
			}
			applyUpdateNoticeDetail(item.contentDetailUrl, forTargetVersion, error);
		});
}

void PLSLoginDataHandler::applyUpdateNoticeDetail(const QString &detailUrl, bool forTargetVersion, const QString &error)
{
	Q_UNUSED(error);
	if (forTargetVersion) {
		m_pendingUpdateState.targetNoticeDetailUrl = detailUrl;
		m_pendingUpdateState.targetNoticeFinished = true;
	} else {
		m_pendingUpdateState.startupNoticeDetailUrl = detailUrl;
		m_pendingUpdateState.startupNoticeFinished = true;
	}
	finalizePendingUpdateResultIfReady();
}

void PLSLoginDataHandler::finalizePendingUpdateResultIfReady()
{
	auto &state = m_pendingUpdateState;
	if (state.hardFailed) {
		m_appUpdataResult.setValue(state.result, true);
		return;
	}
	if (!state.updateApiFinished)
		return;

	switch (state.result.m_updateResult) {
	case AppUpdateResult::AppHasUpdate:
		if (state.waitingForTargetNotice && !state.targetNoticeFinished)
			return;
		state.result.m_updateInfoUrl = state.targetNoticeDetailUrl;
		break;
	case AppUpdateResult::AppNoUpdate:
		if (!state.startupNoticeFinished)
			return;
		state.result.m_updateInfoUrl = state.startupNoticeDetailUrl;
		break;
	default:
		break;
	}

	m_appUpdataResult.setValue(state.result, true);
}

void PLSLoginDataHandler::getAppInitDataFromRemote(const std::function<void()> &callback)
{
	QUrlQuery initQuery;

#if defined(Q_OS_WIN)
	QDir appDir(QApplication::applicationDirPath());

	initQuery.addQueryItem("platformType", "WIN64");
#elif defined(Q_OS_MACOS)
	initQuery.addQueryItem("platformType", "MAC");
#endif
	initQuery.addQueryItem("appType", "LIVE_STUDIO");
	initQuery.addQueryItem("version", PLSLoginFunc::getPrismVersion());
	QUrl initUrl(QString("%1%2").arg(pls_http_api_func::getPrismSynGateWay()).arg(pls_launcher_const::INIT_URL));
	initUrl.setQuery(initQuery);

	QUrlQuery gpopQuery;
	QString gpopPath;
	gpopQuery.addQueryItem("serviceId", "prism_pc");
#if defined(Q_OS_WIN)
	gpopPath = ":/Configs/resource/DefaultResources/win/gpop.json";
	gpopQuery.addQueryItem("deviceType", "win64");
#elif defined(Q_OS_MACOS)
	gpopQuery.addQueryItem("deviceType", "mac");
	gpopPath = ":/Configs/resource/DefaultResources/mac/gpop.json";
#endif
	gpopQuery.addQueryItem("appVersion", PLSLoginFunc::getPrismVersion());
	QUrl qpopUrl(PLSLoginFunc::getGpopUrl());
	qpopUrl.setQuery(gpopQuery);

#if defined(Q_OS_WIN)
	PLS_INFO("PLSLoginDataHandler", "start getAppInitDataFromRemote request, deviceType is win64, appVersion is %s", PLSLoginFunc::getPrismVersion().toUtf8().constData());
#elif defined(Q_OS_MACOS)
	PLS_INFO("PLSLoginDataHandler", "start getAppInitDataFromRemote request, deviceType is mac, appVersion is %s", PLSLoginFunc::getPrismVersion().toUtf8().constData());
#endif

	QJsonDocument localGpopDoc;
	int localGpopVersion = 0;
	QString gpopNameVersionStr = getLocalGpopData(gpopPath, localGpopDoc, localGpopVersion);
	const QString updateInfoFormRegister = getUpdateFromRegister();

	m_pendingUpdateState = PendingUpdateState{};
	requestUpdateNoticeDetail(QStringLiteral(PLS_VERSION), false);

	QUrl updateUrl(APP_UPDATE_URL.arg(PLSGpopData::instance()->getConnection().ssl));

	QUrlQuery query;
	query.addQueryItem(PARAM_PLATFORM_TYPE, PLATFORM_TYPE);
	query.addQueryItem(PARAM_APP_TYPE_KEY, PARAM_APP_TYPE_VALUE);
	query.addQueryItem(PARAM_VERSION, PRISM_VERSION);
	updateUrl.setQuery(query);

	pls::http::Requests requests;
	requests.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .jsonContentType() //
			     .withLog()         //
			     .receiver(this)    //
			     .hmacUrl(qpopUrl, pls_http_api_func::getPrismHamcKey())
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .jsonOkResult([this, gpopPath, localGpopVersion, localGpopDoc, gpopNameVersionStr](const pls::http::Reply &, const QJsonDocument &doc) {
				     pls_async_call_mt(this, [this, doc, gpopPath, localGpopVersion, localGpopDoc, gpopNameVersionStr]() {
					     auto version = doc.object().value("optional").toObject().value("common").toObject().value("version").toInt();
					     auto data = doc.toJson();
					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data jsonOk request, gpop version is %d , software version is %s", version,
						      PLSLoginFunc::getPrismVersion().toUtf8().constData());
					     if (localGpopVersion > version && gpopNameVersionStr == PLSLoginFunc::getPrismVersion()) {
						     data = localGpopDoc.toJson();
						     PLS_INFO("PLSGpopData",
							      "GPOP DATA STATUS: gpop data read from app folder, because the localGpopVersion version is %d , request version is %d isn't  same",
							      localGpopVersion, version);
					     } else {
						     bool isSuccess =
							     pls_write_json(pls_get_app_user_data_file_path_pn(QStringLiteral("/user/gpop_%1.json").arg(PLSLoginFunc::getPrismVersion())), doc);
						     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data read from remote api, localGpopVersion = %d, version = %d. save is %s", localGpopVersion,
							      version, isSuccess ? "success" : "failed");
					     }
					     PLSGpopData::instance()->getGpopData(data);
					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data save file, data length is %d byte", data.length());
				     });
			     })
			     .failResult([this, gpopPath, localGpopDoc](const pls::http::Reply &) {
				     pls_async_call_mt(this, [this, gpopPath, localGpopDoc]() {
					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data request json request failed.");
					     auto data = localGpopDoc.toJson();
					     PLSGpopData::instance()->getGpopData(data);

					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: read local gpop  file, data length is %d", data.length());
				     });
			     }))
		.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .jsonContentType() //
			     .withLog()         //
			     .receiver(this)    //
			     .hmacUrl(initUrl, pls_http_api_func::getPrismHamcKey())
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .jsonOkResult([this](const pls::http::Reply &, const QJsonDocument &doc) {
				     PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: app init is jsonOk request.");
				     initApiSuccessHandle(doc);
			     })
			     .failResult([this](const pls::http::Reply &reply) {
				     PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: app init is failed request.");
				     if (reply.hasErrors()) {
					     int statusCode = reply.statusCode();
					     auto errorStr = reply.errors();
					     pls_async_call_mt(this, [this, statusCode, errorStr]() {
						     PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: request appInit api failed, status code: %d,error = %s", statusCode, errorStr.toUtf8().constData());
						     PLSLoginFunc::sendAction(getActionLogInfo(pls_launcher_const::EVENT_APP, pls_launcher_const::EVENT_APP_INIT,
											       pls_launcher_const::EVENT_APP_INIT_RESULT_FAIL, pls_launcher_const::EVENT_APP_INIT_API_ERROR));
					     });
				     }
			     }))
		.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .withLog()
			     .jsonContentType()
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .hmacUrl(updateUrl, PLS_PC_HMAC_KEY.toUtf8())
			     .receiver(this)
			     .jsonOkResult([this](const pls::http::Reply &, const QJsonDocument &doc) { updateApiSuccessHandle(doc); })
			     .failResult([this](const pls::http::Reply &reply) {
				     PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
				     extraData.printLog = false;
				     auto retData = PLSErrorHandler::getAlertString({reply.statusCode(), reply.error(), reply.data()}, "PRISM", "", extraData);
				     if (retData.prismCode == PLSErrorHandler::ErrCode::PRISM_API_NO_APP_UPDATE) {
					     m_pendingUpdateState.result = PLSAppUpdateResult();
					     m_pendingUpdateState.result.m_updateResult = AppUpdateResult::AppNoUpdate;
					     m_pendingUpdateState.updateApiFinished = true;
					     PLS_INFO(UPDATE_MODULE, "UPDATE STATUS: request appversion api no update available");
					     finalizePendingUpdateResultIfReady();
				     } else {
					     PLSErrorHandler::printLog(retData);
					     m_pendingUpdateState.result = PLSAppUpdateResult();
					     m_pendingUpdateState.updateApiFinished = true;
					     m_pendingUpdateState.hardFailed = true;
					     finalizePendingUpdateResultIfReady();
				     }
			     }));

	pls::http::requests(requests.results([callback, this, updateInfoFormRegister](const pls::http::Replies &) {
		pls_async_call_mt(qApp, [callback, this, updateInfoFormRegister]() {
			PLS_INFO("PLSLoginDataHandler", "getAppInitDataFromRemote request finished");

			if (!updateInfoFormRegister.isEmpty()) {
				PLS_INFO("PLSLoginDataHandler", "update info from register");
				updateApiSuccessHandle(QJsonDocument::fromJson(updateInfoFormRegister.toUtf8()));
			}
			if (callback != nullptr)
				pls_async_call_mt([callback]() { callback(); });
		});
	}));
}

bool PLSLoginDataHandler::getPrismUserInfoFromRemote(const QList<QNetworkCookie> &cookies, const QString &requestUrl, pls::http::Method httpMethod, const QString &loginName, qint32 recentKind)
{
	QEventLoop eventloop;
	auto bodyObj = getSNSLoginParams(loginName);
	bool isSuccess = false;
	QUrl url(requestUrl);
	url.setFragment(QString());
	//PRISM/jackson/20260325/PRISM_PC-5561/remove workInMainThread to avoid UI freeze, dispatch callbacks to main thread
	pls::http::request(pls::http::Request()
				   .method(httpMethod)
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey())
				   .rawHeaders(getRequestApiDefaultHeader())
				   .body(bodyObj)
				   .withLog()            //
				   .receiver(&eventloop) //
				   .cookie(cookies)      //
				   .jsonContentType()
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this, &eventloop, &isSuccess, loginName, recentKind](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   auto userInfo = doc.object();
					   auto cookieHeader = reply.header(QNetworkRequest::SetCookieHeader);
					   pls_async_call_mt([this, &eventloop, &isSuccess, userInfo, cookieHeader, loginName, recentKind]() {
						   savePrismUserInfo(userInfo, cookieHeader, false, loginName, recentKind);
						   pls_set_manual_cookies(NCB2B);
						   PLS_INFO(LAUNCHER_LOGIN, "get prism user info success!");
						   isSuccess = true;
						   eventloop.quit();
					   });
				   })
				   .jsonFailResult([this, &eventloop, &isSuccess, requestUrl, cookies, bodyObj, loginName, recentKind](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   auto replyData = reply.data();
					   auto netError = reply.error();
					   auto statusCode = reply.statusCode();
					   auto urlPath = reply.request().originalUrl().path();
					   auto docObj = doc.object();
					   pls_async_call_mt([this, &eventloop, &isSuccess, requestUrl, cookies, bodyObj, replyData, netError, statusCode, urlPath, docObj, loginName, recentKind]() {
						   PLSErrorHandler::NetworkData data;
						   data.errData = replyData;
						   data.netError = netError;
						   data.statusCode = statusCode;
						   PLSErrorHandler::ExtraData extraData(urlPath);
						   extraData.errPhase = PLSErrPhaseLogin;
						   extraData.pathValueMap = {{"logPlatformName", loginName}};
						   bool isNCB2B = loginName == NCB2B;
						   auto retData = PLSErrorHandler::getAlertString(data, isNCB2B ? NCB2B : "PRISM", isNCB2B ? "DEFAULT_B2BLoginFailedAgain" : "PRISMLoginFailedAgain",
												  extraData);
						   if (retData.prismCode == PLSErrorHandler::ErrCode::PRISM_API_TERM_OF_AGREE) {
							   PLS_WARN(LAUNCHER_LOGIN, "http respose error! user need agree term");
							   QJsonObject agreementParams;
							   if (loginName == APPLE_ID) {
								   agreementParams = docObj.value("param").toObject();
								   getAppleIDAgreementParams(agreementParams, bodyObj);
							   } else if (isNCB2B || loginName == TWITCH || loginName == FACEBOOK) {
								   agreementParams = bodyObj;
							   } else {
								   agreementParams = docObj;
							   }
							   showTermOfView(requestUrl, agreementParams, cookies, isSuccess, eventloop, loginName, recentKind);
						   } else {
							   PLSErrorHandler::directShowAlert(retData, nullptr);
							   eventloop.quit();
						   }
					   });
				   }));
	eventloop.exec();
	return isSuccess;
}

QString PLSLoginDataHandler::getInstallFileUrl()
{
	return m_appUpdataResult.value().m_AppInstallFileUrl;
}

bool PLSLoginDataHandler::isForcePrismAppUpdate()
{
	return m_appUpdataResult.value().m_isForceUpdate;
}

QString PLSLoginDataHandler::getUpdateVersion()
{
	return m_appUpdataResult.value().m_newPrismVersion;
}

QString PLSLoginDataHandler::getUpdateInfoUrl()
{
	return m_appUpdataResult.value().m_updateInfoUrl;
}

AppUpdateResult PLSLoginDataHandler::getUpdateResult()
{
	return m_appUpdataResult.value().m_updateResult;
}

void PLSLoginDataHandler::startDownloadNewPackage(const downloadProgressCallback &callback, const QString &installFileUrl, const QString &gcc)
{
	m_plsCancel = false;

	PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: downloadUpdate file start");
	QDir dir;
	if (!PLSLoginFunc::isExistPath("updates") && !dir.mkpath(PLSLoginFunc::getUserPath("updates"))) {
		updateDownloadFailed();
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: download update dir is not existed");
		callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadFailed);
		return;
	}

	QString updateFilePath = PLSLoginFunc::getUserPath("updates");
	QString fileName = getFileNameFromUlr(installFileUrl);
	updateFilePath += "/" + fileName;

#if defined(Q_OS_MACOS)
	QString updateDir = PLSLoginFunc::getUserPath("updates");
	QString updateAppBundle = pls_get_existed_downloaded_mac_app(updateDir, getUpdateVersion(), true);
	if (!updateAppBundle.isEmpty()) {
		m_localeFilePath = updateAppBundle;
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: downloaded update install bundle file is existed");
		callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadSuccess);
		return;
	}
#endif

	//if download update exe is existed
	if (QFile::exists(updateFilePath)) {
#if defined(Q_OS_MACOS)
		QFileInfo fileInfo(updateFilePath);
		QString unzipFolder = fileInfo.dir().absolutePath();
		bool unzipSuccess = pls_unZipFile(unzipFolder, updateFilePath);
		if (unzipSuccess) {
			m_localeFilePath = unzipFolder + "/" + MAC_PRISM_APP_NAME;
			PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: downloaded update install zip file is existed");
			callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadSuccess);
			return;
		}
#elif defined(Q_OS_WIN)
		m_localeFilePath = updateFilePath;
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: downloaded update install exe file is existed");
		callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadSuccess);
		return;
#endif
	}

	downloadPackageRequest = pls::http::Request();
	pls::http::request(downloadPackageRequest.method(pls::http::Method::Get)
				   .url(installFileUrl)
				   .rawHeader("gcc", gcc)
				   .forDownload(true)
				   .saveFilePath(updateFilePath)
				   .withLog()
				   .allowAbort(true)
				   .workInNewThread()
				   .receiver(PLSLoginMainView::instance())
				   .okResult([callback, this, updateFilePath, fileName](pls::http::Reply reply) {
					   pls_check_app_exiting();
					   m_localeFilePath = updateFilePath;
#if defined(Q_OS_WIN)
					   callback(reply.downloadedBytes(), reply.downloadTotalBytes(), PLSUpdateDownloadState::PLSUpdateDownloadSuccess);
#elif defined(Q_OS_MACOS)
					   QFileInfo fileInfo(updateFilePath);
					   QString unzipFolder = fileInfo.dir().absolutePath();
					   bool unzipSuccess = pls_unZipFile(unzipFolder, updateFilePath);
					   if (!unzipSuccess) {
						   pls_async_call_mt(PLSLoginMainView::instance(), [this, callback]() {
							   this->updateDownloadFailed();
							   callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadFailed);
						   });
						   return;
					   }
					   m_localeFilePath = unzipFolder + "/" + MAC_PRISM_APP_NAME;
					   pls_remove_file(updateFilePath);
					   callback(reply.downloadedBytes(), reply.downloadTotalBytes(), PLSUpdateDownloadState::PLSUpdateDownloadSuccess);
#endif
					   PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: download install exe file success");
				   })
				   .failResult([this, callback](pls::http::Reply reply) {
					   //stop download to notify http-client
					   PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: download install exe file failed");
					   bool isAbort = reply.isAborted();
					   //					   delTmpFileCallback(updateFileTempPath);
					   pls_async_call_mt(PLSLoginMainView::instance(), [this, callback, isAbort]() {
						   pls_check_app_exiting();
						   if (isAbort) {
							   callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadCancel);
						   } else {
							   this->updateDownloadFailed();
							   callback(1, 1, PLSUpdateDownloadState::PLSUpdateDownloadFailed);
						   }
					   });
				   })
				   .progress([callback](pls::http::Reply reply) {
					   PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: current download progress Btye:%d   total Byte:%d", reply.downloadedBytes(), reply.downloadTotalBytes());
					   callback(reply.downloadedBytes(), reply.downloadTotalBytes(), PLSUpdateDownloadState::PLSUpdateDownloadProcess);
				   }));
}

void PLSLoginDataHandler::stopDownloadNewPackage()
{
	m_plsCancel = true;
}

bool PLSLoginDataHandler::isNeedLogin() const
{
	return !(m_isPrismTokenValid && m_isExistUserInfo);
}
bool PLSLoginDataHandler::isTokenVaild() const
{
	return m_isPrismTokenValid;
}
bool PLSLoginDataHandler::isExistUserInfo() const
{
	return m_isExistUserInfo;
}

QString PLSLoginDataHandler::getInstallPackagePath() const
{
	return m_localeFilePath;
}

QString PLSLoginDataHandler::getSnsCallbackUrl(const QString &snsName) const
{
	auto callbackUrl = PLSGpopData::instance()->getSnscallbackUrls().value(snsName.toLower()).url;
	if (callbackUrl.isEmpty()) {
		PLS_INFO("LAUNCHER_LOGIN", "get SNSCallbackUrl = %s, from default.", callbackUrl.toUtf8().constData());

		callbackUrl = PLSGpopData::instance()->getDefaultSnscallbackUrls().value(snsName.toLower()).url;
	}
	PLS_INFO("LAUNCHER_LOGIN", "get SNSCallbackUrl = %s", callbackUrl.toUtf8().constData());
	return callbackUrl;
}

QString PLSLoginDataHandler::getFileNameFromUlr(const QString &fileUrl)
{
	if (auto pos = fileUrl.lastIndexOf('/'); pos >= 0) {
		return fileUrl.mid(pos + 1);
	}
	return fileUrl;
}

void PLSLoginDataHandler::refreshPrismToken(const std::function<void(bool)> &callback)
{
	PLS_PERFORMANCE_FUNCTION();
	QUrl url(QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay()).arg(pls_launcher_const::PRISM_TOKEN_URL));
	if (PLSLoginUserInfo::getInstance()->getToken().isEmpty() || pls_is_empty(PLSLOGINUSERINFO->getUserCode())) {
		m_isExistUserInfo = false;
		PLS_INFO(LAUNCHER_LOGIN, "prism token is invalid need login");
		if (callback)
			callback(false);
	} else {
		m_isExistUserInfo = true;
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .cookie(PLSLoginUserInfo::getInstance()->getPrismCookie())
					   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
					   .withLog()                                          //
					   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
					   .receiver(this)
					   .jsonOkResult([this, callback](const pls::http::Reply &reply, const QJsonDocument &doc) {
						   pls_async_call_mt(this, [callback, this, sessionCookie = reply.header(QNetworkRequest::SetCookieHeader), obj = doc.object()]() {
							   PLS_INFO(LAUNCHER_LOGIN, "prism token refresh success");
							   m_isPrismTokenValid = true;
							   PLSLoginUserInfo::getInstance()->setSessionTokenAndCookie(obj, sessionCookie);
							   if (callback)
								   callback(true);
						   });
					   })
					   .failResult([this, callback](const pls::http::Reply &reply) {
						   pls_async_call_mt(this, [this, statusCode = reply.statusCode(), callback]() {
							   if (statusCode == 401) {
								   PLS_ERROR(LAUNCHER_LOGIN, "prism token refresh failed, need login");
								   m_isPrismTokenValid = false;
								   PLSLoginUserInfo::getInstance()->clearPrismLoginInfo();
							   } else {
								   PLS_INFO(LAUNCHER_LOGIN, "prism token refresh failed,but not experied, not need login");
								   m_isPrismTokenValid = true;
							   }
							   if (callback)
								   callback(m_isPrismTokenValid);
						   });
					   }));
	}
}
static void ncpErrorHandler(int statusCode, const QByteArray &netWorkData, const QNetworkReply::NetworkError error, const QString urlPath, const QString &loginName)
{
	PLSErrorHandler::NetworkData data;
	data.errData = netWorkData;
	data.netError = error;
	data.statusCode = statusCode;
	PLSErrorHandler::ExtraData extraData(urlPath);
	extraData.errPhase = PLSErrPhaseLogin;
	extraData.pathValueMap = {{"logPlatformName", loginName}};
	PLSErrorHandler::showAlert(data, loginName, loginName == NCB2B ? "DEFAULT_B2BLoginFailedAgain" : "PRISMLoginFailedAgain", extraData);
}
void PLSLoginDataHandler::getNCPServiceId(const QString &serviceName, const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback,
					  const QString &loginName)
{
	PLS_INFO(LAUNCHER_LOGIN, "start request ncp service id");
	m_serviceName = serviceName;
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                                                                                                   //
				   .hmacUrl(PRISM_NCP_SERVICE_ID_API.arg(PRISM_SSL).arg(QUrl::toPercentEncoding(serviceName)), pls_http_api_func::getPrismHamcKey()) //
				   .withLog()                                                                                                                        //
				   .receiver(this)                                                                                                                   //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .okResult([this, callback, failedCallback, loginName](const pls::http::Reply &reply) {
					   m_ncpServiceId = reply.data();
					   getNCPAuthUrl(callback, failedCallback, loginName);
				   })
				   .failResult([this, failedCallback, loginName](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp serviceid failed.");
					   auto data = reply.data();
					   auto statusCode = reply.statusCode();
					   auto error = reply.error();
					   auto urlPath = reply.request().originalUrl().path();
					   pls_async_call_mt(this, [data, statusCode, failedCallback, error, urlPath, loginName]() {
						   ncpErrorHandler(statusCode, data, error, urlPath, loginName);

						   if (failedCallback)
							   failedCallback(statusCode, 0);
					   });
				   }));
}
void PLSLoginDataHandler::getNCPAuthUrl(const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback, const QString &loginName)
{

	PLS_INFO(LAUNCHER_LOGIN, "start request ncp auth url");

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                                                                       //
				   .hmacUrl(PRISM_NCP_AUTH_API.arg(PRISM_SSL).arg(m_ncpServiceId), pls_http_api_func::getPrismHamcKey()) //
				   .withLog()                                                                                            //
				   .receiver(this)                                                                                       //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([this, callback](const pls::http::Reply &reply, const QJsonObject &obj) {
					   auto AuthUrl = obj.value("oauthAuthorizeUrl").toString();
					   m_NCB2BAuthUrl = AuthUrl;
					   pls_async_call_mt(this, [callback, this]() { callback(m_NCB2BAuthUrl); });
				   })
				   .failResult([this, failedCallback, loginName](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp auth url failed.");
					   auto data = reply.data();
					   auto statusCode = reply.statusCode();
					   auto error = reply.error();
					   auto urlPath = reply.request().originalUrl().path();
					   pls_async_call_mt(this, [data, statusCode, failedCallback, error, urlPath, loginName]() {
						   ncpErrorHandler(statusCode, data, error, urlPath, loginName);

						   if (failedCallback)
							   failedCallback(statusCode, 0);
					   });
				   }));
}
bool PLSLoginDataHandler::getNCPAccessToken(const QString &url, const QString &loginName)
{
	QEventLoop eventloop;
	bool isSuccess = false;
	//PRISM/jackson/20260325/PRISM_PC-5561/remove workInMainThread to avoid UI freeze, dispatch callbacks to main thread
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                     //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .withLog()                                          //
				   .receiver(&eventloop)                               //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([this, &eventloop, &isSuccess](const pls::http::Reply &reply, const QJsonObject &obj) {
					   pls_async_call_mt([this, &eventloop, &isSuccess, obj]() {
						   m_snsAccessTokenObj = obj;
						   PLSLoginUserInfo::getInstance()->updateNCB2BTokenInfo(m_snsAccessTokenObj.value("access_token").toString(),
													 m_snsAccessTokenObj.value("refresh_token").toString(),
													 m_snsAccessTokenObj.value("expires_in").toInt() + QDateTime::currentSecsSinceEpoch());
						   isSuccess = true;
						   eventloop.quit();
					   });
				   })
				   .failResult([&eventloop, &isSuccess, loginName](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp token failed.");
					   auto data = reply.data();
					   auto statusCode = reply.statusCode();
					   auto error = reply.error();
					   auto urlPath = reply.request().originalUrl().path();
					   pls_async_call_mt([&eventloop, &isSuccess, data, statusCode, error, urlPath, loginName]() {
						   isSuccess = false;
						   ncpErrorHandler(statusCode, data, error, urlPath, loginName);
						   eventloop.quit();
					   });
				   }));
	eventloop.exec();
	return isSuccess;
}

const QJsonObject &PLSLoginDataHandler::getNCB2BServiceConnfigRes() const
{
	return m_NCB2BServiceConfigObj;
}
QString getFilePath(const QString &fileName)
{
	auto path = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH) + "../ncp_service_res/%1";
	return path.arg(fileName);
}
QString PLSLoginDataHandler::getNCB2BServiceLogo() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_logo"));
}

QString PLSLoginDataHandler::getNCB2BServiceNBLogo() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_nb_logo"));
}

QString PLSLoginDataHandler::getNCB2BServiceColorLogo() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_big_color_logo"));
}

QString PLSLoginDataHandler::getNCB2BServiceWhiteLogo() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_big_white_logo"));
}

QString PLSLoginDataHandler::getNCB2BServiceWatermark() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_watermark"));
}

QString PLSLoginDataHandler::getNCB2BServiceOutro() const
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	return getFilePath(QString("%1_%2.png").arg(serviceId).arg("service_outro"));
}

QString PLSLoginDataHandler::getUpdateInfoUrl(const QJsonObject &updateInfoUrlList)
{
	if (IS_KR()) {
		return updateInfoUrlList.value(QStringLiteral("kr")).toString();
	} else {
		return updateInfoUrlList.value(QStringLiteral("en")).toString();
	}
}

void PLSLoginDataHandler::getGoogleCookie(const QString &loginToken, const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &loginName, qint32 recentKind)
{

	QUrl url(QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay()).arg(pls_launcher_const::GOOGLE_LOGIN_URL_TOKEN));
	QJsonObject object;
	object["accessToken"] = loginToken;
	object["snsCd"] = "google";
	QJsonDocument bodyDoc(object);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)                    //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .jsonContentType()
				   .body(bodyDoc.toJson())
				   .withLog() //
				   .attr("token", loginToken)
				   .receiver(PLSLoginMainView::instance())
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .jsonOkResult([this, callback, loginName, recentKind](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   PLS_INFO(LAUNCHER_LOGIN, "get google cookie info and user info");
					   auto cookie = reply.header(QNetworkRequest::SetCookieHeader);
					   auto statusCode = reply.statusCode();
					   auto token = reply.request().attr("token").toString();
					   auto docObj = doc.object();
					   pls_async_call_mt([this, callback, cookie, statusCode, token, docObj, loginName, recentKind]() {
						   getCookieSuccessHandle(callback, cookie, statusCode, token, docObj, loginName, recentKind);
					   });
				   })
				   .failResult([this, callback](const pls::http::Reply &reply) {
					   auto obj = QJsonDocument::fromJson(reply.data()).object();
					   QString errorInfo = QString("code = %1; prism login error;message:%2").arg(obj["code"].toInt()).arg(obj["error"].toString());
					   PLS_ERROR(PLS_LOGIN_MODULE, errorInfo.toUtf8().data());
					   PLS_LOGEX(PLS_LOG_ERROR, LAUNCHER_LOGIN, {{"prismLogin", "Google"}}, "Google %s", errorInfo.toUtf8().data());

					   PLSErrorHandler::NetworkData data;
					   data.errData = reply.data();
					   data.netError = reply.error();
					   data.statusCode = reply.statusCode();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());

					   pls_async_call_mt([this, callback, data, extraData]() {
						   PLSErrorHandler::showAlert(data, "PRISM", "PRISMLoginFailedAgain", extraData);
						   callback(false, QJsonObject());
					   });
				   }));
}

template<typename Callback> void PLSLoginDataHandler::google_regeist_handler(const QVariant &replyCookie, const QString &token, const Callback &callback, const QString &loginName, qint32 recentKind)
{
	QUrl url(QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay()).arg(pls_launcher_const::GOOGLE_LOGIN_URL_TOKEN));
	QJsonObject object;
	object["accessToken"] = token;
	object["snsCd"] = "google";
	object["agreement"] = "true";
	QJsonDocument docBody(object);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)                    //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .header(QNetworkRequest::CookieHeader, replyCookie)
				   .jsonContentType()
				   .body(docBody.toJson())
				   .withLog() //
				   .receiver(PLSLoginMainView::instance())
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .jsonOkResult([this, callback, loginName, recentKind](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   auto cookie = reply.header(QNetworkRequest::SetCookieHeader);
					   auto docObj = doc.object();
					   pls_async_call_mt([this, callback, cookie, docObj, loginName, recentKind]() {
						   savePrismUserInfo(docObj, cookie, true, loginName, recentKind);
						   callback(true, QJsonObject());
					   });
				   })
				   .failResult([callback](const pls::http::Reply &reply) {
					   PLS_ERROR(LAUNCHER_LOGIN, "google login register failed.code =%d,errorInfo = %s", reply.statusCode(), reply.errors().toUtf8().constData());
					   pls_async_call_mt([callback]() { callback(false, QJsonObject()); });
				   }));
}
void PLSLoginDataHandler::pls_google_user_info(const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &redirect_uri, const QString &code, const QString &loginName,
					       qint32 recentKind)
{
	QUrl url(pls_launcher_const::YOUTUBE_API_TOKEN);
	QUrlQuery googleQuery;
	googleQuery.addQueryItem("code", code);
	googleQuery.addQueryItem("client_id", pls_launcher_const::YOUTUBE_CLIENT_ID_);
	googleQuery.addQueryItem("client_secret", pls_launcher_const::YOUTUBE_CLIENT_KEY_);
	googleQuery.addQueryItem("redirect_uri", redirect_uri);
	googleQuery.addQueryItem("grant_type", "authorization_code");
	url.setQuery(googleQuery);
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post) //
				   .url(url)                        //
				   .withLog()                       //
				   .receiver(PLSLoginMainView::instance())
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this, callback, loginName, recentKind](const pls::http::Reply &, const QJsonDocument &doc) {
					   PLS_INFO(LAUNCHER_LOGIN, "google login start request google cookie info");
					   auto accessToken = doc.object()["access_token"].toString();
					   pls_async_call_mt([this, callback, accessToken, loginName, recentKind]() { getGoogleCookie(accessToken, callback, loginName, recentKind); });
				   })
				   .failResult([callback](const pls::http::Reply &reply) {
					   auto obj = QJsonDocument::fromJson(reply.data()).object();
					   QString errorInfo = QString("code = %1; prism login error;message:%2").arg(obj["code"].toInt()).arg(obj["error"].toString());
					   PLS_ERROR(PLS_LOGIN_MODULE, errorInfo.toUtf8().data());
					   PLS_LOGEX(PLS_LOG_ERROR, LAUNCHER_LOGIN, {{"prismLogin", "Google"}}, "Google %s", errorInfo.toUtf8().data());
					   PLSErrorHandler::NetworkData data;
					   data.errData = reply.data();
					   data.netError = reply.error();
					   data.statusCode = reply.statusCode();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());

					   pls_async_call_mt([callback, data, extraData]() {
						   PLSErrorHandler::showAlert(data, "PRISM", "PRISMLoginFailedAgain", extraData);
						   callback(false, QJsonObject());
					   });
				   }));
}

void PLSLoginDataHandler::savePrismUserInfo(const QJsonObject &userInfo, const QVariant &neo_sesCookies, bool isNewUser, const QString &loginName, qint32 recentKind)
{
	if (m_loginResultCommitted) {
		PLS_INFO(PLS_LOGIN_MODULE, "ignore duplicated login result. incoming=%s, committed=%s", loginName.toUtf8().constData(),
			 m_committedLoginPlatform.toUtf8().constData());
		return;
	}

	PLS_INFO(PLS_LOGIN_MODULE, "login success and save prism user info.");

	auto userInfo_ = userInfo;
	auto hashUserCode = userInfo_.value("hashedUserCode").toString();
	auto userId = userInfo_.value("userCode").toString();
	GlobalVars::logUserID = userId.toUtf8().constData();
	GlobalVars::maskingLogUserID = hashUserCode.toUtf8().constData();
	pls_set_user_id(GlobalVars::logUserID.c_str(), PLS_SET_TAG_KR);
	pls_set_user_id(GlobalVars::maskingLogUserID.c_str(), PLS_SET_TAG_CN);
	pls_set_user_id(userId.isEmpty() ? QStringLiteral("prismDefaultUser") : userId);
	pls_set_hash_user_id(hashUserCode.isEmpty() ? QStringLiteral("Hr_prismDefaultUser") : hashUserCode);
	PLS_INFO(PLS_LOGIN_MODULE, "set user code id and hash code id");

	for (auto cookie : neo_sesCookies.value<QList<QNetworkCookie>>()) {
		if ("NEO_SES" == cookie.name()) {
			userInfo_.insert("NEO_SES", cookie.toRawForm(QNetworkCookie::NameAndValueOnly).constData());
			break;
		}
	}

	userInfo_.insert("login_name", loginName);

	userInfo_.insert(common::SNS_ACCESS_TOKEN, m_snsAccessTokenObj.value("access_token").toString());
	userInfo_.insert(common::SNS_REFRESH_TOKEN, m_snsAccessTokenObj.value("refresh_token").toString());
	userInfo_.insert(common::SNS_EXPIRED_IN, m_snsAccessTokenObj.value("expires_in").toInt() + QDateTime::currentSecsSinceEpoch());
	if (loginName == NCB2B) {
		userInfo_.insert("NCP_service_id", m_ncpServiceId);
		userInfo_.insert("NCP_service_name", m_serviceName);
		userInfo_.insert("NCP_service_auth_url", m_NCB2BAuthUrl);
	}
	userInfo_.insert(common::SNS_TOKEN_TYPE, m_snsAccessTokenObj.value("token_type").toString());

	auto userPath = pls_get_user_path(CONFIGS_USER_CONFIG_PATH);
	if (!pls_write_json_cbor(userPath, userInfo_)) {
		PLS_ERROR(PLS_LOGIN_MODULE, "save prism user info failed. loginName=%s", loginName.toUtf8().constData());
	}
	m_loginResultCommitted = true;
	m_committedLoginPlatform = loginName;
	PLSRecentLoginStore::recordSuccess(recentKind);
	PLSLoginUserInfo::getInstance()->getUserLoginInfo();
	PLSLoginUserInfo::getInstance()->selfFlag(true);
}

void PLSLoginDataHandler::getPrismThumbnail(const std::function<void()> &callback)
{
	PLS_PERFORMANCE_FUNCTION();
	auto thumbnailUrl = PLSLoginUserInfo::getInstance()->getprofileThumbanilUrl();
	auto serviceConfigUrl = QString(PRISM_NCP_SERVICE_CONFIG_API).arg(PRISM_SSL).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId());

	QEventLoop loop;
	QPointer<QEventLoop> tmpLoop(&loop);
	pls::http::Requests requests;
	auto ncpThumbnailRequest = PLSLOGINDATAHANDLER->getNCPThumbnail();
	auto userThumbnailRequest = PLSLOGINDATAHANDLER->getUserThumbnail();

	if (ncpThumbnailRequest.has_value()) {
		requests.add(ncpThumbnailRequest.value());
	}
	if (userThumbnailRequest.has_value()) {
		requests.add(userThumbnailRequest.value());
	}
	pls::http::requests(requests.results([callback, tmpLoop](const pls::http::Replies &) {
		pls_async_call_mt(qApp, [callback, tmpLoop]() {
			PLS_INFO("PLSLoginDataHandler", "get thumbnail ,ncpThumbnail,paid term, service config res finished");
			if (callback != nullptr)
				callback();
		});
		if (pls_object_is_valid(tmpLoop)) {
			PLS_INFO("PLSLoginDataHandler", "getPrismThumbnail eventloop quit");
			tmpLoop->quit();
		}
	}));
	if (requests.count() > 0) {
		tmpLoop->exec();
	}
}

QPixmap PLSLoginDataHandler::getCurrentThumbnail() const
{
	auto imagePath = pls_get_app_user_data_dir_path_pn(shared_values::prism_user_image_path);
	auto ret = loadThumbnail(imagePath);
	if (ret.isNull()) {
		imagePath = ":/images/img-profile-blank.svg";
		ret = pls_shared_paint_svg(imagePath, QSize(50, 50));
	}
	return ret;
}

std::optional<pls::http::Request> PLSLoginDataHandler::getNCPThumbnail()
{
	auto serviceConfigUrl = QString(PRISM_NCP_SERVICE_CONFIG_API).arg(PRISM_SSL).arg(PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId());
	if (PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId().isEmpty())
		return std::nullopt;

	pls::http::Request serviceConfigReq;
	serviceConfigReq.method(pls::http::Method::Get)
		.hmacUrl(serviceConfigUrl, pls_http_api_func::getPrismHamcKey()) //
		.cookie(PLSLoginUserInfo::getInstance()->getPrismCookie())
		.withLog()      //
		.receiver(this) //
		.timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
		.objectOkResult([this](const pls::http::Reply &reply, const QJsonObject &obj) {
			pls_async_call_mt([this, obj]() {
				m_NCB2BServiceConfigObj = obj;
				downloadNCB2BServiceRes();
			});
		})
		.failResult([this](const pls::http::Reply &reply) {
			PLS_INFO(LAUNCHER_LOGIN, "get ncp service data failed.");
			PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
			auto retData = PLSErrorHandler::getAlertString({reply.statusCode(), reply.error(), reply.data()}, NCB2B, "", extraData);
			auto data = reply.data();
			if (retData.prismCode == PLSErrorHandler::ErrCode::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED) {
				m_isNeedShowB2BDisableAlert = true;
			}
			pls_async_call_mt([this, data]() { m_NCB2BServiceConfigObj = QJsonDocument::fromJson(data).object(); });
		});
	return serviceConfigReq;
}

bool PLSLoginDataHandler::isFacebookThumbnailHost(const QString &host)
{
	static const QStringList kFacebookApexDomains = {QStringLiteral("facebook.com"), QStringLiteral("fbsbx.com")};
	for (const auto &apex : kFacebookApexDomains) {
		if (host.compare(apex, Qt::CaseInsensitive) == 0 || host.endsWith(QStringLiteral(".") + apex, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

std::optional<pls::http::Request> PLSLoginDataHandler::getUserThumbnail(const QString &url, int retryCount)
{
	auto thumbnailPath = PLSLoginFunc::getUserPath("user", "prismThumbnail.png");
	auto thumbnailUrl = !url.isEmpty() ? url : PLSLoginUserInfo::getInstance()->getprofileThumbanilUrl();
	if (thumbnailUrl.isEmpty())
		return std::nullopt;
	// strip PRISM private headers for Facebook thumbnail hosts (incl. the CDN redirect target); Facebook's CDN closes the connection when it sees them
	const bool isFacebookHost = isFacebookThumbnailHost(QUrl(thumbnailUrl).host());
	pls::http::Request thumbnailReq(isFacebookHost ? pls::http::Exclude::NoDefaultRequestHeaders : pls::http::Exclude::NoExclude);
	thumbnailReq.method(pls::http::Method::Get)
		.url(thumbnailUrl) //
		.withLog()         //
		.receiver(this)    //
		.timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
		.additional([](QNetworkRequest *request) { request->setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy); })
		.okResult([this, thumbnailPath](const pls::http::Reply &reply) {
			pls_async_call_mt(this, [data = reply.data(), thumbnailPath, this]() {
				QPixmap pixmap;
				pixmap.loadFromData(data);
				pixmap = pixmap.width() > pixmap.height() ? pixmap.copy(qAbs(pixmap.width() / 2 - pixmap.height() / 2), 0, pixmap.height(), pixmap.height())
									  : pixmap.copy(0, qAbs(pixmap.height() / 2 - pixmap.width() / 2), pixmap.width(), pixmap.width());
				this->saveThumbnail(pixmap, thumbnailPath);
				updatePrismLogo();
				PLS_INFO(LAUNCHER_LOGIN, "get prism user thubnail success.");
			});
		})
		.failResult([thumbnailPath, retryCount, this, thumbnailUrl](const pls::http::Reply &reply) {
			QByteArray location = reply.rawHeader("Location");
			auto statusCode = reply.statusCode();
			bool isRedirect = (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) && !location.isEmpty();

			PLS_INFO(LAUNCHER_LOGIN, "get prism user thubnail failed. isRedirect:%s retryCount:%i  url:%s", pls_bool_2_string(isRedirect), retryCount, location.constData());

			if (isRedirect) {
				auto newRequest = getUserThumbnail(location, retryCount);
				if (newRequest.has_value()) {
					pls::http::request(newRequest.value());
				}

			} else if (retryCount > 0) {
				QTimer::singleShot(1000, this, [this, thumbnailUrl, retryCount]() {
					pls_check_app_exiting();
					auto newRequest = getUserThumbnail(thumbnailUrl, retryCount - 1);
					if (newRequest.has_value()) {
						pls::http::request(newRequest.value());
					}
				});
			}
		});
	return thumbnailReq;
}

std::optional<pls::http::Request> PLSLoginDataHandler::getServiceTermsHTML(bool isSync)
{
	static const QString pcServiceTermsId = "APP_SERVICE_TERMS";
	auto url = PRISM_AUTH_API_BASE.arg(PRISM_SSL).append("/terms/latest");
	QEventLoop loop;
	QPointer<QEventLoop> loopPtr(&loop);
	auto finishLoop = [loopPtr, this]() {
		if (loopPtr) {
			pls_async_call(this, [loopPtr]() { loopPtr->quit(); });
		}
	};
	pls::http::Request request;
	request.method(pls::http::Method::Get)
		.hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
		.receiver(this)
		.withLog()
		.okResult([this, finishLoop](const pls::http::Reply &reply) {
			PLS_INFO(LAUNCHER_LOGIN, "get latest terms json is %s.", reply.data().constData());
			auto arr = pls_to_json_array(reply.data());
			if (arr.isEmpty()) {
				m_serviceTermsHtml.setValue(QString());
				finishLoop();
				return;
			}
			for (const auto &v : arr) {
				if (!v.isObject())
					continue;
				QJsonObject obj = v.toObject();
				QString termsId = pls_get_attr(obj, "termsId", QString());
				if (!pls_is_equal(termsId, pcServiceTermsId, Qt::CaseInsensitive))
					continue;
				QJsonObject urlsObj = obj.value("urls").toObject();
				auto lang = pls_get_current_language_short_str();
				if (lang != "ko") {
					lang = "en";
				}
				QJsonObject langObj = urlsObj.value(lang).toObject();
				QJsonObject contentsObj = langObj.value("contents").toObject();
				QString pc = pls_get_attr(contentsObj, "pc", QString());
				if (!pc.isEmpty()) {
					pls::http::request(pls::http::Request()
								    .method(pls::http::Method::Get)
								    .url(pc)
								    .receiver(this)
								    .withLog()
								    .okResult([this, finishLoop](const pls::http::Reply &reply) {
									    m_serviceTermsHtml.setValue(QString::fromUtf8(reply.data()), true);
									    finishLoop();
								    })
								    .failResult([this, finishLoop](const pls::http::Reply &reply) {
									    PLS_ERROR(LAUNCHER_LOGIN, "get terms html failed: %s", reply.errors().toUtf8().constData());
									    m_serviceTermsHtml.setValue(QString());
									    finishLoop();
								    }));
					return;
				}
				m_serviceTermsHtml.setValue(QString());
				finishLoop();
				return;
			}
			m_serviceTermsHtml.setValue(QString());
			finishLoop();
		})
		.failResult([this, finishLoop](const pls::http::Reply &reply) {
			PLS_ERROR(LAUNCHER_LOGIN, "get latest terms api failed, reason: %s, status code: %d", reply.errors().toUtf8().constData(), reply.statusCode());
			m_serviceTermsHtml.setValue(QString());
			finishLoop();
		});
	if (isSync) {
		pls::http::request(request);
		loop.exec();
		return std::nullopt;
	}
	return request;
}

QString PLSLoginDataHandler::getServiceTermsHtml()
{
	return m_serviceTermsHtml.value();
}

QPixmap PLSLoginDataHandler::loadThumbnail(const QString &filePath) const
{
	QFile file(filePath);
	QByteArray data;
	if (!file.open(QIODevice::ReadOnly)) {
		return QPixmap();
	}
	data = file.readAll();
	auto orginData = QByteArray::fromBase64(data);
	QPixmap pix;
	pix.loadFromData(orginData);

	return pix;
}

void PLSLoginDataHandler::initApiSuccessHandle(const QJsonDocument &doc)
{
	if (doc.isNull() || doc.object().isEmpty()) {
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: request appInit api json parse failed");
		return;
	}
	auto object = doc.object();
	auto objMap = object.toVariantMap();
	if (object.contains("error_code")) {
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS:  request appInit api response object contains error_code");
		return;
	}
	auto gcc = object.value(QLatin1String("gcc")).toString();
	pls_set_gcc(gcc);
	auto gccByte = gcc.toUtf8();
	PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: request appInit api success, gcc: %s; hashUserCode:%s", gccByte.constData(), GlobalVars::maskingLogUserID.c_str());

	std::string currentEnvironment = pls_is_dev() ? "Dev" : "Rel";
	PLS_INIT_INFO(MAINFRAME_MODULE, "version: " PLS_VERSION ", processId: %ld, prismSession: %s, currentRunEnvironment:%s, gcc:%s;", pls_current_process_id(), GlobalVars::prismSession.c_str(),
		      currentEnvironment.c_str(), gccByte.constData());

	PLSLoginFunc::sendAction(getActionLogInfo(pls_launcher_const::EVENT_APP, pls_launcher_const::EVENT_APP_INIT, pls_launcher_const::EVENT_APP_INIT_RESULT_SUCCESS, ""));
}

void PLSLoginDataHandler::updateApiSuccessHandle(const QJsonDocument &doc)
{
	if (doc.isNull() || doc.object().isEmpty()) {
		PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: request app update lastest api json parse failed");
		m_pendingUpdateState.result = PLSAppUpdateResult();
		m_pendingUpdateState.updateApiFinished = true;
		m_pendingUpdateState.hardFailed = true;
		finalizePendingUpdateResultIfReady();
		return;
	}
	auto appUpdateObject = doc.object();
	PLSAppUpdateResult updateResult;
	updateResult.m_isForceUpdate = appUpdateObject.value(QStringLiteral("updateType")).toString() == QStringLiteral("FORCE");
	updateResult.m_newPrismVersion = appUpdateObject.value(QStringLiteral("version")).toString();
	updateResult.m_AppInstallFileUrl = appUpdateObject.value(QStringLiteral("fileUrl")).toString();

	if (appUpdateObject.value(QStringLiteral("updateType")).toString() == QStringLiteral("UPDATE") || updateResult.m_isForceUpdate) {
		updateResult.m_updateResult = AppUpdateResult::AppHasUpdate;
	}
	m_pendingUpdateState.result = updateResult;
	m_pendingUpdateState.hardFailed = false;
	m_pendingUpdateState.updateApiFinished = true;
	m_pendingUpdateState.waitingForTargetNotice = updateResult.m_updateResult == AppUpdateResult::AppHasUpdate;
	if (m_pendingUpdateState.waitingForTargetNotice) {
		m_pendingUpdateState.targetNoticeFinished = false;
		m_pendingUpdateState.targetNoticeDetailUrl.clear();
		requestUpdateNoticeDetail(updateResult.m_newPrismVersion, true);
	} else {
		finalizePendingUpdateResultIfReady();
	}

	PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: update available, isForceUpdate: %s, version: %s, fileUrl: %s", updateResult.m_isForceUpdate ? "true" : "false",
		 updateResult.m_newPrismVersion.toUtf8().constData(), updateResult.m_AppInstallFileUrl.toUtf8().constData());
}

QString PLSLoginDataHandler::getVersionFromFileUrl(const QString &updateUrl)
{
	QUrl url(updateUrl);
	auto fileName = url.fileName();
	PLS_INFO(UPDATE_MODULE, "get update url file name = %s", fileName.toUtf8().constData());
	QRegularExpression re("\\d+\\.\\d+\\.\\d+\\.\\d+");
	QRegularExpressionMatch match = re.match(fileName);
	return match.hasMatch() ? match.captured() : "";
}

void PLSLoginDataHandler::getCookieSuccessHandle(const std::function<void(bool ok, const QJsonObject &)> &callback, const QVariant &cookie, int statusCode, const QString &token,
						 const QJsonObject &docObj, const QString &loginName, qint32 recentKind)
{
	if (202 == statusCode && 10000 == docObj["code"].toInt()) {
		PLS_WARN(PLS_LOGIN_MODULE, "http respose error! user need agree term.");
		PLS_LOGEX(PLS_LOG_WARN, LAUNCHER_LOGIN, {{"prismLogin", "Google"}}, "Google http respose error! user need agree term.");
		PLSLoginMainView::instance()->raise();
		PLSLoginMainView::instance()->activateWindow();
		if (PLSTermsOfAgreeView::showTermsDialog(PLSLoginMainView::instance())) {
			PLS_INFO(PLS_LOGIN_MODULE, "user select agree term.");
			google_regeist_handler(cookie, token, callback, loginName, recentKind);
		} else {
			PLS_WARN(PLS_LOGIN_MODULE, "user select donot agree term.");
			pls_invoke_safe(callback, false, QJsonObject());
		}
	} else if (200 == statusCode) {
		savePrismUserInfo(docObj, cookie, false, loginName, recentKind);
		pls_invoke_safe(callback, true, QJsonObject());
	} else {
		PLS_WARN(PLS_LOGIN_MODULE, "google platform get cookie api failed, status code = %d.", statusCode);
		pls_invoke_safe(callback, false, QJsonObject());
	}
}

void PLSLoginDataHandler::getUserInfoFromOldVersion(const QString &path) {}

static QString GetTime()
{
	QDateTime cdt(QDateTime::currentDateTime());
	cdt.setOffsetFromUtc(cdt.offsetFromUtc());
	return cdt.toString(Qt::ISODateWithMs);
}

QByteArray PLSLoginDataHandler::getActionLogInfo(const QString &event1, const QString &event2, const QString &event3, const QString &target) const
{
	QJsonObject obj;
	obj["eventAt"] = GetTime();
	obj["event1"] = event1;
	obj["event2"] = event2;
	obj["event3"] = event3;
	obj["targetId"] = target;
	obj["resourceId"] = GlobalVars::maskingLogUserID.c_str();
	QJsonArray array;
	array.push_back(obj);

	QJsonDocument doc;
	doc.setArray(array);

	return doc.toJson();
}

void PLSLoginDataHandler::showTermOfView(const QString &requestUrl, const QJsonObject &body, const QList<QNetworkCookie> &cookies, bool &isSuccess, QEventLoop &eventLoop, const QString &loginName,
					 qint32 recentKind)
{
	PLSLoginMainView::instance()->raise();
	PLSLoginMainView::instance()->activateWindow();
	pls_async_call_mt([this, &eventLoop, &isSuccess, cookies, body, requestUrl, loginName, recentKind]() {
		const QString prismLoginName = requestUrl.contains("whalespace") ? pls_launcher_const::PLS_WHALESPACE_NAME : "";
		if (PLSTermsOfAgreeView::showTermsDialog(PLSLoginMainView::instance(), prismLoginName)) {
			PLS_INFO(PLS_LOGIN_MODULE, "user select agree term.");
			QString url = QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay()).arg(pls_launcher_const::SNS_LOGIN_SIGNUP_URL);

			QJsonObject data(body);
			if (loginName == NCB2B || loginName == APPLE_ID || loginName == TWITCH || loginName == FACEBOOK) {
				data.insert("agreement", true);
				url = requestUrl;
			}
			requestPrivacy(url, data, QVariant::fromValue<QList<QNetworkCookie>>(cookies), isSuccess, eventLoop, loginName, recentKind);
		} else {
			PLS_WARN(PLS_LOGIN_MODULE, "user select donot agree term.");
			eventLoop.quit();
		}
	});
}

void PLSLoginDataHandler::requestPrivacy(const QString &url, const QJsonObject &body, const QVariant &cookies, bool &isSuccess, QEventLoop &eventLoop, const QString &loginName, qint32 recentKind)
{
	QPointer<QEventLoop> eventLoopTmp(&eventLoop);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post)                    //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .header(QNetworkRequest::CookieHeader, cookies)
				   .jsonContentType()
				   .body(body)
				   .withLog() //
				   .receiver(eventLoopTmp)
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .checkResult([](const pls::http::Reply &reply) { return HTTP_STATUS_CODE_200 == reply.statusCode(); })
				   .jsonOkResult([this, &isSuccess, eventLoopTmp, loginName, recentKind](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   pls_async_invoke(eventLoopTmp,
							    [this, &isSuccess, eventLoopTmp, data = doc.object(), header = reply.header(QNetworkRequest::SetCookieHeader), loginName, recentKind]() {
								    savePrismUserInfo(data, header, true, loginName, recentKind);
								    PLS_INFO(LAUNCHER_LOGIN, "get prism user info success!");
								    isSuccess = true;
								    eventLoopTmp->quit();
							    });
				   })
				   .jsonFailResult([&isSuccess, eventLoopTmp, this, loginName](const pls::http::Reply &reply, const QJsonDocument &doc) {
					   auto statusCode = reply.statusCode();
					   auto obj = doc.object();
					   auto hasCode = obj.contains("code");
					   auto code = obj.value("code").toInt();
					   auto message = obj.value("message").toString();

					   if (500 == statusCode || 202 == statusCode || 501 == statusCode) {
						   PLS_ERROR(LAUNCHER_LOGIN, "get prism user info error code =%d, messgae = %s", code, message.toUtf8().constData());
					   } else {
						   PLS_ERROR(LAUNCHER_LOGIN, "prism login failed");
					   }

					   if (loginName == NCB2B || loginName == APPLE_ID) {
						   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
						   extraData.errPhase = PLSErrPhaseLogin;
						   PLSErrorHandler::showAlert({reply.statusCode(), reply.error(), reply.data()}, NCB2B, "DEFAULT_PRISMLoginFailedAgain", extraData);
					   }
					   isSuccess = false;
					   pls_async_invoke(eventLoopTmp, [eventLoopTmp]() { eventLoopTmp->quit(); });
				   }));
}

QString PLSLoginDataHandler::getLocalGpopData(const QString &appLocalGpopPath, QJsonDocument &doc, int &version)
{
	QJsonDocument appDatadoc;
	QJsonDocument userDataDoc;
	int appDatagpopVersion = 0;
	int userDataGpopVersion = 0;
	auto retFuc = [](QJsonDocument &tmpDoc, int &tmpVersion, const QString &path, const QString &dataPosition) {
		bool isSuccess = pls_read_json(tmpDoc, path);
		int version = 0;
		if (isSuccess) {
			tmpVersion = tmpDoc.object().value("optional").toObject().value("common").toObject().value("version").toInt();
			PLS_INFO("PLSGpopData", "read %s gpop data success, version = %d", dataPosition.toUtf8().constData(), tmpVersion);
		} else {
			PLS_INFO("PLSGpopData", "read %s gpop data failed", dataPosition.toUtf8().constData());
		}
	};
	retFuc(appDatadoc, appDatagpopVersion, appLocalGpopPath, "app");
	QString gpopJsonPath = pls_get_app_user_data_file_path_pn(QStringLiteral("/user/gpop_%1.json").arg(PLSLoginFunc::getPrismVersion()));
	retFuc(userDataDoc, userDataGpopVersion, gpopJsonPath, "user");

	if (userDataGpopVersion >= appDatagpopVersion) {
		doc = userDataDoc;
		version = userDataGpopVersion;
		PLS_INFO("PLSGpopData", "local gpop data from user dir, version = %d", userDataGpopVersion);
	} else {
		doc = appDatadoc;
		version = appDatagpopVersion;
		PLS_INFO("PLSGpopData", "local gpop data from app dir,version = %d", appDatagpopVersion);
	}
	return userDataDoc.isEmpty() ? "" : PLSLoginFunc::getPrismVersion();
}

void PLSLoginDataHandler::initCustomChannelObj()
{
	auto tmpServiceName = PLSLOGINUSERINFO->getNCPPlatformServiceName().toUtf8().toBase64().replace('/', '-');
	if (tmpServiceName.isEmpty()) {
		return;
	}
	QJsonObject chatObj = {
		{"offNormal", QString("images/chat/btn-tab-%1-off-normal.png").arg(tmpServiceName)}, {"offHover", QString("images/chat/btn-tab-%1-off-over.png").arg(tmpServiceName)},
		{"offClick", QString("images/chat/btn-tab-%1-off-click.png").arg(tmpServiceName)},   {"offDisable", QString("images/chat/btn-tab-%1-off-disable.png").arg(tmpServiceName)},
		{"onNormal", QString("images/chat/btn-tab-%1-on-normal.png").arg(tmpServiceName)},   {"onHover", QString("images/chat/btn-tab-%1-on-over.png").arg(tmpServiceName)},
		{"onClick", QString("images/chat/btn-tab-%1-on-click.png").arg(tmpServiceName)},     {"webIcon", QString("images/chat/web-%1.png").arg(tmpServiceName)}};

	m_serviceResLocalObj = {{"platform", NCB2B},
				{"name", PLSLOGINUSERINFO->getNCPPlatformServiceName()},
				{"serviceName", PLSLOGINUSERINFO->getNCPPlatformServiceName()},
				{"tagIcon", QString("images/B2B_%1_tagIcon.png").arg(tmpServiceName)},
				{"dashboardButtonIcon", QString("images/B2B_%1_addch_logo.png").arg(tmpServiceName)},
				{"addChannelButtonIcon", QString("images/B2B_%1_addch_logo_off.png").arg(tmpServiceName)},
				{"addChannelButtonConnectedIcon", QString("images/B2B_%1_addch_logo_on.png").arg(tmpServiceName)},
				{"channelSettingBigIcon", QString("images/B2B_%1_addch_logo_large.png").arg(tmpServiceName)},
				{"chatIcon", chatObj}};
}

void PLSLoginDataHandler::downloadNCB2BServiceRes(bool bRetry)
{
	if (m_NCB2BServiceConfigObj.isEmpty() && !bRetry) {
		return;
	}

	m_urlAndHowSaves.clear();
	auto filePath = [](const QString &fileName) -> QString {
		auto path = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH) + "../ncp_service_res/%1";
		return path.arg(fileName);
	};
	auto serviceId = m_NCB2BServiceConfigObj.value("serviceId").toString();

	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceLogoWithBackgroundPath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_logo")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceLogoWithBackgroundPath").toString()));
	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceLogoWithoutBackgroundPath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_nb_logo")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceLogoWithoutBackgroundPath").toString()));
	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceLogoForPcColorPath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_big_color_logo")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceLogoForPcColorPath").toString()));
	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceLogoForPcWhitePath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_big_white_logo")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceLogoForPcWhitePath").toString()));
	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceOutroPath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_outro")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceOutroPath").toString()));
	m_urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave()
					   .url(m_NCB2BServiceConfigObj.value("serviceWatermarkPath").toString())
					   .filePath(filePath(QString("%1_%2.png").arg(serviceId).arg("service_watermark")))
					   .keyPrefix(m_NCB2BServiceConfigObj.value("serviceWatermarkPath").toString()));

	auto downResult = [this, bRetry](const std::list<pls::rsm::DownloadResult> &results) {
		for (auto &res : results) {
			auto url = res.m_urlAndHowSave.url().toString().toUtf8();
			if (res.hasFilePath()) {
				PLS_INFO("PLSNCB2BServiceRes", "b2b service res down success, res = %s", url.constData());
			} else {
				PLS_ERROR("PLSNCB2BServiceRes", "b2b service res down failed, res = %s", url.constData());
			}
		}
		if (!bRetry) {
			pls_async_call_mt([this]() {
				pls_mkdir(pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH) + "images");
				pls_mkdir(pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH) + "images/chat");

				handleB2BServiceLogowithBG();
				handleB2BServiceLogoNBG();
				handleB2BServiceBigLogo();
				handleB2BServiceBigLogowithColor();
				emit updateNCB2BIcon();
			});
		}
	};
	pls::rsm::getDownloader()->download(m_urlAndHowSaves, this, downResult);
}

void PLSLoginDataHandler::reDownloadWaterMark()
{
	getNCB2BServiceResFromRemote([this](const QJsonObject &data) { downloadNCB2BServiceRes(); }, nullptr, this);
}

bool PLSLoginDataHandler::isNeedShowB2BServiceAlert()
{
	return m_isNeedShowB2BDisableAlert;
}

void PLSLoginDataHandler::getNCB2BServiceResFromRemote(const std::function<void(const QJsonObject &data)> &successCallback,
						       const std::function<void(const QJsonObject &data, const PLSErrorHandler::RetData &retData)> &failCallback, QObject *receiver)
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	if (serviceId.isEmpty()) {
		PLS_ERROR(LAUNCHER_LOGIN, "service id is empty");
		pls_async_call_mt([failCallback]() { failCallback(QJsonObject(), {}); });
		return;
	}
	auto serviceConfigUrl = QString(PRISM_NCP_SERVICE_CONFIG_API).arg(PRISM_SSL).arg(serviceId);

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                                  //
				   .hmacUrl(serviceConfigUrl, pls_http_api_func::getPrismHamcKey()) //
				   .cookie(PLSLoginUserInfo::getInstance()->getPrismCookie())
				   .withLog()          //
				   .receiver(receiver) //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([receiver, this, successCallback](const pls::http::Reply &reply, const QJsonObject &obj) {
					   pls_async_call_mt(receiver, [this, successCallback, obj]() {
						   m_NCB2BServiceConfigObj = obj;
						   pls_invoke_safe(successCallback, obj);
					   });
				   })
				   .failResult([this, receiver, failCallback](const pls::http::Reply &reply) {
					   QJsonObject data = QJsonDocument::fromJson(reply.data()).object();
					   PLSErrorHandler::ExtraData extraData(reply.request().originalUrl().path());
					   auto retData = PLSErrorHandler::getAlertString({reply.statusCode(), reply.error(), reply.data()}, NCB2B, "", extraData);
					   pls_async_call_mt(receiver, [failCallback, data, retData]() { pls_invoke_safe(failCallback, data, retData); });
					   PLS_ERROR(LAUNCHER_LOGIN, "get ncp service data failed.");
				   }));
}

QString PLSLoginDataHandler::getNCB2BLogoUrl()
{
	if (m_NCB2BServiceConfigObj.isEmpty()) {
		PLS_WARN("PLSNCB2BServiceRes", "m_NCB2BServiceConfigObj is null");
		return QString();
	}
	return m_NCB2BServiceConfigObj.value("serviceLogoWithBackgroundPath").toString();
}

void PLSLoginDataHandler::handleB2BServiceLogowithBG()
{
	auto policyImagePath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH);
	auto serviceLogo = getNCB2BServiceLogo();
	if (!QFile::exists(serviceLogo)) {
		return;
	}
	QImage original;
	original.load(serviceLogo);
	auto targetPixMap = scaleAndCrop(original, QSize(34, 34));
	auto pixmap = QPixmap::fromImage(targetPixMap);
	pls_shared_circle_mask_image(pixmap);
	bool isSuccess = pixmap.save(policyImagePath + m_serviceResLocalObj.value("tagIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save tagIcon is %s", isSuccess ? "true" : "false");
}

void PLSLoginDataHandler::handleB2BServiceLogoNBG()
{
	auto policyImagePath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH);
	auto serviceLogo = getNCB2BServiceNBLogo();
	if (!QFile::exists(serviceLogo)) {
		return;
	}
	QImage original;
	original.load(serviceLogo);

	bool isSuccess = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("offNormal").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon offNormal is %s", isSuccess ? "true" : "false");

	bool isSuccessOffHover = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("offHover").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon offHover is %s", isSuccessOffHover ? "true" : "false");

	bool isSuccessOffClick = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("offClick").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon offClick is %s", isSuccessOffClick ? "true" : "false");

	bool isSuccessOffdisable = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("offDisable").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon offDisable is %s", isSuccessOffdisable ? "true" : "false");

	bool isSuccessOnNormal = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("onNormal").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon onNormal is %s", isSuccessOnNormal ? "true" : "false");

	bool isSuccessOnHover = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("onHover").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon onHover is %s", isSuccessOnHover ? "true" : "false");

	bool isSuccessOnClick = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("onClick").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon onClick is %s", isSuccessOnClick ? "true" : "false");

	bool isSuccessWeb = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("webIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon webIcon is %s", isSuccessWeb ? "true" : "false");
}

void darkenImage(QImage &image, qreal factor)
{
	for (int y = 0; y < image.height(); ++y) {
		for (int x = 0; x < image.width(); ++x) {
			QRgb pixel = image.pixel(x, y);
			int alpha = qAlpha(pixel) * factor;
			image.setPixel(x, y, qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), alpha));
		}
	}
}

void PLSLoginDataHandler::handleB2BServiceBigLogo()
{
	auto policyImagePath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH);
	auto serviceLogo = getNCB2BServiceWhiteLogo();
	if (!QFile::exists(serviceLogo)) {
		return;
	}
	QImage original;
	original.load(serviceLogo);

	auto dashboardPixMap = scaleAndCrop(original, QSize(95, 33));
	darkenImage(dashboardPixMap, 0.68);
	bool isSuccess = dashboardPixMap.save(policyImagePath + m_serviceResLocalObj.value("dashboardButtonIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save dashboardButtonIcon is %s", isSuccess ? "true" : "false");

	auto addChannelPixMap = scaleAndCrop(original, QSize(115, 40));
	darkenImage(addChannelPixMap, 0.7);
	bool isSuccessAddChannel = addChannelPixMap.save(policyImagePath + m_serviceResLocalObj.value("addChannelButtonIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save addChannelButtonIcon is %s", isSuccessAddChannel ? "true" : "false");

	auto channelSettingPixMap = scaleAndCrop(original, QSize(170, 59));
	darkenImage(channelSettingPixMap, 0.68);
	bool isSuccessChannelSetting = channelSettingPixMap.save(policyImagePath + m_serviceResLocalObj.value("channelSettingBigIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save channelSettingBigIcon is %s", isSuccessChannelSetting ? "true" : "false");
}

void PLSLoginDataHandler::handleB2BServiceBigLogowithColor()
{
	auto policyImagePath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH);
	auto serviceLogo = getNCB2BServiceColorLogo();
	if (!QFile::exists(serviceLogo)) {
		return;
	}
	QImage original;
	original.load(serviceLogo);

	auto addChannelConnectedPixMap = scaleAndCrop(original, QSize(115, 40));
	bool isSuccess = addChannelConnectedPixMap.save(policyImagePath + m_serviceResLocalObj.value("addChannelButtonConnectedIcon").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save addChannelButtonConnectedIcon is %s", isSuccess ? "true" : "false");
}

void PLSLoginDataHandler::getAppleIDAgreementParams(QJsonObject &agreementParams, const QJsonObject &data)
{
	for (auto inter = data.constBegin(); inter != data.constEnd(); ++inter) {
		agreementParams.insert(inter.key(), inter.value());
	}
}

QJsonObject PLSLoginDataHandler::getSNSLoginParams(const QString &loginName)
{
	QJsonObject params;
	if (loginName == NCB2B || loginName == TWITCH || loginName == FACEBOOK) {
		params.insert("accessToken", m_snsAccessTokenObj.value("access_token").toString());
		params.insert("serviceId", m_ncpServiceId);
		params.insert("snsCd", loginName.toLower());
		params.insert("agreement", false);
	} else if (loginName == APPLE_ID) {
		params.insert("snsCode", m_snsCode);
		params.insert("snsCallbackUrl", APPLE_ID_REDIRECT_URI);
		params.insert("snsCd", "apple");
	} else {
		params.insert("snsCd", loginName.toLower());
		params.insert("agreement", false);
	}
	return params;
}

QImage PLSLoginDataHandler::scaleAndCrop(const QImage &original, const QSize &originTargetSize)
{
	QSize targetSize = originTargetSize * 3;
	QSize originalSize = original.size();
	double imageWHA = double(originalSize.width()) / double(originalSize.height());
	double thumbnailWHA = double(targetSize.width()) / double(targetSize.height());
	QImage targetPixmap = (imageWHA < thumbnailWHA) ? original.scaledToWidth(targetSize.width(), Qt::SmoothTransformation) : original.scaledToHeight(targetSize.height(), Qt::SmoothTransformation);
	auto targetPixmapSize = targetPixmap.size();
	if (targetPixmapSize == targetSize)
		return targetPixmap;
	QImage pixmap = targetPixmap.copy((targetPixmapSize.width() - targetSize.width()) / 2, (targetPixmapSize.height() - targetSize.height()) / 2, targetSize.width(), targetSize.height());
	return pixmap;
}

QUrl PLSLoginDataHandler::getSnsAuthUrl(const QString &_url, const QString &clientId, const QString &secret, const QString &redirectUri, const QString &scope, const QString &auth_type,
					const QString &loginName)
{
	QUrl url(_url);
	QUrlQuery query;
	query.addQueryItem(HTTP_CLIENT_ID, clientId);
	query.addQueryItem(HTTP_REDIRECT_URI, redirectUri);
	query.addQueryItem("response_type", "code");
	query.addQueryItem("auth_type", auth_type);
	query.addQueryItem("scope", scope);
	query.addQueryItem("force_verify", "true");
	query.addQueryItem(HTTP_CLIENT_SECRET, secret);
	if (loginName == APPLE_ID) {
		query.addQueryItem("response_mode", "form_post");
		query.addQueryItem("state", "RANDOM_STRING");
	}
	url.setQuery(query);
	return url;
}

bool PLSLoginDataHandler::getSNSAccessToken(const QString &urlStr, const QString &clientId, const QString &secret, const QString redirectUri, const QString &code, const QString &loginName)
{
	bool isSuccess = false;
	QEventLoop eventloop;
	QVariantMap parameters;
	parameters[HTTP_CODE] = code;
	parameters[HTTP_CLIENT_ID] = clientId;
	parameters[HTTP_REDIRECT_URI] = redirectUri;
	parameters[HTTP_CLIENT_SECRET] = secret;
	parameters[HTTP_GRANT_TYPE] = HTTP_AUTHORIZATION_CODE;

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Post) //
				   .url(urlStr)
				   .urlParams(parameters)
				   .receiver(&eventloop) //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([this, &eventloop, &isSuccess, loginName](const pls::http::Reply &reply, const QJsonObject &obj) {
					   PLS_INFO(LAUNCHER_LOGIN, "get %s token success.", loginName.toUtf8().constData());
					   pls_async_invoke(&eventloop, [this, &isSuccess, &eventloop, data = obj]() {
						   m_snsAccessTokenObj = data;
						   PLSLoginUserInfo::getInstance()->updateNCB2BTokenInfo(m_snsAccessTokenObj.value("access_token").toString(),
													 m_snsAccessTokenObj.value("refresh_token").toString(),
													 m_snsAccessTokenObj.value("expires_in").toInt() + QDateTime::currentSecsSinceEpoch(),
													 m_snsAccessTokenObj.value("token_type").toString());
						   isSuccess = true;
						   eventloop.quit();
					   });
				   })
				   .failResult([&eventloop, &isSuccess, this, loginName](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get %s token failed.", loginName.toUtf8().constData());
					   isSuccess = false;
					   auto data = reply.data();
					   auto statusCode = reply.statusCode();
					   auto error = reply.error();
					   auto urlPath = reply.request().originalUrl().path();
					   ncpErrorHandler(statusCode, data, error, urlPath, loginName);
					   eventloop.quit();
				   }));
	eventloop.exec();
	return isSuccess;
}

bool PLSLoginDataHandler::isSupportAutoChannelLogins()
{
	QStringList supportAutoChannels = {TWITCH, FACEBOOK /*, "Naver"*/};
	auto prismLoginName = PLSLOGINUSERINFO->getLoginPlatformName();
	for (auto channelName : supportAutoChannels) {
		if (prismLoginName.compare(channelName, Qt::CaseInsensitive) == 0)
			return true;
	}
	return false;
}

QJsonObject &PLSLoginDataHandler::getCustomChannelObj()
{
	return m_serviceResLocalObj;
}

void PLSLoginDataHandler::updateDownloadFailed() const
{
	pls_check_app_exiting();
	PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_UPDATE_DOWNLOAD_FAILED, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSLoginDataHandler::updateDownloadFailed"),
					      PLSLoginMainView::instance());
}

bool PLSLoginDataHandler::saveThumbnail(const QPixmap &pixmap, const QString &filePath) const
{
	QByteArray data;
	QBuffer buffer(&data);
	buffer.open(QIODevice::WriteOnly);
	if (!pixmap.save(&buffer, "PNG"))
		return false;
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	file.write(data.toBase64());
	file.close();
	return true;
}
