#ifndef PLSLABORATORYMANAGE_H
#define PLSLABORATORYMANAGE_H

#include <QObject>
#include <QVariantMap>
#include <QThreadPool>
#include <QReadWriteLock>
#include "PLSApp.h"
#include "libhttp-client.h"

enum class DownloadZipErrorType {
	DownloadZipNoError,
	DownloadZipNotExistInServer,
	DownloadZipNetworkError,
	DownloadZipUnzipFileError,
	DownloadZipCopyDllToAppFolderFailed,
	DownloadZipLoadDllFailed,
	DownloadZipDllIsLoaded,
};

using DownloadZipCallback = std::function<void(bool succeeded, DownloadZipErrorType errorType)>;

namespace laboratory_data {

const QString g_laboratoryType = "type";
const QString g_laboratoryDetailPage = "detailPage";
const QString g_laboratoryId = "id";
const QString g_laboratoryTitle = "title";
const QString g_laboratoryVersion = "version";
const QString g_laboratoryNeedInstall = "needInstall";
const QString g_laboratoryOpenRestart = "openRestart";
const QString g_laboratoryCloseRestart = "closeRestart";
#if defined(Q_OS_WIN)
const QString g_laboratoryPrismVersion = "prismVersion";
#elif defined(Q_OS_MACOS)
const QString g_laboratoryPrismVersion = "prismMacVersion";
#endif
const QString g_settingsDownloadUrl = "downloadUrl";
const QString g_settingsTimeout = "timeoutMs";
const QString g_settingsDllName = "dllName";
};

extern const QString g_laboratoryZipType;
extern const QString g_laboratoryDllType;

extern const QString g_labDownloadName;
extern const QString g_labDownloadZipFilePath;
extern const QString g_labDownloadUnzipFolderPath;
extern const QString g_labDownloadFileListPath;
extern const QString g_labDownloadDllFileNameList;
extern const QString g_labDownloadVersion;
extern const QString g_labDownloadId;
extern const QString g_labDownloadType;
extern const QString g_labDownloadUseState;
extern const char *const MODULE_ABORATORY;

extern const QString g_labDllVersionKey;
extern const QString g_labDllNameKey;

using LaboratoryMap = QMap<QString, QVariantMap>;

class PLSLaboratoryManage : public QObject {
	Q_OBJECT

public:
	static PLSLaboratoryManage *instance();
	void requestLabJsonData();
	void checkLabDllUpdate();
	void checkLabZipUpdate();
	void checkSingleLabZipValidity(const QString &labId);
	bool checkLabNeedInstall(const QString &labId);
	~PLSLaboratoryManage() override;
	explicit PLSLaboratoryManage(QObject *parent = nullptr);
	const QStringList &getLabIdList();
	QString getStringInfo(const QString &labId, const QString &key);
	QString getSettingStringInfo(const QString &labId, const QString &key);
	int getSettingIntInfo(const QString &labId, const QString &key);
	QString getLabZipFilePath(const QString &labId);
	QString getLabDetailPageFilePathByLabId(const QString &labId);
	QString getAppDetailPageFilePathByLabId(const QString &labId);
	QString getLabZipUnzipFolderPath(const QString &labId);
	bool getBoolInfo(const QString &labId, const QString &key);
	void printLog(const QString &subLog) const;
	void printKRLog(const QString &subLog) const;
	void downloadLabZipFile(const PLSCancel &cancel, const QObject *receiver, const QString &labId, const DownloadZipCallback &callback = nullptr);
	bool isPrismLowVersion(const QString &labId) const;
	void getDllNameList(const QString &dirPath, QStringList &dllFileNameList) const;
	void getFilePathListRecursive(const QString &dirPath, QStringList &filePathList);
	bool copyDirWithRecursive(const QString &fromDir, const QString &toDir) const;
	bool copyDirFiles(const QString &fromDir, const QString &toDir) const;
	QString getOldPrismPluginFolderPath() const;
	QString getOldPrismPluginDataFolderPath() const;
	QString getPrismPluginFolderPath() const;
	QString getPrismPluginDataFolderPath() const;
	QString getDllNameWithoutSuffix(const QString &labId);
	QString getAppDllFilePathWithLabId(const QString &labId);
	QString getAppDllDataDirPathByLabId(const QString &labId);
	QString getAppDllDataDirPathByDllName(const QString &dllName) const;
	void saveDllInfoToAppPluginDataFolder(const QString &ladId);
	QString getDllInfoByDirPath(const QString &dirPath, const QString &key) const;
	QString getDllInfoByLabId(const QString &ladId, const QString &key);
	QString getDllInfoByDllName(const QString &dllName, const QString &key) const;
	bool isDllType(const QString &ladId);
	void enumObsModules();
	bool isDllLoadedByDllName(const QString &dllName);
	bool isDownloadSuccess(const QString &labId);
	void saveLaboratoryUseState(const QString &labId, bool used);
	bool getLaboratoryUseState(const QString &labId) const;
	bool isUsedForCurrentDllBinPath(const QString &bin_path) const;
	bool isValidForLabFunc(const QString &labId) const;

private:
	void initThreadPool();
	void initLaboratoryData();
	bool checkLabJsonValid(const QString &destPath) const;
	static QString getFilePath(const QString &path);
	QString getFilePathByURL(const QString &url) const;
	QString getFileNameByURL(const QString &url) const;

	void loadDownloadLabCache();
	void saveDownloadLabCache() const;
	void deleteFileOrFolder(const QString &strPath) const;
	void deleteZipFilesAndUnzipFolder(const QString &labId);
	void checkThreadPoolIfDone() const;

	bool isValidFileList(const QStringList &filePathList) const;
	int compareVersion(const QString &v1, const QString &v2) const;
	void deleteOrUpdateAppDllFolder();
	void deleteAppDllWithLabId(const QString &labId);
	void deleteAppDllWithDllName(const QString &dllName) const;
	void deleteDllNameFileByPath(const QString &dllDirPath, const QString &dllName) const;
	void downloadLabZipFileSuccess(const QObject *receiver, const QString &labId, const DownloadZipCallback &callback);
	void unzipFileSuccess(const QString &labId, const QVariantMap &downloadMap, const DownloadZipCallback &callback);
	void downloadLabZipFileFail(const QString &labId, const DownloadZipCallback &callback, DownloadZipErrorType errorType);
	void downloadZipFileSuccessCallback(const QVariantMap &downloadMap, const QString &labId, const DownloadZipCallback &callback);
	bool isExistedAppDll(const QString &dllName) const;
	bool copyDownloadLabCacheDllFileToAppPluginFolder(const QVariantMap &downloadMap);
	void deleteDownloadLabCacheDLLObject(const QVariantMap &downloadMap);

	QStringList m_labIdList;
	LaboratoryMap m_LaboratoryInfos;
	LaboratoryMap downloadLabCache;
	LaboratoryMap downloadPluginLabCache;
	QThreadPool m_threadPool;
	QStringList m_dllLoadedPathList;
	QMap<QString, pls::http::Request> m_cancelRequestMap;
	PLSCancel m_plsCancel;
	bool m_labRequesting{false};
};

#define LAB_LOG(sublog) PLSLaboratoryManage::instance()->printLog(sublog)
#define LAB_KR_LOG(sublog) PLSLaboratoryManage::instance()->printKRLog(sublog)
#define LabManage PLSLaboratoryManage::instance()

#endif // PLSLABORATORYMANAGE_H
