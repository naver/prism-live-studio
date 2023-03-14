#include <Windows.h>
#include "PLSHttpHelper.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkConfiguration>
#include <QNetworkInterface>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QFile>
#include <QMutex>
#include <QDir>
#include <QUuid>
#include <atomic>
#include <util/windows/win-version.h>

#include "log.h"
#include "pls-gpop-data.hpp"
#include "ui-config.h"

using namespace std;

namespace {
template<typename Result> class SyncResult {
	bool finished = false;
	Result result;
	mutable QMutex mutex;
	QEventLoop eventLoop;

public:
	static SyncResult *create() { return new SyncResult<Result>(); }

	void exec() { eventLoop.exec(); }

	void init(const Result &result) { this->result = result; }

	template<typename Item> void initList(const Item &item, int count)
	{
		for (int i = 0; i < count; ++i) {
			this->result.append(item);
		}
	}

	Result get() const
	{
		QMutexLocker locker(&mutex);
		return result;
	}

	bool isFinished() const
	{
		QMutexLocker locker(&mutex);
		return finished;
	}

	void finish(const Result &result)
	{
		QMutexLocker locker(&mutex);
		this->finished = true;
		this->result = result;
		this->eventLoop.quit();
	}

	void destroy()
	{
		if (isFinished()) {
			delete this;
		} else {
			PLS_WARN(MODULE_PLSHttpHelper, "SyncResult not deleted.");
		}
	}
};

QString getSuffix(const QString &fileName)
{
	int index = fileName.lastIndexOf(".");
	if (index >= 0) {
		return fileName.mid(index);
	}
	return fileName;
}
void removeSuffix(QString &fileName)
{
	int index = fileName.lastIndexOf(".");
	if (index >= 0) {
		fileName = fileName.left(index);
	}
}
bool saveImage(QString &filePath, QNetworkReply *reply, const QString &saveDir, const QString &saveFileNamePrefix, const QString &saveFileName, const QByteArray &data, bool isModifySuffix)
{
	QString fileName = saveFileName;
	if (fileName.isEmpty()) {
		fileName = reply->request().url().fileName();
	}

	if (fileName.isEmpty()) {
		fileName = QUuid::createUuid().toString();
	}

	if (saveFileName.isEmpty() || isModifySuffix) {
		fileName = PLSHttpHelper::imageContentTypeToFileName(reply->header(QNetworkRequest::ContentTypeHeader).toString(), fileName);
	}

	filePath = saveDir + "/" + saveFileNamePrefix + fileName;
	QDir dir(saveDir);
	if (!dir.exists()) {
		dir.mkpath(saveDir);
	}
	if (QFile::exists(filePath)) {
		QFile::remove(filePath);
	}

	QFile file(filePath);
	if (!file.open(QFile::WriteOnly)) {
		PLS_ERROR(MODULE_PLSHttpHelper, "save image failed,reason: %s", file.errorString().toUtf8().constData());
		return false;
	}

	file.write(data);
	return true;
}
}

PLSHttpHelper *PLSHttpHelper::instance()
{
	static PLSHttpHelper _instance;

	return &_instance;
}

