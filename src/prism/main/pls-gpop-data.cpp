#include "pls-gpop-data.hpp"
#include "pls-gpop-data.hpp"
#include "pls-gpop-data.hpp"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"
#include "log/log.h"
#include "log/module_names.h"
#include "PLSPlatformApi/prism/PLSPlatformPrism.h"
#include "vlive/PLSAPIVLive.h"
#include "TextMotionTemplateDataHelper.h"
#include "ui-config.h"
#include <QDir>
#include "ChannelCommonFunctions.h"

#define PRISM_GPOP_PATH "PRISMLiveStudio/user/gpop.json"

PLSGpopData *PLSGpopData::instance()
{
	static PLSGpopData gpopdata;
	return &gpopdata;
}

void PLSGpopData::getGpopDataRequest()
{
	QEventLoop loop;
	connect(this, &PLSGpopData::finished, this, [&]() {
		loop.exit();
		initGpopData();
	});

	QString verstr = QString("%1.%2.%3").arg(PLS_RELEASE_CANDIDATE_MAJOR).arg(PLS_RELEASE_CANDIDATE_MINOR).arg(PLS_RELEASE_CANDIDATE_PATCH);
	m_gpopURL = QString(PLS_GPOP).append("&appVersion=").append(verstr);
	connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyResultData, this, &PLSGpopData::onReplyResultData);
	connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyErrorDataWithSatusCode, this, &PLSGpopData::onReplyErrorData);
	PLSNetworkAccessManager::getInstance()->createHttpRequest(PLSNetworkAccessManager::Operation::GetOperation, m_gpopURL, false, QVariantMap(), QVariantMap(), true, false);
	loop.exec();
}

const QByteArray PLSGpopData::getDefaultValuesOf(const QString &key)
{
	auto file = findFileInResources(ChannelData::defaultSourcePath, key);
	QByteArray data;
	PLSJsonDataHandler::getJsonArrayFromFile(data, file);
	return data;
}

PLSGpopData::PLSGpopData(QObject *parent) : QObject(parent)
{
	//connect(this, &PLSGpopData::finished, this, &PLSGpopData::initGpopData, Qt::QueuedConnection);
}

PLSGpopData::~PLSGpopData() {}

void PLSGpopData::getGpopDataLog(const QByteArray &data, const QString &version)
{
	QString logInfo;
	QVariantMap obj;
	QJsonDocument doc;
	PLSJsonDataHandler::getValuesFromByteArray(data, "connection", obj);
	doc.setObject(QJsonObject::fromVariantMap(obj));
	auto apisStr = doc.toJson();
	PLSJsonDataHandler::getValuesFromByteArray(data, "common", obj);
	doc.setObject(QJsonObject::fromVariantMap(obj));
	auto commStr = doc.toJson();
	logInfo = apisStr + commStr + QString("version:%1").arg(version);
	PLS_INFO("PLSGpopData", "gpopdata :%s", logInfo.toUtf8().constData());
}

void PLSGpopData::onReplyResultData(int statusCode, const QString &url, const QByteArray array)
{

	if (m_gpopURL == url && statusCode == HTTP_STATUS_CODE_200) {
		//add check version gpop
		QVariantMap value;
		QString version;
		PLSJsonDataHandler::getValuesFromByteArray(array, "win64", value);
		version = value.value("version").toString();
		getGpopDataLog(array, version);
		QString appVersion = QString("%1.%2.%3").arg(PLS_RELEASE_CANDIDATE_MAJOR).arg(PLS_RELEASE_CANDIDATE_MINOR).arg(PLS_RELEASE_CANDIDATE_PATCH);
		if (0 == appVersion.compare(version, Qt::CaseInsensitive)) {
			PLS_INIT_INFO("PLSGpopData", "gpop data update success.");
			m_gpopDataArray = array;
			PLSJsonDataHandler::saveJsonFile(array, pls_get_user_path(PRISM_GPOP_PATH));
		} else {
			PLS_INIT_WARN("PLSGpopData", "No matching version of gpop found from server");
			QDir appDir(qApp->applicationDirPath());
			QString gpopPath = appDir.absoluteFilePath("data/prism-studio/user/gpop.json");
			PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, gpopPath);
		}

		emit finished();
	}
}

void PLSGpopData::onReplyErrorData(int, const QString &url, const QString &, const QString &errorInfo)
{
	Q_UNUSED(url);
	Q_UNUSED(errorInfo);
	if (m_gpopURL == url) {
		PLS_INIT_ERROR("PLSGpopData", "get gpop data failed, errorMsg = %s", errorInfo.toUtf8().data());
		emit finished();
	}
}

