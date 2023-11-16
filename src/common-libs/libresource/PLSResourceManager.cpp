#include "PLSResourceManager.h"
#include "PLSResCommonFuns.h"
#include "PLSCommonConst.h"
#include "PLSLicenseManager.h"
#include "libutils-api.h"
#include "liblog.h"
#include "pls-common-define.hpp"
#include <qcoreapplication.h>
#include <QRandomGenerator>
#include <set>
#include <QFile>
QVariantHash PLSResourceManager::m_paramData = QVariantHash();

PLSResourceManager::PLSResourceManager()
{
#if defined(Q_OS_WIN)
	auto appDir = pls_get_dll_dir("libresource") + "/../../data/prism-studio";
#elif defined(Q_OS_MACOS)
	auto appDir = pls_get_app_resource_dir() + "/data/prism-studio";
#endif
	m_secretDir = pls_get_app_data_dir() + "secret";
	m_resDir = makePath(QString(pls_res_const::resourceDir).arg(appDir));
	m_resTemplateDir = makePath(QString(pls_res_const::resTemplateDir).arg(appDir));
	m_resCacheDir = makePath(QString(pls_res_const::resCacheDir).arg(m_secretDir));
	m_resTempDir = makePath(QString(pls_res_const::resTmpDir).arg(m_secretDir));
	getOldResCahe();
	getLocalCategoriesJson();
}

QString PLSResourceManager::getmoduleName(resource_modules resourceModules)
{
	const auto metaEnum = QMetaEnum::fromType<resource_modules>();
	return QString(metaEnum.valueToKey(static_cast<int>(resourceModules))).toLower();
}

PLSResourceManager::resource_modules PLSResourceManager::getModuleId(const QString &moduleIdStr)
{
	const auto metaEnum = QMetaEnum::fromType<resource_modules>();
	return static_cast<resource_modules>(metaEnum.keyToValue(moduleIdStr.toUtf8().data()));
}

QString PLSResourceManager::resourceModuleTocategoryId(resource_modules resourceModules) //The string is the same as the field in the category.json file
{
	switch (resourceModules) {
	case resource_modules::Beauty:
		return "beauty";
	case resource_modules::Color_Filter:
		return "color";
	case resource_modules::Library:
		return "library";
	case resource_modules::Textmotion:
		return "textTemplatePC";
	case resource_modules::Reaction:
		return "reaction";
	case resource_modules::Virtual:
		return "virtual_bg";
	case resource_modules::Music:
		return "music";
	default:
		return "";
	}
}

void PLSResourceManager::setParams(const QVariantHash &paramsData)
{
	m_paramData = paramsData;
	PLSResCommonFuns::setGcc(paramsData.value(pls_res_const::gcc).toString());
}

QVariantHash &PLSResourceManager::getParams()
{
	return m_paramData;
}

QString PLSResourceManager::getResourceTmpFilePath(const QString &fileName) const
{
	auto filePath = m_resTempDir + "/" + fileName;
	return filePath;
}

void PLSResourceManager::delInvalidPath(QString &path) const
{
	path.replace("//", "/");
}

void PLSResourceManager::getOldResCahe()
{
	auto fileInfos = getFiles(m_resCacheDir, {"*.json"});
	for (auto file : fileInfos) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "get old res cache file name = %s", file.fileName().toUtf8().constData());
		auto array = QJsonDocument::fromJson(PLSResCommonFuns::readFile(file.absoluteFilePath())).array();
		m_oldDownloadFileList.insert(file.baseName(), array);
	}
}

void PLSResourceManager::getLocalCategoriesJson()
{
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "get local category json");
	auto localCategoryPath = PLSResCommonFuns::getAppLocationPath() + "/resources/" + pls_res_const::categoryName;
	if (!QFileInfo(localCategoryPath).exists()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "local category json not exist, will use app data dir.");
		QDir appDir(pls_get_dll_dir("libresource"));
#if defined(Q_OS_WIN)
		localCategoryPath = appDir.absoluteFilePath(QString("../../data/prism-studio/user/%1").arg(pls_res_const::categoryName));

#elif defined(Q_OS_MACOS)
		localCategoryPath = pls_get_app_resource_dir() + (QString("/data/prism-studio/user/%1").arg(pls_res_const::categoryName));

#endif // defined(Q_OS_WIN)
	}

	auto categoriesData = PLSResCommonFuns::readFile(localCategoryPath);
	parseResourceJson(m_localCategoriesData, categoriesData);
}

PLSResourceManager *PLSResourceManager::instance()
{
	static PLSResourceManager resourceMgr;
	return &resourceMgr;
}

