#include "PLSFileDownloader.h"
#include <pls-common-define.hpp>
#include <frontend-api\frontend-api.h>
#include <QPointer>
#include "log/module_names.h"
#include "GiphyWebHandler.h"
#include "liblog.h"
#include "platform.hpp"

PLSFileDownloader::PLSFileDownloader(QObject *parent) : QObject(parent)
{
	this->moveToThread(&threadDownload);
}

PLSFileDownloader::~PLSFileDownloader()
{
	stopped = true;
	threadDownload.quit();
	threadDownload.wait();

	if (nullptr != manager) {
		manager->deleteLater();
		manager = nullptr;
	}
	ClearTask();
}

void PLSFileDownloader::Get(const DownloadTaskData &taskData, bool forceDownload)
{
	if (!running)
		Start();
	QMetaObject::invokeMethod(this, "excuteTask", Qt::QueuedConnection, Q_ARG(const DownloadTaskData &, taskData), Q_ARG(bool, forceDownload));
}

void PLSFileDownloader::Start()
{
	if (running)
		return;
	running = true;
	stopped = false;
	threadDownload.start();
}

void PLSFileDownloader::Stop()
{
	StopInner();
}

bool PLSFileDownloader::IsRunning()
{
	return running;
}

void PLSFileDownloader::Retry(retryCallback_t callback)
{
	mutex.lock();
	auto tasks = tasksRetry;
	mutex.unlock();
	if (tasks.isEmpty())
		return;
	qDebug("retry task size=%d,retry download...", tasks.size());
	int sizeTask = tasks.size();
	for (int i = 0; i < sizeTask; ++i) {
		DownloadTaskData task = tasks.dequeue();
		if (callback && !callback(task))
			continue;
		Get(task, false);
	}
}

bool PLSFileDownloader::IsDownloadFileExsit(const DownloadTaskData &taskData, QString &outputFileName)
{
	QUrl url = QUrl::fromEncoded(taskData.url.toLocal8Bit());
	QString fileName = taskData.outputFileName;

	QFileInfo info(url.path());
	if (fileName.isEmpty()) {
		fileName = info.fileName();
	} else {
		fileName.append(".").append(info.suffix());
	}
	outputFileName = taskData.outputPath + fileName;
	return QFile::exists(outputFileName);
}

void PLSFileDownloader::SetCheckVersionCallback(CheckFileVersionCallback callback, void *param)
{
	QMutexLocker locker(&mutex);
	checkversionCallback = callback;
	callbackParam = param;
}

void PLSFileDownloader::AbortTask(const DownloadTaskData &taskData)
{
	QMetaObject::invokeMethod(this, "abortTaskInner", Qt::QueuedConnection, Q_ARG(const DownloadTaskData &, taskData));
}

void PLSFileDownloader::DownloadSuccess(TaskResponData responData, QNetworkReply *reply)
{
	if (!reply)
		return;
	auto url = reply->url();
	QString fileName = responData.taskData.outputFileName;
	QFileInfo info(url.path());
	if (fileName.isEmpty()) {
		fileName = info.fileName();
	} else {
		fileName.append(".").append(info.suffix());
	}
	QByteArray rawData = reply->readAll();

	QVariant value = reply->header(QNetworkRequest::ContentLengthHeader);
	std::string name = GetFileName(fileName.toUtf8().constData());
	if (value.isValid()) {
		if (value.toULongLong() != (qulonglong)rawData.size()) {
			PLS_WARN(MAIN_FILE_DOWNLOADER, "[FILE-DOWNLOAD]%s download finished. Contentlength: %llu, received size: %d", name.c_str(), value.toULongLong(), rawData.size());
		}
	}

	if (responData.taskData.rawDataCallback) {
		responData.taskData.rawDataCallback(rawData, responData);
		emit downloadResult(responData);
		return;
	}

	if (saveToDisk(fileName, responData.taskData.outputPath, rawData, responData.taskData.version)) {
		responData.fileName = fileName;
		if (responData.taskData.callback) {
			responData.taskData.callback(responData);
		}
		emit downloadResult(responData);

	} else {
		PLS_ERROR(MAIN_FILE_DOWNLOADER, "Save file to disk failed.File name=%s", name.c_str());
		responData.errorString = QString("Save file to disk failed");
		responData.resultType = ResultStatus::ERROR_OCCUR;
		if (responData.taskData.callback) {
			responData.taskData.callback(responData);
		}
		emit downloadResult(responData);
	}
}