Connection PLSGpopData::getConnection()
{
	if (m_connection.ssl.isEmpty()) {
		PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, pls_get_user_path(PRISM_GPOP_PATH));
		initConnection();
	}
	return m_connection;
}
Common PLSGpopData::getCommon()
{
	if (m_common.deviceType.isEmpty()) {
		PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, pls_get_user_path(PRISM_GPOP_PATH));
		initCommon();
	}
	return m_common;
}

void PLSGpopData::getGpopDataFromLocal()
{
	if (m_gpopDataArray.isEmpty()) {

		QString gpopPath = pls_get_user_path(PRISM_GPOP_PATH);
		if (!QFileInfo(gpopPath).exists()) {
			QDir appDir(qApp->applicationDirPath());
			gpopPath = appDir.absoluteFilePath("data/prism-studio/user/gpop.json");
		}
		PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, gpopPath);
	}
}

void PLSGpopData::initCommon()
{
	QVariantMap map;
	QVariant value;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, "optional", map);
	Common common;
	common.stage = map.value(name2str(common)).toMap().value(name2str(stage)).toString();
	common.regionCode = map.value(name2str(common)).toMap().value(name2str(regionCode)).toString();
	common.deviceType = map.value(name2str(common)).toMap().value(name2str(deviceType)).toString();
	m_common = common;
}

void PLSGpopData::initSnscallbackUrls()
{
	QMap<QString, SnsCallbackUrl> snscallbackUrls;
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(snsCallbackUrls), map);
	for (auto key : map.keys()) {
		SnsCallbackUrl callbackUrl;
		callbackUrl.callbackUrlType = key;
		callbackUrl.url = map.value(key).toString();
		snscallbackUrls.insert(key, callbackUrl);
	}
	m_snsCallbackUrls = snscallbackUrls;
}

void PLSGpopData::initOpenLicenseUrl()
{
	m_openSourceLicenseMap.clear();
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(openSourceLicense), map);
	for (auto key : map.keys()) {
		QString url = map.value(key).toString();
		m_openSourceLicenseMap.insert(key, url);
	}
}

void PLSGpopData::initRtmpDestination()
{
	QMap<int, RtmpDestination> rtmpDestinations;
	QVariantMap map;
	QVariantList list;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(rtmpDestination), list);
	if (list.isEmpty()) {
		useDefaultValues(name2str(rtmpDestination), list);
	}
	for (auto map : list) {
		RtmpDestination rtmpDestionation;
		rtmpDestionation.streamName = map.toMap().value(name2str(streamName)).toString();
		rtmpDestionation.rtmpUrl = map.toMap().value(name2str(rtmpUrl)).toString();
		rtmpDestionation.exposure = map.toMap().value(name2str(exposure)).toBool();
		rtmpDestionation.order = map.toMap().value(name2str(order)).toInt();
		if (rtmpDestionation.exposure) {
			rtmpDestinations.insert(rtmpDestionation.order, rtmpDestionation);
		}
		bool hasResolutionLimit = map.toMap().value(name2str(hasResolutionLimit)).toBool();
		if (rtmpDestionation.exposure && hasResolutionLimit) {
			QString idName = map.toMap().value(name2str(id)).toString();
			QMap resolutionFps = map.toMap().value(name2str(resolutionFps)).toMap();
			m_rtmpFPSMap.insert(idName, resolutionFps);
		}
	}
	m_destinations = rtmpDestinations;
}

void PLSGpopData::initConnection()
{
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, "apis", map);
	Connection connection;
	connection.ssl = pls_is_dev_server() ? "https://dev-global.apis.naver.com" : "https://global.apis.naver.com";
	connection.url = map.value(name2str(url)).toString();
	connection.fallback.ssl = map.value(name2str(fallback)).toMap().value(name2str(ssl)).toString();
	connection.fallback.url = map.value(name2str(fallback)).toMap().value(name2str(url)).toString();
	m_connection = connection;
}

void PLSGpopData::initVliveNotice()
{
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, "vliveReorganizationNotice", map);
	VliveNotice liveNotice;
	liveNotice.version = map.value(name2str(version)).toString();
	liveNotice.isNeedNotice = map.value(name2str(needNotice)).toBool();
	liveNotice.pageUrl = map.value(name2str(noticePageUrl)).toString();
	m_vliveNotice = liveNotice;
}

