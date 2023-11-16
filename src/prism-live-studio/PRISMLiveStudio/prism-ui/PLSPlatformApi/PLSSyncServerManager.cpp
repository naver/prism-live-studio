
#include "PLSSyncServerManager.hpp"
#include "frontend-internal.hpp"
#include "PLSChannelDataAPI.h"
#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSResCommonFuns.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSPlatformApi.h"
#include "PLSResourceManager.h"
#include "PLSServerStreamHandler.hpp"

#define PUBLISH_VERSION QStringLiteral("version")
#define POLICY_PUBLISH_JSON_NAME QStringLiteral("Policy_Publish.json")
#define POLICY_PLATFORM_JSON_NAME QStringLiteral("Policy_Platform.json")
#define OPEN_SOURCE_JSON_NAME QStringLiteral("OpenSource.json")

constexpr auto REACTIONPATH = "PRISMLiveStudio/library/library_Policy_PC/Prism_Sticker_Reaction.json";
constexpr auto SHOPPING_CATEGORY_PATH = "PRISMLiveStudio/library/library_Policy_PC/navershoppingCategory.json";

using namespace common;

PLSSyncServerManager *PLSSyncServerManager::instance()
{
	static PLSSyncServerManager syncServerManager;
	return &syncServerManager;
}

PLSSyncServerManager::PLSSyncServerManager(QObject *parent) : QObject(parent) 
{
	connect(PLSRESOURCEMGR_INSTANCE, &PLSResourceManager::libraryNeedUpdate, this, &PLSSyncServerManager::onReceiveLibraryNeedUpdate);
	int appBundleVersion = 0;
	getSyncServerAppBundleJsonObject(POLICY_PUBLISH_JSON_NAME, m_policyPublishDefaultValueObject, appBundleVersion);
	QString log = QString("sync server status: read Policy_Publish.json default value , version is %1")
			      .arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	getSyncServerAppBundleJsonObject(POLICY_PLATFORM_JSON_NAME, m_policyPlatformDefaultValueObject, appBundleVersion);
	log = QString("sync server status: read Policy_Platform.json default value , version is %1")
		      .arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	getSyncServerAppBundleJsonObject(OPEN_SOURCE_JSON_NAME, m_openSourceDefaultValueObject, appBundleVersion);
	log = QString("sync server status: read OpenSource.json default value , version is %1").arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::updatePolicyPublishByteArray()
{
	PLS_INFO("SyncServer", "sync server status: start update policy publish and policy platform json object");
	QJsonObject policyPublishObject;
	getSyncServerJsonObject(POLICY_PUBLISH_JSON_NAME, policyPublishObject);
	QJsonObject policyPlatformObject;
	getSyncServerJsonObject(POLICY_PLATFORM_JSON_NAME, policyPlatformObject);
	initSupportedResolutionFPS(policyPublishObject);
	initRtmpDestination(policyPublishObject);
	initMultiplePlatformMaxBitrate(policyPublishObject);
	initNaverPlatformWhiteList(policyPublishObject);
	initStreamServiceList(policyPublishObject);
	initPlatformLiveTimeLimit(policyPublishObject);
	initPlatformVersionInfo(policyPublishObject);
	initNaverShoppingInfo(policyPlatformObject);
	QJsonObject openSourceObject;
	getSyncServerJsonObject(OPEN_SOURCE_JSON_NAME, openSourceObject);
	initOpenSourceInfo(openSourceObject);
	PLS_INFO("SyncServer", "sync server status: end update policy publish and policy platform json object");
}

bool PLSSyncServerManager::getSyncServerAppBundleJsonObject(const QString &jsonName, QJsonObject &appBundleJsonObject, int &appBundleVersion)
{
	QString qrcLogPath = QString("prism-live-studio/PRISMLiveStudio/prism-ui/defaultSources.qrc/%1").arg(jsonName);

	QString log = QString("sync server status: start reading app qrc %1 file").arg(qrcLogPath);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	QByteArray appBundleByteArray;
	auto qrcPath = findFileInResources(ChannelData::defaultSourcePath, jsonName);
	PLSJsonDataHandler::getJsonArrayFromFile(appBundleByteArray, qrcPath);
	if (appBundleByteArray.size() == 0) {
		log = QString("sync server status: %1 is not existed.").arg(qrcLogPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		return false;
	}

	QJsonParseError appBundleError;
	QJsonDocument appBundleDoc = QJsonDocument::fromJson(appBundleByteArray, &appBundleError);
	if (appBundleError.error != QJsonParseError::NoError) {
		log = QString("sync server status: %1 is not in the correct json format").arg(qrcLogPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		return false;
	}

	appBundleJsonObject = appBundleDoc.object();
	appBundleVersion = appBundleJsonObject[PUBLISH_VERSION].toInt();
	log = QString("sync server status: %1 version is %2 , %3 file size is %4 byte").arg(qrcLogPath).arg(appBundleVersion).arg(jsonName).arg(appBundleByteArray.size());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return true;
}

bool PLSSyncServerManager::getSyncServerUserFolderJsonObject(const QString &jsonName, QJsonObject &userFolderJsonObject, int &userFolderVersion)
{
	QString userFolderJsonPath = QString("PRISMLiveStudio/library/library_Policy_PC/%1").arg(jsonName);

	QString log = QString("sync server status: start reading user AppData Folder %1 file").arg(userFolderJsonPath);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	QByteArray userFolderByteArray;
	PLSJsonDataHandler::getJsonArrayFromFile(userFolderByteArray, pls_get_user_path(userFolderJsonPath));
	if (userFolderByteArray.size() == 0) {
		log = QString("sync server status: %1 is not existed.").arg(userFolderJsonPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		return false;
	}

	QJsonParseError error;
	QJsonDocument doc = QJsonDocument::fromJson(userFolderByteArray, &error);
	if (error.error != QJsonParseError::NoError) {
		log = QString("sync server status: %1 is not in the correct json format").arg(userFolderJsonPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		return false;
	}

	userFolderJsonObject = doc.object();
	userFolderVersion = userFolderJsonObject[PUBLISH_VERSION].toInt();
	log = QString("sync server status: %1 version is %2 , %3 file size is %4 byte").arg(userFolderJsonPath).arg(userFolderVersion).arg(jsonName)
			      .arg(userFolderByteArray.size());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return true;
}

void PLSSyncServerManager::getSyncServerJsonObject(const QString &jsonName, QJsonObject &jsonObject)
{
	QJsonObject appBundleJsonObject;
	int appBundleVersion = 0;
	getSyncServerAppBundleJsonObject(jsonName, appBundleJsonObject, appBundleVersion);

	QJsonObject userFolderJsonObject;
	int userFolderVersion = 0;
	getSyncServerUserFolderJsonObject(jsonName, userFolderJsonObject, userFolderVersion);

	if (userFolderVersion >= appBundleVersion) {
		jsonObject = userFolderJsonObject;
		QString userFolderJsonPath = QString("PRISMLiveStudio/library/library_Policy_PC/%1").arg(jsonName);
		QString log = QString("sync server status: use %1 file").arg(userFolderJsonPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	} else {
		jsonObject = appBundleJsonObject;
		QString qrcLogPath = QString("prism-live-studio/PRISMLiveStudio/prism-ui/defaultSources.qrc/%1").arg(jsonName);
		QString log = QString("sync server status: use %1 file").arg(qrcLogPath);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	}
}

void PLSSyncServerManager::initMultiplePlatformMaxBitrate(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init multiple platform max bitrate info");
	QJsonValue value = policyPublishJsonObject.value(name2str(multiplePlatformMaxBitrate));
	m_iMultiplePlatformMaxBitrate = value.toInt();
	PLS_INFO("SyncServer", "sync server status: init multiple platform max bitrate info success , m_iMultiplePlatformMaxBitrate is %d", m_iMultiplePlatformMaxBitrate);
}

void PLSSyncServerManager::initNaverPlatformWhiteList(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init naver platform white list info");

	QVariantList list;
	m_naverPlatformWhiteList.clear();

	QJsonValue value = policyPublishJsonObject.value(name2str(naverPlatformWhitelist));
	if (!value.isArray()) {
		PLS_INFO("SyncServer", "sync server status: init naver platform white list info failed , because the naverPlatformWhitelist is not json array");
		return;
	}

	QJsonArray jsonArray = value.toArray();
	for (const QJsonValue &value : jsonArray) {
		m_naverPlatformWhiteList.append(value.toString());
	}
	QString log = QString("sync server status: init naver platform white list info success , m_naverPlatformWhiteList count() is %1").arg(m_naverPlatformWhiteList.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initStreamServiceList(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init stream server list info");
	QJsonValue value = policyPublishJsonObject.value(name2str(streamService));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init stream server list info failed ,because the streamService value is not json object");
		return;
	}
	QJsonObject itemObject = value.toObject();
	m_streamService = itemObject.toVariantMap();
	QString log = QString("sync server status: init stream server list info success , m_streamService count() is %1").arg(m_streamService.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initPlatformLiveTimeLimit(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init platform max live time info");
	QJsonValue value = policyPublishJsonObject.value(name2str(platformLiveTimeLimit));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init platform max live time info failed ,because the platformLiveTimeLimit value is not json object");
		return;
	}
	QJsonObject itemObject = value.toObject();
	m_platformLiveTimeLimit = itemObject.toVariantMap();
	QString log = QString("sync server status: init platform max live time info success , m_platformLiveTimeLimit count() is %1").arg(m_platformLiveTimeLimit.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initPlatformVersionInfo(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init platform required version info");
#if defined(Q_OS_MACOS)
	QJsonValue platformValue = policyPublishJsonObject.value(name2str(mac));
#else
	QJsonValue platformValue = policyPublishJsonObject.value(name2str(win64));
#endif
	if (!platformValue.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init platform required version info failed ,because the platform value is not json object");
		return;
	}
	QJsonObject platformObject = platformValue.toObject();
	QJsonValue requiredVersionValue = platformObject.value(name2str(requiredVersion));
	if (!requiredVersionValue.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init platform required version info failed ,because the required version value is not json object");
		return;
	}
	QJsonObject requiredVersionObject = requiredVersionValue.toObject();
	m_platformVersionInfo = requiredVersionObject.toVariantMap();
	QString log = QString("sync server status: init platform required version inf success , m_platformVersionInfo count() is %1").arg(m_platformVersionInfo.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initNaverShoppingInfo(const QJsonObject &policyPlatformJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init navershopping info");
	QJsonValue value = policyPlatformJsonObject.value(name2str(navershopping));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init navershopping info failed , because the navershopping value is not json object");
		return;
	}
	QJsonObject platformObject = value.toObject();

	m_navershoppingTermofUse = platformObject.value(name2str(serviceTermsOfUse)).toString();
	QString log = QString("sync server status: init navershopping serviceTermsOfUse info , m_navershoppingTermofUse is %1").arg(m_navershoppingTermofUse);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	m_navershoppingOperationPolicy = platformObject.value(name2str(operationPolicy)).toString();
	log = QString("sync server status: init navershopping operationPolicy info , m_navershoppingOperationPolicy is %1").arg(m_navershoppingOperationPolicy);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	m_navershoppingNotes = platformObject.value(name2str(serviceNotes)).toString();
	log = QString("sync server status: init navershopping serviceNotes info , m_navershoppingNotes is %1").arg(m_navershoppingNotes);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	auto lang = pls_get_current_country_short_str().toUpper();
	QJsonValue noticeOnAutomaticExtractionOfProductSectionsValue = platformObject.value(name2str(noticeOnAutomaticExtractionOfProductSections));
	if (!noticeOnAutomaticExtractionOfProductSectionsValue.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init navershopping info failed , because the noticeOnAutomaticExtractionOfProductSections is not json object");
		return;
	}

	QVariantMap noticeOnAutomaticExtractionOfProductSectionsMap = noticeOnAutomaticExtractionOfProductSectionsValue.toObject().toVariantMap();
	if (noticeOnAutomaticExtractionOfProductSectionsMap.find(lang) != noticeOnAutomaticExtractionOfProductSectionsMap.end()) {
		m_noticeOnAutomaticExtractionOfProductSections = noticeOnAutomaticExtractionOfProductSectionsMap.value(lang).toString();
		log = QString("sync server status: init navershopping automatic extraction info , m_noticeOnAutomaticExtractionOfProductSections is %1 , lang is %2")
			      .arg(m_noticeOnAutomaticExtractionOfProductSections)
			      .arg(lang);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	}

	if (m_noticeOnAutomaticExtractionOfProductSections.isEmpty()) {
		lang = "US";
		if (noticeOnAutomaticExtractionOfProductSectionsMap.find(lang) != noticeOnAutomaticExtractionOfProductSectionsMap.end()) {
			m_noticeOnAutomaticExtractionOfProductSections = noticeOnAutomaticExtractionOfProductSectionsMap.value(lang).toString();
			log = QString("sync server status: init navershopping automatic extraction default value info , m_noticeOnAutomaticExtractionOfProductSections is %1 , lang is %2")
				      .arg(m_noticeOnAutomaticExtractionOfProductSections)
				      .arg(lang);
			PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		}
	}

	QJsonValue voluntaryReviewProductsValue = platformObject.value(name2str(voluntaryReviewProducts));
	if (!voluntaryReviewProductsValue.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init navershopping info failed , because the voluntaryReviewProductsValue is not json object");
		return;
	}
	QVariantMap voluntaryReviewProductsMap = voluntaryReviewProductsValue.toObject().toVariantMap();
	if (voluntaryReviewProductsMap.find(lang) != voluntaryReviewProductsMap.end()) {
		m_voluntaryReviewProducts = voluntaryReviewProductsMap.value(lang).toString();
		log = QString("sync server status: init navershopping voluntary review products info , m_voluntaryReviewProducts is %1 , lang is %2").arg(m_voluntaryReviewProducts)
			      .arg(lang);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	}

	if (m_voluntaryReviewProducts.isEmpty()) {
		lang = "US";
		if (voluntaryReviewProductsMap.find(lang) != voluntaryReviewProductsMap.end()) {
			m_voluntaryReviewProducts = voluntaryReviewProductsMap.value(lang).toString();
			log = QString("sync server status: init navershopping voluntary review products default value info , m_voluntaryReviewProducts is %1 , lang is %2").arg(m_voluntaryReviewProducts).arg(lang);
			PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
		}
	}

	QJsonValue categoryValue = platformObject.value(name2str(category));
	if (!categoryValue.isArray()) {
		PLS_INFO("SyncServer", "sync server status: init navershopping info failed , because the categoryValue is not json object");
		return;
	}
	m_productCategoryJsonArray = categoryValue.toArray();

	log = QString("sync server status: init navershopping info success , m_productCategoryJsonArray count() is %1").arg(m_productCategoryJsonArray.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initOpenSourceInfo(const QJsonObject &openSourceJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init open source info");
	m_openSourceLicense = openSourceJsonObject.value(name2str(openSourceLicense)).toString();
	PLS_INFO("SyncServer", "sync server status: init open source info success , open source is %s", m_openSourceLicense.toUtf8().constData());
}

bool PLSSyncServerManager::initSupportedResolutionFPS(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init supported resolution fps info");
	QJsonValue value = policyPublishJsonObject.value(name2str(supportedResolutionFPS));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init supported resolution fps info failed ,because the supportedResolutionFPS value is not json object");
		return false;
	}

	m_platformFPSMap.clear();
	QJsonObject itemObject = value.toObject();
	QJsonObject::iterator it;
	for (it = itemObject.begin(); it != itemObject.end(); it++) {
		QString platformKey = it.key();
		QJsonObject obj = it.value().toObject();
		bool limit = obj.value(name2str(hasResolutionLimit)).toBool();
		if (!limit) {
			continue;
		}
		QVariantMap map = obj.value(name2str(resolutionFps)).toObject().toVariantMap();
		m_platformFPSMap.insert(platformKey, map);
	}
	QString log = QString("sync server status: init supported resolution fps info success ,m_platformFPSMap count() is %1").arg(m_platformFPSMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return true;
}

void PLSSyncServerManager::initRtmpDestination(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init rtmp destination info");
	QMap<int, RtmpDestination> rtmpDestinations;

	QJsonValue value = policyPublishJsonObject.value(name2str(rtmpDestination));
	if (!value.isArray()) {
		PLS_INFO("SyncServer", "sync server status: init rtmp destination info failed ,because the supportedResolutionFPS value is not json array");
		return;
	}

	QVariantList list = value.toArray().toVariantList();
	if (list.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: init rtmp destination info failed , because rtmpDestination list count is zero");
		return;
	}

	QVariantMap rtmpFPSMap;
	for (const auto &map : list) {
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
			rtmpFPSMap.insert(idName, resolutionFps);
		}
	}
	m_destinations = rtmpDestinations;
	m_rtmpFPSMap = rtmpFPSMap;
	QString log = QString("sync server status: init rtmp destination info success , m_rtmpFPSMap count() is %1 , m_destinations count() is %2").arg(m_rtmpFPSMap.count()).arg(m_destinations.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

const QVariantList &PLSSyncServerManager::getResolutionsList()
{
	if (!m_resolutionsInfos.isEmpty()) {
		return m_resolutionsInfos;
	}
	QByteArray data;
	QString path = pls_get_user_path(CONFIGS_RESOLUTIONGUIDE_PATH);
	if (bool isOk = PLSJsonDataHandler::getJsonArrayFromFile(data, path); isOk) {
		PLSJsonDataHandler::jsonTo(data, m_resolutionsInfos);
	}

	return m_resolutionsInfos;
}

const QMap<QString, QVariantList> &PLSSyncServerManager::getStickerReaction()
{
	if (!m_reaction.isEmpty()) {
		return m_reaction;
	}
	QByteArray data;
	QString path = pls_get_user_path(REACTIONPATH);
	if (bool isOk = PLSJsonDataHandler::getJsonArrayFromFile(data, path); isOk) {
		QVariantMap reaction;
		PLSJsonDataHandler::getValuesFromByteArray(data, name2str(reaction), reaction);
		for (auto key : reaction.keys()) {
			QVariantList list;
			PLSJsonDataHandler::getValuesFromByteArray(data, key, list);
			m_reaction.insert(key, list);
		}
	}

	return m_reaction;
}

QVariantMap PLSSyncServerManager::getSupportedResolutionFPSMap()
{
	if (m_platformFPSMap.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get supported resolution fps info from AppData or App Bundle, the m_platformFPSMap is empty");
		updatePolicyPublishByteArray();
		if (m_platformFPSMap.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get supported resolution fps info from app default value json file, the m_platformFPSMap is empty");
			initSupportedResolutionFPS(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get supported resolution fps info finished , m_platformFPSMap count() is %1").arg(m_platformFPSMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_platformFPSMap;
}

QVariantMap PLSSyncServerManager::getLivePlatformResolutionFPSMap()
{
	auto platform = PLS_PLATFORM_API->getPlatformNaverShoppingLIVE();
	if (platform == nullptr) {
		PLS_INFO("SyncServer", "sync server status: get live platform resolution fps info , navershopping platform is not login");
		return getSupportedResolutionFPSMap();
	}
	QVariantMap platformFPSMap = getSupportedResolutionFPSMap();
	QVariant navershoppingHighSlv = platformFPSMap.value(NAVER_SHOPPING_HIGH_SLV_RESOLUTION_KEY);
	if (platform->isHighResolutionSlvByPrepareInfo()) {
		platformFPSMap.insert(NAVER_SHOPPING_RESOLUTION_KEY, navershoppingHighSlv);
	}
	QString log = QString("sync server status:get live platform resolution fps info finished, platformFPSMap count() is %1").arg(platformFPSMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return platformFPSMap;
}

const QVariantMap &PLSSyncServerManager::getRtmpPlatformFPSMap()
{
	if (m_rtmpFPSMap.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get rtmp platform resolution or fps info from AppData or App Bundle , the m_rtmpFPSMap is empty");
		updatePolicyPublishByteArray();
		if (m_rtmpFPSMap.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get rtmp platform resolution or fps info from app default value json file , the m_rtmpFPSMap is empty");
			initRtmpDestination(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get rtmp platform fps info finished , m_rtmpFPSMap count() is %1").arg(m_rtmpFPSMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_rtmpFPSMap;
}

QMap<int, RtmpDestination> PLSSyncServerManager::getRtmpDestination()
{
	if (m_destinations.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get rtmp destination info from AppData or App Bundle , the m_destinations is empty");
		updatePolicyPublishByteArray();
		if (m_destinations.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get rtmp destination info from app default value json file , the m_destinations is empty");
			initRtmpDestination(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get rtmp destination info finished , m_destinations count() is %1").arg(m_destinations.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_destinations;
}

int PLSSyncServerManager::getMultiplePlatformMaxBitrate()
{
	if (!m_iMultiplePlatformMaxBitrate.has_value()) {
		PLS_INFO("SyncServer", "sync server status: get multiple platform max bitrate info from AppData or App Bundle , the m_iMultiplePlatformMaxBitrate is zero");
		updatePolicyPublishByteArray();
		if (!m_iMultiplePlatformMaxBitrate.has_value()) {
			PLS_INFO("SyncServer", "sync server status: get multiple platform max bitrate info from app default value json file , the m_iMultiplePlatformMaxBitrate is zero");
			initMultiplePlatformMaxBitrate(m_policyPublishDefaultValueObject);
		}
	}
	int value = m_iMultiplePlatformMaxBitrate.value();
	QString log = QString("sync server status: get multiple platform max bitrate info finished , the m_iMultiplePlatformMaxBitrate value is %1").arg(value);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return value;
}

QStringList PLSSyncServerManager::getNaverPlatformWhiteList()
{
	if (m_naverPlatformWhiteList.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get navershopping plaform white list from AppData or App Bundle , the m_naverPlatformWhiteList is empty");
		updatePolicyPublishByteArray();
		if (m_naverPlatformWhiteList.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get navershopping plaform white list from app default value json file , the m_naverPlatformWhiteList is empty");
			initNaverPlatformWhiteList(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get navershopping plaform white list finished , the m_naverPlatformWhiteList count() is %1").arg(m_naverPlatformWhiteList.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_naverPlatformWhiteList;
}

const QVariantMap &PLSSyncServerManager::getStreamService()
{
	if (m_streamService.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status:get stream server info from AppData or App Bundle , the m_streamService is empty");
		updatePolicyPublishByteArray();
		if (m_streamService.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status:get stream server info from app default value json file , the m_streamService is empty");
			initStreamServiceList(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get stream server info finished , the m_streamService count() is %1").arg(m_streamService.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_streamService;
}

PlatformLiveTime PLSSyncServerManager::getPlatformLiveTime(bool isDirect, const QString &platformName)
{
	if (m_platformLiveTimeLimit.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get platform live time info from AppData or App Bundle , the m_platformLiveTimeLimit is empty");
		updatePolicyPublishByteArray();
		if (m_platformLiveTimeLimit.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get platform live time info from app default value json file , the m_platformLiveTimeLimit is empty");
			initPlatformLiveTimeLimit(m_policyPublishDefaultValueObject);
		}
	}
	QString directKey = isDirect ? "directLive" : "inDirectLive";
	QString platformNameKey = "general";
	if (platformName == BAND) {
		platformNameKey = "band";
	} else if (platformName == NAVER_SHOPPING_LIVE) {
		platformNameKey = "navershopping";
	}
	QVariantMap platformMap = m_platformLiveTimeLimit.value(directKey).toMap().value(platformNameKey).toMap();
	PlatformLiveTime liveTime;
	liveTime.countdownReminderMinutesList = platformMap.value(name2str(countdownReminderMinutesList)).toStringList();
	liveTime.maxLiveMinutes = platformMap.value(name2str(maxLiveMinutes)).toInt();
	QString log = QString("sync server status: get platform live time finished , countdownReminderMinutesList size is %1 , maxLiveMinutes is %2 , platformName is %3")
			      .arg(liveTime.countdownReminderMinutesList.size())
			      .arg(liveTime.maxLiveMinutes)
			      .arg(platformName);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return liveTime;
}

const QVariantMap &PLSSyncServerManager::getPlatformVersionInfo()
{
	if (m_platformVersionInfo.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get platform required version info from AppData or App Bundle , the m_platformVersionInfo is empty");
		updatePolicyPublishByteArray();
		if (m_platformVersionInfo.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get platform required version info from app default value json file , the m_platformVersionInfo is empty");
			initPlatformVersionInfo(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get platform required version info finished , the m_platformVersionInfo count() is %1").arg(m_platformVersionInfo.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_platformVersionInfo;
}

bool PLSSyncServerManager::isPresetRTMP(const QString &url)
{
	static QStringList presetRTMP;
	if (presetRTMP.isEmpty()) {
		presetRTMP = getNaverPlatformWhiteList();
	}
	bool isPreset = false;
	for (const auto &rule : presetRTMP) {
		QRegularExpression regx(rule);
		regx.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
		auto matchRe = regx.match(url);
		if (matchRe.hasMatch()) {
			isPreset = true;
			break;
		}
	}
	return isPreset;
}

const QString &PLSSyncServerManager::getNaverShoppingTermOfUse()
{
	if (m_navershoppingTermofUse.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get naver shopping term of use info from AppData or App Bundle , the m_navershoppingTermofUse is empty");
		updatePolicyPublishByteArray();
		if (m_navershoppingTermofUse.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get naver shopping term of use info from app default value json file , the m_navershoppingTermofUse is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get naver shopping term of use info finished , the m_navershoppingTermofUse is %1").arg(m_navershoppingTermofUse);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_navershoppingTermofUse;
}

const QString &PLSSyncServerManager::getNaverShoppingOperationPolicy()
{
	if (m_navershoppingOperationPolicy.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get naver shopping operation policy info from AppData or App Bundle , the m_navershoppingOperationPolicy is empty");
		updatePolicyPublishByteArray();
		if (m_navershoppingOperationPolicy.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get naver shopping operation policy info from app default value json file , the m_navershoppingOperationPolicy is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get naver shopping operation policy info finished , the m_navershoppingOperationPolicy is %1").arg(m_navershoppingOperationPolicy);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_navershoppingOperationPolicy;
}

const QString &PLSSyncServerManager::getNaverShoppingNotes()
{
	if (m_navershoppingNotes.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get naver shopping note info from AppData or App Bundle , the m_navershoppingNotes is empty");
		updatePolicyPublishByteArray();
		if (m_navershoppingNotes.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get naver shopping note info from app default value json file , the m_navershoppingNotes is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get naver shopping note info finished , the m_navershoppingNotes is %1").arg(m_navershoppingNotes);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_navershoppingNotes;
}

const QString &PLSSyncServerManager::getVoluntaryReviewProducts()
{
	if (m_voluntaryReviewProducts.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get naver shopping voluntary review products info from AppData or App Bundle , the m_voluntaryReviewProducts is empty");
		updatePolicyPublishByteArray();
		if (m_voluntaryReviewProducts.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get naver shopping voluntary review products info from app default value json file , the m_voluntaryReviewProducts is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get naver shopping voluntary review products info finished , the m_voluntaryReviewProducts is %1").arg(m_voluntaryReviewProducts);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_voluntaryReviewProducts;
}

const QString &PLSSyncServerManager::getNoticeOnAutomaticExtractionOfProductSections()
{
	if (m_noticeOnAutomaticExtractionOfProductSections.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get naver shopping notice on automatic extraction of product sectionss info from AppData or App Bundle , the m_noticeOnAutomaticExtractionOfProductSections is empty");
		updatePolicyPublishByteArray();
		if (m_noticeOnAutomaticExtractionOfProductSections.isEmpty()) {
			PLS_INFO(
				"SyncServer",
				"sync server status: get naver shopping notice on automatic extraction of product sectionss info from app default value json file , the m_noticeOnAutomaticExtractionOfProductSections is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get naver shopping notice on automatic extraction of product sectionss info finished , the m_noticeOnAutomaticExtractionOfProductSections is %1")
			      .arg(m_noticeOnAutomaticExtractionOfProductSections);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_noticeOnAutomaticExtractionOfProductSections;
}

const QJsonArray &PLSSyncServerManager::getProductCategoryJsonArray()
{
	if (m_productCategoryJsonArray.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get navershopping rhythmicity product array from AppData or App Bundle , the m_productCategoryJsonArray is empty");
		updatePolicyPublishByteArray();
		if (m_productCategoryJsonArray.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get navershopping rhythmicity product array from app default value json file , the m_productCategoryJsonArray is empty");
			initNaverShoppingInfo(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get navershopping rhythmicity product array finished, the m_productCategoryJsonArray count() is %1").arg(m_productCategoryJsonArray.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_productCategoryJsonArray;
}

std::vector<std::string> PLSSyncServerManager::getGPUBlacklist(const QString &jsonName)
{
	QJsonObject jsonObject;
	getSyncServerJsonObject(jsonName, jsonObject);

	std::vector<std::string> gpuModels;
	QJsonValue blacklist = jsonObject.value(name2str(blacklist));
	if (!blacklist.isObject()) {
		return gpuModels;
	}

	auto value = blacklist.toObject().value(name2str(gpuModels));
	if (!value.isArray()) {
		return gpuModels;
	}

	QVariantList list = value.toArray().toVariantList();

	for (const auto &item : list) {
		gpuModels.push_back(item.toString().toStdString());
	}

	return gpuModels;
}

const QString &PLSSyncServerManager::getOpenSourceLicense()
{
	if (m_openSourceLicense.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get open souerce info from AppData or App Bundle , the m_openSourceLicense is empty");
		updatePolicyPublishByteArray();
		if (m_openSourceLicense.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get open source info from app default value json file , the m_openSourceLicense is empty");
			initOpenSourceInfo(m_openSourceDefaultValueObject);
		}
	}
	return m_openSourceLicense;
}

void PLSSyncServerManager::onReceiveLibraryNeedUpdate() 
{
	PLS_INFO("SyncServer", "sync server status: library request finished, start update json object data");
	updatePolicyPublishByteArray();
}
