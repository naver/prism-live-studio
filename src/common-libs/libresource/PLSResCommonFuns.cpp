#include "PLSResCommonFuns.h"
#include <QStandardPaths>
#include "PLSResourceManager.h"
#include "libhttp-client.h"
#include <qcoreapplication.h>
#include "PLSCommonConst.h"
#include <qcoreapplication.h>
#include "liblog.h"

#if defined(Q_OS_WIN)
#include <Windows.h>
#include "WindowsUnzip/unzip.h"
#endif

QString PLSResCommonFuns::g_gcc = "KR";

void PLSResCommonFuns::downloadResource(const QString &url, const donwloadResultCallback &result, const QString &resPath, bool isRecode, bool isHmac, bool isSync, int timeout)
{
	if (PLSRESOURCEMGR_INSTANCE->resourceDownloadStatus(url) == PLSResDownloadStatus::Downloading) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "this resource is downloading url = %s", url.toUtf8().constData());
		return;
	} else {
		PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(url, PLSResDownloadStatus::Downloading);
	}
	QEventLoop loop;
	pls::http::Request request;
	request.method(pls::http::Method::Get)                                              //
		.hmacUrl(url, isHmac ? pls_http_api_func::getPrismHamcKey() : QByteArray()) //
		.rawHeader("gcc", g_gcc)                                                    //
		.forDownload(true)                                                          //
		.saveFilePath(resPath)                                                      //
		.withLog()                                                                  //
		.timeout(timeout)
		.okResult([&loop, result, url, isRecode, isSync](const pls::http::Reply &) {
			PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(url, PLSResDownloadStatus::DownloadSuccess);

			if (result) {
				result(PLSResEvents::RES_DOWNLOAD_SUCCESS, url);
			}
			if (isRecode) {
				QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
			}
			if (isSync) {

				loop.quit();
			}
		})
		.failResult([&loop, result, url, isRecode, isSync](const pls::http::Reply &) {
			PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(url, PLSResDownloadStatus::DownloadFailed);
			if (result) {
				result(PLSResEvents::RES_DOWNLOAD_FAILED, url);
			}
			if (isRecode) {
				QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
			}
			if (isSync) {
				loop.quit();
			}
		});
	if (url.contains(pls_resource_const::PLS_CATEGORY_URL)) {
		request.rawHeader("X-prism-appversion", QString(""));
	}
	if (isSync) {
		request.receiver(&loop);
	}
	pls::http::request(request);
	if (isSync) {
		loop.exec();
	}
}

void PLSResCommonFuns::downloadResources(const QMap<QString, QString> &urlPaths, const downloadsMutilResultCallback &results, bool isRecode)
{
	if (urlPaths.isEmpty()) {
		return;
	}
	pls::http::Requests requests;
	for (auto urlInter = urlPaths.constBegin(); urlInter != urlPaths.constEnd(); ++urlInter) {
		auto urlStr = urlInter.key();
		if (PLSRESOURCEMGR_INSTANCE->resourceDownloadStatus(urlStr) == PLSResDownloadStatus::Downloading) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "this resource is downloading url = %s", urlStr.toUtf8().constData());
			continue;
		} else {
			PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(urlStr, PLSResDownloadStatus::Downloading);
		}
		requests.add(pls::http::Request()
				     .method(pls::http::Method::Get) //
				     .url(urlStr)                    //
				     .rawHeader("gcc", g_gcc)        //
				     .forDownload(true)              //
				     .saveFilePath(urlInter.value()) //
				     .withLog()                      //
				     .receiver(qApp)
				     .attr("originalUrl", urlStr)
				     .okResult([isRecode](const pls::http::Reply &reply) {
					     PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(reply.request().attr("originalUrl").toString(), PLSResDownloadStatus::DownloadSuccess);
					     if (isRecode) {
						     QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
					     }
				     })
				     .failResult([isRecode](const pls::http::Reply &reply) {
					     PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(reply.request().attr("originalUrl").toString(), PLSResDownloadStatus::DownloadFailed);

					     if (isRecode) {
						     QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
					     }
				     }));
	}
	pls::http::requests(requests.results([results](const pls::http::Replies &replies) {
		QMap<QString, bool> status;
		replies.replies([&status](const pls::http::Reply &reply) { status[reply.request().attr("originalUrl").toString()] = reply.isDownloadOk(); });
		if (results) {
			results(status, std::any_of(status.begin(), status.end(), [](bool ok) { return !ok; }) ? PLSResEvents::RES_DOWNLOAD_FAILED : PLSResEvents::RES_DOWNLOAD_SUCCESS);
		}
	}));
}

