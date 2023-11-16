#include "PLSLaboratoryManage.h"
#include <QDir>
#include <QCoreApplication>
#include <QThreadPool>
#include <QStandardPaths>
#include "log/log.h"
#include "json-data-handler.hpp"
#include "frontend-api.h"
#include "PLSLabDownloadFile.h"
#include "log/log.h"
#include "ui-config.h"
#include <QSettings>
#include <obs.hpp>
#include "PLSLaboratoryInstallView.h"
#include "login-user-info.hpp"
#include "prism-version.h"
#include "pls/pls-obs-api.h"
#include "ChannelCommonFunctions.h"

constexpr auto LABORATORY_DIR = "PRISMLiveStudio/laboratory/";
constexpr auto LAB_JSON_NAME = "Laboratory.json";
constexpr auto DOWNLOAD_LAB_JSON_NAME = "downloadLab.json";

const QString g_laboratoryZipType = "ZIPType";
const QString g_laboratoryDllType = "DLLType";

const QString g_labDownloadName = "name";
const QString g_labDownloadZipFilePath = "zipFilePath";
const QString g_labDownloadUnzipFolderPath = "unzipFolderPath";
const QString g_labDownloadFileListPath = "fileListPath";
const QString g_labDownloadDllFileNameList = "dllFileNameList";
const QString g_labDownloadVersion = "version";
const QString g_labDownloadId = "id";
const QString g_labDownloadType = "type";
const QString g_labDownloadUseState = "useState";

const char *const MODULE_ABORATORY = "LaboratoryModule";

constexpr auto LAB_DLL_MESSAGE_INFO = "LabDllMessageInfo";
const QString g_labDllVersionKey = "LabDllVersionInfo";
const QString g_labDllNameKey = "LabDllNameInfo";

static inline void createDir(const QDir &dir)
{
	if (!dir.exists()) {
		dir.mkpath(dir.absolutePath());
	}
}

PLSLaboratoryManage *PLSLaboratoryManage::instance()
{
	static PLSLaboratoryManage manage;
	return &manage;
}

void PLSLaboratoryManage::requestLabJsonData()
{
	if (m_labRequesting) {
		return;
	}
	m_labRequesting = true;
	QString url = PLS_LAB.arg(PRISM_SSL);
	pls::http::request(pls::http::Request() //
				   .method(pls::http::Method::Get)
				   .hmacUrl(url, PLS_PC_HMAC_KEY.toUtf8())
				   .workInNewThread()
				   .cookie(pls_get_prism_cookie())
				   .cookie(PLSLoginUserInfo::getInstance()->getSessionCookie())
				   .receiver(this)
				   .result([this, url](const pls::http::Reply &reply) {
					   m_labRequesting = false;
					   if (!reply.isOk()) {
						   printLog("download the Laboratory.json failed");
						   return;
					   }
					   QJsonParseError jsonError;
					   auto data = reply.data();
					   QJsonDocument respjson = QJsonDocument::fromJson(data, &jsonError);
					   bool isValidJson = jsonError.error == QJsonParseError::NoError;
					   if (!isValidJson) {
						   printLog("the Downloaded Laboratory.json parse error");
						   return;
					   }
					   QFileInfo fi(getFilePath(LAB_JSON_NAME));
					   QString filePath = fi.absoluteFilePath();
					   if (!PLSJsonDataHandler::saveJsonFile(respjson.toJson(), filePath)) {
						   printLog("save the Laboratory.json file failed.");
						   return;
					   }
					   printLog("save the Laboratory.json file success.");
				   }));
}

void PLSLaboratoryManage::checkLabDllUpdate()
{
	requestLabJsonData();
	initLaboratoryData();
	loadDownloadLabCache();
	deleteOrUpdateAppDllFolder();
}

void PLSLaboratoryManage::checkLabZipUpdate()
{
	if (m_LaboratoryInfos.isEmpty()) {
		return;
	}

	printLog("Check the validity of the downloadLabCache");
	QStringList labIdList = downloadLabCache.keys();
	for (const QString &labId : labIdList) {
		QVariantMap map = downloadLabCache.value(labId);
		QString name = map.value(g_labDownloadName).toString();
		//The Id of the downloaded ZIP package lab cannot be found on the server
		if (!m_LaboratoryInfos.contains(labId)) {
			printLog(QString("lab object in downloadLab.json cannot be found in laboratory.json file, lab id is %1, lab name is %2 ").arg(labId).arg(name));
			downloadLabCache.remove(labId);
			deleteFileOrFolder(map.value(g_labDownloadZipFilePath).toString());
			deleteFileOrFolder(map.value(g_labDownloadUnzipFolderPath).toString());
			continue;
		}
	}

	printLog("start traversing the Laboratory.json file and download the detail page html");
	for (const QString &key : m_LaboratoryInfos.keys()) {
		//Download the introduction page for each plugin
		QString title = getStringInfo(key, laboratory_data::g_laboratoryTitle);
		printLog(QString("start download detail page html, lab id is %1 , lab name is %2 ").arg(key).arg(title));
		pls::http::request(pls::http::Request()
					   .method(pls::http::Method::Get)
					   .url(getStringInfo(key, laboratory_data::g_laboratoryDetailPage))
					   .forDownload(true)
					   .saveFilePath(getLabDetailPageFilePathByLabId(key))
					   .workInNewThread()
					   .timeout(PRISM_NET_REQUEST_TIMEOUT)
					   .result([](const pls::http::Reply &) {
						   //download result not case
					   }));
		if (!checkLabNeedInstall(key)) {
			printLog(QString("No installation required for current labs, lab id is %1 , lab name is %2 ").arg(key).arg(title));
			continue;
		}
		if (!downloadLabCache.contains(key)) {
			printLog(QString("the current lab is not downloaded, lab id is %1 , lab name is %2").arg(key).arg(title));
			deleteZipFilesAndUnzipFolder(key);
			continue;
		}
		printLog(QString("the current lab is downloaded, lab id is %1 , lab name is %2").arg(key).arg(title));
		printLog(QString("start checking single downloaded lab zip file validity, lab id is %1 , lab name is %2").arg(key).arg(title));
		checkSingleLabZipValidity(key);
		printLog(QString("end checking single downloaded lab zip file validity, lab id is %1 , lab name is %2").arg(key).arg(title));
	}
	printLog("end traversing the Laboratory.json file and download the detail page html");
	saveDownloadLabCache();
}