void PLSFileDownloader::excuteTask(const DownloadTaskData &taskData, bool forceDownload)
{
	if (!manager) {
		manager = new QNetworkAccessManager;
		connect(manager, &QNetworkAccessManager::finished, this, &PLSFileDownloader::downloadFinished);
		connect(manager, &QNetworkAccessManager::networkAccessibleChanged, this, &PLSFileDownloader::OnNetworkAccessChanged);
	}
	QUrl url = QUrl::fromEncoded(taskData.url.toLocal8Bit());
	QString userFileName;
	if (!forceDownload && IsDownloadFileExsit(taskData, userFileName)) {
		QMutexLocker locer(&mutex);
		if (!checkversionCallback || checkversionCallback && (*checkversionCallback)(taskData, userFileName, callbackParam)) {
			TaskResponData responData;
			responData.taskData = taskData;
			responData.fileName = userFileName;
			if (taskData.rawDataCallback) {
				QFile file(userFileName);
				if (file.open(QIODevice::ReadOnly)) {
					QByteArray data = file.readAll();
					taskData.rawDataCallback(data, responData);
					file.close();
				} else {
					PLS_WARN("Open file: %s failed, redownload it.", qPrintable(taskData.outputFileName));
					goto DownloadFlow;
				}
			}
			if (taskData.callback) {
				taskData.callback(responData);
			}
			emit downloadResult(responData);
			return;
		}
	}

DownloadFlow:
	if (QNetworkAccessManager::Accessible != manager->networkAccessible()) {
		if (taskData.needRetry) {
			QMutexLocker locer(&mutex);
			qDebug("Network is not accessible,add task to retry list, current retry task size is:%d", tasksRetry.size());
			tasksRetry.enqueue(taskData);
		}
		QString errorString("Network is not accessible");
		TaskResponData responData;
		responData.taskData = taskData;
		responData.errorString = errorString;
		responData.resultType = ResultStatus::ERROR_OCCUR;
		if (taskData.rawDataCallback) {
			taskData.rawDataCallback(QByteArray(), responData);
		}
		if (taskData.callback) {
			taskData.callback(responData);
		}
		emit downloadResult(responData);
		return;
	}

	QNetworkRequest request(url);
	QTimer *timer = nullptr;
	if (taskData.timeoutMs > 0) {
		timer = new QTimer;
		timer->setSingleShot(true);
	}

	QNetworkReply *reply = manager->get(request);
	if (timer) {
		QPointer<QNetworkReply> reply_guarded(reply);
		QPointer<QTimer> time_guarded(timer);
		connect(timer, &QTimer::timeout, this, [this, taskData = taskData, reply_guarded, time_guarded]() {
			if (stopped)
				return;
			if (reply_guarded && time_guarded) {
				if (reply_guarded->isRunning()) {
					QSignalBlocker blocker(reply_guarded);
					reply_guarded->abort();
					taskDownloads.remove(reply_guarded);
					timerMap.remove(reply_guarded);
					reply_guarded->deleteLater();
					time_guarded->deleteLater();
				}
			}

			TaskResponData responData;
			responData.taskData = taskData;
			responData.resultType = ResultStatus::ERROR_OCCUR;
			responData.subType = ErrorSubType::Error_Timeout;
			if (responData.taskData.callback) {
				responData.taskData.callback(responData);
			}
			emit downloadResult(responData);
		});
		timer->start(taskData.timeoutMs);
	}
	connect(reply, &QNetworkReply::downloadProgress, [=](qint64 bytesReceived, qint64 bytesTotal) {
		qDebug("downloading file: %s, %d%%", qUtf8Printable(url.fileName()), (bytesTotal > 0) ? (bytesReceived / bytesTotal) * 100 : 0);
		TaskResponData responData;
		responData.taskData = taskData;
		responData.fileName = userFileName;
		emit downloadProgress(responData, bytesReceived, bytesTotal);
	});

	taskDownloads.insert(reply, taskData);
	if (timer)
		timerMap.insert(reply, timer);
}

void PLSFileDownloader::sslErrors(QList<QSslError>)
{
	qDebug("ssl error occured");
}