void PLSResourceManager::startCheckResource()
{
	PLS_INFO("ResourceMgr", __FUNCTION__);

	auto categoryTempPath = m_secretDir + '/' + pls_res_const::categoryName;
	auto categoryPath = PLSResCommonFuns::getAppLocationPath() + "/resources/" + pls_res_const::categoryName;
	auto result_ = [this, categoryTempPath, categoryPath](PLSResEvents resEvent, const QString &) {
		bool isSuccess = false;
		QByteArray categoriesData;
		if (PLSResEvents::RES_DOWNLOAD_FAILED == resEvent) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "category.json download failed and use local category");
			isSuccess = QFile::exists(categoryPath);
		} else if (PLSResEvents::RES_DOWNLOAD_SUCCESS == resEvent) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "category.json download success and use new category");
			isSuccess = PLSResCommonFuns::copyFile(categoryTempPath, categoryPath);
		}
		categoriesData = isSuccess ? PLSResCommonFuns::readFile(categoryPath)
					   : PLSResCommonFuns::readFile(pls_get_dll_dir("libresources-download") + "/data/prism-studio/user/" + pls_res_const::categoryName);
		parseResourceJson(m_newCategoriesData, categoriesData);
		appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
		checkResourceUpdate();
	};
	auto categoryUrl = QString("%1%2").arg(pls_http_api_func::getPrismSynGateWay()).arg(pls_resource_const::PLS_CATEGORY_URL);
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start request new category.json");
	PLSResCommonFuns::downloadResource(categoryUrl, result_, categoryTempPath, false, true);
}

QHash<QString, QVariant> PLSResourceManager::getALLNeedDownloadResInfo() const
{
	saveCaheJsonNeed();
	QVariantHash hash;
	for (auto needdownFileInter = m_needDownloadFileList.constBegin(); needdownFileInter != m_needDownloadFileList.constEnd(); ++needdownFileInter) {
		hash.insert(needdownFileInter.key(), needdownFileInter.value());
	}
	qDebug() << hash;
	return {{"resCheck", hash}};
}
qint64 PLSResourceManager::getTotalDownloadNum() const
{
	return m_totalNums;
}
qint64 PLSResourceManager::getCurrentDownloadNum() const
{
	return m_downloadedNums;
}
void PLSResourceManager::startDownloadResources(const QHash<QString, QVariant> &needDownLoadFileList)
{
	PLS_INFO("ResourceManager", __FUNCTION__);
	m_totalNums = 0;
	m_downloadedNums = 0;
	for (auto downInter = needDownLoadFileList.constBegin(); downInter != needDownLoadFileList.constEnd(); ++downInter) {
		auto fileList = downInter.value().toJsonArray();
		auto fileKey = downInter.key();
		m_totalNums += fileList.size();
		PLSResCommonFuns::downloadResources(fileList, nullptr, true, false);
	}
}

QJsonArray PLSResourceManager::getNeedDownloadResInfo(const QString &moduleName) const
{
	QJsonArray needDownloadFileList;
	QJsonArray fileArray;
	auto filePath = m_resCacheDir + QString("/%1.json").arg(moduleName);
	if (!pls_read_json(fileArray, filePath)) {
		return needDownloadFileList;
	}
	for (auto fileInfo : fileArray) {
		auto obj = fileInfo.toObject();
		auto type = obj.value("type").toString();

		if (type == "zip") {
			findZipSubCaheInfo(needDownloadFileList, obj);

		} else {
			auto filePath_ = obj.value(pls_res_const::resourcePath).toString();
			if (!isFileExist(filePath_)) {
				needDownloadFileList.append(obj);
			}
		}
	}
	return needDownloadFileList;
}

QString PLSResourceManager::getModuleJsonUrl(resource_modules modules)
{
	getLocalCategoriesJson();
	auto url = m_localCategoriesData.value(resourceModuleTocategoryId(modules)).value(pls_res_const::resourceModuleUrl).toString();
	return url;
}

void PLSResourceManager::appendResourceNum(qint64 resourceNum)
{
	m_totalNums += resourceNum;
}

void PLSResourceManager::appendResourceDownloadedNum(qint64 downloadedNum)
{
	m_downloadedNums += downloadedNum;
}