void PLSLaboratoryManage::checkSingleLabZipValidity(const QString &labId)
{
	QString serverFileVersion = getStringInfo(labId, laboratory_data::g_laboratoryVersion);
	QVariantMap map = downloadLabCache.value(labId);
	QString localFileVersion = map.value(g_labDownloadVersion).toString();
	bool isLowPrism = isPrismLowVersion(labId);
	printLog(QString("start checking downloaded file version information, downloaded version is %1, server file version is %2").arg(localFileVersion).arg(serverFileVersion));
	if (serverFileVersion != localFileVersion && !isLowPrism) {
		printLog(QString("downloaded file version information is invalid, downloaded Version is %1 , server file version is %2").arg(localFileVersion).arg(serverFileVersion));
		downloadLabZipFile(m_plsCancel, this, labId);
		return;
	}
	printLog("downloaded file version information is valid");

	printLog(QString("start checking the validity of the downloaded file list, lab id is %1").arg(labId));
	QStringList labFileListPath = map.value(g_labDownloadFileListPath).toStringList();
	if (!isValidFileList(labFileListPath) && !isLowPrism) {
		printLog(QString("the downloaded file list is invalid, lab lab id is %1").arg(labId));
		downloadLabZipFile(m_plsCancel, this, labId);
		return;
	}
	printLog("the downloaded file list is valid");
}

bool PLSLaboratoryManage::checkLabNeedInstall(const QString &labId)
{
	bool needInstall = getBoolInfo(labId, laboratory_data::g_laboratoryNeedInstall);
	if (!needInstall && !downloadLabCache.contains(labId)) {
		QVariantMap downloadMap;
		downloadMap.insert(g_labDownloadId, labId);
		downloadMap.insert(g_labDownloadName, getStringInfo(labId, laboratory_data::g_laboratoryTitle));
		downloadMap.insert(g_labDownloadVersion, getStringInfo(labId, laboratory_data::g_laboratoryVersion));
		downloadMap.insert(g_labDownloadType, getStringInfo(labId, laboratory_data::g_laboratoryType));
		downloadMap.insert(g_labDownloadUseState, false);
		downloadLabCache.insert(labId, downloadMap);
		printLog(QString("create labs that don't require installation, lab Id is %1 , lab title is %2 , lab version is %3 , lab type is %4")
				 .arg(labId)
				 .arg(getStringInfo(labId, laboratory_data::g_laboratoryTitle))
				 .arg(getStringInfo(labId, laboratory_data::g_laboratoryVersion))
				 .arg(getStringInfo(labId, laboratory_data::g_laboratoryType)));
	}
	return needInstall;
}

PLSLaboratoryManage::~PLSLaboratoryManage()
{
	printLog("laboratory manage release");
	saveDownloadLabCache();
}

PLSLaboratoryManage::PLSLaboratoryManage(QObject *parent) : QObject(parent)
{
	printLog("laboratory manage init");
	initThreadPool();
}

void PLSLaboratoryManage::initThreadPool()
{
	m_threadPool.setMaxThreadCount(1);
}

const QStringList &PLSLaboratoryManage::getLabIdList()
{
	if (m_labIdList.isEmpty()) {
		initLaboratoryData();
	}
	return m_labIdList;
}

QString PLSLaboratoryManage::getStringInfo(const QString &labId, const QString &key)
{
	if (m_LaboratoryInfos.isEmpty()) {
		initLaboratoryData();
	}
	return m_LaboratoryInfos[labId][key].toString();
}

QString PLSLaboratoryManage::getSettingStringInfo(const QString &labId, const QString &key)
{
	if (m_LaboratoryInfos.isEmpty()) {
		initLaboratoryData();
	}
	QVariantMap labInfoMap = m_LaboratoryInfos.value(labId);
	QVariantMap settingMap = labInfoMap.value(name2str(settings)).toMap();
	return settingMap.value(key).toString();
}

int PLSLaboratoryManage::getSettingIntInfo(const QString &labId, const QString &key)
{
	if (m_LaboratoryInfos.isEmpty()) {
		initLaboratoryData();
	}
	QVariantMap labInfoMap = m_LaboratoryInfos.value(labId);
	QVariantMap settingMap = labInfoMap.value(name2str(settings)).toMap();
	return settingMap.value(key).toInt();
}

QString PLSLaboratoryManage::getLabZipFilePath(const QString &labId)
{
	QString downloadUrl = getSettingStringInfo(labId, laboratory_data::g_settingsDownloadUrl);
	if (downloadUrl.isEmpty()) {
		return QString();
	}
	return getFilePath(getFileNameByURL(downloadUrl));
}

QString PLSLaboratoryManage::getLabDetailPageFilePathByLabId(const QString &labId)
{
	QString detailPageUrl = getStringInfo(labId, laboratory_data::g_laboratoryDetailPage);
	QString fileName = getFileNameByURL(detailPageUrl);
	QString filePath = QString("%1DetailPage/%2").arg(LABORATORY_DIR).arg(fileName);
	QFileInfo fi(pls_get_user_path(filePath));
	createDir(fi.dir());
	return fi.absoluteFilePath();
}

QString PLSLaboratoryManage::getAppDetailPageFilePathByLabId(const QString &labId)
{
	QString detailPageUrl = getStringInfo(labId, laboratory_data::g_laboratoryDetailPage);
	QString fileName = getFileNameByURL(detailPageUrl);
	QString filePath = pls_get_app_dir() + QString("/data/prism-studio/user/%1").arg(fileName);
	return filePath;
}

QString PLSLaboratoryManage::getLabZipUnzipFolderPath(const QString &labId)
{
	QString strPath = getLabZipFilePath(labId);
	if (strPath.isEmpty()) {
		return QString();
	}
	QFileInfo fileInfo(strPath);
	QString dirPath = fileInfo.absoluteDir().path();
	return QString("%1/%2").arg(dirPath).arg(fileInfo.baseName());
}

bool PLSLaboratoryManage::getBoolInfo(const QString &labId, const QString &key)
{
	if (m_LaboratoryInfos.isEmpty()) {
		initLaboratoryData();
	}
	return m_LaboratoryInfos[labId][key].toBool();
}

void PLSLaboratoryManage::printLog(const QString &subLog) const
{
	QString log = QString("laboratory status: ") + subLog;
	PLS_INFO(MODULE_ABORATORY, "%s", log.toUtf8().constData());
}

void PLSLaboratoryManage::printKRLog(const QString &subLog) const
{
	QString log = QString("laboratory status: ") + subLog;
	PLS_INFO_KR(MODULE_ABORATORY, "%s", log.toUtf8().constData());
}

