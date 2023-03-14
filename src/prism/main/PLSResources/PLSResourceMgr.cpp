#include "PLSResourceMgr.h"
#include <QDir>
#include <qapplication.h>
#include "json-data-handler.hpp"
#include "PLSBeautyResHandler.h"
#include "PLSColorResHandler.h"
#include "PLSLibraryResHandler.h"
#include "PLSStickerResHandler.h"
#include "PLSTMResHandler.h"
#include "PLSSenseTimeResHandler.h"
#include "PLSMotionNetwork.h"

#define PRISM_SSL pls_get_gpop_connection().ssl
#define PRISM_CATETORY_PATH "PRISMLiveStudio/user/categorys.json"

PLSResourceMgr::PLSResourceMgr()
{
	getLocalResourceInfo();
}
PLSResourceMgr *PLSResourceMgr::instance()
{
	static PLSResourceMgr resMgr;
	return &resMgr;
}
void PLSResourceMgr::getNewResourceJson(downloadCategoryResCallback callback)
{

	auto _onSuccessed = [=](QNetworkReply * /*networkReplay*/, int /*code*/, QByteArray data, void * /*context*/) {
		m_bSuccessDownloadCategory = true;
		parseResourceJson(m_newResourceInfos, data);
		downloadAllResources(ResourceFlag::Virtual_bg);
		checkResourceUpdate();
		PLSJsonDataHandler::saveJsonFile(data, pls_get_user_path(PRISM_CATETORY_PATH));
		PLSSenseTimeResHandler::checkAndDownloadSensetimeResource();
	};
	auto _onFail = [=](QNetworkReply * /*networkReplay*/, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		//TODO: down category json failed handle
		PLS_ERROR("downloadRes", "download category json falied");
		m_bSuccessDownloadCategory = false;
		callback(m_bSuccessDownloadCategory, m_isAlreadyTips);
	};

	PLSHmacNetworkReplyBuilder builder(PLS_CATEGORY.arg(PRISM_SSL), HmacType::HT_PRISM);
	PLS_HTTP_HELPER->connectFinished(builder.get(), nullptr, _onSuccessed, _onFail);
}

QString PLSResourceMgr::getResourceJson(ResourceFlag resourceFlag)
{
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();
	QString resourceKey = metaEnum.valueToKey(static_cast<int>(resourceFlag));
	return m_newResourceInfos.value(resourceKey.toLower()).resourceUrl;
}

PLSResourceMgr::~PLSResourceMgr()
{
	//abort();
}

