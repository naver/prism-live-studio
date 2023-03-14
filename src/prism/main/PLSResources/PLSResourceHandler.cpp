#include <Windows.h>
#include <stdio.h>
#include "unzip.h"
#include "pls-app.hpp"

#include "PLSResourceHandler.h"

#include <QDir>
#include <QFile>

PLSResourceHandler::PLSResourceHandler(const QStringList &resUrls, QObject *parent, bool isUseSubThread)
	: QObject(parent), m_resUrls(resUrls), m_manager(new QNetworkAccessManager(this)), m_isUseSubThread(isUseSubThread)
{
	connect(this, &PLSResourceHandler::threadHandlerSignal, this, &PLSResourceHandler::threadHandler);
	if (m_isUseSubThread) {
		this->moveToThread(&m_workThread);
		m_workThread.start();
	}
	connect(
		m_manager, &QObject::destroyed, this,
		[=]() {
			m_workThread.quit();
			m_workThread.wait();
		},
		Qt::QueuedConnection);
}

PLSResourceHandler::~PLSResourceHandler() {}

void PLSResourceHandler::startDownLoadRes(DownLoadResourceMode downMode, const QMap<QString, QString> &resourceUrlsPath)
{
	for (auto key : resourceUrlsPath.keys()) {
		m_resourceUrlToPaths.insert(key, resourceUrlsPath.value(key));
	}
	m_resourceDownStatus.clear();
	m_downMode = downMode;

	if (DownLoadResourceMode::Part == downMode) {
		m_resUrls = resourceUrlsPath.keys();
	}
	emit threadHandlerSignal(m_resUrls);
}

void PLSResourceHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All: {
		QString fileName;
		if (!m_resFileName.isEmpty()) {
			fileName = m_resFileName;
		} else {
			fileName = QString("%1.json").arg(m_resItemName.toLower());
		}
		if (!PLSJsonDataHandler::saveJsonFile(data, m_relativeResPath + fileName)) {
			PLS_ERROR(MAINFILTER_MODULE, "save to json %s failed", fileName.toUtf8().constData());
		}
	}

	break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSResourceHandler::setItemName(const QString &itemName)
{
	m_resItemName = itemName;
}

void PLSResourceHandler::setRelativeResPath(const QString &relativePath)
{

	m_relativeResPath = relativePath;

	QDir _dir(m_relativeResPath);
	if (!_dir.exists()) {
		bool ok = _dir.mkpath(m_relativeResPath);
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir failed.");
			return;
		}
	}
}