bool PLSLaboratoryManage::checkLabJsonValid(const QString &destPath) const
{
	printLog("start checking if the laboratory.json file in the user directory");
	printKRLog(QString("start checking the laboratory.json file in the user directory, dest path is ") + destPath);

	//laboratory dir not exist, then create laboratory dir
	QFileInfo fileInfo(destPath);
	if (!fileInfo.dir().exists()) {
		printLog("The laboratory folder in the user directory does not exist and needs to be created in the user directory.");
		bool ok = fileInfo.dir().mkpath(fileInfo.absolutePath());
		if (!ok) {
			printLog("failed to create laboratory folder in user directory.");
			return false;
		}
	}
	printLog("The laboratory folder in the user directory exists.");
	printKRLog(QString("The laboratory folder in the user directory exists, the laboratory folder path is ") + fileInfo.dir().absolutePath());

	//laboratory json file not exist, then copy json file to dest path from app cache file
	if (!fileInfo.exists()) {

		//user local laboratory.json is not existed
		printLog("The laboratory.json file does not exist in the user laboratory folder");

		//get app cache laboratory json file path，then open the path
		//applicationDirPath
		QString appJsonPath = findFileInResources(ChannelData::defaultSourcePath, "Laboratory.json");
		printLog("Open the laboratory.json file in the App installation");
		printKRLog(QString("Open the laboratory.json file in the App installation, the App installation laboratory.json file path is ") + appJsonPath);

		QFile file(appJsonPath);
		if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
			printLog("Failed to open the laboratory.json file in the installation package");
			file.close();
			return false;
		}

		//copy app cache file to dest path
		printLog("Copy the laboratory.json file of the App installation package to the user directory");
		bool result = file.copy(destPath);
		if (!result) {
			printLog(QString("Failed to copy the laboratory.json file of the App installation package to the user directory, reson is %1").arg(file.errorString()));
			file.close();
			return false;
		}

		file.close();
	}
	printLog("The laboratory.json file exist in the user directory");
	return true;
}

void PLSLaboratoryManage::downloadLabZipFile(const PLSCancel &cancel, const QObject *receiver, const QString &labId, const DownloadZipCallback &callback)
{
	//If the downloaded lab ID is invalid, the direct return indicates that the download failed.
	if (!m_LaboratoryInfos.contains(labId)) {
		printLog(QString("download zip file failed because lab id is not valid, lab id is %1").arg(labId));
		callback(false, DownloadZipErrorType::DownloadZipNotExistInServer);
		return;
	}

	//Start downloading lab objects
	QString labTitle = getStringInfo(labId, laboratory_data::g_laboratoryTitle);
	printLog(QString("start download laboratory object , lab id is %1 , lab title is %2").arg(labId).arg(labTitle));

	//If the current lab ID has already been downloaded, delete the local download ZIP package and unzip folder
	if (downloadLabCache.contains(labId)) {
		printLog(QString("need to delete files that have been downloaded and cached, lab id is %1 , lab title is %2").arg(labId).arg(labTitle));
		QVariantMap map = downloadLabCache.value(labId);
		deleteFileOrFolder(map.value(g_labDownloadZipFilePath).toString());
		deleteFileOrFolder(map.value(g_labDownloadUnzipFolderPath).toString());
		deleteDownloadLabCacheDLLObject(map);
	}

	//Start executing the download lab ZIP installation package request
	QString url = getSettingStringInfo(labId, laboratory_data::g_settingsDownloadUrl);
	pls::http::Request request;
	request.method(pls::http::Method::Get)
		.url(url)
		.forDownload(true)
		.saveFilePath(getLabZipFilePath(labId))
		.workInMainThread()
		.receiver(receiver)
		.timeout(getSettingIntInfo(labId, laboratory_data::g_settingsTimeout))
		.result([this, labId, labTitle, callback, receiver](const pls::http::Reply &reply) {
			m_cancelRequestMap.take(labId);
			int code = reply.statusCode();
			auto error = reply.error();
			auto data = reply.data();
			if (!reply.isOk()) {
				printLog(
					QString("download laboratory object failed, lab id is %1, lab title is %2, stautsCode is %3, QNetworkError is %4").arg(labId).arg(labTitle).arg(code).arg(error));
				downloadLabZipFileFail(labId, callback, DownloadZipErrorType::DownloadZipNetworkError);
				return;
			}
			downloadLabZipFileSuccess(receiver, labId, callback);
		});
	pls::http::request(request);
	m_cancelRequestMap.insert(labId, request);
	QObject::connect(&cancel, &PLSCancel::cancelSignal, this, [this, labId]() {
		printLog(QString("The user clicks the close button on the installation page to cancel the request to download the laboratory object {id:%1}").arg(labId));
		pls::http::Request cancelRequst = m_cancelRequestMap.take(labId);
		cancelRequst.abort();
	});
}

bool PLSLaboratoryManage::isPrismLowVersion(const QString &labId) const
{
	QVariantMap labInfoMap = m_LaboratoryInfos.value(labId);
	QString labVersion = labInfoMap.value(laboratory_data::g_laboratoryPrismVersion).toString();
	//1 means the lab version is greater than the Prism software version
	if (compareVersion(labVersion, PLS_VERSION) == 1) {
		printLog(QString("The Prism version requested by the lab is too high, lab prism version is %1 ,current prism version: %2").arg(labVersion).arg(PLS_VERSION));
		return true;
	}
	printLog(QString("The Prism version required by the lab satisfies, lab prism version is %1 ,current prism version: %2").arg(labVersion).arg(PLS_VERSION));
	return false;
}

void PLSLaboratoryManage::getDllNameList(const QString &dirPath, QStringList &dllFileNameList) const
{
	QStringList fileList;
	QDir dir(dirPath);
	QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Dirs);
	for (const QFileInfo &fileInfo : fileInfoList) {
		if (fileInfo.isFile() && fileInfo.suffix() == "dll") {
			dllFileNameList.append(fileInfo.baseName());
		}
	}
}

void PLSLaboratoryManage::getFilePathListRecursive(const QString &dirPath, QStringList &filePathList)
{
	QStringList fileList;
	QDir dir(dirPath);
	QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Dirs);
	for (const QFileInfo &fileInfo : fileInfoList) {
		if (fileInfo.isDir()) {
			getFilePathListRecursive(fileInfo.absoluteFilePath(), filePathList);
		}
		if (fileInfo.isFile()) {
			filePathList.append(fileInfo.absoluteFilePath());
		}
	}
}

int PLSLaboratoryManage::compareVersion(const QString &v1, const QString &v2) const
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