void PLSResourceManager::downloadedZipHandle(const QString &zipFilePath, const QString &zipName, const QJsonArray &fileListInfo, const QString &dstDir) const
{
	pls_unused(dstDir);
	if (!isFileExist(zipFilePath)) {
		return;
	}
	if (!fileListInfo.isEmpty()) {
		QFileInfoList fileList;

		bool isSuccess = PLSResCommonFuns::unZip(m_resTempDir, zipFilePath, zipName, true);
		if (isSuccess) {
			PLSResCommonFuns::findAllFiles(QString("%1/%2").arg(m_resTempDir).arg(zipName.section('.', 0, 0)), fileList);
		}
		for (auto resInfo : fileListInfo) {
			auto webName = resInfo.toObject().value(pls_res_const::resourceWebName).toString();
			auto resName = resInfo.toObject().value(pls_res_const::resourceName).toString();
			auto resPath = resInfo.toObject().value(pls_res_const::resourcePath).toString();

			auto inter = std::find_if(fileList.constBegin(), fileList.constEnd(), [webName](QFileInfo fileInfo) { return (fileInfo.fileName() == webName); });
			if (inter == fileList.constEnd()) {
				continue;
			}
			PLSResCommonFuns::copyFile(inter->absoluteFilePath(), resPath);
			if (webName != resName) {
				QFileInfo info(resPath);
				auto dirPath = info.absolutePath();
				QFile file(dirPath + "/" + webName);
				file.rename(resName);
			}
		}
	} else {
		copyTextmotionScreenRes(zipFilePath, zipName);
	}
}

struct DownloadInfo {
	QString itemId;
	QString url;
	QString zipName;
	QString zipPath;
};
static void getDownloadTask(const QJsonArray &items, const std::set<QString> &filter, const QString &tmpPath, std::vector<DownloadInfo> &tasks)
{
	for (const auto &item : items) {
		auto tmpItemId = item.toObject().value("itemId").toString();
		if (filter.find(tmpItemId) != std::end(filter)) {
			auto url = item.toObject().value("resourceUrl").toString();
			auto zipName = url.split('/').last();
			auto zipPath = tmpPath + "/" + zipName;

			DownloadInfo info;
			info.itemId = tmpItemId;
			info.url = url;
			info.zipName = zipName;
			info.zipPath = zipPath;
			tasks.emplace_back(info);
		}
	}
}

void PLSResourceManager::getLibraryJsons(const QString &path, const QString &itemId)
{
	auto doc = QJsonDocument::fromJson(PLSResCommonFuns::readFile(path));
	auto items = doc.object().value("items").toArray();

	std::vector<DownloadInfo> tasks;
	getDownloadTask(items, {itemId, pls_http_api_func::getSenseTimeId()}, m_resTempDir, tasks);

	if (tasks.empty()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "parse library json failed");
		QMetaObject::invokeMethod(this, "parseResTemplates", Qt::QueuedConnection);
		appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
		return;
	}

	for (const auto &info : tasks) {
		PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "set %s path = %s", qUtf8Printable(info.zipName), qUtf8Printable(info.zipPath));
		PLSResCommonFuns::downloadResource(
			info.url,
			[info, itemId, this](PLSResEvents event, const QString &) {
				if (itemId == info.itemId) {
					onDownloadLibrary(event, info.zipName, info.zipPath, info.url);
				} else if (pls_http_api_func::getSenseTimeId() == info.itemId) {
					onDownloadLicense(event, info.zipName, info.zipPath, info.url);
				}
			},
			info.zipPath, false, false);
	}
}

void PLSResourceManager::parseResourceJson(QMap<QString, QVariantMap> &resourceInfos, const QByteArray &resourceJsonData) const
{
	QJsonParseError error;
	auto doc = QJsonDocument::fromJson(resourceJsonData, &error);
	if (error.error != QJsonParseError::NoError) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "category json parse error code = %d", error.error);
		return;
	}
	auto items = doc.array();
	for (const auto &item : items) {
		auto id = item.toObject().value(pls_res_const::resourceCategoryId).toString();
		auto url = item.toObject().value(pls_res_const::resourceModuleUrl).toString();
		auto version = item.toObject().value(pls_res_const::resourceModuleVersion).toInt();
		resourceInfos.insert(id, {{pls_res_const::resourceModuleUrl, url}, {pls_res_const::resourceModuleVersion, version}});
	}
}

