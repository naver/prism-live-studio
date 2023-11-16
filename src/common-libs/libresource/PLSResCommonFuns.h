#ifndef PLSRESCOMMONFUNS_H
#define PLSRESCOMMONFUNS_H

#include "PLSResourceHandle_global.h"
#include "PLSResEnums.h"
#include <QString>
#include <QVariantMap>
#include <QFileInfoList>

class QObject;
class PLSResourceMgr;
using donwloadResultCallback = std::function<void(PLSResEvents event, const QString &url)>;

using downloadsMutilResultCallback = std::function<void(const QMap<QString, bool> &resDownloadStatus, PLSResEvents event)>;

using downloadMutilCallback = std::function<void(const QMap<QString, bool> &resDownloadStatus, bool isSuccess, const QJsonArray &resSaveInfo)>;

using unzipFilesCallback = std::function<void(const QStringList &lists)>;

struct PLSRESOURCEHANDLE_EXPORT PLSResCommonFuns {

	static void downloadResource(const QString &url, const donwloadResultCallback &result, const QString &resPath = QString(), bool isRecode = true, bool isHmac = false, bool isSync = false,
				     int timeout = 15000);
	static void downloadResources(const QMap<QString, QString> &urlPaths, const downloadsMutilResultCallback &results, bool isRecode = true);
	static void downloadResources(const QJsonArray &urlPaths, const downloadMutilCallback &results, bool isRecode = true, bool isNeedSubThread = false);

	static bool unZip(const QString &dstDirPath, const QString &srcFilePath, const QString &compressName, bool isRemoveFile = true, unzipFilesCallback callback = nullptr);
	static bool moveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove = false);
	static void findAllFiles(const QString &absolutePath, QFileInfoList &fileList);
	static bool copyFile(const QString &srcFilePath, const QString &dstFilePath);
	static QString getAppLocationPath();
	static QString getUserSubPath(const QString &subpath);
	static bool checkPath(const QString &dirName, bool createIfNotExist = true);
	static void setGcc(const QString &gcc);
	static QByteArray readFile(const QString &absolutePath);
	static void saveFile(const QString &absolutePath, const QByteArray &data);
	static bool copyDirectory(const QString &srcPath, const QString &dstPath, bool coverFileIfExist);
	static QString g_gcc;
};

#endif // PLSRESCOMMONFUNS_H