void PLSResourceMgr::getLocalResourceInfo()
{
	QString categoryPath = pls_get_user_path(PRISM_CATETORY_PATH);
	if (!QFileInfo(categoryPath).exists()) {
		QDir appDir(qApp->applicationDirPath());
		categoryPath = appDir.absoluteFilePath("data/prism-studio/user/categorys.json");
	}
	PLSJsonDataHandler::getJsonArrayFromFile(m_LocalCategoriesJsonData, categoryPath);
	parseResourceJson(m_localeResourceInfos, m_LocalCategoriesJsonData);
}
void PLSResourceMgr::parseResourceJson(QMap<QString, ResourceInfo> &resourceInfos, const QByteArray &resourceJsonData)
{
	if (PLSJsonDataHandler::isValidJsonFormat(resourceJsonData)) {

		QJsonArray items = QJsonDocument::fromJson(resourceJsonData).array();
		for (auto item : items) {
			auto id = item.toObject().value("categoryId").toString();
			ResourceInfo info;
			info.resourceUrl = item.toObject().value("resourceUrl").toString();
			info.version = item.toObject().value("version").toInt();
			resourceInfos.insert(id, info);
		}
	}
}
void PLSResourceMgr::checkResourceUpdate()
{
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();
	for (int i = 0; i < metaEnum.keyCount(); i++) {
		QString enumName = QString(metaEnum.key(i)).toLower();
		ResourceFlag resourceFlag = static_cast<ResourceFlag>(metaEnum.keyToValue(metaEnum.key(i)));
		QString resourcePath = getResourcePath(resourceFlag);
		PLSResourceHandler *resourceHandler = nullptr;
		if (m_newResourceInfos.find(enumName) != m_newResourceInfos.end()) {
			if (m_localeResourceInfos.find(enumName) != m_localeResourceInfos.end()) {
				//match resource version
				auto newResInfo = m_newResourceInfos[enumName];
				auto localResInfo = m_localeResourceInfos[enumName];
				if (localResInfo.version != newResInfo.version || isNotExistPath(resourcePath, resourceFlag)) {
					resourceHandler = creatResourceHandler(static_cast<ResourceFlag>(metaEnum.keyToValue(metaEnum.key(i))), {newResInfo.resourceUrl}, enumName, resourcePath);
				} else {
					PLS_INFO("downloadRes", "%s resource is lastest not need update", enumName.toUtf8().constData());
				}
			} else {
				//locale categories file not exist
				auto newResInfo = m_newResourceInfos[enumName];
				resourceHandler = creatResourceHandler(resourceFlag, {newResInfo.resourceUrl}, enumName, resourcePath);
			}
		}
		if (resourceHandler) {
			resourceHandler->startDownLoadRes();
		}
	}
	auto libResHandler = getResourceHandler(ResourceFlag::Library);
	if (libResHandler) {
		PLS_INFO("downloadRes", "senseTime resouce check new version nee udpate");
		auto senseTimeRes = creatResourceHandler(ResourceFlag::SenseTime, {getResourceJson(ResourceFlag::Library)}, "SenseTime", getResourcePath(ResourceFlag::Beauty));
		if (senseTimeRes) {
			senseTimeRes->startDownLoadRes();
		}
	}
}

PLSResourceHandler *PLSResourceMgr::creatResourceHandler(ResourceFlag resourceFlag, const QStringList &urls, const QString &resourceId, const QString &relativePath)
{
	PLSResourceHandler *resoureHandler = nullptr;
	switch (resourceFlag) {
	case ResourceFlag::Beauty:
		resoureHandler = new PLSBeautyResHandler(urls);
		break;
	case ResourceFlag::Color:
		resoureHandler = new PLSColorResHandler(urls);
		break;
	case ResourceFlag::Library:
		resoureHandler = new PLSLibraryResHandler(urls);
		break;
	case ResourceFlag::Template:
		resoureHandler = new PLSTMResHandler(urls);
		break;
	case ResourceFlag::Reaction:
		resoureHandler = new PLSStickerResHandler(urls);
		break;
	case ResourceFlag::SenseTime:
		resoureHandler = new PLSSenseTimeResHandler(urls, false);
		break;
	case ResourceFlag::Virtual_bg: {
		resoureHandler = new PLSResourceHandler(urls);
		resoureHandler->setResFileName(VIRTUALFILENAME);
		QJsonObject obj;
		obj["resourceUrl"] = urls.first();
		PLSMotionNetwork::instance()->downloadResource(obj);
	} break;

	case ResourceFlag::Music:
		resoureHandler = new PLSResourceHandler(urls);
		resoureHandler->setResFileName(MUSIC_JSON_FILE);
		break;
	default:
		break;
	}
	if (resoureHandler) {
		m_resourceHandlers.insert(resourceFlag, resoureHandler);
		resoureHandler->setRelativeResPath(relativePath);
		resoureHandler->setItemName(resourceId);
	}
	PLS_INFO("downloadRes", "create %s instance to start sub thread ", resourceId.toUtf8().constData());
	return resoureHandler;
}