void PLSLaboratoryManage::deleteOrUpdateAppDllFolder()
{
	//Delete the plugins that have been downloaded locally again, but the plugins that have been deleted by the server
	printLog("Check the validity of the downloadPluginLabCache plugin");
	for (const QString &labId : downloadPluginLabCache.keys()) {

		//Get the dictionary information that has been downloaded locally
		QVariantMap map = downloadLabCache.value(labId);

		//Check only plugin type lab objects
		QString dllName = map.value(g_labDownloadName).toString();

		//This plugin has been downloaded locally, but the server does not have this plugin, Example lab project to be transformed into a formal project in the future
		if (!m_LaboratoryInfos.contains(labId)) {
			printLog(QString("the lab id in downloadLab.json cannot be found in laboratory.json file, downloaded dll info, lab id is %1, dll name is %2 ").arg(labId).arg(dllName));
			downloadLabCache.remove(labId);
			deleteDownloadLabCacheDLLObject(map);
			deleteFileOrFolder(map.value(g_labDownloadZipFilePath).toString());
			deleteFileOrFolder(map.value(g_labDownloadUnzipFolderPath).toString());
			continue;
		}

		//Get the lab version of download.json and Get the plugin version number of the App plugin directory
		QString downloadFileVersion = map.value(g_labDownloadVersion).toString();
		QString appDllVersion = getDllInfoByDllName(dllName, g_labDllVersionKey);
		printLog(QString("the downloaded object existed in web server , downloaded dll id is %1 , downloaded dll name is %2 , downloaded dll version is %3 , app plugin dll version is %4 ")
				 .arg(labId)
				 .arg(dllName)
				 .arg(downloadFileVersion)
				 .arg(appDllVersion));

		//The plug-in object is not in the program directory. An example is the case where the plug-in is deleted during the upgrade and installation. or The version of the plugin is inconsistent with the downloaded version
		if (downloadFileVersion != appDllVersion || !isExistedAppDll(dllName)) {
			printLog(QString("Perform copy downloaded plugin operation, downloaded dll id is %1, downloaded dll name is %2 ").arg(labId).arg(dllName));
			if (!copyDownloadLabCacheDllFileToAppPluginFolder(map)) {
				downloadLabCache.remove(labId);
				deleteFileOrFolder(map.value(g_labDownloadZipFilePath).toString());
				deleteFileOrFolder(map.value(g_labDownloadUnzipFolderPath).toString());
			}
		}
	}

	//The local download.json file is deleted, you need to use the remote file name and dll name to download the ZIP file and the App plug-in object locally
	printLog("There are plugins in m_LaboratoryInfos, and plugins not in downloadLabCache are deleted");
	for (const QString &labId : m_LaboratoryInfos.keys()) {
		if (downloadLabCache.contains(labId)) {
			continue;
		}
		if (!isDllType(labId)) {
			continue;
		}
		QString labTitle = getStringInfo(labId, laboratory_data::g_laboratoryTitle);
		printLog(QString("laboratory.json lab object is not contains download.json file, title is %1 , lab id is %2 ").arg(labTitle).arg(labId));
		deleteZipFilesAndUnzipFolder(labId);
		deleteAppDllWithLabId(labId);
	}
}

void PLSLaboratoryManage::deleteAppDllWithLabId(const QString &labId)
{
	QString matchDllName = getDllNameWithoutSuffix(labId);
	printLog(QString("start delete plugin object by plugin id, plugin id is %1 , plugin name is %2").arg(labId).arg(matchDllName));
	if (matchDllName.isEmpty()) {
		return;
	}
	deleteAppDllWithDllName(matchDllName);
	printLog(QString("end delete plugin object by plugin id, plugin id is %1 , plugin name is %2").arg(labId).arg(matchDllName));
}

void PLSLaboratoryManage::deleteAppDllWithDllName(const QString &dllName) const
{
	printLog(QString("start delete plugin object by plugin name, plugin name is %1").arg(dllName));

	//Both the new Prism plugin path and the old plugin path are removed
	printLog(QString("delete %1.dll object in prism-plugins directory").arg(dllName));
	deleteDllNameFileByPath(getOldPrismPluginFolderPath(), dllName);
	printLog(QString("delete %1.dll object in lab-plugins directory").arg(dllName));
	auto toDir = getPrismPluginFolderPath();
	deleteDllNameFileByPath(toDir, dllName);

	auto prismPluginDataFolderPath = getAppDllDataDirPathByDllName(dllName);
	auto oldPrismPluginDataFolderPath = getOldPrismPluginDataFolderPath() + "/" + dllName;
	printLog(QString("delete plugin data folder in data/lab-plugins/%1 directory").arg(dllName));
	printKRLog(QString("delete plugin folder in data/lab-plugins directory, the data/lab-plugins path is ") + prismPluginDataFolderPath);
	deleteFileOrFolder(prismPluginDataFolderPath);
	printLog(QString("delete plugin data folder in data/prism-plugins/%1 directory").arg(dllName));
	printKRLog(QString("delete plugin folder in data/prism-plugins directory, the data/prism-plugins path is ") + oldPrismPluginDataFolderPath);
	deleteFileOrFolder(oldPrismPluginDataFolderPath);

	printLog(QString("end delete plugin object by plugin name, plugin name is %1").arg(dllName));
}

void PLSLaboratoryManage::deleteDllNameFileByPath(const QString &dllDirPath, const QString &dllName) const
{
	QDir targetDir(dllDirPath);
	QFileInfoList fileInfoList = targetDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

	bool containsDll = false;
	for (const QFileInfo &fileInfo : fileInfoList) {
		if (fileInfo.baseName() == dllName) {
			QString toFilePath = targetDir.filePath(fileInfo.fileName());
			containsDll = true;
			deleteFileOrFolder(toFilePath);
		}
	}

	printLog(QString("plugins directory %1 %2.dll").arg(containsDll ? "contain" : "not contain").arg(dllName));
}

void PLSLaboratoryManage::downloadLabZipFileSuccess(const QObject *receiver, const QString &labId, const DownloadZipCallback &callback)
{
	printLog(QString("download laboratory object success , lab id is %1").arg(labId));
	PLSLabDownloadFile *file = pls_new<PLSLabDownloadFile>(labId);
	connect(
		file, &PLSLabDownloadFile::taskSucceeded, this,
		[labId, callback, this, receiver](const QVariantMap &downloadMap) {
			if (!pls_object_is_valid(receiver)) {
				printLog(QString("The user clicks the close button on the installation page to cancel compress object the laboratory object {id:%1}").arg(labId));
				deleteZipFilesAndUnzipFolder(labId);
				return;
			}
			unzipFileSuccess(labId, downloadMap, callback);
		},
		Qt::BlockingQueuedConnection);
	connect(
		file, &PLSLabDownloadFile::taskFailed, this, [labId, callback, this] { downloadLabZipFileFail(labId, callback, DownloadZipErrorType::DownloadZipUnzipFileError); },
		Qt::BlockingQueuedConnection);
	connect(file, &PLSLabDownloadFile::taskRelease, this, [this] { checkThreadPoolIfDone(); });
	m_threadPool.start(file);
}

void PLSLaboratoryManage::unzipFileSuccess(const QString &labId, const QVariantMap &downloadMap, const DownloadZipCallback &callback)
{
	printLog(QString("unzip laboratory zip file success, lab id is %1").arg(labId));
	QString type = downloadMap.value(g_labDownloadType).toString();
	QString downloadName = downloadMap.value(g_labDownloadName).toString();
	if (type != g_laboratoryDllType) {
		printLog(QString("non-plugin type request lab task success, lab id is %1, download lab name is %2").arg(labId).arg(downloadName));
		downloadZipFileSuccessCallback(downloadMap, labId, callback);
		return;
	}

	if (isDllLoadedByDllName(downloadName)) {
		printLog(QString("The current plugin has been loaded , plugin type request lab task failed, dllName is %1, lab id is %2").arg(downloadName).arg(labId));
		downloadLabZipFileFail(labId, callback, DownloadZipErrorType::DownloadZipDllIsLoaded);
		return;
	}

	printLog(QString("start copying the downloaded ZIP plugin to the prism-plugins directory, lab id is %1").arg(labId));
	if (!copyDownloadLabCacheDllFileToAppPluginFolder(downloadMap)) {
		printLog(QString("copying plugin files, lab id is %1 failed").arg(labId));
		downloadLabZipFileFail(labId, callback, DownloadZipErrorType::DownloadZipCopyDllToAppFolderFailed);
		return;
	}
	printLog(QString("copying the downloaded ZIP plugin to the prism-plugins directory success, lab id is %1").arg(labId));
	printLog(QString("plugin type request lab task success, id is %1").arg(labId));
	downloadZipFileSuccessCallback(downloadMap, labId, callback);
}

