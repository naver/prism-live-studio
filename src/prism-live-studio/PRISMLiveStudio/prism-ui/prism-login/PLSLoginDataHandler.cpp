#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif
#include "PLSLoginDataHandler.h"
#include "libhttp-client.h"
#include "PLSResCommonFuns.h"
#include <qeventloop.h>
#include <qmetaobject.h>
#include <QUrlQuery>
#include <qapplication.h>
#include <qdir.h>
#include "liblog.h"
#include "PLSAlertView.h"
#include <qbuffer.h>
#include <qimagereader.h>
#include "pls-shared-values.h"
#include "pls-shared-functions.h"
#include "libutils-api.h"
#include <QDir>
#include "pls-common-define.hpp"
#include <qsettings.h>
#include <limits.h>
#include "pls-gpop-data.hpp"
#include "login-user-info.hpp"
#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "pls-gpop-data.hpp"
#include <QVersionNumber>
#include "pls-notice-handler.hpp"
#include "obs-app.hpp"
#include <log/log.h>
#include "ChannelCommonFunctions.h"
#include "PLSNCB2BError.h"

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
bool isHasUpdate(const QString &remoteVersionFromApi, const QString &remoteVersionFromFileUrl)
{
	auto remoteVersionFromApiNum = QVersionNumber::fromString(remoteVersionFromApi);
	auto remoteVersionFromFileUrlNum = QVersionNumber::fromString(remoteVersionFromFileUrl);
	bool isOk = remoteVersionFromApiNum.isPrefixOf(remoteVersionFromFileUrlNum);
	if (!isOk) {
		PLS_ERROR("AppUpdate", "remoteVersionFromApi = %s, remoteVersionFromFileUrl = %s match is %s", remoteVersionFromApi.toUtf8().constData(), remoteVersionFromFileUrl.toUtf8().constData(),
			  pls_bool_2_string(isOk));
	}

	return isOk ? QVersionNumber::fromString(PLSLoginFunc::getPrismVersionWithBuild()) < remoteVersionFromFileUrlNum : false;
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

QVariantMap PLSLoginDataHandler::getRequestApiDefaultHeader(bool hasGcc) const
{
	QVariantMap headMap;
	if (hasGcc) {
		headMap[pls_launcher_const::HEADER_PRISM_GCC] = GlobalVars::gcc.c_str();
	}
#if defined(Q_OS_WIN)
	pls_win_ver_t ver = pls_get_win_ver();
#elif defined(Q_OS_MACOS)
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
#endif
	headMap[pls_launcher_const::HEADER_PRISM_LANGUAGE] = QString(pls_prism_get_locale());
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

void PLSLoginDataHandler::getAppInitDataFromRemote(const std::function<void()> &callback)
{
	QEventLoop loop;
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

	QUrl updateUrl(LASTEST_UPDATE_URL.arg(PLSGpopData::instance()->getConnection().ssl));

	QUrlQuery query;
	query.addQueryItem(PARAM_PLATFORM_TYPE, PLATFORM_TYPE);
	query.addQueryItem(PARAM_APP_TYPE_KEY, PARAM_APP_TYPE_VALUE);
	updateUrl.setQuery(query);

	QString noticeUrl(PLS_NOTICE_URL.arg(PLSGpopData::instance()->getConnection().ssl));
	pls::http::Requests requests;
	requests.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .jsonContentType() //
			     .withLog()         //
			     .receiver(&loop)   //
			     .hmacUrl(qpopUrl, pls_http_api_func::getPrismHamcKey())
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .jsonOkResult([this, gpopPath, localGpopVersion, localGpopDoc, gpopNameVersionStr](const pls::http::Reply &, const QJsonDocument &doc) {
				     auto version = doc.object().value("optional").toObject().value("common").toObject().value("version").toInt();
				     auto data = doc.toJson();
				     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data jsonOk request, gpop version is %d , software version is %s", version,
					      PLSLoginFunc::getPrismVersion().toUtf8().constData());
				     if (localGpopVersion > version && gpopNameVersionStr == PLSLoginFunc::getPrismVersion()) {
					     data = localGpopDoc.toJson();
					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data read from app folder, because the localGpopVersion version is %d , request version is %d isn't  same",
						      localGpopVersion, version);
				     } else {
					     bool isSuccess =
						     pls_write_json(pls_get_app_data_dir(QStringLiteral("PRISMLiveStudio/user/")) + QString("gpop_%1.json").arg(PLSLoginFunc::getPrismVersion()), doc);
					     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data read from remote api, localGpopVersion = %d, version = %d. save is %s", localGpopVersion, version,
						      isSuccess ? "success" : "failed");
				     }
				     PLSGpopData::instance()->getGpopData(data);
				     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data save file, data length is %d byte", data.length());
			     })
			     .failResult([this, gpopPath, localGpopDoc](const pls::http::Reply &) {
				     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: gpop data request json request failed.");
				     auto data = localGpopDoc.toJson();
				     PLSGpopData::instance()->getGpopData(data);

				     PLS_INFO("PLSGpopData", "GPOP DATA STATUS: read local gpop  file, data length is %d", data.length());
			     }))
		.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .jsonContentType() //
			     .withLog()         //
			     .receiver(&loop)   //
			     .hmacUrl(initUrl, pls_http_api_func::getPrismHamcKey())
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .jsonOkResult([this](const pls::http::Reply &, const QJsonDocument &doc) {
				     pls_async_call_mt(this, [this, doc]() {
					     PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: app init is jsonOk request.");
					     initApiSuccessHandle(doc);
				     });
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
			     .receiver(&loop)
			     .jsonOkResult([this](const pls::http::Reply &, const QJsonDocument &doc) { updateApiSuccessHandle(doc); })
			     .failResult([this](const pls::http::Reply &reply) {
				     QByteArray array = reply.data();
				     auto statusCode = reply.statusCode();
				     pls_async_call_mt(this, [this, statusCode, array] {
					     if (common::HTTP_STATUS_CODE_404 == statusCode) {
						     m_updateResult = AppUpdateResult::AppNoUpdate;
						     PLS_INFO(UPDATE_MODULE, "UPDATE STATUS: request appversion api no update available");
					     } else if (getPrismApiError(array, statusCode) == PRISM_API_ERROR::SystemExccedTimeLimitError) {
						     m_updateResult = AppUpdateResult::AppHMacExceedTimeLimit;
						     PLS_ERROR(UPDATE_MODULE, "UPDATE STATUS: request update appversion api failed, hmac time exceed limit");
					     } else {
						     PLS_ERROR(UPDATE_MODULE, "UPDATE STATUS: request update appversion api failed, status code: %d", statusCode);
					     }
				     });
			     }))
		.add(pls::http::Request()
			     .method(pls::http::Method::Get)                           //
			     .hmacUrl(noticeUrl, pls_http_api_func::getPrismHamcKey()) //
			     .jsonContentType()
			     .rawHeader(common::HTTP_HEAD_CC_TYPE, pls_prism_get_locale().section('-', 1, 1))
			     .rawHeader("X-prism-apptype", QString("LIVE_STUDIO"))
			     .withLog()
			     .receiver(&loop)
			     .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
			     .okResult([this](const pls::http::Reply &reply) {
				     auto data = reply.data();
				     pls_async_call_mt(this, [data]() {
					     PLS_INFO(PLS_LOGIN_MODULE, "get notice info success.");
					     PLSNoticeHandler::getInstance()->saveNoticeInfo(data);
				     });
			     })
			     .failResult([this](const pls::http::Reply &reply) {
				     auto statusCode = reply.statusCode();
				     pls_async_call_mt(this, [statusCode]() {
					     if (statusCode == 404) {
						     PLS_INFO(PLS_LOGIN_MODULE, "get notice is empty.");
					     } else {
						     PLS_ERROR(PLS_LOGIN_MODULE, "get notice info failed.");
					     }
				     });
			     }))
		.add(pls::http::Request()
			     .method(pls::http::Method::Get)
			     .jsonContentType()       //
			     .withLog()               //
			     .receiver(&loop)         //
			     .url(TWITCH_API_INGESTS) //
			     .timeout(PRISM_NET_REQUEST_TIMEOUT)
			     .objectOkResult([this](const pls::http::Reply &reply, const QJsonObject &jsonObject) {
				     m_twitchServiceListObj = jsonObject;
				     auto servicePath = pls_get_user_path("PRISMLiveStudio/plugin_config/rtmp-services/twitch_ingests.json");
				     pls_write_json(servicePath, jsonObject);
			     })
			     .failResult([this](const pls::http::Reply &reply) {
				     auto statusCode = reply.statusCode();
				     auto errorData = reply.data();
				     auto servicePath = pls_get_user_path("PRISMLiveStudio/plugin_config/rtmp-services/twitch_ingests.json");
				     bool isSuccess = pls_read_json(m_twitchServiceListObj, servicePath);
				     PLS_ERROR(PLS_LOGIN_MODULE, "get twitch service info failed.statusCode = %d, errorData = %s; read local twitch service json is %s", statusCode,
					       errorData.constData(), isSuccess ? "success" : "falied");
			     }));

	QPointer<QEventLoop> tmpLoop(&loop);
	pls::http::requests(requests.results([callback, tmpLoop, this](const pls::http::Replies &) {
		pls_async_call_mt(qApp, [callback, tmpLoop, this]() {
			PLS_INFO("PLSLoginDataHandler", "getAppInitDataFromRemote request finished");

			auto updateInfoFormRegister = getUpdateFromRegister();
			if (!updateInfoFormRegister.isEmpty()) {
				PLS_INFO("PLSLoginDataHandler", "update info from register");
				updateApiSuccessHandle(QJsonDocument::fromJson(updateInfoFormRegister.toUtf8()));
			}

			if (callback != nullptr)
				callback();
		});
		if (pls_object_is_valid(tmpLoop)) {
			tmpLoop->quit();
		}
	}));
	loop.exec();
}