void PLSResourceManager::checkResourceUpdate()
{
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start check resource whther need update or not.");

	QMetaEnum metaEnum = QMetaEnum::fromType<resource_modules>();
	QMap<QString, QString> urlPaths;
	QMap<QString, QString> urlTempPaths;
	for (int i = 0; i < metaEnum.keyCount(); i++) {
		auto enumValue = metaEnum.value(i);
		auto enumKeyStr = resourceModuleTocategoryId(static_cast<resource_modules>(enumValue));
		auto newResInfo = m_newCategoriesData.value(enumKeyStr);
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "current select resource module is %s", enumKeyStr.toUtf8().constData());

		if (newResInfo.isEmpty()) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "current select resource module not found ,please check category.json and module name");
			continue;
		}
		auto dirName = getmoduleName(static_cast<resource_modules>(enumValue));
		auto dirPath = makePath(PLSResCommonFuns::getAppLocationPath() + "/" + dirName);

		if (dirPath.isEmpty()) {
			PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "make dir %s failed.", dirName.toUtf8().constData());
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "make dir failed.");
		}
		PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "make dir %s success.", dirName.toUtf8().constData());
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "make dir success.");
		auto resUrl = newResInfo.value(pls_res_const::resourceModuleUrl).toString();
		auto filePath = dirPath + "/" + dirName + ".json";
		auto fileTempPath = m_secretDir + "/" + dirName + ".json";
		urlPaths.insert(resUrl, filePath);
		urlTempPaths.insert(resUrl, fileTempPath);
	}
	if (urlPaths.isEmpty()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "resouce module is empty  can not download res");
		emit resCheckFinished();
		return;
	}
	appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
	PLSResCommonFuns::downloadResources(
		urlTempPaths,
		[this, urlPaths, urlTempPaths](const QMap<QString, bool> &resDownloadStatus, PLSResEvents) {
			for (auto downInter = resDownloadStatus.constBegin(); downInter != resDownloadStatus.constEnd(); ++downInter) {
				if (downInter.value()) {
					PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "resourceJson = %s, downlaod success", downInter.key().toUtf8().constData());
					PLSResCommonFuns::copyFile(urlTempPaths.value(downInter.key()), urlPaths.value(downInter.key()));
				} else {
					PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "resourceJson = %s, downlaod failed", downInter.key().toUtf8().constData());
				}
			}
			auto keys = resDownloadStatus.keys();
			auto inter = std::find_if(keys.constBegin(), keys.constEnd(), [](const QString key) { return key.contains("library"); });
			if (inter != keys.constEnd() && resDownloadStatus.value(*inter)) {
				PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start request sub module in library module");
				QMetaObject::invokeMethod(this, "getLibraryJsons", Qt::QueuedConnection, Q_ARG(const QString &, urlPaths.value(*inter)),
							  Q_ARG(const QString &, pls_http_api_func::getPolicyId()));
			} else {
				QMetaObject::invokeMethod(this, "parseResTemplates", Qt::QueuedConnection);
			}
			appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
		},
		false);
}

bool PLSResourceManager::isExistPath(const QString &resDir) const
{
	QDir dir(resDir);
	bool isExist = dir.exists();
	if (!isExist) {
		isExist = dir.mkpath(resDir);
	}
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "make path is %s", isExist ? "Success" : "Failed");
	return isExist;
}
QString PLSResourceManager::makePath(const QString &resDir) const
{
	QDir dir(resDir);
	bool isExist = dir.exists();
	if (!isExist) {
		isExist = dir.mkpath(resDir);
	}
	PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "resDir = %s,make path is %s", resDir.toUtf8().constData(), isExist ? "Success" : "Failed");
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "make path is %s", isExist ? "Success" : "Failed");

	return dir.path();
}
bool PLSResourceManager::isFileExist(const QString &absolutePath) const
{
	return QFileInfo::exists(absolutePath);
}
QFileInfoList PLSResourceManager::getFiles(const QString &dirPath, const QStringList &filterList) const
{
	QDir dir(dirPath);
	return dir.entryInfoList(filterList, QDir::Files, QDir::Name);
}