void PLSHttpHelper::connectFinished(QNetworkReply *networkReply, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, dataErrorFunction onFinished, void *const context,
				    int iTimeout, bool bPrintLog)
{
	bool isMasking = networkReply->property("urlMasking").isValid();
	auto url = isMasking ? networkReply->property("urlMasking").toString() : networkReply->url().toString();

	auto _onFinished = [=] {
		//assert(!networkReply->url().toString().isEmpty());
		const char *KEY_PROCESSED = "processed";
		if (networkReply->property(KEY_PROCESSED).toBool()) {
			return;
		}
		networkReply->setProperty(KEY_PROCESSED, true);

		if (nullptr == onSucceed && nullptr == onFailed && nullptr == onFinished) {
			networkReply->deleteLater();
			return;
		}

		auto statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		auto data = networkReply->readAll();
		auto error = networkReply->error();

		if (QNetworkReply::NoError == networkReply->error()) {
			if (bPrintLog) {
				auto contentTypeStr = networkReply->header(QNetworkRequest::ContentTypeHeader).toString();

				PLS_INFO(MODULE_PLSHttpHelper, "http response success, url = %s, ", url.toUtf8().constData());

				PLS_INFO_KR(MODULE_PLSHttpHelper, "http response success, url = %s, response data = %s", networkReply->url().toString().toUtf8().constData(),
					    contentTypeStr.contains("image") ? "is image res." : data.constData());
			}
			if (nullptr != onSucceed) {
				onSucceed(networkReply, statusCode, data, context);
			}
		} else {
			if (bPrintLog) {
				PLS_ERROR(MODULE_PLSHttpHelper, "http response error! url = %s networkReplyError = %d statusCode = %d.", url.toUtf8().constData(), networkReply->error(), statusCode);

				PLS_ERROR_KR(MODULE_PLSHttpHelper, "http response error! url = %s networkReplyError = %d statusCode = %d  errorMessage = %s.",
					     networkReply->url().toString().toUtf8().constData(), networkReply->error(), statusCode, data.constData());
			}
			if (onFailed != nullptr) {
				onFailed(networkReply, statusCode, data, error, context);
			}
		}

		if (onFinished != nullptr) {
			onFinished(networkReply, statusCode, data, error, context);
		}

		networkReply->deleteLater();
	};

	if (bPrintLog) {
		const char *pOperator = "Custom";
		switch (networkReply->operation()) {
		case QNetworkAccessManager::HeadOperation:
			pOperator = "Head";
			break;
		case QNetworkAccessManager::GetOperation:
			pOperator = "Get";
			break;
		case QNetworkAccessManager::PutOperation:
			pOperator = "Put";
			break;
		case QNetworkAccessManager::PostOperation:
			pOperator = "Post";
			break;
		case QNetworkAccessManager::DeleteOperation:
			pOperator = "Delete";
			break;
		}

		PLS_INFO(MODULE_PLSHttpHelper, "http request start:%s, url = %s.", pOperator, url.toUtf8().constData());
		if (isMasking) {
			PLS_INFO_KR(MODULE_PLSHttpHelper, "http request start:%s, url = %s.", pOperator, networkReply->url().toString().toUtf8().constData());
		}
	}

	QObject::connect(networkReply, &QNetworkReply::finished, nullptr != receiver ? receiver : networkReply, _onFinished);

	if (iTimeout > 0) {
		QTimer::singleShot(iTimeout, networkReply, &QNetworkReply::abort);
	}

	if (receiver) {
		QObject::connect(receiver, &QObject::destroyed, networkReply, &QNetworkReply::abort, Qt::QueuedConnection);
	}
}

