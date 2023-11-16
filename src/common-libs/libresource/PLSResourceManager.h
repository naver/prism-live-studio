#ifndef PLSRESOURCEMANAGER_H
#define PLSRESOURCEMANAGER_H

#include "PLSResourceHandle_global.h"
#include <QObject>
#include <QVariantMap>
#include <qhash.h>
#include <qmap.h>
#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaEnum>
#include "PLSResEnums.h"
#include <qmutex.h>

#define PLSRESOURCEMGR_INSTANCE PLSResourceManager::instance()
class resource_modules;
using startDownloadCallback = std::function<void(bool)>;
using resCheckCallback = std::function<void(resource_modules modules)>;

class PLSRESOURCEHANDLE_EXPORT PLSResourceManager : public QObject {
	Q_OBJECT
public:
	enum class resource_modules {
		None = 0,
		Textmotion,
		Virtual,
		PrismSticker,
		Music,
		Library,
		Beauty,
		Color_Filter,
		Reaction,
	};
	Q_ENUM(resource_modules)
	static PLSResourceManager *instance();
	static QString getmoduleName(resource_modules resourceModules);
	static resource_modules getModuleId(const QString &moduleIdStr);
	static QString resourceModuleTocategoryId(resource_modules resourceModules);
	static void setParams(const QVariantHash &paramsData);
	static QVariantHash &getParams();

	QString getResourceTmpFilePath(const QString &fileName) const;
	bool isExistPath(const QString &resDir) const;
	QString makePath(const QString &resDir) const;
	void startCheckResource();
	QHash<QString, QVariant> getALLNeedDownloadResInfo() const;
	qint64 getTotalDownloadNum() const;
	qint64 getCurrentDownloadNum() const;

	void startDownloadResources(const QHash<QString, QVariant> &needDownLoadFileList);

	QJsonArray getNeedDownloadResInfo(const QString &moduleName) const;
	QString getModuleJsonUrl(resource_modules modules);
	void copyTextmotionScreenRes(const QString &zipFilePath, const QString &zipFileName) const;
	void copyTextmotionWeb() const;
	void updateResourceDownloadStatus(const QString &url, PLSResDownloadStatus downloadStatus);
	PLSResDownloadStatus resourceDownloadStatus(const QString &url);
public slots:
	void appendResourceNum(qint64 resourceNum);
	void appendResourceDownloadedNum(qint64 downloadedNum);
	void downloadedZipHandle(const QString &zipFilePath, const QString &zipName, const QJsonArray &fileListInfo, const QString &dstDir = QString()) const;
	void getBannerJson();
private slots:
	void getLibraryJsons(const QString &path, const QString &itemId);
	void parseResTemplates();

private:
	explicit PLSResourceManager();
	~PLSResourceManager() override = default;
	void delInvalidPath(QString &path) const;
	void getOldResCahe();
	void getLocalCategoriesJson();
	void parseResourceJson(QMap<QString, QVariantMap> &categoriesData, const QByteArray &resourceJsonData) const;
	void checkResourceUpdate();

	bool isFileExist(const QString &absolutePath) const;

	QFileInfoList getFiles(const QString &dirPath, const QStringList &filterList) const;

	void saveNewCahe() const;
	QJsonArray findNeedDownloadCaheInfo(const QJsonArray &oldCahe, const QJsonArray &newCahe) const;
	void getNeedDownloadFileList();
	void saveCaheJsonNeed() const;
	void findZipSubCaheInfo(QJsonArray &fileInfoArray, const QJsonObject &obj) const;

	void getJsonCaheFiles(const QString &resTemplateFilePath);
	void getJsonCaheFile(const QJsonArray &urls, const QJsonArray &versions, const QJsonArray &copys, const QString &type, const QString &saveDir, const QStringList &prefix,
			     const QStringList &suffix, const QJsonArray &titles, QJsonArray &arrayCahe) const;
	QJsonArray getzipCaheJson(const QJsonArray &copys, const QString &zipName, const QString &saveDir) const;

	void getBasePathValue(QJsonArray &result, const QJsonObject &obj, QStringList basePath, QString name);
	void getBasePathValue(QJsonArray &result, const QJsonArray &arr, QStringList basePath, QString name);
	void getBasePathValue(QJsonArray &result, const QJsonDocument &doc, QStringList basePath);
	QStringList toQStringList(const QJsonArray &arr) const;

	void onDownloadLibrary(PLSResEvents event, const QString &zipName, const QString &zipPath, const QString &url);
	void onDownloadLicense(PLSResEvents event, const QString &zipName, const QString &zipPath, const QString &url) const;

signals:
	void startDownloadResourceSignal();
	void resCheckFinished();
	void bannerJsonDownloaded();
	void libraryNeedUpdate();

private:
	static QVariantHash m_paramData;
	std::atomic<qint64> m_totalCheckNum = 0;
	std::atomic<qint64> m_checkedNum = 0;
	std::atomic<qint64> m_totalNums = 0;
	std::atomic<qint64> m_downloadedNums = 0;
	QMap<QString, QVariantMap> m_localCategoriesData;
	QMap<QString, QVariantMap> m_newCategoriesData;
	QString m_resTemplateDir;
	QString m_resCacheDir;
	QString m_resDir;
	QString m_resTempDir;
	QMap<QString, QJsonArray> m_newDownloadFileList;
	QMap<QString, QJsonArray> m_oldDownloadFileList;
	QMap<QString, QJsonArray> m_needDownloadFileList;
	QDir m_resourcesDir;
	QString m_secretDir;
	QMap<QString, PLSResDownloadStatus> m_resourceDownloadStatus;
	QMutex m_mutex;
};

#endif // PLSRESOURCEMANAGER_H