QString PLSResourceHandler::getRelativeResPath()
{
	return m_relativeResPath;
}
void PLSResourceHandler::saveResource(const QByteArray &data, const QString &filePath)
{
	QFile file(filePath);
	if (!file.open(QFile::WriteOnly)) {
		PLS_ERROR(MAINFILTER_MODULE, "save image failed,  reason: %s", file.errorString().toUtf8().constData());
		return;
	}
	file.write(data);
	file.close();
}
void PLSResourceHandler::setResUrls(const QStringList &urls)
{
	m_resUrls = urls;
}
void PLSResourceHandler::setResFileName(const QString &fileName)
{
	m_resFileName = fileName;
}
bool PLSResourceHandler::saveFile(const QString &path, const QString &fileName, const QByteArray &data)
{
	auto srcPath = QString("%1%2").arg(path).arg(fileName);
	QFile f(srcPath);
	if (!f.open(QIODevice::WriteOnly))
		return false;
	f.write(data);
	f.close();
	return true;
}
bool PLSResourceHandler::unCompress(const QString &path, const QString &compressName, bool isRemoveFile)
{

	auto srcPath = QString("%1%2").arg(path).arg(compressName);

	if (!QFile::exists(srcPath)) {
		PLS_WARN(DATA_ZIP_UZIP, "zip file name = %s not exist", qUtf8Printable(compressName));
		return false;
	}

	HZIP zip = OpenZip(srcPath.toStdWString().c_str(), 0);
	if (zip == 0) {
		PLS_WARN(DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	ZRESULT result = SetUnzipBaseDir(zip, QString(path).toStdWString().c_str());
	if (result != ZR_OK) {
		CloseZip(zip);
		PLS_WARN(DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	ZIPENTRY zipEntry;
	result = GetZipItem(zip, -1, &zipEntry);
	if (result != ZR_OK) {
		CloseZip(zip);
		PLS_WARN(DATA_ZIP_UZIP, "zip file name = %s uzip error", qUtf8Printable(compressName));
		return false;
	}

	int numitems = zipEntry.index;
	for (int i = 0; i < numitems; i++) {
		result = GetZipItem(zip, i, &zipEntry);
		result = UnzipItem(zip, i, zipEntry.name);
		if (result != ZR_OK) {
			continue;
		}
	}
	CloseZip(zip);
	PLS_INFO(DATA_ZIP_UZIP, "zip file name = %s uzip success", qUtf8Printable(compressName));

	if (isRemoveFile) {
		QFile::remove(srcPath);
	}

	return true;
}

void PLSResourceHandler::removeAllJsonFile(const QString &path)
{
	QDir sourceDir(path);

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (auto &fileInfo : fileInfoList) {
		if (fileInfo.fileName() == "." || fileInfo.fileName() == ".." || fileInfo.isDir())
			continue;

		if (fileInfo.fileName().endsWith(".json")) {
			sourceDir.remove(fileInfo.fileName());
		}
	}
}
bool PLSResourceHandler::moveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove)
{
	QDir sourceDir(srcDir);
	QDir targetDir(destDir);

	if (!targetDir.exists()) {
		bool ok = targetDir.mkpath(destDir);
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir failed.");
			return false;
		}
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (auto &fileInfo : fileInfoList) {
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

QStringList PLSResourceHandler::getLanguageKey()
{
	QStringList _langList;
	for (const auto &locale : GetLocaleNames()) {
		QString langId(locale.first.c_str());
		_langList.append(langId.split('-')[0]);
	}
	return _langList;
}

void PLSResourceHandler::abort()
{
	for (auto reply : m_replys) {
		if (reply) {
			reply->abort();
		}
	}
	m_eventLoop.quit();
	m_manager->deleteLater();
}

void PLSResourceHandler::appendReply(QNetworkReply *reply)
{
	if (reply) {
		m_replys.append(reply);
	}
}

void PLSResourceHandler::downPartResourceCallback()
{
	if (m_resourceDownStatus.size() == m_resUrls.size()) {
		downPartResourceSignal(m_resourceDownStatus);
	}
}

void PLSResourceHandler::threadHandler(const QStringList &urls)
{
	auto _onSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void *context) {
		auto downMode = static_cast<PLSResourceHandler *>(context)->m_downMode;
		auto urlStr = networkReplay->url().toString();
		if (DownLoadResourceMode::Part == downMode) {
			m_resourceDownStatus.insert(urlStr, true);
			downPartResourceCallback();
		}
		doWorks(data, downMode, m_resourceUrlToPaths.value(urlStr));
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void *context) {
		auto downMode = static_cast<PLSResourceHandler *>(context)->m_downMode;
		if (DownLoadResourceMode::Part == downMode) {
			m_resourceDownStatus.insert(networkReplay->url().toString(), false);
			downPartResourceCallback();
		}
		PLS_ERROR(MAINFILTER_MODULE, "Get %s resource failed: url = %s,data = %s", m_resItemName.toUtf8().constData(), networkReplay->url().toString().toUtf8().data(), data.data());
		if (!m_isUseSubThread) {
			m_eventLoop.quit();
		}
	};
	for (auto url : urls) {
		PLSNetworkReplyBuilder builder(url);
		auto reply = builder.get(m_manager);
		PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, this, PRISM_NET_DOWNLOAD_TIMEOUT);
		appendReply(reply);
	}
	if (!m_isUseSubThread && !urls.isEmpty()) {
		m_eventLoop.exec();
	}
}

void PLSResourceHandler::reDownloadResources(const ResFileInfo &resourceInfo, fileHandlerCallback callback)
{
	qDebug() << "resourceInfo.fileName = " << resourceInfo.fileName << "resourceInfo.resUrl = " << resourceInfo.resUrl << "resourceInfo.savePath = " << resourceInfo.savePath;
	PLS_INFO(MAINFILTER_MODULE, "redownloadResouece file=%s,url=%s", resourceInfo.fileName.toUtf8().data(), resourceInfo.resUrl.toUtf8().data());

	auto fileCallback = [=](bool ok, const QString & /*imagePath*/, void * /*context*/) {
		if (ok) {
			if (callback) {
				callback(resourceInfo);
			}
		} else {
			//continue download message
			if (m_reDownloadCount--) {
				emit requestReDownload(resourceInfo, callback);
			} else {
				PLS_WARN(MAINFILTER_MODULE, "resourece file download 3th failed , url = %s", resourceInfo.resUrl.toUtf8().constData());
			}
		}
	};

	PLSNetworkReplyBuilder builder(resourceInfo.resUrl);
	auto reply = builder.get(m_manager);
	PLSHttpHelper::downloadImageAsync(reply, this, resourceInfo.savePath, fileCallback, "", resourceInfo.fileName, {}, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT, true, false);
	appendReply(reply);
}
