
#include "PLSSyncServerManager.hpp"
#include "frontend-internal.hpp"
#include "PLSChannelDataAPI.h"
#include "PLSChannelSupportVideEncoder.h"
#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSPlatformApi.h"
#include "PLSServerStreamHandler.hpp"
#include "login-user-info.hpp"
#include "utils-api.h"
#include "PLSLoginDataHandler.h"

#define PUBLISH_VERSION QStringLiteral("version")
#define POLICY_PUBLISH_JSON_NAME QStringLiteral("Policy_Publish.json")
#define POLICY_PLATFORM_JSON_NAME QStringLiteral("Policy_Platform.json")
#define WATER_MARK_JSON_NAME QStringLiteral("Policy_Watermark_Config.json")
#define OUTRO_JSON_NAME QStringLiteral("Policy_Outro_Config.json")
#define STICKER_JSON_NAME QStringLiteral("Prism_Sticker_Reaction.json")
#define CHAT_PLATFORM_IMG_PATH QStringLiteral("chat/platform_img/")

#define OPEN_SOURCE_FILE_SUFFIX QStringLiteral("html")
#define OPEN_SOURCE_FILE_PREFIX QStringLiteral("License ")

using namespace common;

struct CategoryLibrary : public pls::rsm::ICategory {
	PLS_RSM_CATEGORY(CategoryLibrary)

	QString categoryId(pls::rsm::IResourceManager *mgr) const override { return PLS_RSM_CID_LIBRARY; }

	QString getLibraryItemId() const { return pls_prism_is_dev() ? QStringLiteral("LIBRARY_1452") : QStringLiteral("LIBRARY_1225"); }