void PLSGpopData::initBlackList()
{
	QVariantList list;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(gpuModels), list);
	BlackList blackList;
	blackList.gpuModels.clear();
	for (auto value : list) {
		blackList.gpuModels.push_back(value.toString());
	}
	list.clear();
	blackList.graphicsDrivers.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(graphicsDrivers), list);
	for (auto value : list) {
		blackList.graphicsDrivers.push_back(value.toString());
	}
	list.clear();
	blackList.deviceDrivers.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(deviceDrivers), list);
	for (auto value : list) {
		blackList.deviceDrivers.push_back(value.toString());
	}
	list.clear();
	blackList.thirdPartyPlugins.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(3rdPlugins), list);
	for (auto value : list) {
		blackList.thirdPartyPlugins.push_back(value.toString());
	}
	list.clear();
	blackList.vstPlugins.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(vstPlugins), list);
	for (auto value : list) {
		blackList.vstPlugins.push_back(value.toString());
	}
	list.clear();
	blackList.thirdPartyPrograms.clear();
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(3rdPrograms), list);
	for (auto value : list) {
		blackList.thirdPartyPrograms.push_back(value.toString());
	}
	blackList.exceptionTypes.clear();
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(exceptionTypes), map);
	for (auto key : map.keys()) {
		blackList.exceptionTypes.insert(key, map.value(key).toString());
	}
	list.clear();

	m_blackList = blackList;
	m_blackList.isEmpty = false;
}

void PLSGpopData::initH265Param()
{
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "h265", value)) {
		h265opened = value.toBool();
	}
	emit initH265Finished();
}

void PLSGpopData::initWGCParam()
{
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "uwp_borderless_enable", value)) {
		bool wgcBorderlessEnable = value.toBool();
		obs_set_wgc_borderless_enable(wgcBorderlessEnable);
	}
}

void PLSGpopData::initCameraRestartTimes()
{
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "camera_auto_restart_times", value)) {
		camera_auto_restart_times = value.toInt();
		PLS_INIT_INFO("PLSGpopData", "Camera auto restart times: %d", camera_auto_restart_times);
	}

	emit initCameraRestartTimesFinished();
}

void PLSGpopData::initFrameDropPercentThreshold()
{
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "drop_network_frame_precent_threshold", value)) {
		dropNetworkFramePrecentThreshold = value.toFloat();
	}
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "drop_rendering_frame_precent_threshold", value)) {
		dropRenderingFramePrecentThreshold = value.toFloat();
	}
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "drop_encoding_frame_precent_threshold", value)) {
		dropEncodingFramePrecentThreshold = value.toFloat();
	}
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "buffered_duration_ms", value)) {
		bufferedDurationMs = value.toInt();
	}
}

void PLSGpopData::initUIBlockingTimeS()
{
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "ui_blocking_time_s", value)) {
		uiBlockingTimeS = max(value.toInt(), 5);
	}
}

void PLSGpopData::initNaverShoppingTermOfUse()
{
	m_navershoppingTermofUse.clear();
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(NaverShoppingLiveServiceTermsOfUse), map);
	for (auto key : map.keys()) {
		QString url = map.value(key).toString();
		m_navershoppingTermofUse.insert(key, url);
	}
}

void PLSGpopData::initNaverShoppingNotes()
{
	m_navershoppingNotes.clear();
	QVariantMap map;
	PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(NaverShoppingLiveServiceNotes), map);
	for (auto key : map.keys()) {
		QString url = map.value(key).toString();
		m_navershoppingNotes.insert(key, url);
	}
}

void PLSGpopData::initMultiplePlatformMaxBitrate()
{
	m_iMultiplePlatformMaxBitrate = 10000;
	QVariant value;
	if (PLSJsonDataHandler::getValueFromByteArray(m_gpopDataArray, "multiplePlatformMaxBitrate", value)) {
		m_iMultiplePlatformMaxBitrate = value.toInt();
	}
}

void PLSGpopData::initPlatformVersionInfo()
{
	if (!PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(requiredVersion), m_platformVersionInfo)) {
		PLS_INIT_ERROR("PLSGpopData", __FUNCTION__ ":can't find ");
		useDefaultValues(name2str(requiredVersion), m_platformVersionInfo);
	}
}

void PLSGpopData::initNaverPlatformWhiteList()
{
	QVariantList list;
	m_naverPlatformWhiteList.clear();
	if (!PLSJsonDataHandler::getValuesFromByteArray(m_gpopDataArray, name2str(NaverPlatformWhitelist), list)) {
		PLS_INIT_ERROR("PLSGpopData", __FUNCTION__ ":can't find ");
	}
	for (auto url : list) {
		m_naverPlatformWhiteList.append(url.toString());
	}
}