void PLSHttpHelper::downloadImageAsync(QNetworkReply *reply, const QObject *receiver, const QString &saveDir, ImageCallback callback, const QString &saveFileNamePrefix, const QString &saveFileName,
				       const QString &defaultImagePath, void *context, int iTimeout, bool bPrintLog, bool isModifySuffix)
{
	connectFinished(
		reply, receiver,
		[=](QNetworkReply *reply, int, const QByteArray &data, void *) {
			QString filePath;
			if (saveImage(filePath, reply, saveDir, saveFileNamePrefix, saveFileName, data, isModifySuffix)) {
				callback(true, filePath, context);
			} else {
				callback(false, defaultImagePath, context);
			}
		},
		[=](QNetworkReply *, int, const QByteArray &, QNetworkReply::NetworkError, void *) { callback(false, defaultImagePath, context); }, nullptr, context, iTimeout, bPrintLog);
}
QPair<bool, QString> PLSHttpHelper::downloadImageSync(QNetworkReply *networkReply, const QObject *receiver, const QString &saveDir, const QString &saveFileNamePrefix, const QString &saveFileName,
						      const QString &defaultImagePath, int iTimeout, bool bPrintLog, bool isModifySuffix)
{
	auto asyncResult = SyncResult<QPair<bool, QString>>::create();
	downloadImageAsync(
		networkReply, receiver, saveDir, [=](bool ok, const QString &imagePath, void *) { asyncResult->finish(QPair<bool, QString>(ok, imagePath)); }, saveFileNamePrefix, saveFileName,
		defaultImagePath, nullptr, iTimeout, bPrintLog, isModifySuffix);

	asyncResult->exec();
	auto result = asyncResult->get();
	asyncResult->destroy();
	return result;
}
void PLSHttpHelper::downloadImagesAsync(QList<QNetworkReply *> &replyList, const QObject *receiver, const QString &saveDir, ImagesCallback callback, const QString &saveFileNamePrefix,
					const QList<QString> &saveFileNameList, const QString &defaultImagePath, void *context, int iTimeout, bool bPrintLog, bool isModifySuffix)
{
	if (replyList.isEmpty()) {
		callback(QList<QPair<bool, QString>>(), context);
		return;
	}

	struct Waiting {
		std::atomic<int> count;
		QList<QPair<bool, QString>> result;
	};

	Waiting *waiting = new Waiting();

	waiting->count = replyList.size();
	for (int i = 0; i < waiting->count; ++i) {
		waiting->result.append(QPair<bool, QString>());
	}

	auto _callback = [=](bool ok, const QString &imagePath, void *context) {
		waiting->result[(int)(intptr_t)context] = QPair<bool, QString>(ok, imagePath);
		if (!(waiting->count -= 1)) {
			callback(waiting->result, context);
			delete waiting;
		}
	};

	for (int i = 0; i < replyList.size(); ++i) {
		if (i < saveFileNameList.size()) {
			downloadImageAsync(replyList.at(i), receiver, saveDir, _callback, saveFileNamePrefix, saveFileNameList.at(i), defaultImagePath, (void *)(intptr_t)i, iTimeout, bPrintLog,
					   isModifySuffix);
		} else {
			downloadImageAsync(replyList.at(i), receiver, saveDir, _callback, saveFileNamePrefix, QString(), defaultImagePath, (void *)(intptr_t)i, iTimeout, bPrintLog, isModifySuffix);
		}
	}
}
QList<QPair<bool, QString>> PLSHttpHelper::downloadImagesSync(QList<QNetworkReply *> &replyList, const QObject *receiver, const QString &saveDir, const QString &saveFileNamePrefix,
							      const QList<QString> &saveFileNameList, const QString &defaultImagePath, int iTimeout, bool bPrintLog, bool isModifySuffix)
{
	auto asyncResult = SyncResult<QList<QPair<bool, QString>>>::create();
	asyncResult->initList(QPair<bool, QString>(), replyList.size());
	downloadImagesAsync(
		replyList, receiver, saveDir, [=](const QList<QPair<bool, QString>> &imageList, void *) { asyncResult->finish(imageList); }, saveFileNamePrefix, saveFileNameList, defaultImagePath,
		nullptr, iTimeout, bPrintLog, isModifySuffix);

	asyncResult->exec();
	auto result = asyncResult->get();
	asyncResult->destroy();
	return result;
}

QString PLSHttpHelper::getLocalIPAddr()
{
	for (auto &addr : QNetworkInterface::allAddresses()) {
		if (!addr.isLoopback() && (addr.protocol() == QAbstractSocket::IPv4Protocol)) {
			return addr.toString();
		}
	}

	return QString();
}

QString PLSHttpHelper::getUserAgent()
{
	win_version_info wvi = {0};
	get_win_ver(&wvi);

	LANGID langId = GetUserDefaultUILanguage();
#ifdef Q_OS_WIN64
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x64 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#else
	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x86 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#endif
}

QString PLSHttpHelper::getFileName(const QString &filePath)
{
	int index = filePath.lastIndexOf(QRegExp("[/\\]"));
	if (index >= 0) {
		return filePath.mid(index + 1);
	}
	return filePath;
}

QString PLSHttpHelper::imageContentTypeToFileName(const QString &contentType, const QString &fileName)
{
	QString newFileName = fileName;
	if (contentType.isEmpty()) {
		return newFileName;
	} else if (contentType == "image/png") {
		removeSuffix(newFileName);
		newFileName += ".png";
	} else if (contentType == "image/gif") {
		removeSuffix(newFileName);
		newFileName += ".gif";
	} else if (contentType == "image/jpeg") {
		removeSuffix(newFileName);
		newFileName += ".jpeg";
	} else if (contentType == "image/bmp") {
		removeSuffix(newFileName);
		newFileName += ".bmp";
	} else if (contentType == "image/webp") {
		removeSuffix(newFileName);
		newFileName += ".webp";
	}
	return newFileName;
}
QString PLSHttpHelper::imageFileNameToContentType(const QString &fileName)
{
	QString suffix = getSuffix(fileName);
	if (suffix == ".png") {
		return "image/png";
	} else if (suffix == ".gif") {
		return "image/gif";
	} else if (suffix == ".jpg" || suffix == ".jpeg") {
		return "image/jpeg";
	} else if (suffix == ".bmp") {
		return "image/bmp";
	} else if (suffix == ".webp") {
		return "image/webp";
	}
	return QString();
}