void PLSFileDownloader::downloadFinished(QNetworkReply *reply)
{
	if (!reply)
		return;
	auto timer = timerMap.value(reply, nullptr);
	if (timer) {
		timer->stop();
		timer->deleteLater();
	}

	if (stopped) {
		taskDownloads.remove(reply);
		timerMap.remove(reply);
		reply->deleteLater();
		return;
	}
	QUrl url = reply->url();
	TaskResponData responData;
	responData.taskData = taskDownloads[reply];
	if (QNetworkReply::NoError != reply->error()) {
		if (responData.taskData.needRetry) {
			QMutexLocker locer(&mutex);
			tasksRetry.enqueue(responData.taskData);
		}
		PLS_ERROR(MAIN_FILE_DOWNLOADER, "Download of %s failed: %s\n", url.toEncoded().constData(), qPrintable(reply->errorString()));
		QString errorString;
		errorString.sprintf("Download of %s failed: %s", url.toEncoded().constData(), reply->errorString().constData());
		responData.errorString = errorString;
		responData.resultType = ResultStatus::ERROR_OCCUR;
		if (responData.taskData.callback) {
			responData.taskData.callback(responData);
		}
		emit downloadResult(responData);
	} else {
		if (GiphyWebHandler::isHttpRedirect(reply)) {
			PLS_ERROR(MAIN_FILE_DOWNLOADER, "Request was redirected.url=%s", url.toEncoded().constData());
			responData.errorString = QString("Request was redirected.");
			responData.resultType = ResultStatus::ERROR_OCCUR;
			if (responData.taskData.callback) {
				responData.taskData.callback(responData);
			}
			emit downloadResult(responData);
		} else {
			DownloadSuccess(responData, reply);
		}
	}
	taskDownloads.remove(reply);
	timerMap.remove(reply);
	reply->deleteLater();
}

void PLSFileDownloader::OnNetworkAccessChanged(NetworkAccessible accessible)
{
	mutex.lock();
	auto tasks = tasksRetry;
	mutex.unlock();
	if (QNetworkAccessManager::Accessible == accessible) {
		if (tasks.isEmpty())
			return;
		qDebug("retry task size=%d,retry download...", tasks.size());
		int sizeTask = tasks.size();
		for (int i = 0; i < sizeTask; ++i) {
			DownloadTaskData task = tasks.dequeue();
			excuteTask(task, false);
			QThread::msleep(50);
		}
	} else {
		auto iter = taskDownloads.begin();
		while (iter != taskDownloads.end()) {
			if (iter->needRetry)
				tasks.enqueue(iter.value());
			iter.key()->deleteLater();
			iter = taskDownloads.erase(iter);
		}
	}
}

void PLSFileDownloader::abortTaskInner(const DownloadTaskData &taskData)
{
	auto iter = taskDownloads.begin();
	while (iter != taskDownloads.end()) {
		if (iter.value().equals(taskData)) {
			if (iter.key() != nullptr) {
				iter.key()->abort();
			}
			break;
		}
		iter++;
	}
}

void PLSFileDownloader::StopInner()
{
	stopped = true;
	threadDownload.quit();
	threadDownload.wait();

	if (nullptr != manager) {
		manager->deleteLater();
		manager = nullptr;
	}
	ClearTask();
}

bool PLSFileDownloader::saveToDisk(QString &filename, const QString &outputPath, const QByteArray &data, qint64 /*version*/)
{
	QString path(outputPath);
	QDir dir(path);
	if (!dir.exists()) {
		dir.mkpath(path);
	}
	filename = path + "/" + filename;
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly)) {
		std::string name = GetFileName(filename.toUtf8().constData());
		PLS_ERROR(MAIN_FILE_DOWNLOADER, "Could not open %s for writing: %s\n", name.c_str(), qPrintable(file.errorString()));
		return false;
	}

	file.write(data);
	file.close();
	return true;
}

void PLSFileDownloader::ClearTask()
{
	auto iter = taskDownloads.begin();
	while (iter != taskDownloads.end()) {
		iter.key()->deleteLater();
		iter = taskDownloads.erase(iter);
	}

	auto iter_timer = timerMap.begin();
	while (iter_timer != timerMap.end()) {
		if (iter_timer.value()) {
			iter_timer.value()->stop();
			iter_timer.value()->deleteLater();
		}
		iter_timer = timerMap.erase(iter_timer);
	}
}