	bool itemNeedDownload(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override
	{
		if (item.itemId() == getLibraryItemId())
			return true;
		return false;
	}
	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override
	{
		PLS_INFO(moduleName(), "getItemDownloadUrlAndHowSaves library %s", item.itemId().toUtf8().constData());
		urlAndHowSaves.push_back(pls::rsm::UrlAndHowSave() //
						 .names({QStringLiteral("resourceUrl")})
						 .fileName(pls::rsm::FileName::FromUrl)
						 .needDecompress(true)
						 .decompress([](const auto &, const auto &filePath) { return pls::rsm::unzip(filePath, QString(), false); }));
	}
	void itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override
	{
		PLS_INFO(moduleName(), "itemDownloaded library %s %s", item.itemId().toUtf8().constData(), ok ? "ok" : "failed");

		pls_async_call_mt([this, item, ok]() {
			PLS_SYNC_SERVER_MANAGE->libraryNeedUpdate(ok);
			if (ok) {

				PLSErrorHandler::instance()->loadJson();

				auto dstWebPath = pls::rsm::getAppDataPath("textTemplatePC/web");
				auto textmotionPath = item.filePath("Library_Policy_PC/textmotion/web");
				pls_async_invoke([this, dstWebPath, textmotionPath]() {
					auto isSuccess = pls_copy_dir(textmotionPath, dstWebPath);
					PLS_INFO(moduleName(), "copytextmotion res from library to textmotion dir  %s", isSuccess ? "success" : "failed");
				});
			}
		});
	}
	bool checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override { return false; }
	pls::rsm::UniqueId getItemUniqueId(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override
	{
		if (item.itemId() == getLibraryItemId())
			return PLS_RSM_UID_LIBRARY_POLICY_PC;
		return {};
	}
	QString getItemHomeDir(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override
	{
		if (item.itemId() == getLibraryItemId())
			return pls::rsm::getAppDataPath(QStringLiteral("library"));
		return {};
	}
};

PLSSyncServerManager *PLSSyncServerManager::instance()
{
	static PLSSyncServerManager syncServerManager;
	return &syncServerManager;
}

PLSSyncServerManager::PLSSyncServerManager(QObject *parent) : QObject(parent)
{
	connect(this, &PLSSyncServerManager::libraryNeedUpdate, this, &PLSSyncServerManager::onReceiveLibraryNeedUpdate);
	int appBundleVersion = 0;
	getSyncServerAppBundleJsonObject(POLICY_PUBLISH_JSON_NAME, m_policyPublishDefaultValueObject, appBundleVersion);
	QString log = QString("sync server status: read Policy_Publish.json default value , version is %1").arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	getSyncServerAppBundleJsonObject(POLICY_PLATFORM_JSON_NAME, m_policyPlatformDefaultValueObject, appBundleVersion);
	log = QString("sync server status: read Policy_Platform.json default value , version is %1").arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	getSyncServerAppBundleJsonObject(WATER_MARK_JSON_NAME, m_waterMarkDefaultValueObject, appBundleVersion);
	log = QString("sync server status: read Policy_Watermark_Config.json default value , version is %1").arg(appBundleVersion);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	getSyncServerAppBundleJsonObject(OUTRO_JSON_NAME, m_outroDefaultValueObject, appBundleVersion);
	log = QString("sync server status: read Policy_Outro_PC_Config.json default value , version is %1").arg(appBundleVersion);
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
	initRemoteControlInfo(policyPublishObject);
	initPlatformVersionInfo(policyPublishObject);
	initNaverShoppingInfo(policyPlatformObject);
	initTwitchWhipServer(policyPlatformObject);
	initSupportedPlatforms(policyPublishObject);

	QJsonObject waterMarkObject;
	getSyncServerJsonObject(WATER_MARK_JSON_NAME, waterMarkObject);
	initWaterMark(waterMarkObject);

	QJsonObject outroObject;
	getSyncServerJsonObject(OUTRO_JSON_NAME, outroObject);
	initOutroPolicy(outroObject);

	initOpenSourceInfo();
	PLS_INFO("SyncServer", "sync server status: end update policy publish and policy platform json object");
}

void PLSSyncServerManager::updateSupportedPlatforms()
{
	PLS_INFO("SyncServer", "sync server status: start update SupportedPlatforms object");
	QJsonObject policyPublishObject;
	getSyncServerJsonObject(POLICY_PUBLISH_JSON_NAME, policyPublishObject);
	initSupportedPlatforms(policyPublishObject);
	PLS_INFO("SyncServer", "sync server status: end update SupportedPlatforms object");
}

void PLSSyncServerManager::updateChatTagIcon()
{
	for (auto platformName : m_supportedPlatformsList) {
		auto path = m_supportedPlatformsMap.value(platformName).toMap().value("chatIcon").toMap().value("webIcon").toString();
		if (path.isEmpty()) {
			PLS_INFO("SyncServer", "chat tag icon path  not exist");
			continue;
		}
		auto absoluteSrcPath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH + path);
		auto absoluteDstSrcPath = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH + CHAT_PLATFORM_IMG_PATH + platformName + ".svg");

		PLS_INFO_KR("SyncServer", "copy tag icon path: srcPath = %s, dstPath = %s", absoluteSrcPath.toUtf8().constData(), absoluteDstSrcPath.toUtf8().constData());
		if (QFile::exists(absoluteSrcPath)) {

			bool isSuccess = pls_copy_file(absoluteSrcPath, absoluteDstSrcPath);
			PLS_INFO("SyncServer", "chat tag icon copy is %s", isSuccess ? "success" : "failed");
		} else {
			PLS_INFO("SyncServer", "chat tag icon not exist");
		}
	}
}

bool PLSSyncServerManager::getSyncServerAppBundleJsonObject(const QString &jsonName, QJsonObject &appBundleJsonObject, int &appBundleVersion)
{
	QString qrcLogPath = QString("prism-live-studio/PRISMLiveStudio/prism-ui/defaultSources.qrc/%1").arg(jsonName);

	QString log = QString("sync server status: start reading app qrc %1 file").arg(qrcLogPath);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	auto qrcPath = findFileInResources(ChannelData::defaultSourcePath, jsonName);
	QString errMeg;
	if (!pls_read_json(appBundleJsonObject, qrcPath, &errMeg)) {
		PLS_INFO("SyncServer", "read %s json failed, %s", qrcLogPath.toUtf8().constData(), errMeg.toUtf8().constData());
		return false;
	}

	appBundleVersion = appBundleJsonObject[PUBLISH_VERSION].toInt();
	log = QString("sync server status: %1 version is %2 , %3 file").arg(qrcLogPath).arg(appBundleVersion).arg(jsonName);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return true;
}

bool PLSSyncServerManager::getSyncServerUserFolderJsonObject(const QString &jsonName, QJsonObject &userFolderJsonObject, int &userFolderVersion)
{
	QString userFolderJsonPath = QString("%1%2").arg(CONFIGS_LIBRARY_POLICY_PATH).arg(jsonName);

	QString log = QString("sync server status: start reading user AppData Folder %1 file").arg(userFolderJsonPath);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	QString errMessage;
	if (!pls_read_json(userFolderJsonObject, pls_get_user_path(userFolderJsonPath), &errMessage)) {
		PLS_INFO("SyncServer", "read %s json failed, %s", userFolderJsonPath.toUtf8().constData(), errMessage.toUtf8().constData());
		return false;
	}
	userFolderVersion = userFolderJsonObject[PUBLISH_VERSION].toInt();
	log = QString("sync server status: %1 version is %2 , %3 file ").arg(userFolderJsonPath).arg(userFolderVersion).arg(jsonName);
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
		QString userFolderJsonPath = QString("%1%2").arg(CONFIGS_LIBRARY_POLICY_PATH).arg(jsonName);
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

void PLSSyncServerManager::initRemoteControlInfo(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init remote control info");
	QJsonValue value = policyPublishJsonObject.value(name2str(remoteControlPlatforms));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init remote control info failed ,because the  remote control value is not json object");
		return;
	}
	QJsonObject itemObject = value.toObject();
	m_remoteControlPlatformsInfo = itemObject.toVariantMap();
	QString log = QString("sync server status: init remote control info success , m_remoteControlPlatformsInfo count() is %1").arg(m_remoteControlPlatformsInfo.count());
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
		log = QString("sync server status: init navershopping voluntary review products info , m_voluntaryReviewProducts is %1 , lang is %2").arg(m_voluntaryReviewProducts).arg(lang);
		PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	}