bool PLSLoginDataHandler::getPrismUserInfoFromRemote(const QList<QNetworkCookie> &cookies, const QString &requestUrl, pls::http::Method httpMethod)
{
	return false;
}

QString PLSLoginDataHandler::getInstallFileUrl() const
{
	return m_AppInstallFileUrl;
}

bool PLSLoginDataHandler::isForcePrismAppUpdate() const
{
	return m_isForceUpdate;
}

QString PLSLoginDataHandler::getUpdateVersion() const
{
	return m_newPrismVersion;
}

QString PLSLoginDataHandler::getUpdateInfoUrl() const
{
	return m_updateInfoUrl;
}

AppUpdateResult PLSLoginDataHandler::getUpdateResult() const
{
	return m_updateResult;
}

void PLSLoginDataHandler::startDownloadNewPackage(const downloadProgressCallback &callback, const QString &installFileUrl, const QString &gcc)
{
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

void PLSLoginDataHandler::refreshPrismToken()
{

	QUrl url(QString("%1%2").arg(pls_http_api_func::getPrismAuthGateWay()).arg(pls_launcher_const::PRISM_TOKEN_URL));
	if (PLSLoginUserInfo::getInstance()->getToken().isEmpty()) {
		m_isExistUserInfo = false;
		PLS_INFO(LAUNCHER_LOGIN, "prism token is invalid need login");
	} else {
		QEventLoop loop;
		m_isExistUserInfo = true;
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .cookie(PLSLoginUserInfo::getInstance()->getPrismCookie())
					   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
					   .workInMainThread()                                 //
					   .withLog()                                          //
					   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
					   .receiver(&loop)
					   .jsonOkResult([this, &loop](const pls::http::Reply &reply, const QJsonDocument &doc) {
						   PLS_INFO(LAUNCHER_LOGIN, "prism token refresh success");
						   m_isPrismTokenValid = true;
						   auto sessionCookie = reply.header(QNetworkRequest::SetCookieHeader);
						   auto obj = doc.object();
						   PLSLoginUserInfo::getInstance()->setSessionTokenAndCookie(obj, sessionCookie);
						   loop.quit();
					   })
					   .failResult([this, &loop](const pls::http::Reply &reply) {
						   if (reply.statusCode() == 401) {
							   PLS_ERROR(LAUNCHER_LOGIN, "prism token refresh failed, need login");
							   m_isPrismTokenValid = false;
							   PLSLoginUserInfo::getInstance()->clearPrismLoginInfo();
						   } else {
							   PLS_INFO(LAUNCHER_LOGIN, "prism token refresh failed,but not experied, not need login");
							   m_isPrismTokenValid = true;
						   }
						   loop.quit();
					   }));
		loop.exec();
	}
}
static void ncpErrorHandler(int statusCode, bool hasCode, int code, const QString &message)
{
	
}
void PLSLoginDataHandler::getNCPServiceId(const QString &serviceName, const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback)
{
	PLS_INFO(LAUNCHER_LOGIN, "start request ncp service id");
	m_serviceName = serviceName;
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                                                                                                   //
				   .hmacUrl(PRISM_NCP_SERVICE_ID_API.arg(PRISM_SSL).arg(QUrl::toPercentEncoding(serviceName)), pls_http_api_func::getPrismHamcKey()) //
				   .withLog()                                                                                                                        //
				   .receiver(this)                                                                                                                   //
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .okResult([this, callback, failedCallback](const pls::http::Reply &reply) {
					   m_ncpServiceId = reply.data();
					   getNCPAuthUrl(callback, failedCallback);
				   })
				   .failResult([this, failedCallback](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp serviceid failed.");
					   auto objData = QJsonDocument::fromJson(reply.data()).object();
					   bool hasCode = objData.contains("code");
					   auto code = objData.value("code").toInt();
					   auto errorCode = objData.value("errorCode").toString();
					   auto statusCode = reply.statusCode();
					   auto message = objData.value("message").toString();
					   if (!errorCode.isEmpty() && objData.value("code").isUndefined()) {
						   code = errorCode.toInt();
						   PLS_ERROR(LAUNCHER_LOGIN, "code = %d, errorCode = %s", code, errorCode.toUtf8().constData());
					   }
					   pls_async_call_mt(this, [code, statusCode, failedCallback, hasCode, message]() {
						   ncpErrorHandler(statusCode, hasCode, code, message);

						   if (failedCallback)
							   failedCallback(statusCode, code);
					   });
				   }));
}
void PLSLoginDataHandler::getNCPAuthUrl(const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback)
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
				   .failResult([this, failedCallback](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp auth url failed.");
					   auto objData = QJsonDocument::fromJson(reply.data()).object();
					   bool hasCode = objData.contains("code");
					   auto message = objData.value("message").toString();

					   auto code = objData.value("code").toInt();
					   auto errorCode = objData.value("errorCode").toString();
					   auto statusCode = reply.statusCode();
					   if (!errorCode.isEmpty() && objData.value("code").isUndefined()) {
						   code = errorCode.toInt();
						   PLS_ERROR(LAUNCHER_LOGIN, "code = %d, errorCode = %s", code, errorCode.toUtf8().constData());
					   }

					   pls_async_call_mt(this, [code, statusCode, failedCallback, hasCode, message]() {
						   ncpErrorHandler(statusCode, hasCode, code, message);

						   if (failedCallback)
							   failedCallback(statusCode, code);
					   });
				   }));
}
bool PLSLoginDataHandler::getNCPAccessToken(const QString &url)
{
	QEventLoop eventloop;
	bool isSuccess = false;
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get)                     //
				   .hmacUrl(url, pls_http_api_func::getPrismHamcKey()) //
				   .withLog()                                          //
				   .receiver(&eventloop)                               //
				   .workInMainThread()
				   .timeout(pls_launcher_const::HTTP_REQUEST_TIME_OUT)
				   .objectOkResult([this, &eventloop, &isSuccess](const pls::http::Reply &reply, const QJsonObject &obj) {
					   m_ncpAccessTokenObj = obj;
					   PLSLoginUserInfo::getInstance()->updateNCB2BTokenInfo(m_ncpAccessTokenObj.value("access_token").toString(),
												 m_ncpAccessTokenObj.value("refresh_token").toString(),
												 m_ncpAccessTokenObj.value("expires_in").toInt() + QDateTime::currentSecsSinceEpoch());
					   isSuccess = true;
					   eventloop.quit();
				   })
				   .failResult([&eventloop, &isSuccess](const pls::http::Reply &reply) {
					   PLS_INFO(LAUNCHER_LOGIN, "get ncp token failed.");
					   isSuccess = false;
					   auto obj = QJsonDocument::fromJson(reply.data()).object();
					   auto statusCode = reply.statusCode();
					   auto hasCode = obj.contains("code");
					   auto code = obj.value("code").toInt();
					   auto message = obj.value("message").toString();
					   ncpErrorHandler(statusCode, hasCode, code, message);
					   eventloop.quit();
				   }));
	eventloop.exec();
	return isSuccess;
}