void PLSLaboratoryManage::downloadLabZipFileFail(const QString &labId, const DownloadZipCallback &callback, DownloadZipErrorType errorType)
{
	deleteZipFilesAndUnzipFolder(labId);
	downloadLabCache.remove(labId);
	if (callback) {
		callback(false, errorType);
	}
}

void PLSLaboratoryManage::downloadZipFileSuccessCallback(const QVariantMap &downloadMap, const QString &labId, const DownloadZipCallback &callback)
{
	deleteFileOrFolder(getLabZipFilePath(labId));
	downloadLabCache.insert(labId, downloadMap);
	if (callback) {
		callback(true, DownloadZipErrorType::DownloadZipNoError);
	}
}

bool PLSLaboratoryManage::isExistedAppDll(const QString &dllName) const
{
	printLog(QString("start checking if the %1.dll file existed in app plugin folder").arg(dllName));
	if (dllName.isEmpty()) {
		printLog("dll name is empty");
		return false;
	}
	auto dllPath = getPrismPluginFolderPath() + +"/" + dllName + ".dll";
	printKRLog(QString("app dll file path is ") + dllPath);
	QFileInfo fileInfo(dllPath);
	if (!fileInfo.exists()) {
		printLog(QString("the %1.dll file is not existed in app plugin folder").arg(dllName));
		return false;
	}
	auto prismPluginDataFolderPath = getAppDllDataDirPathByDllName(dllName);
	printKRLog(QString("app dll data folder path is ") + prismPluginDataFolderPath);
	QDir dir(prismPluginDataFolderPath);
	if (!dir.exists()) {
		printLog(QString("the %1.dll data folder is not existed in app plugin data folder").arg(dllName));
		return false;
	}
	printLog(QString("the %1.dll file existed in app plugin folder").arg(dllName));
	return true;
}

bool PLSLaboratoryManage::copyDownloadLabCacheDllFileToAppPluginFolder(const QVariantMap &downloadMap)
{
	QString type = downloadMap.value(g_labDownloadType).toString();
	if (type != g_laboratoryDllType) {
		return false;
	}

	//If the basic information of the downloaded plugin is invalid, delete the local resources
	QString dllName = downloadMap.value(g_labDownloadName).toString();
	if (dllName.isEmpty()) {
		printLog("The downloaded dll is invalid, downloaded lab info, dllName is empty");
		return false;
	}

	//If it is a plug-in type and the plug-in has not been loaded into the App
	deleteDownloadLabCacheDLLObject(downloadMap);

	//First determine whether the locally downloaded plug-in file is valid
	QString labId = downloadMap.value(g_labDownloadId).toString();
	QStringList labFileListPath = downloadMap.value(g_labDownloadFileListPath).toStringList();
	if (!isValidFileList(labFileListPath)) {
		printLog(QString("The list of downloaded files is invalid, downloaded lab info, lab id is %1, dllName is %2").arg(labId).arg(dllName));
		return false;
	}

	printLog("Copy the locally downloaded plugin file to the program's plugin directory");
	QString zipFolderPath = downloadMap.value(g_labDownloadUnzipFolderPath).toString();
	if (!copyDirFiles(zipFolderPath, getPrismPluginFolderPath())) {
		printLog("failed to copy the locally downloaded plug-in file to the program's plug-in directory");
		return false;
	}
	printLog("suceess to copy the locally downloaded plugin file to the program's plugin directory");

	printLog("Copy the locally downloaded plugin data folder to the program's plugin data directory");
	auto prismPluginDataFolderPath = getAppDllDataDirPathByDllName(dllName);
	auto zipDataFolderPath = zipFolderPath + "/data";
	if (!LabManage->copyDirWithRecursive(zipDataFolderPath, prismPluginDataFolderPath)) {
		printLog("The locally downloaded plugin data is copied to the program's plugin data directory failed");
		return false;
	}
	printLog("suceess to copy the locally downloaded plugin data folder to the program's plugin data directory");

	QString downloadFileVersion = downloadMap.value(g_labDownloadVersion).toString();
	printLog(QString("save app dll info to app data/prism-plugins folder , appDllVersion id is %1 , appDllName is %2").arg(downloadFileVersion).arg(dllName));
	QDir dataDir(prismPluginDataFolderPath);
	QString labDllFilePath = dataDir.absoluteFilePath(QString("labDll.ini"));
	QSettings langSetting(labDllFilePath, QSettings::IniFormat);
	langSetting.beginGroup(LAB_DLL_MESSAGE_INFO);
	langSetting.setValue(g_labDllVersionKey, downloadFileVersion);
	langSetting.setValue(g_labDllNameKey, dllName);
	langSetting.endGroup();
	langSetting.sync();
	printLog("save app dll info to app data/prism-plugins folder success");

	return true;
}

void PLSLaboratoryManage::deleteDownloadLabCacheDLLObject(const QVariantMap &downloadMap)
{
	//If it is a plug-in type and the plug-in has not been loaded into the App
	QStringList dllNameList = downloadMap.value(g_labDownloadDllFileNameList).toStringList();

	//There is no plugin list field in download, you need to use the original plugin name
	if (dllNameList.isEmpty()) {
		QString dllName = downloadMap.value(g_labDownloadName).toString();
		deleteAppDllWithDllName(dllName);
		return;
	}

	//Remove all plugins associated with the plugin
	for (auto dllName : dllNameList) {
		if (isDllLoadedByDllName(dllName)) {
			continue;
		}
		deleteAppDllWithDllName(dllName);
	}
}