void PLSResCommonFuns::downloadResources(const QJsonArray &urlPaths, const downloadMutilCallback &results, bool isRecode, bool)
{
	if (urlPaths.isEmpty()) {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "url is empty, stop donwload");
		return;
	}
	pls::http::Requests requests;

	for (auto downloadInfo : urlPaths) {
		auto obj = downloadInfo.toObject();
		auto type = obj.value(pls_res_const::resourceType).toString();
		QString path = obj.value(pls_res_const::resourcePath).toString();
		auto url = obj.value(pls_res_const::resourceUrl).toString();
		if (type == "zip") {
			auto zipName = url.split('/').last();
			path = PLSResourceManager::instance()->getResourceTmpFilePath(zipName);
		}
		if (PLSRESOURCEMGR_INSTANCE->resourceDownloadStatus(url) == PLSResDownloadStatus::Downloading) {
			PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "this resource is downloading url = %s", url.toUtf8().constData());
			continue;
		} else {
			PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(url, PLSResDownloadStatus::Downloading);
		}
		requests.add(pls::http::Request()
				     .method(pls::http::Method::Get) //
				     .url(url)                       //
				     .rawHeader("gcc", g_gcc)        //
				     .forDownload(true)              //
				     .saveFilePath(path)             //
				     .withLog()                      //
				     .receiver(qApp)
				     .attr("originalUrl", url)
				     .workInGroup(pls::http::RS_DOWNLOAD_WORKER_GROUP)
				     .okResult([isRecode, path, obj](const pls::http::Reply &reply) {
					     PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(reply.request().attr("originalUrl").toString(), PLSResDownloadStatus::DownloadSuccess);

					     if (isRecode) {
						     QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
					     }
					     auto requestUrl = reply.request().attr("originalUrl").toString();
					     auto zipName = requestUrl.split('/').last();
					     if (requestUrl.contains(".zip")) {
						     PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "this is zip file need uzip.");
						     QMetaObject::invokeMethod(PLSResourceManager::instance(), "downloadedZipHandle", Qt::QueuedConnection, Q_ARG(const QString &, path),
									       Q_ARG(const QString &, zipName), Q_ARG(const QJsonArray &, obj.value(pls_res_const::resource_sub_files).toArray()));
					     }
				     })
				     .failResult([isRecode](const pls::http::Reply &reply) {
					     PLSRESOURCEMGR_INSTANCE->updateResourceDownloadStatus(reply.request().attr("originalUrl").toString(), PLSResDownloadStatus::DownloadFailed);

					     if (isRecode) {
						     QMetaObject::invokeMethod(PLSResourceManager::instance(), "appendResourceDownloadedNum", Qt::QueuedConnection, Q_ARG(qint64, 1));
					     }
				     }));
	}
	pls::http::requests(requests.results([urlPaths, results](const pls::http::Replies &replies) {
		QMap<QString, bool> status;
		replies.replies([&status](const pls::http::Reply &reply) { status[reply.request().attr("originalUrl").toString()] = reply.isDownloadOk(); });
		if (results) {
			results(status, !std::any_of(status.begin(), status.end(), [](bool ok) { return !ok; }), urlPaths);
		}
	}));
}