void PLSResourceManager::saveNewCahe() const
{
	if (m_newDownloadFileList.isEmpty()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "res fileList cache empty");
		return;
	}

	for (auto resSubModuleInter = m_newDownloadFileList.constBegin(); resSubModuleInter != m_newDownloadFileList.constEnd(); ++resSubModuleInter) {
		QJsonDocument doc;
		doc.setArray(resSubModuleInter.value());
		PLSResCommonFuns::saveFile(QString("%1/%2.json").arg(m_resCacheDir).arg(resSubModuleInter.key()), doc.toJson(QJsonDocument::Indented));
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "save new %s resource cache file list. ", resSubModuleInter.key().toUtf8().constData());
	}
}
QJsonArray PLSResourceManager::findNeedDownloadCaheInfo(const QJsonArray &oldCahe, const QJsonArray &newCahe) const
{
	QJsonArray needFileListArray;
	for (auto caheObj : newCahe) {
		auto obj = caheObj.toObject();
		auto type = obj.value(pls_res_const::resourceType).toString();
		auto fileVersion = obj.value(pls_res_const::resourceModuleVersion).toInt();
		auto fileUrl = obj.value(pls_res_const::resourceUrl).toString();
		auto webName = obj.value(pls_res_const::resourceWebName).toString();
		if (type == "zip") {
			auto dataInter = std::find_if(oldCahe.constBegin(), oldCahe.constEnd(), [fileUrl, fileVersion](QJsonValue oldObj) {
				auto oldFileUrl = oldObj.toObject().value(pls_res_const::resourceUrl).toString();
				auto oldFileVersion = oldObj.toObject().value(pls_res_const::resourceModuleVersion).toInt();
				return (fileVersion == oldFileVersion && oldFileUrl == fileUrl);
			});
			if (dataInter == oldCahe.constEnd()) {
				needFileListArray.append(obj);
			} else {
				findZipSubCaheInfo(needFileListArray, obj);
			}
		} else {
			auto filePath = obj.value(pls_res_const::resourcePath).toString();
			auto dataInter = std::find_if(oldCahe.constBegin(), oldCahe.constEnd(), [webName, fileVersion, filePath, this](QJsonValue oldObj) {
				auto oldWebname = oldObj.toObject().value(pls_res_const::resourceWebName).toString();
				auto oldFileUrl = oldObj.toObject().value(pls_res_const::resourceUrl).toString();
				auto oldFileVersion = oldObj.toObject().value(pls_res_const::resourceModuleVersion).toInt();
				return (fileVersion == oldFileVersion && isFileExist(filePath) && (webName == oldWebname));
			});
			if (dataInter == oldCahe.constEnd()) {
				needFileListArray.append(obj);
			}
		}
	}
	return needFileListArray;
}

void PLSResourceManager::findZipSubCaheInfo(QJsonArray &fileInfoArray, const QJsonObject &obj) const
{
	QJsonObject objData = obj;
	auto subFileList = objData.value(pls_res_const::resource_sub_files).toArray();
	QJsonArray subFileArray;
	for (auto subFile : subFileList) {
		auto subFilePath = subFile.toObject().value(pls_res_const::resourcePath).toString();
		if (!isFileExist(subFilePath)) {
			subFileArray.append(subFile);
		}
	}
	if (!subFileArray.isEmpty()) {
		objData[pls_res_const::resource_sub_files] = subFileArray;
		fileInfoArray.append(objData);
	}
}

void PLSResourceManager::getNeedDownloadFileList()
{
	if (m_oldDownloadFileList.isEmpty()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "old cache file list is empty");
		m_needDownloadFileList = m_newDownloadFileList;
		return;
	}

	for (auto moduleInter = m_newDownloadFileList.constBegin(); moduleInter != m_newDownloadFileList.constEnd(); ++moduleInter) {
		auto newModulevalue = moduleInter.value();
		auto newModuleKey = moduleInter.key();
		if (m_oldDownloadFileList.find(newModuleKey) == m_oldDownloadFileList.end()) {
			m_needDownloadFileList.insert(newModuleKey, newModulevalue);
			continue;
		}
		auto oldModulevalue = m_oldDownloadFileList.value(newModuleKey);
		auto oldModuleKey = newModuleKey;
		QJsonArray donwLoadFileList = findNeedDownloadCaheInfo(oldModulevalue, newModulevalue);
		m_needDownloadFileList.insert(newModuleKey, donwLoadFileList);
	}
}

void PLSResourceManager::saveCaheJsonNeed() const
{
	QJsonDocument doc;
	QJsonObject root;
	for (auto resSubModuleInter = m_needDownloadFileList.constBegin(); resSubModuleInter != m_needDownloadFileList.constEnd(); ++resSubModuleInter) {
		auto valueMap = resSubModuleInter.value();
		doc.setArray(valueMap);
		PLSResCommonFuns::saveFile(QString("%1/need/_need_%3.json").arg(m_resCacheDir).arg(resSubModuleInter.key()), doc.toJson(QJsonDocument::Indented));
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "save need %s resource cache file. ", resSubModuleInter.key().toUtf8().constData());
	}
}

void PLSResourceManager::parseResTemplates()
{
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start parse resource template json");
	auto fileInfos = getFiles(m_resTemplateDir, {"*.json"});
	if (fileInfos.isEmpty()) {
		PLS_ERROR(pls_resource_const::RESOURCE_DOWNLOAD, "res template is empty please check!");
		emit resCheckFinished();
		return;
	}
	for (auto file : fileInfos) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start parse resource template json template name = %s", file.fileName().toUtf8().constData());
		getJsonCaheFiles(file.absoluteFilePath());
	}
	appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
	saveNewCahe();
	appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "get need download cache file list");
	getNeedDownloadFileList();
	appendResourceDownloadedNum(10);
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "res check finished.start request download res");
	emit resCheckFinished();
}