bool PLSLaboratoryManage::isDownloadSuccess(const QString &labId)
{
	printLog(QString("start checking if the current lab is downloaded successfully, lab id is %1").arg(labId));
	if (!downloadLabCache.contains(labId)) {
		printLog(QString("the current lab is not downloaded successfully, downloadLabCache not contains lab id , lab id is %1").arg(labId));
		return false;
	}

	QVariantMap map = downloadLabCache.value(labId);
	QStringList labFileListPath = map.value(g_labDownloadFileListPath).toStringList();
	QString type = map.value(g_labDownloadType).toString();
	if (type != g_laboratoryDllType) {
		if (!isValidFileList(labFileListPath)) {
			printLog(QString("the current lab is not downloaded successfully, non-plugin type file list is invalid, lab id is %1").arg(labId));
			return false;
		}
		printLog(QString("the current lab is downloaded successfully, non-plugin type file list is valid, lab id is %1").arg(labId));
		return true;
	}

	QString dllName = map.value(g_labDownloadName).toString();
	printLog(QString("start to check if the %1.dll exists in the app plugin directory,lab id is %2").arg(dllName).arg(labId));
	if (!isExistedAppDll(dllName)) {
		printLog(QString("the %1.dll does not exist in the app plugin directory , lab id is %2").arg(dllName).arg(labId));
		if (!isValidFileList(labFileListPath)) {
			printLog(QString("the current lab is not downloaded successfully, plugin type file list is invalid , lab id is %1").arg(labId));
			return false;
		}
		if (!copyDownloadLabCacheDllFileToAppPluginFolder(map)) {
			printLog(QString("the current lab is not downloaded successfully, failed to copy the current plugin file, lab id is %1").arg(labId));
			return false;
		}
	}
	printLog(QString("the %1.dll file exist in the App plugin directory, lab id is %2").arg(dllName).arg(labId));

	printLog(QString("start calling the load plugin function, lab id is %1").arg(labId));
	if (!pls_load_plugin(getAppDllFilePathWithLabId(labId).toUtf8().constData(), getAppDllDataDirPathByLabId(labId).toUtf8().constData())) {
		printLog(QString("the current lab is not downloaded successfully, failed to load current plugin, lab id is %1").arg(labId));
		return false;
	}
	printLog("success to load current plugin");
	printLog(QString("the current lab is downloaded successfully, lab id is %1").arg(labId));

	return true;
}

void PLSLaboratoryManage::saveLaboratoryUseState(const QString &labId, bool used)
{
	if (!downloadLabCache.contains(labId)) {
		return;
	}
	printLog(QString("save current lab used state, lab id is %1, check state is %2 ").arg(labId).arg(used ? "checked" : "unchecked"));
	QVariantMap &map = downloadLabCache[labId];
	map.insert(g_labDownloadUseState, used);
	saveDownloadLabCache();
}

bool PLSLaboratoryManage::getLaboratoryUseState(const QString &labId) const
{
	if (!downloadLabCache.contains(labId)) {
		return false;
	}
	const QVariantMap &map = downloadLabCache.value(labId);
	return map.value(g_labDownloadUseState).toBool();
}

bool PLSLaboratoryManage::isUsedForCurrentDllBinPath(const QString &bin_path) const
{
	QFileInfo fileInfo(bin_path);
	QString matchDllName = fileInfo.baseName();
	QString dirName1 = fileInfo.dir().absolutePath();
	QString dirName2 = fileInfo.dir().path();
	if (fileInfo.dir().path().startsWith(common::LABORATORY_PLUGIN_FOLDER_NAME)) {
		bool dllIsOpen = false;
		for (const QString &labId : downloadLabCache.keys()) {
			QVariantMap map = downloadLabCache.value(labId);
			QString type = map.value(g_labDownloadType).toString();
			if (type != g_laboratoryDllType) {
				continue;
			}
			QString dllName = map.value(g_labDownloadName).toString();
			bool open = map.value(g_labDownloadUseState).toBool();
			if (dllName == matchDllName && open && !isPrismLowVersion(labId)) {
				dllIsOpen = true;
				break;
			}
		}
		printLog(QString("laboratory plugin object is %1 ,loaded is %2").arg(matchDllName).arg(dllIsOpen ? "success" : "failed"));
		return dllIsOpen;
	}
	return true;
}

bool PLSLaboratoryManage::isValidForLabFunc(const QString &labId) const
{
	printLog(QString("start checking current laboratory is valid for lab func , lab id is %1").arg(labId));
	if (!downloadLabCache.contains(labId)) {
		printLog(QString("current laboratory is not downloaded , lab id is %1").arg(labId));
		return false;
	}
	QVariantMap map = downloadLabCache.value(labId);
	QStringList labFileListPath = map.value(g_labDownloadFileListPath).toStringList();
	if (!isValidFileList(labFileListPath)) {
		printLog(QString("current laboratory file list is invalid , lab id is %1").arg(labId));
		return false;
	}
	printLog(QString("end checking current laboratory is valid , lab id is %1").arg(labId));
	return true;
}

QString PLSLaboratoryManage::getFilePath(const QString &path)
{
	QFileInfo fi(pls_get_user_path(LABORATORY_DIR + path));
	createDir(fi.dir());
	return fi.absoluteFilePath();
}

QString PLSLaboratoryManage::getFilePathByURL(const QString &url) const
{
	QString fileName = getFileNameByURL(url);
	QString filePath = getFilePath(fileName);
	QFileInfo fileInfo(filePath);
	return fileInfo.absoluteFilePath();
}

QString PLSLaboratoryManage::getFileNameByURL(const QString &url) const
{
	QStringList strList = url.split("/");
	if (!strList.isEmpty()) {
		return strList.last();
	}
	return QString();
}

void PLSLaboratoryManage::saveDllInfoToAppPluginDataFolder(const QString &ladId)
{
	auto prismPluginDataFolderPath = getAppDllDataDirPathByLabId(ladId);
	QDir dataDir(prismPluginDataFolderPath);
	QString labDllFilePath = dataDir.absoluteFilePath(QString("labDll.ini"));
	QSettings langSetting(labDllFilePath, QSettings::IniFormat);
	langSetting.beginGroup(LAB_DLL_MESSAGE_INFO);
	printLog(QString("start save app dll info to app data/prism-plugins folder , lab id is %1").arg(ladId));
	QString appDllVersion = getStringInfo(ladId, laboratory_data::g_laboratoryVersion);
	QString appDllName = getDllNameWithoutSuffix(ladId);
	langSetting.setValue(g_labDllVersionKey, appDllVersion);
	langSetting.setValue(g_labDllNameKey, appDllName);
	printLog(QString("end save app dll info to app data/prism-plugins folder , appDllVersion id is %1 , appDllName is %2").arg(appDllVersion).arg(appDllName));
	langSetting.endGroup();
	langSetting.sync();
}

QString PLSLaboratoryManage::getDllInfoByDirPath(const QString &dirPath, const QString &key) const
{
	QDir dataDir(dirPath);
	QString labDllFilePath = dataDir.absoluteFilePath(QString("labDll.ini"));
	QSettings langSetting(labDllFilePath, QSettings::IniFormat);
	langSetting.beginGroup(LAB_DLL_MESSAGE_INFO);
	QString value = langSetting.value(key).toString();
	langSetting.endGroup();
	return value;
}

QString PLSLaboratoryManage::getDllInfoByLabId(const QString &ladId, const QString &key)
{
	auto prismPluginDataFolderPath = getAppDllDataDirPathByLabId(ladId);
	return getDllInfoByDirPath(prismPluginDataFolderPath, key);
}

QString PLSLaboratoryManage::getDllInfoByDllName(const QString &dllName, const QString &key) const
{
	auto prismPluginDataFolderPath = getAppDllDataDirPathByDllName(dllName);
	return getDllInfoByDirPath(prismPluginDataFolderPath, key);
}