bool PLSResCommonFuns::unZip(const QString &dstDirPath, const QString &srcFilePath, const QString &compressName, bool isRemoveFile, unzipFilesCallback callback)
{
	auto srcPath = srcFilePath;

	if (!QFile::exists(srcPath)) {
		PLS_WARN(pls_resource_const::DATA_ZIP_UZIP, "zip file name = %s not exist", qUtf8Printable(compressName));
		return false;
	}

#if defined(Q_OS_WIN)
	HZIP zip = OpenZip(srcPath.toStdWString().c_str(), nullptr);
	if (zip == nullptr) {
		PLS_WARN(pls_resource_const::DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	ZRESULT result = SetUnzipBaseDir(zip, QString(dstDirPath).toStdWString().c_str());
	if (result != ZR_OK) {
		CloseZip(zip);
		PLS_WARN(pls_resource_const::DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	ZIPENTRY zipEntry;
	result = GetZipItem(zip, -1, &zipEntry);
	if (result != ZR_OK) {
		CloseZip(zip);
		PLS_WARN(pls_resource_const::DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	int numitems = zipEntry.index;
	QStringList lists;
	for (int i = 0; i < numitems; i++) {
		GetZipItem(zip, i, &zipEntry);
		if (!wcsncmp(zipEntry.name, L"__MACOSX", 8))
			continue;
		result = UnzipItem(zip, i, zipEntry.name);
		if (result != ZR_OK) {
			continue;
		}
		lists << dstDirPath + "/" + (QString::fromWCharArray(zipEntry.name));
	}

	if (callback) {
		callback(lists);
	}

	CloseZip(zip);
	PLS_INFO(pls_resource_const::DATA_ZIP_UZIP, "zip file name = %s uzip success", qUtf8Printable(compressName));
#elif defined(Q_OS_MACOS)
	bool result = pls_unZipFile(dstDirPath, srcFilePath);
	if (!result) {
		return false;
	}
#endif
	if (isRemoveFile) {
		QFile::remove(srcPath);
	}

	return true;
}

bool PLSResCommonFuns::moveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove)
{

	QDir sourceDir(srcDir);
	QDir targetDir(destDir);

	if (!targetDir.exists()) {
		bool ok = targetDir.mkpath(destDir);
		if (!ok) {
			PLS_ERROR(pls_resource_const::RESOURCE_DOWNLOAD, "mkdir failed.");
			return false;
		}
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (const auto &fileInfo : fileInfoList) {
		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		if (fileInfo.isDir()) {
			moveDirectoryToDest(fileInfo.filePath(), destDir + fileInfo.fileName().append("/"));
			continue;
		}

		if (targetDir.exists(fileInfo.fileName())) {
			targetDir.remove(fileInfo.fileName());
		}

		if (!QFile::copy(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()))) {
			continue;
		}
	}
	if (isRemove) {
		sourceDir.removeRecursively();
	}
	return true;
}
void PLSResCommonFuns::findAllFiles(const QString &absolutePath, QFileInfoList &fileList)
{
	QDir dir(absolutePath);
	foreach(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs))
	{
		findAllFiles(info.filePath(), fileList);
	}
	foreach(QFileInfo info, dir.entryInfoList(QDir::Files))
	{
		fileList.append(info);
	}
}

bool PLSResCommonFuns::copyFile(const QString &srcFilePath, const QString &dstFilePath)
{
	if (QFileInfo info(dstFilePath); info.exists()) {
		bool isSuccess = QFile::remove(dstFilePath);
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "remove file dstFilePath is %s", isSuccess ? "Success" : "Failed");
	} else {
		PLSResourceManager::instance()->makePath(info.absolutePath());
	}
	QFile file(srcFilePath);
	bool isSuccess = file.copy(dstFilePath);
	PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "copy file  is %s", isSuccess ? "Success" : file.errorString().toUtf8().constData());
	PLS_INFO_KR(pls_resource_const::RESOURCE_DOWNLOAD, "copy file  is %s,srcFile =%s,dstFile = %s", isSuccess ? "Success" : file.errorString().toUtf8().constData(),
		    srcFilePath.toUtf8().constData(), dstFilePath.toUtf8().constData());
	file.close();
	return isSuccess;
}

QString PLSResCommonFuns::getAppLocationPath()
{
	auto PrismAppName = "PRISMLiveStudio";
	if (auto locations = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::AppDataLocation); !locations.isEmpty()) {
		auto appPath = locations.first();
		appPath = appPath.replace(QCoreApplication::applicationName(), PrismAppName);

		if (QDir dir(appPath); !dir.exists()) {
			dir.mkpath(appPath);
		}
		return appPath;
	}
	return QString();
}

QString PLSResCommonFuns::getUserSubPath(const QString &subpath)
{
	return PLSResCommonFuns::getAppLocationPath() + "/" + subpath;
}

bool PLSResCommonFuns::checkPath(const QString &dirName, bool createIfNotExist)
{
	auto resDir = getUserSubPath(dirName);
	QDir dir(resDir);
	bool isExist = dir.exists();
	if (!isExist && createIfNotExist) {
		dir.mkpath(resDir);
	}

	return isExist;
}

void PLSResCommonFuns::setGcc(const QString &gcc)
{
	g_gcc = gcc;
}
QByteArray PLSResCommonFuns::readFile(const QString &absolutePath)
{
	QByteArray array;
	QFile file(absolutePath);
	if (file.open(QIODevice::ReadOnly)) {
		array = file.readAll();
	} else {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "readFile  failed, error =%s", file.errorString().toUtf8().constData());
	}
	file.close();
	return array;
}

void PLSResCommonFuns::saveFile(const QString &absolutePath, const QByteArray &data)
{
	QFile file(absolutePath);
	if (!file.open(QFile::WriteOnly)) {
		return;
	} else {
		PLS_INFO(pls_resource_const::RESOURCE_DOWNLOAD, "saveFile failed, error =%s", file.errorString().toUtf8().constData());
	}
	file.write(data);
	file.close();
}

bool PLSResCommonFuns::copyDirectory(const QString &srcPath, const QString &dstPath, bool coverFileIfExist)
{
	QDir srcDir(srcPath);
	QDir dstDir(dstPath);
	if (!dstDir.exists()) {
		if (!dstDir.mkpath(dstDir.absolutePath()))
			return false;
	}

	QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);
	for (QFileInfo fileInfo : fileInfoList) {
		if (fileInfo.isDir()) {
			if (!copyDirectory(fileInfo.filePath(), dstDir.filePath(fileInfo.fileName()), coverFileIfExist)) {
				return false;
			}
			continue;
		}
		if (dstDir.exists(fileInfo.fileName())) {
			if (!coverFileIfExist) {
				continue;
			}
			dstDir.remove(fileInfo.fileName());
		}
		if (!QFile::copy(fileInfo.filePath(), dstDir.filePath(fileInfo.fileName()))) {
			return false;
		}
	}
	return true;
}