void PLSResourceManager::getBannerJson()
{
	auto bannerUrl = pls_http_api_func::getPrismSynGateWay() + "/pc-banner";
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "start request new banner json");
	auto bannerTmp = pls_get_prism_subpath(common::PLS_BANNANR_JSON) + ".tmp";
	auto handleResult = [this, bannerTmp](PLSResEvents event, const QString &) {
		auto bannerFile = pls_get_prism_subpath(common::PLS_BANNANR_JSON);
		QFile::remove(bannerFile);
		if (event != PLSResEvents::RES_DOWNLOAD_SUCCESS) {
			QFile::remove(bannerTmp);
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, " request new banner json falied");
			emit bannerJsonDownloaded();
			return;
		}
		QFile::rename(bannerTmp, bannerFile);
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, " request new banner json ok");
		emit bannerJsonDownloaded();
	};
	PLSResCommonFuns::downloadResource(bannerUrl, handleResult, bannerTmp, false, true);
}

void PLSResourceManager::getJsonCaheFiles(const QString &resTemplateFilePath)
{
	auto templateDoc = QJsonDocument::fromJson(PLSResCommonFuns::readFile(resTemplateFilePath));
	if (templateDoc.isNull() || templateDoc.isEmpty()) {
		PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "the res template is empty, path = %s", resTemplateFilePath.toUtf8().constData());
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "the res template is empty");
		return;
	}
	auto jsonObj = templateDoc.object();
	auto moduleName = jsonObj.value("moduleName").toString();
	auto moduleId = getModuleId(moduleName);
	auto saveDir = jsonObj.value("saveDir").toString();
	auto moduleJson = jsonObj.value("moduleJson").toString();
	auto files = templateDoc.object().value("files").toArray();
	auto resPath = PLSResCommonFuns::getAppLocationPath() + '/' + moduleJson;
	QJsonArray arrayCahe;

	QJsonObject moduleJsonObj;
	QFileInfo info(resPath);
	moduleJsonObj.insert("resource_name", moduleName + ".json");
	moduleJsonObj.insert("resource_path", resPath);
	auto data = resourceModuleTocategoryId(moduleId);
	moduleJsonObj.insert("resource_url", m_newCategoriesData.value(resourceModuleTocategoryId(moduleId)).value(pls_res_const::resourceModuleUrl).toString());
	moduleJsonObj.insert("type", "json");
	moduleJsonObj.insert("version", m_newCategoriesData.value(resourceModuleTocategoryId(moduleId)).value(pls_res_const::resourceModuleVersion).toInt());
	moduleJsonObj.insert(pls_res_const::resourceSize, info.lastModified().toString());
	moduleJsonObj.insert(pls_res_const::resourceTime, info.size());
	arrayCahe.append(moduleJsonObj);

	QJsonParseError error;
	auto resDoc = QJsonDocument::fromJson(PLSResCommonFuns::readFile(resPath), &error);
	if (resDoc.isNull() || resDoc.isEmpty()) {
		PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "the res  is empty, path = %s", resPath.toUtf8().constData());
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "the res  is empty.");

		return;
	}
	for (auto file : files) {
		auto obj = file.toObject();
		const QString type = obj.value("type").toString();
		QJsonArray jsonBasePath = obj.value("basePath").toArray();
		QJsonArray url = obj.value("url").toArray();
		QJsonArray title = obj.value("title").toArray();
		QJsonArray version = obj.value("version").toArray();
		auto renameRule = obj.value("renameRule").toObject();
		QJsonArray copys = obj.value("copys").toArray();
		auto prefix = toQStringList(renameRule.value("prefix").toArray());
		auto suffix = toQStringList(renameRule.value("suffix").toArray());

		if (resource_modules::Textmotion == moduleId) {
			QString ignoreKey = pls_prism_get_locale() == "ko-KR" ? "en" : "ko";
			if (toQStringList(url).contains(ignoreKey)) {
				//if in kr language, then not download en resource, vice versa
				PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "textmotion ignore other language res path: %s", toQStringList(url).join("/").toUtf8().constData());
				continue;
			}
		}

		QJsonArray versions;
		getBasePathValue(versions, resDoc, toQStringList(jsonBasePath) + toQStringList(version));
		QJsonArray urls;
		getBasePathValue(urls, resDoc, toQStringList(jsonBasePath) + toQStringList(url));
		QJsonArray titles;
		if (!title.isEmpty()) {
			getBasePathValue(titles, resDoc, toQStringList(jsonBasePath) + toQStringList(title));
		}
		getJsonCaheFile(urls, versions, copys, type, saveDir, prefix, suffix, titles, arrayCahe);
	}
	m_newDownloadFileList.insert(moduleName.toLower(), arrayCahe);
}
void PLSResourceManager::getJsonCaheFile(const QJsonArray &urls, const QJsonArray &versions, const QJsonArray &copys, const QString &type, const QString &saveDir, const QStringList &prefix,
					 const QStringList &suffix, const QJsonArray &titles, QJsonArray &arrayCahe) const
{
	auto size = urls.size();

	for (int index = 0; index < size; ++index) {
		auto url = urls[index].toString();
		auto webName = url.split('/').last();
		QJsonObject obj;
		obj.insert(pls_res_const::resourceUrl, url);
		obj.insert(pls_res_const::resourceModuleVersion, versions[index].toInt());
		obj.insert(pls_res_const::resourceType, type);

		if (type == "zip") {
			obj.insert(pls_res_const::resource_sub_files, getzipCaheJson(copys, webName.section('.', 0, 0), saveDir));

		} else {
			obj.insert(pls_res_const::resourceWebName, webName);
			if (!prefix.isEmpty() && webName.startsWith(prefix[0])) {
				webName.replace(0, prefix[0].length(), prefix[1]);
			}
			if (!suffix.isEmpty() && webName.endsWith(suffix[0])) {
				webName.replace(webName.lastIndexOf(suffix[0]), suffix[0].length(), suffix[1]);
			}

			if (!titles[index].isNull() && !prefix.isEmpty()) {
				//textmotion handle
				auto fileType = webName.split('.').last();
				webName = QString("%1%2.%3").arg(prefix[1]).arg(titles[index].toString()).arg(fileType);
			}
			obj.insert(pls_res_const ::resourceName, webName);
			obj.insert(pls_res_const ::resourcePath, QString("%1/%2/%3").arg(PLSResCommonFuns::getAppLocationPath()).arg(saveDir).arg(webName));
		}
		arrayCahe.append(obj);
	}
}
QJsonArray PLSResourceManager::getzipCaheJson(const QJsonArray &copys, const QString &zipName, const QString &saveDir) const
{
	if (copys.isEmpty()) {
		return copys;
	}
	QJsonArray subFilesArray;
	for (auto copy : copys) {
		auto copyObj = copy.toObject();
		auto srcDir = copyObj.value("src").toString();
		auto dstDir = copyObj.value("dst").toString();
		auto fileList = copyObj.value("fileList").toArray();
		if (fileList.isEmpty()) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "zip file template error, please add sub file info");
			continue;
		}
		for (auto file : fileList) {
			auto srcName = file.toObject().value("srcFileName").toString();
			auto dstName = file.toObject().value("dstFileName").toString();
			QJsonObject obj;
			obj.insert(pls_res_const::resourceWebName, srcName);
			obj.insert(pls_res_const::resourceName, dstName);
			auto filePath = QString("%1/%2/%3/%4/%5")
						.arg(PLSResCommonFuns::getAppLocationPath())
						.arg(saveDir)
						.arg(zipName)
						.arg(dstDir.isEmpty() ? srcDir : dstDir)
						.arg(dstName.isEmpty() ? srcName : dstName);
			delInvalidPath(filePath);
			obj.insert(pls_res_const::resourcePath, filePath);
			subFilesArray.append(obj);
		}
	}
	return subFilesArray;
}
void PLSResourceManager::getBasePathValue(QJsonArray &result, const QJsonDocument &doc, QStringList basePath)
{
	if (basePath.isEmpty()) {
		result = doc.array();
		return;
	}
	QString first = basePath.takeFirst();

	if (first == "[]") {
		first = basePath.takeFirst();
		getBasePathValue(result, doc.array(), basePath, first);
	} else {
		getBasePathValue(result, doc.object(), basePath, first);
	}
}
void PLSResourceManager::getBasePathValue(QJsonArray &result, const QJsonObject &obj, QStringList basePath, QString name)
{
	if (basePath.isEmpty()) {
		if (name.endsWith("[]")) {
			name = name.left(name.length() - 2);
		}
		result.append(obj[name]);
		return;
	}
	QString first = basePath.takeFirst();
	if (name.endsWith("[]")) {
		name = name.left(name.length() - 2);
		getBasePathValue(result, obj[name].toArray(), basePath, first);
	} else {
		getBasePathValue(result, obj[name].toObject(), basePath, first);
	}
}
void PLSResourceManager::getBasePathValue(QJsonArray &result, const QJsonArray &arr, QStringList basePath, QString name)
{
	if (basePath.isEmpty()) {
		if (name.endsWith("[]")) {
			name = name.left(name.length() - 2);
		}
		for (auto jval : arr) {
			result.append(jval.toObject()[name]);
		}
		return;
	}
	QString first = basePath.takeFirst();

	if (name.endsWith("[]")) {
		name = name.left(name.length() - 2);
		for (auto jval : arr) {
			getBasePathValue(result, jval.toObject()[name].toArray(), basePath, first);
		}
	} else {
		for (auto jval : arr) {
			getBasePathValue(result, jval.toObject()[name].toObject(), basePath, first);
		}
	}
}
QStringList PLSResourceManager::toQStringList(const QJsonArray &arr) const
{
	QStringList list;
	for (auto i : arr) {
		list.append(i.toString());
	}
	return list;
}