void PLSLoginDataHandler::setLoginName(const QString &loginName)
{
	m_loginName = loginName;
}

QString PLSLoginDataHandler::getLoginName() const
{
	return m_loginName;
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

const QJsonObject &PLSLoginDataHandler::getTwitchServiceList() const
{
	return m_twitchServiceListObj;
}

QList<QPair<QString, QString>> PLSLoginDataHandler::getTwitchServer() const
{
	QList<QPair<QString, QString>> services;
	auto ingests = m_twitchServiceListObj["ingests"].toArray();
	for (auto ingest : ingests) {
		auto name = ingest.toObject().value("name").toString();
		auto server = ingest.toObject().value("url_template").toString();
		auto lastIndex = server.lastIndexOf('/');
		auto serverUrl = server.left(lastIndex);
		services.append(QPair<QString, QString>(name, serverUrl));
	}
	if (services.size() > 0) {
		services.insert(0, QPair<QString, QString>(tr("setting.output.server.auto"), services.first().second));
	}
	return services;
}

QString PLSLoginDataHandler::getUpdateInfoUrl(const QJsonObject &updateInfoUrlList)
{
	if (pls_prism_get_locale() == "ko-KR") {
		return updateInfoUrlList.value(QStringLiteral("kr")).toString();
	} else {
		return updateInfoUrlList.value(QStringLiteral("en")).toString();
	}
}

void PLSLoginDataHandler::getGoogleCookie(const QString &loginToken, const std::function<void(bool ok, const QJsonObject &)> &callback) {}
template<typename Callback> void PLSLoginDataHandler::google_regeist_handler(const QVariant &replyCookie, const QString &token, const Callback &callback)
{

}
void PLSLoginDataHandler::pls_google_user_info(const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &redirect_uri, const QString &code)
{
	
}

void PLSLoginDataHandler::savePrismUserInfo(const QJsonObject &userInfo, const QVariant &neo_sesCookies)
{
	
}

void PLSLoginDataHandler::getPrismThumbnail(const std::function<void()> &callback)
{
}

QPixmap PLSLoginDataHandler::getCurrentThumbnail() const
{
	auto imagePath = PLSResCommonFuns::getUserSubPath(shared_values::prism_user_image_path);
	auto ret = loadThumbnail(imagePath);
	if (ret.isNull()) {
		imagePath = ":/images/img-profile-blank.svg";
		ret = pls_shared_paint_svg(imagePath, QSize(50, 50));
	}
	return ret;
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
	GlobalVars::gcc = object.value(QLatin1String("gcc")).toString().toStdString();

	PLS_INFO(UPDATE_MODULE, "APP INIT STATUS: request appInit api success, gcc: %s; hashUserCode:%s", GlobalVars::gcc.c_str(), GlobalVars::maskingLogUserID.c_str());

	pls_set_gcc(pls_is_empty(GlobalVars::gcc.c_str()) ? "KR" : GlobalVars::gcc.c_str());

	std::string currentEnvironment = pls_prism_is_dev() ? "Dev" : "Rel";
	PLS_INIT_INFO(MAINFRAME_MODULE, "version: " PLS_VERSION ", processId: %ld, prismSession: %s, currentRunEnvironment:%s, gcc:%s;", pls_current_process_id(), GlobalVars::prismSession.c_str(),
		      currentEnvironment.c_str(), GlobalVars::gcc.c_str());

	PLSLoginFunc::sendAction(getActionLogInfo(pls_launcher_const::EVENT_APP, pls_launcher_const::EVENT_APP_INIT, pls_launcher_const::EVENT_APP_INIT_RESULT_SUCCESS, ""));
}

void PLSLoginDataHandler::updateApiSuccessHandle(const QJsonDocument &doc)
{
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

void PLSLoginDataHandler::getCookieSuccessHandle(const std::function<void(bool ok, const QJsonObject &)> &callback, const pls::http::Reply &reply, const QJsonDocument &doc)
{
	
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

void PLSLoginDataHandler::showTermOfView(const QString &requestUrl, const QByteArray &body, const QList<QNetworkCookie> &cookies, bool &isSuccess, QEventLoop &eventLoop)
{
	
}

void PLSLoginDataHandler::requestPrivacy(const QString &url, const QByteArray &body, const QVariant &cookies, bool &isSuccess, QEventLoop &eventLoop)
{
	
}

QString PLSLoginDataHandler::getLocalGpopData(const QString &appLocalGpopPath, QJsonDocument &doc, int &version)
{
	return "";
}

void PLSLoginDataHandler::initCustomChannelObj()
{
	auto tmpServiceName = PLSLOGINUSERINFO->getNCPPlatformServiceName().toUtf8().toBase64().replace('/', '-');

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
	
}

void PLSLoginDataHandler::reDownloadWaterMark()
{
	getNCB2BServiceResFromRemote([this](const QJsonObject &data) { downloadNCB2BServiceRes(); }, nullptr, this);
}

bool PLSLoginDataHandler::isNeedShowB2BServiceAlert()
{
	return m_isNeedShowB2BDisableAlert;
}

void PLSLoginDataHandler::getNCB2BServiceResFromRemote(const std::function<void(const QJsonObject &data)> &successCallback, const std::function<void(const QJsonObject &data)> &failCallback,
						       QObject *receiver)
{
	auto serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	if (serviceId.isEmpty()) {
		PLS_ERROR(LAUNCHER_LOGIN, "service id is empty");
		pls_async_call_mt([failCallback]() { failCallback(QJsonObject()); });
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
					   pls_async_call_mt(receiver, [failCallback, data]() { pls_invoke_safe(failCallback, data); });
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

	bool isSuccessOnNoraml = original.save(policyImagePath + m_serviceResLocalObj.value("chatIcon").toObject().value("onNormal").toString(), "PNG");
	PLS_INFO("PLSNCB2BServiceRes", "save chatIcon onNormal is %s", isSuccessOnNoraml ? "true" : "false");

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

QJsonObject &PLSLoginDataHandler::getCustomChannelObj()
{
	return m_serviceResLocalObj;
}

void PLSLoginDataHandler::updateDownloadFailed() const
{
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