bool PLSLaboratoryManage::isDllType(const QString &ladId)
{
	QString type = getStringInfo(ladId, laboratory_data::g_laboratoryType);
	return type == g_laboratoryDllType;
}

void PLSLaboratoryManage::enumObsModules()
{
	m_dllLoadedPathList.clear();
	auto EnumModule = [](void *param, obs_module_t *module_) {
		const char *path = obs_get_module_binary_path(module_);
		auto self = (PLSLaboratoryManage *)param;
		self->m_dllLoadedPathList.append(path);
	};
	obs_enum_modules(EnumModule, this);
}

bool PLSLaboratoryManage::isDllLoadedByDllName(const QString &dllName)
{
	enumObsModules();
	printLog(QString("start checking if the plugin has been loaded by the plugin name, plugin name is %1").arg(dllName));
	for (const auto &path : m_dllLoadedPathList) {
		QFileInfo fileInfo(path);
		if (fileInfo.baseName() == dllName) {
			printLog(QString("plugin name is loaded , plugin name is %1").arg(dllName));
			return true;
		}
	}
	printLog(QString("plugin name is not loaded , plugin name is %1").arg(dllName));
	return false;
}

void PLSLaboratoryManage::loadDownloadLabCache()
{
	printLog("begin initializing download data according to the downloadLab.json file");

	//First check whether the Json data on the server side is empty
	if (m_LaboratoryInfos.isEmpty()) {
		printLog("end initializing download data, laboratory data is empty");
		return;
	}

	QByteArray bytes;
	QString downloadImageCacheFile = getFilePath(DOWNLOAD_LAB_JSON_NAME);
	PLSJsonDataHandler::getJsonArrayFromFile(bytes, downloadImageCacheFile);
	if (bytes.isEmpty()) {
		printLog("end initializing download data,the data obtained from the downloadLab.json file is empty");
		return;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		printLog(QString("end initializing download data, failed to parse downloadLab.json file data. reason: %1").arg(parseError.errorString()));
		return;
	}

	for (auto it : doc.array()) {
		QVariantMap map = it.toObject().toVariantMap();
		QString labId = map.value(g_labDownloadId).toString();
		if (labId.isEmpty() || map.isEmpty()) {
			printLog(QString("the current user downloaded lab id is %1 , map is %2 ").arg(labId.isEmpty() ? "empty" : "has value").arg(map.isEmpty() ? "empty" : "has value"));
			continue;
		}
		downloadLabCache.insert(labId, map);
		QString labType = map.value(g_labDownloadType).toString();
		QString labName = map.value(g_labDownloadName).toString();
		if (labType == g_laboratoryDllType && !labName.isEmpty()) {
			downloadPluginLabCache.insert(labId, map);
		}
		printLog(QString("the current user downloaded lab info ,lab id is %1 , title is %2 , type is %3 , open button used is %4")
				 .arg(labId)
				 .arg(labName)
				 .arg(labType)
				 .arg(map.value(g_labDownloadUseState).toBool() ? "checked" : "unchecked"));
	}
	printLog(QString("end initializing download data, The number of labs that the current user has downloaded count is %1").arg(downloadLabCache.count()));
}

void PLSLaboratoryManage::saveDownloadLabCache() const
{
	QJsonArray array;
	for (const QVariantMap &map : downloadLabCache.values()) {
		array.append(QJsonObject(QJsonDocument::fromJson(QJsonDocument::fromVariant(QVariant(map)).toJson()).object()));
	}
	QString downloadImageCacheFile = getFilePath(DOWNLOAD_LAB_JSON_NAME);
	QJsonDocument doc(array);
	if (!PLSJsonDataHandler::saveJsonFile(doc.toJson(), downloadImageCacheFile)) {
		printLog("save the downloadLab.json file failed.");
		return;
	}
	printLog("save the downloadLab.json file success.");
}

void PLSLaboratoryManage::deleteFileOrFolder(const QString &strPath) const
{
	if (strPath.isEmpty()) {
		printLog("deleteFileOrFolder failed, because path is empty");
		return;
	}

	QFileInfo fileInfo(strPath);
	if (!fileInfo.exists()) {
		if (fileInfo.fileName() == fileInfo.baseName()) {
			printLog(QString("delete folder is not existed , folder name is %1").arg(fileInfo.fileName()));
			printKRLog(QString("delete folder is not existed , file path is ") + strPath);
		} else {
			printLog(QString("delete file is not existed , file name is %1").arg(fileInfo.fileName()));
			printKRLog(QString("delete file is not existed , file path is ") + strPath);
		}
		return;
	}

	if (fileInfo.isFile()) {
		QFile file(strPath);
		if (file.remove()) {
			printLog(QString("delete file is success , file name is %1").arg(fileInfo.fileName()));
			printKRLog(QString("delete file is success , file path is ") + strPath);
			return;
		}
		printLog(QString("delete file is failed, file name is %1, failed reson is %2").arg(fileInfo.fileName()).arg(file.errorString()));
		printKRLog(QString("delete file is failed, file path is ") + strPath + QString(" , failed reson is %1").arg(file.errorString()));
	} else if (fileInfo.isDir()) {
		QDir qDir(strPath);
		bool result = qDir.removeRecursively();
		printLog(QString("delete folder is %1 , folder name is %2").arg(result ? "success" : "failed").arg(fileInfo.fileName()));
		printKRLog(QString("delete folder is %1 , folder path is ").arg(result ? "success" : "failed") + strPath);
	}
}

void PLSLaboratoryManage::deleteZipFilesAndUnzipFolder(const QString &labId)
{
	QString zipFilePath = getLabZipFilePath(labId);
	QString zipFolderPath = getLabZipUnzipFolderPath(labId);
	QString title = getStringInfo(labId, laboratory_data::g_laboratoryTitle);
	printLog(QString("delete zip file, lab id is %1 , lab title is %2").arg(labId).arg(title));
	printKRLog(QString("delete zip file, lab id is %1 ,lab title is %2 , delete zip path is ").arg(labId).arg(title) + zipFilePath);
	deleteFileOrFolder(zipFilePath);
	printLog(QString("delete unzip folder, lab id is %1 , lab title is %2").arg(labId).arg(title));
	printKRLog(QString("delete unzip folder, lab id is %1 , lab title is %2 , delete unzip folder is ").arg(labId).arg(title) + zipFolderPath);
	deleteFileOrFolder(zipFolderPath);
}

void PLSLaboratoryManage::checkThreadPoolIfDone() const
{
	if (!m_threadPool.activeThreadCount()) {
		saveDownloadLabCache();
	}
}