void PLSResourceManager::onDownloadLibrary(PLSResEvents event, const QString &zipName, const QString &zipPath, const QString &url)
{
	appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
	if (event == PLSResEvents::RES_DOWNLOAD_FAILED) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "resource failed file = %s,url = %s", zipName.toUtf8().constData(), url.toUtf8().constData());
	} else {
		auto dstDir = PLSResCommonFuns::getAppLocationPath() + '/' + getmoduleName(resource_modules::Library);
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "set copy dst  library_policy_pc.zip.");

		PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "set copy dst  library_policy_pc.zip path = %s", dstDir.toUtf8().constData());
		if (bool isSuccess = PLSResCommonFuns::unZip(dstDir, zipPath, zipName, true); !isSuccess) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "library res uzip failed. File = %s, url = %s", zipName.toUtf8().constData(), qUtf8Printable(url));
		}
	}

	QMetaObject::invokeMethod(
		this,
		[this]() {
			parseResTemplates();
			emit libraryNeedUpdate();
		},
		Qt::QueuedConnection);
	appendResourceDownloadedNum(10 + qint64(QRandomGenerator::global()->bounded(10)));
}

void PLSResourceManager::onDownloadLicense(PLSResEvents event, const QString &, const QString &zipPath, const QString &) const
{
	if (event == PLSResEvents::RES_DOWNLOAD_FAILED) {
		PLS_WARN(pls_resource_const::RESOURCE_DOWNLOAD, "[TRACE-LICENSE] Failed to download license zip file.");
		return;
	}
	if (!PLSLicenseManager::CopyLicenseZip(zipPath)) {
		PLS_WARN(pls_resource_const::RESOURCE_DOWNLOAD, "[TRACE-LICENSE] Failed to copy license zip file.");
		return;
	}
	if (!PLSLicenseManager::HandleLicenseZipFile()) {
		PLS_WARN(pls_resource_const::RESOURCE_DOWNLOAD, "[TRACE-LICENSE] Failed to handle license zip file.");
	}
}