QMap<QString, SnsCallbackUrl> PLSGpopData::getSnscallbackUrls()
{
	if (0 == m_snsCallbackUrls.size()) {
		getGpopDataFromLocal();
		initSnscallbackUrls();
	}
	return m_snsCallbackUrls;
}
QMap<int, RtmpDestination> PLSGpopData::getRtmpDestination()
{
	if (0 == m_destinations.size()) {
		getGpopDataFromLocal();
		initRtmpDestination();
	}
	return m_destinations;
}
QMap<QString, QString> &PLSGpopData::getOpenSourceLicenseMap()
{
	return m_openSourceLicenseMap;
}

VliveNotice PLSGpopData::getVliveNotice()
{
	if (m_vliveNotice.pageUrl.isEmpty()) {
		PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, pls_get_user_path(PRISM_GPOP_PATH));
		initVliveNotice();
	}
	return m_vliveNotice;
}

BlackList PLSGpopData::getBlackList()
{
	if (m_blackList.isEmpty) {
		PLSJsonDataHandler::getJsonArrayFromFile(m_gpopDataArray, pls_get_user_path(PRISM_GPOP_PATH));
		initBlackList();
	}
	return m_blackList;
}

bool PLSGpopData::getH265opened()
{
	return h265opened;
}

QMap<QString, QString> &PLSGpopData::getNaverShoppingTermOfUse()
{
	if (0 == m_navershoppingTermofUse.size()) {
		getGpopDataFromLocal();
		initNaverShoppingTermOfUse();
	}
	return m_navershoppingTermofUse;
}

QMap<QString, QString> &PLSGpopData::getNaverShoppingNotes()
{
	if (0 == m_navershoppingNotes.size()) {
		getGpopDataFromLocal();
		initNaverShoppingNotes();
	}
	return m_navershoppingNotes;
}

int PLSGpopData::getMultiplePlatformMaxBitrate()
{
	if (m_iMultiplePlatformMaxBitrate == 0) {
		getGpopDataFromLocal();
		initMultiplePlatformMaxBitrate();
	}

	return m_iMultiplePlatformMaxBitrate;
}

const QVariantMap &PLSGpopData::getPlatformVersionInfo()
{
	return m_platformVersionInfo;
}

int PLSGpopData::getCameraRestartTimes()
{
	return camera_auto_restart_times;
}

float PLSGpopData::getDropNetworkFramePrecentThreshold()
{
	return dropNetworkFramePrecentThreshold;
}

float PLSGpopData::getDropRenderingFramePrecentThreshold()
{
	return dropRenderingFramePrecentThreshold;
}

float PLSGpopData::getDropEncodingFramePrecentThreshold()
{
	return dropEncodingFramePrecentThreshold;
}

int PLSGpopData::getBufferedDurationMs()
{
	return bufferedDurationMs;
}

int PLSGpopData::getUIBlockingTimeS()
{
	return uiBlockingTimeS;
}

QStringList PLSGpopData::getNaverPlatformWhiteList()
{
	if (0 == m_naverPlatformWhiteList.size()) {
		getGpopDataFromLocal();
		initRtmpDestination();
	}
	return m_naverPlatformWhiteList;
}

bool PLSGpopData::isPresetRTMP(const QString &url)
{
	static QStringList presetRTMP;
	if (presetRTMP.isEmpty()) {
		presetRTMP = getNaverPlatformWhiteList();
	}
	bool isPreset = false;
	for (const auto &rule : presetRTMP) {
		QRegExp regx(rule, Qt::CaseInsensitive, QRegExp::Wildcard);
		auto pos = regx.indexIn(url);
		if (pos != -1) {
			isPreset = true;
			break;
		}
	}
	return isPreset;
}

const QVariantMap &PLSGpopData::rtmpFPSMap()
{
	return m_rtmpFPSMap;
}

void PLSGpopData::initGpopData()
{
	getGpopDataFromLocal();

	initCommon();
	initSnscallbackUrls();
	initOpenLicenseUrl();
	initRtmpDestination();
	initConnection();

	initBlackList();
	initH265Param();
	initWGCParam();
	initCameraRestartTimes();
	initFrameDropPercentThreshold();
	initUIBlockingTimeS();

	initVliveNotice();

	initNaverShoppingTermOfUse();
	initNaverShoppingNotes();
	initMultiplePlatformMaxBitrate();

	initPlatformVersionInfo();

	initNaverPlatformWhiteList();

	emit initGpopDataFinished();
}