bool PLSLaboratoryManage::copyDirWithRecursive(const QString &fromDir, const QString &toDir) const
{
	QDir sourceDir(fromDir);
	QDir targetDir(toDir);
	if (!targetDir.exists() && !targetDir.mkpath(targetDir.absolutePath())) {
		printLog("create copy recursive destination dir failed");
		return false;
	}
	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	foreach(QFileInfo fileInfo, fileInfoList)
	{
		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		if (fileInfo.isDir()) {
			if (!copyDirWithRecursive(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName())))
				return false;
		} else {
			if (targetDir.exists(fileInfo.fileName())) {
				targetDir.remove(fileInfo.fileName());
			}
			QString destinationPath = targetDir.filePath(fileInfo.fileName());
			QFile file(fileInfo.filePath());
			if (!file.copy(destinationPath)) {
				printLog(QString("copy recursive copy desination path failed failed reason is %1").arg(file.errorString()));
				return false;
			}
		}
	}
	return true;
}

bool PLSLaboratoryManage::copyDirFiles(const QString &fromDir, const QString &toDir) const
{
	QDir sourceDir(fromDir);
	QDir targetDir(toDir);
	if (!targetDir.exists() && !targetDir.mkdir(targetDir.absolutePath())) {
		return false;
	}
	QFileInfoList fileInfoList = sourceDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
	for (const QFileInfo &fileInfo : fileInfoList) {
		QString destinationPath = targetDir.filePath(fileInfo.fileName());
		QFile file(fileInfo.filePath());
		if (!file.copy(destinationPath)) {
			printLog(QString("copy source file to destination folder failed, source file name is %1 , destination dir name is %2 , failed reason is %3")
					 .arg(fileInfo.fileName())
					 .arg(targetDir.dirName())
					 .arg(file.errorString()));
			printKRLog(QString("copy source file to destination folder failed, source file path is ") + fileInfo.filePath() + QString(" , destination dir path is ") + targetDir.path() +
				   QString(" , failed reason is %1").arg(file.errorString()));
			return false;
		}
		printLog(QString("copy source file to destination folder success, source file name is %1 , destination dir name is %2").arg(fileInfo.fileName()).arg(targetDir.dirName()));
		printKRLog(QString("copy source file to destination folder success, source file path is ") + fileInfo.filePath() + QString(" , destination dir path is ") + targetDir.path());
	}
	return true;
}

QString PLSLaboratoryManage::getOldPrismPluginFolderPath() const
{
#ifdef _WIN32
	return QCoreApplication::applicationDirPath() + "/" + common::LABORATORY_OLD_PLUGIN_FOLDER_NAME;
#else
	auto dirs = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
	auto path = dirs.first() + "/" + common::LABORATORY_OLD_PLUGIN_FOLDER_NAME;
	pls_mkdir(path);
	return path;
#endif
}

QString PLSLaboratoryManage::getOldPrismPluginDataFolderPath() const
{
#ifdef _WIN32
	return QCoreApplication::applicationDirPath() + "/data" + "/" + common::LABORATORY_OLD_PLUGIN_FOLDER_NAME;
#else
	auto dirs = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
	auto path = dirs.first() + "/data/" + common::LABORATORY_OLD_PLUGIN_FOLDER_NAME;
	pls_mkdir(path);
	return path;
#endif
}

QString PLSLaboratoryManage::getPrismPluginFolderPath() const
{
#ifdef _WIN32
	return QCoreApplication::applicationDirPath() + "/" + common::LABORATORY_PLUGIN_FOLDER_NAME;
#else
	auto dirs = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
	auto path = dirs.first() + "/" + common::LABORATORY_PLUGIN_FOLDER_NAME;
	pls_mkdir(path);
	return path;
#endif
}

QString PLSLaboratoryManage::getPrismPluginDataFolderPath() const
{
#ifdef _WIN32
	return QCoreApplication::applicationDirPath() + "/data" + "/" + common::LABORATORY_PLUGIN_FOLDER_NAME;
#else
	auto dirs = QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation);
	auto path = dirs.first() + "/data/" + common::LABORATORY_PLUGIN_FOLDER_NAME;
	pls_mkdir(path);
	return path;
#endif
}

QString PLSLaboratoryManage::getDllNameWithoutSuffix(const QString &labId)
{
	QString dllName = getSettingStringInfo(labId, laboratory_data::g_settingsDllName);
	QString dllFileName = dllName.section('.', 0, 0);
	return dllFileName;
}

QString PLSLaboratoryManage::getAppDllFilePathWithLabId(const QString &labId)
{
	QString binPath = getPrismPluginFolderPath() + "/" + getSettingStringInfo(labId, laboratory_data::g_settingsDllName);
	return binPath;
}

QString PLSLaboratoryManage::getAppDllDataDirPathByLabId(const QString &labId)
{
	auto dirPath = getPrismPluginDataFolderPath() + "/" + getDllNameWithoutSuffix(labId);
	return dirPath;
}

QString PLSLaboratoryManage::getAppDllDataDirPathByDllName(const QString &dllName) const
{
	auto dirPath = getPrismPluginDataFolderPath() + "/" + dllName;
	return dirPath;
}

bool PLSLaboratoryManage::isValidFileList(const QStringList &filePathList) const
{
	printLog(QString("start checking laboratory file list"));
	bool result = true;
	for (auto filePath : filePathList) {
		QFileInfo fileInfo(filePath);
		if (!fileInfo.exists()) {
			printKRLog(QString("check laboratory invalid file is ") + filePath);
			result = false;
			break;
		}
	}
	printLog(QString("the laboratory file list is %1").arg(result ? "valid" : "invalid"));
	return result;
}

void PLSLaboratoryManage::initLaboratoryData()
{
	printLog("begin initializing laboratory data in memory according to the laboratory.json file");
	if (!m_LaboratoryInfos.isEmpty()) {
		return;
	}

	QFileInfo fi(getFilePath(LAB_JSON_NAME));
	QString filePath = fi.absoluteFilePath();
	if (!checkLabJsonValid(filePath)) {
		printLog("get the the laboratory.json file path packaged in the App");
		filePath = findFileInResources(ChannelData::defaultSourcePath, "Laboratory.json");
	}

	QByteArray byteArray;
	PLSJsonDataHandler::getJsonArrayFromFile(byteArray, filePath);
	if (byteArray.size() == 0) {
		printLog("get the data under the laboratory.json file path is empty");
	}

	QVariantList variantList;
	PLSJsonDataHandler::jsonTo(byteArray, variantList);

	for (auto variant : variantList) {
		QVariantMap variantMap = variant.toMap();
		QString titleValue = variantMap.value(laboratory_data::g_laboratoryTitle).toMap().value(pls_get_current_language_short_str()).toString();
		variantMap.insert(laboratory_data::g_laboratoryTitle, titleValue);
		QString labId = variantMap.value(laboratory_data::g_laboratoryId).toString();
		m_LaboratoryInfos.insert(labId, variantMap);
		m_labIdList.append(labId);
		printLog(QString("the laboratory.json lab info ,lab id is %1 , title is %2 ").arg(labId).arg(titleValue));
	}
	printLog(QString("end initializing laboratory data, The current number of lab functions count is %1").arg(m_LaboratoryInfos.count()));
}