void PLSResourceManager::copyTextmotionScreenRes(const QString &zipFilePath, const QString &zipFileName) const
{
	bool isSuccess = PLSResCommonFuns::unZip(makePath(PLSResCommonFuns::getAppLocationPath() + "/textmotion/web/static/screen_img"), zipFilePath, zipFileName, false);
	if (!isSuccess) {
		bool isRemoveSuccess = QFile::remove(zipFilePath);
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "textmotion screen file = %s del zip  %s", zipFileName.toUtf8().constData(), isRemoveSuccess ? "success" : "failed");
	}
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "textmotion screen file = %s uzip  %s", zipFileName.toUtf8().constData(), isSuccess ? "success" : "failed");
}

void PLSResourceManager::copyTextmotionWeb() const
{

	QString dstWebPath = PLSResCommonFuns::getAppLocationPath() + "/textmotion/web";
#if defined(Q_OS_WIN)
	QString srcWebPath = pls_get_dll_dir("libresource") + "/../../data/prism-plugins/prism-text-template-source/web";
#elif defined(Q_OS_MACOS)
	QString srcWebPath = pls_get_dll_dir("prism-text-template-source") + "/web";
#endif
	auto isSuccess = PLSResCommonFuns::copyDirectory(srcWebPath, dstWebPath, true);
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "copyTextmotionWeb %s", isSuccess ? "success" : "failed");
}

void PLSResourceManager::updateResourceDownloadStatus(const QString &url, PLSResDownloadStatus downloadStatus)
{
	QMutexLocker locker(&m_mutex);
	if (downloadStatus == PLSResDownloadStatus::DownloadSuccess) {
		m_resourceDownloadStatus.remove(url);
	} else {
		m_resourceDownloadStatus[url] = downloadStatus;
	}
}

PLSResDownloadStatus PLSResourceManager::resourceDownloadStatus(const QString &url)
{
	QMutexLocker locker(&m_mutex);
	if (m_resourceDownloadStatus.find(url) != m_resourceDownloadStatus.end()) {
		return m_resourceDownloadStatus.value(url);
	}
	return PLSResDownloadStatus::None;
}