	if (m_voluntaryReviewProducts.isEmpty()) {
		lang = "US";
		if (voluntaryReviewProductsMap.find(lang) != voluntaryReviewProductsMap.end()) {
			m_voluntaryReviewProducts = voluntaryReviewProductsMap.value(lang).toString();
			log = QString("sync server status: init navershopping voluntary review products default value info , m_voluntaryReviewProducts is %1 , lang is %2")
				      .arg(m_voluntaryReviewProducts)
				      .arg(lang);
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

void PLSSyncServerManager::initOpenSourceInfo()
{
	return;
}

void PLSSyncServerManager::initTwitchWhipServer(const QJsonObject &policyPlatformJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init Twitch WhipServer");
	QJsonValue value = policyPlatformJsonObject.value(name2str(twitch));
	if (!value.isObject()) {
		PLS_INFO("SyncServer", "sync server status: init Twitch WhipServer failed , because the Twitch WhipServer value is not json object");
		return;
	}
	QJsonObject platformObject = value.toObject();
	m_twitchWhipServer = platformObject.value(name2str(whipServer)).toString();
	QString log = QString("sync server status: init Twitch WhipServer, m_twitchWhipServer is %1").arg(m_twitchWhipServer);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initSupportedPlatforms(const QJsonObject &policyPublishJsonObject)
{
	PLS_INFO("SyncServer", "sync server status: start init supported platforms info");
	QJsonValue value = policyPublishJsonObject.value(name2str(supportedPlatforms));
	if (!value.isObject()) {
		PLS_ERROR("SyncServer", "sync server status: init supported platforms info failed ,because the supported platforms value is not json object");
		return;
	}

	QJsonObject itemObject = value.toObject();

	// get login platform list
	m_loginObject = itemObject.value(name2str(login)).toObject();

	// get channel platform list
	m_channelObject = itemObject.value(name2str(channel)).toObject();

	// get resolution guide list
	QJsonValue guideValue = itemObject.value(name2str(resolutionGuide));
	if (!guideValue.isArray()) {
		PLS_ERROR("SyncServer", "sync server status: init supported platforms info failed ,because the resolutionGuide value is not json array");
		return;
	}
	m_newResolutionGuide = guideValue.toArray().toVariantList();

	m_supportedPlatformsMap.clear();
	m_supportedPlatformsList.clear();

	QList<QString> GpopChannelList = PLSGpopData::instance()->getChannelList();
	for (auto name : GpopChannelList) {
		QJsonObject::iterator it = m_channelObject.find(name);
		if (it != m_channelObject.end()) {
			QVariantMap map = (*it).toObject().toVariantMap();
			if (map.isEmpty()) {
				m_supportedPlatformsMap.insert(name, map);
				m_supportedPlatformsList.append(name);
			} else {
				auto platform = map.value(name2str(platform)).toString();
				auto serviceName = map.value(name2str(serviceName)).toString();
				if (!serviceName.isEmpty()) {
					QString userLoginPlatform = PLSLoginUserInfo::getInstance()->getLoginPlatformName();
					QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
					if (platform == userLoginPlatform && serviceName == userServiceName) {
						m_supportedPlatformsMap.insert(name, map);
						m_supportedPlatformsList.append(name);
					}
				} else {
					m_supportedPlatformsMap.insert(name, map);
					m_supportedPlatformsList.append(name);
				}
			}
		} else {
			auto map = PLSLoginDataHandler::instance()->getCustomChannelObj().toVariantMap();
			m_supportedPlatformsMap.insert(name, map);
			m_supportedPlatformsList.append(name);
		}
	}

	QString log = QString("sync server status: init supported platforms info success ,m_supportedPlatforms count() is %1").arg(m_supportedPlatformsList.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initWaterMark(const QJsonObject &waterMarkDefaultValueObject)
{
	PLS_INFO("SyncServer", "sync server status: start init WaterMark info");
	auto watermarkMap = waterMarkDefaultValueObject.toVariantMap();
	if (watermarkMap.isEmpty()) {
		PLS_ERROR("SyncServer", "sync server status: init WaterMark info failed");
		return;
	}
	m_watermarkObject = waterMarkDefaultValueObject;
	QString log = QString("sync server status: init WaterMark info success ,watermark count() is %1").arg(watermarkMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

void PLSSyncServerManager::initOutroPolicy(const QJsonObject &outroDefaultValueObject)
{
	PLS_INFO("SyncServer", "sync server status: start OutroPolicy info");

	auto outroMap = outroDefaultValueObject.toVariantMap();
	if (outroMap.isEmpty()) {
		PLS_ERROR("SyncServer", "sync server status: init OutroPolicy info failed");
		return;
	}
	m_outroObject = outroDefaultValueObject;
	QString log = QString("sync server status: init OutroPolicy success ,outroMap count() is %1").arg(outroMap.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
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
	QString log =
		QString("sync server status: init rtmp destination info success , m_rtmpFPSMap count() is %1 , m_destinations count() is %2").arg(m_rtmpFPSMap.count()).arg(m_destinations.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
}

const QVariantList &PLSSyncServerManager::getResolutionsList()
{
	if (!m_resolutionsInfos.isEmpty()) {
		return m_resolutionsInfos;
	}
	auto list1 = getNewResolutionGuide();
	QVariantList list2;
	QString path = pls_get_user_path(CONFIGS_RESOLUTIONGUIDE_PATH);
	pls_read_json(list2, path);

	if (list2.isEmpty()) {
		auto qrcPath = findFileInResources(ChannelData::defaultSourcePath, name2str(ResolutionGuide));
		pls_read_json(list2, qrcPath);
	}

	list2.append(list1);
	QStringList GpopResolutionsList = PLSGpopData::instance()->getChannelResolutionGuidList();
	for (auto name : GpopResolutionsList) {
		for (const auto &data : list2) {
			auto channelName = data.toMap().value(name2str(channel_name)).toString();
			auto platform = data.toMap().value(name2str(platform)).toString();
			auto serviceName = data.toMap().value(name2str(serviceName)).toString();
			if (name == channelName) {
				if (platform.isEmpty() && serviceName.isEmpty()) {
					m_resolutionsInfos.append(data);
					break;
				} else {
					QString userLoginPlatform = PLSLoginUserInfo::getInstance()->getLoginPlatformName();
					QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
					if (platform == userLoginPlatform && serviceName == userServiceName) {
						m_resolutionsInfos.append(data);
						break;
					}
				}
			}
		}
	}
	return m_resolutionsInfos;
}

const QMap<QString, QVariantList> &PLSSyncServerManager::getStickerReaction()
{
	if (!m_reaction.isEmpty()) {
		return m_reaction;
	}
	QJsonObject obj;
	QString path = pls_get_user_path(CONFIGS_LIBRARY_POLICY_PATH + STICKER_JSON_NAME);
	if (auto isOk = pls_read_json(obj, path); isOk) {
		QVariantMap reaction;
		auto value = pls_find_attr(obj, name2str(reaction));
		if (value.has_value() && value.value().isObject()) {
			reaction = value.value().toObject().toVariantMap();
			for (auto key : reaction.keys()) {
				QVariantList list;
				value = pls_find_attr(obj, key);
				if (value.has_value() && value.value().isArray()) {
					list = value.value().toArray().toVariantList();
					m_reaction.insert(key, list);
				}
			}
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
	} else if (platformName == NCB2B) {
		platformNameKey = "ncp";
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

QString PLSSyncServerManager::getRemoteControlMobilePlatform(const QString &platformName)
{

	if (m_remoteControlPlatformsInfo.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get remote control info from AppData or App Bundle , the m_remoteControlPlatformsInfo is empty");
		updatePolicyPublishByteArray();
		if (m_remoteControlPlatformsInfo.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get remote control info from app default value json file , the m_remoteControlPlatformsInfo is empty");
			initRemoteControlInfo(m_policyPublishDefaultValueObject);
		}
	}

	QVariantMap platformMap = m_remoteControlPlatformsInfo.value(platformName).toMap();
	QString mobilePlatformName = platformMap.value(name2str(mobilePlatform)).toString();
	QString log = QString("sync server status: get remote control info finished , platformName is %1 , mobilePlatformName is %2").arg(platformName).arg(mobilePlatformName);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	return mobilePlatformName;
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
		PLS_INFO(
			"SyncServer",
			"sync server status: get naver shopping notice on automatic extraction of product sectionss info from AppData or App Bundle , the m_noticeOnAutomaticExtractionOfProductSections is empty");
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
			initOpenSourceInfo();
		}
	}
	return m_openSourceLicense;
}

const QString &PLSSyncServerManager::getTwitchWhipServer()
{

	if (m_twitchWhipServer.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get twitch WhipServer info from AppData or App Bundle , the m_twitchWhipServer is empty");
		updatePolicyPublishByteArray();
		if (m_twitchWhipServer.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: get twitch WhipServer info from app default value json file , the m_twitchWhipServer is empty");
			initTwitchWhipServer(m_policyPlatformDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get twitch WhipServer info finished , the m_twitchWhipServer is %1").arg(m_twitchWhipServer);
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());

	return m_twitchWhipServer;
}

const QStringList &PLSSyncServerManager::getSupportedPlatformsList()
{
	if (m_supportedPlatformsList.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_supportedPlatforms from AppData or App Bundle , the m_supportedPlatforms is empty");
		updatePolicyPublishByteArray();
		if (m_supportedPlatformsList.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_supportedPlatforms from app default value json file , the m_supportedPlatforms is empty");
			initSupportedPlatforms(m_policyPublishDefaultValueObject);
		}
	}
	return m_supportedPlatformsList;
}

const QVariantMap &PLSSyncServerManager::getSupportedPlatformsMap()
{
	if (m_supportedPlatformsMap.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_supportedPlatformsMap from AppData or App Bundle , the m_supportedPlatformsMap is empty");
		updatePolicyPublishByteArray();
		if (m_supportedPlatformsMap.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_supportedPlatformsMap from app default value json file , the m_supportedPlatformsMap is empty");
			initSupportedPlatforms(m_policyPublishDefaultValueObject);
		}
	}
	return m_supportedPlatformsMap;
}

const QVariantList &PLSSyncServerManager::getNewResolutionGuide()
{
	if (m_newResolutionGuide.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_newResolutionGuide from AppData or App Bundle , the m_newResolutionGuide is empty");
		updatePolicyPublishByteArray();
		if (m_newResolutionGuide.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_newResolutionGuide from app default value json file , the m_newResolutionGuide is empty");
			initSupportedPlatforms(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get m_newResolutionGuide finished , m_newResolutionGuide count() is %1").arg(m_newResolutionGuide.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_newResolutionGuide;
}

const QJsonObject &PLSSyncServerManager::getLoginObject()
{
	if (m_loginObject.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_loginObject from AppData or App Bundle , the m_loginObject is empty");
		updatePolicyPublishByteArray();
		if (m_loginObject.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_loginObject from app default value json file , the m_loginObject is empty");
			initSupportedPlatforms(m_policyPublishDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get m_loginObject finished , m_loginObject count() is %1").arg(m_loginObject.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_loginObject;
}

const QJsonObject &PLSSyncServerManager::getWaterMarkConfigObject()
{
	if (m_watermarkObject.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_watermarkObject from AppData or App Bundle , the m_watermarkObject is empty");
		updatePolicyPublishByteArray();
		if (m_watermarkObject.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_watermarkObject from app default value json file , the m_watermarkObject is empty");
			initWaterMark(m_waterMarkDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get m_watermarkObject finished , m_watermarkObject count() is %1").arg(m_watermarkObject.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_watermarkObject;
}

const QJsonObject &PLSSyncServerManager::getOutroPolicyConfigObject()
{
	if (m_outroObject.isEmpty()) {
		PLS_INFO("SyncServer", "sync server status: get m_outroObject from AppData or App Bundle , the m_outroObject is empty");
		updatePolicyPublishByteArray();
		if (m_outroObject.isEmpty()) {
			PLS_INFO("SyncServer", "sync server status: m_outroObject from app default value json file , the m_outroObject is empty");
			initOutroPolicy(m_outroDefaultValueObject);
		}
	}
	QString log = QString("sync server status: get m_outroObject finished , m_outroObject count() is %1").arg(m_outroObject.count());
	PLS_INFO("SyncServer", "%s", log.toUtf8().constData());
	return m_outroObject;
}

const QString &PLSSyncServerManager::getWaterMarkResLocalPath(const QString &platformName)
{
	m_watermarkLocalPath.clear();
	if (NCB2B == platformName) {
		m_watermarkLocalPath = PLSLoginDataHandler::instance()->getNCB2BServiceWatermark();
	}
	PLS_INFO("SyncServer", "getWaterMarkResLocalPath: %s", m_watermarkLocalPath.toUtf8().constData());
	return m_watermarkLocalPath;
}

const QVariantMap &PLSSyncServerManager::getOutroResLocalPathAndText(const QString &platformName)
{
	m_outroPathAndText.clear();
	if (NCB2B == platformName) {
		QString outroPath = PLSLoginDataHandler::instance()->getNCB2BServiceOutro();
		m_outroPathAndText.insert(OUTRO_PATH, outroPath);
		auto resObject = PLSLoginDataHandler::instance()->getNCB2BServiceConnfigRes();
		QString outroText = resObject.value("serviceOutroText").toString();
		if (outroText.isEmpty()) {
			PLS_WARN("SyncServer", "Outro text is empty");
		}
		m_outroPathAndText.insert(OUTRO_TEXT, outroText);
	}
	PLS_INFO("SyncServer", "getOutroResLocalPath: %s,text :%s", m_outroPathAndText.value(OUTRO_PATH).toString().toUtf8().constData(),
		 m_outroPathAndText.value(OUTRO_TEXT).toString().toUtf8().constData());
	return m_outroPathAndText;
}

int PLSSyncServerManager::compareVersion(const QString &v1, const QString &v2) const
{
	QStringList v1List = v1.split(".");
	QStringList v2List = v2.split(".");
	auto len1 = v1List.count();
	auto len2 = v2List.count();
	for (int i = 0; i < qMin(len1, len2); i++) {
		if (v1List.at(i).toUInt() > v2List.at(i).toUInt()) {
			return 1;
		} else if (v1List.at(i).toUInt() < v2List.at(i).toUInt()) {
			return -1;
		}
	}
	return 0;
}

void PLSSyncServerManager::onReceiveLibraryNeedUpdate(bool isSucceed)
{
	if (!isSucceed) {
		PLS_WARN("SyncServer", "sync server status: library request fail, maybe use qrc json");
		return;
	}
	PLS_INFO("SyncServer", "sync server status: library request finished, start update json object data");
	updatePolicyPublishByteArray();
}