QString PLSResourceMgr::getResourcePath(ResourceFlag resourceFlag)
{
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();

	QString relativePath = metaEnum.valueToKey(static_cast<int>(resourceFlag));
	switch (resourceFlag) {
	case ResourceFlag::Beauty:
		break;
	case ResourceFlag::Color:
		relativePath = "color_filter";
		break;
	case ResourceFlag::Library:
		relativePath = "user/Library_Policy_PC";
		break;
	case ResourceFlag::Template:
		relativePath = "textmotion";
		break;
	case ResourceFlag::Virtual_bg:
		relativePath = "virtual";
		break;
	case ResourceFlag::Reaction:
		relativePath = "prism_sticker";
		break;
	case ResourceFlag::Music:
	default:
		break;
	}
	relativePath = pls_get_user_path(QString("PRISMLiveStudio/%1/").arg(relativePath)).toLower();
	return relativePath;
}

bool PLSResourceMgr::isNotExistPath(const QString &path, ResourceFlag /*resourceFlag*/)
{
	QDir dir(path);
	bool isNotExist = dir.isEmpty() || (!dir.exists());
	return isNotExist;
}

void PLSResourceMgr::downloadPartResources(ResourceFlag resourceFlag, const QMap<QString, QString> resourceUrlToPaths)
{
	PLSResourceHandler *resourceHandler = nullptr;
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();

	if (m_resourceHandlers.find(resourceFlag) != m_resourceHandlers.end()) {
		resourceHandler = m_resourceHandlers.value(resourceFlag);
	} else {
		resourceHandler = creatResourceHandler(resourceFlag, resourceUrlToPaths.keys(), metaEnum.valueToKey(static_cast<int>(resourceFlag)), getResourcePath(resourceFlag));
	}
	resourceHandler->startDownLoadRes(DownLoadResourceMode::Part, resourceUrlToPaths);
}

void PLSResourceMgr::downloadAllResources(ResourceFlag resourceFlag)
{
	PLSResourceHandler *resourceHandler = nullptr;
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();

	if (m_resourceHandlers.find(resourceFlag) != m_resourceHandlers.end()) {
		resourceHandler = m_resourceHandlers.value(resourceFlag);
		QString url = getResourceJson(resourceFlag);
		resourceHandler->setResUrls({url});
	} else {
		resourceHandler = creatResourceHandler(resourceFlag, {getResourceJson(resourceFlag)}, metaEnum.valueToKey(static_cast<int>(resourceFlag)), getResourcePath(resourceFlag));
	}
	if (resourceHandler) {
		resourceHandler->startDownLoadRes();
	}
}

PLSResourceHandler *PLSResourceMgr::getResourceHandler(ResourceFlag resourceFlag)
{
	if (m_resourceHandlers.find(resourceFlag) != m_resourceHandlers.end()) {
		return m_resourceHandlers.value(resourceFlag);
	}
	return nullptr;
}

void PLSResourceMgr::downloadSenseTimeResources()
{
	PLSResourceHandler *resourceHandler = nullptr;
	QMetaEnum metaEnum = QMetaEnum::fromType<ResourceFlag>();

	if (m_resourceHandlers.find(ResourceFlag::SenseTime) != m_resourceHandlers.end()) {
		resourceHandler = m_resourceHandlers.value(ResourceFlag::SenseTime);
		QString url = getResourceJson(ResourceFlag::Library);
		resourceHandler->setResUrls({url});
	} else {
		resourceHandler = creatResourceHandler(ResourceFlag::SenseTime, {getResourceJson(ResourceFlag::Library)}, "SenseTime", getResourcePath(ResourceFlag::Beauty));
	}
	if (resourceHandler) {
		resourceHandler->startDownLoadRes();
	}
}

bool PLSResourceMgr::isSuccessDownloadCategory()
{
	return m_bSuccessDownloadCategory;
}

void PLSResourceMgr::setAleadyShowTips()
{
	m_isAlreadyTips = true;
}

bool PLSResourceMgr::isAleadyShowTips()
{
	return m_isAlreadyTips;
}

void PLSResourceMgr::abort()
{
	for (auto handler = m_resourceHandlers.constBegin(); handler != m_resourceHandlers.constEnd(); ++handler) {
		auto pHandler = handler.value();
		if (pHandler) {
			pHandler->abort();
		}
	}
}
