#include "PLSFileDownloader.h"
#include <pls-common-define.hpp>
#include <frontend-api/frontend-api.h>
#include <QPointer>
#include "log/module_names.h"
#include "giphy/GiphyWebHandler.h"
#include "liblog.h"
#include "platform.hpp"
#include "utils-api.h"

using namespace downloader;

PLSFileDownloader::PLSFileDownloader(QObject *parent) : QObject(parent)
{
	pls_network_state_monitor([this](bool accessible) { OnNetworkAccessChanged(accessible); });
}

PLSFileDownloader::~PLSFileDownloader()
{
	stopped = true;
	ClearTask();
}

void PLSFileDownloader::Get(const DownloadTaskData &taskData, bool forceDownload)
{
	if (!running)
		Start();
	excuteTask(taskData, forceDownload);
}

void PLSFileDownloader::Start()
{
	if (running)
		return;
	running = true;
	stopped = false;
}

void PLSFileDownloader::Stop()
{
	StopInner();
}

bool PLSFileDownloader::IsRunning() const
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
	auto sizeTask = tasks.size();
	for (qsizetype i = 0; i < sizeTask; ++i) {
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

void PLSFileDownloader::DownloadSuccess(TaskResponData responData, const pls::http::Reply &reply)
{
	auto url = reply.request().url();
	QString fileName = responData.taskData.outputFileName;
	QFileInfo info(url.path());
	if (fileName.isEmpty()) {
		fileName = info.fileName();
	} else {
		fileName.append(".").append(info.suffix());
	}
	QByteArray rawData = reply.data();

	QVariant value = reply.header(QNetworkRequest::ContentLengthHeader);
	auto name = getFileName(fileName);
	if (value.isValid()) {
		if (value.toULongLong() != (qulonglong)rawData.size()) {
			PLS_WARN(MAIN_FILE_DOWNLOADER, "[FILE-DOWNLOAD]%s download finished. Contentlength: %llu, received size: %d", qUtf8Printable(name), value.toULongLong(), rawData.size());
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
		PLS_ERROR(MAIN_FILE_DOWNLOADER, "Save file to disk failed.File name=%s", qUtf8Printable(name));
		responData.errorString = QString("Save file to disk failed");
		responData.resultType = ResultStatus::ERROR_OCCUR;
		if (responData.taskData.callback) {
			responData.taskData.callback(responData);
		}
		emit downloadResult(responData);
	}
}

bool PLSFileDownloader::UseCacheFile(const DownloadTaskData &taskData, const QString &userFileName)
{
	QMutexLocker locer(&mutex);
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
			PLS_WARN("Open file: %s failed, redownload it.", qUtf8Printable(taskData.outputFileName));
			return false;
		}
	}
	if (taskData.callback) {
		taskData.callback(responData);
	}
	emit downloadResult(responData);
	return true;
}

void PLSFileDownloader::DownloadFlow(const DownloadTaskData &taskData)
{
	if (!pls_get_network_state()) {
		if (taskData.needRetry) {
			QMutexLocker locer(&mutex);
			qDebug("Network is not accessible,add task to retry list, current retry task size is:%llu", tasksRetry.size());
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

	StartRequest(taskData);
}

void PLSFileDownloader::StartRequest(const DownloadTaskData &taskData)
{
	QUrl url = QUrl::fromEncoded(taskData.url.toLocal8Bit());
	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, "")               //
				   .forDownload(false)             //
				   .saveFilePath("")               //
				   .withLog()                      //
				   .receiver(qApp)                 //
				   .allowAbort(true)
				   .progress([this, taskData, url](const pls::http::Reply &reply) {
					   TaskResponData responData;
					   responData.taskData = taskData;
					   responData.fileName = url.fileName();
					   emit downloadProgress(responData, reply.downloadedBytes(), reply.downloadTotalBytes());
				   })
				   .result([this, taskData](const pls::http::Reply &reply) {
					   if (reply.isTimeout()) {
						   OnRequestTimeout(taskData);
					   } else {
						   downloadFinished(reply, taskData);
					   }
				   }));
}

void PLSFileDownloader::excuteTask(const DownloadTaskData &taskData, bool forceDownload)
{
	if (forceDownload) {
		DownloadFlow(taskData);
		return;
	}

	QString userFileName;
	if (IsDownloadFileExsit(taskData, userFileName)) {
		if (!UseCacheFile(taskData, userFileName))
			DownloadFlow(taskData);
		return;
	}

	DownloadFlow(taskData);
}

void PLSFileDownloader::downloadFinished(const pls::http::Reply &reply, const DownloadTaskData &downloaData)
{
	if (stopped) {
		return;
	}

	QUrl url = reply.request().url();
	TaskResponData responData;
	responData.taskData = downloaData;
	if (QNetworkReply::NoError != reply.error()) {
		if (responData.taskData.needRetry) {
			QMutexLocker locer(&mutex);
			tasksRetry.enqueue(responData.taskData);
		}
		PLS_ERROR(MAIN_FILE_DOWNLOADER, "Download of %s failed: %s\n", url.toEncoded().constData(), qUtf8Printable(reply.errors()));
		responData.errorString = QString::asprintf("Download of %s failed: %s", url.toEncoded().constData(), qUtf8Printable(reply.errors()));
		responData.resultType = ResultStatus::ERROR_OCCUR;
		if (responData.taskData.callback) {
			responData.taskData.callback(responData);
		}
		emit downloadResult(responData);
	} else {
		if (GiphyWebHandler::isHttpRedirect(reply.reply())) {
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
}

void PLSFileDownloader::OnNetworkAccessChanged(bool accessible)
{
	mutex.lock();
	auto tasks = tasksRetry;
	mutex.unlock();
	if (accessible) {
		if (tasks.isEmpty())
			return;
		qDebug("retry task size=%d,retry download...", tasks.size());
		auto sizeTask = tasks.size();
		for (qsizetype i = 0; i < sizeTask; ++i) {
			DownloadTaskData task = tasks.dequeue();
			excuteTask(task, false);
			QThread::msleep(50);
		}
	} else {
		auto iter = taskDownloads.begin();
		while (iter != taskDownloads.end()) {
			if (iter->needRetry)
				tasks.enqueue(iter.value());
			iter = taskDownloads.erase(iter);
		}
	}
}

void PLSFileDownloader::StopInner()
{
	stopped = true;
	ClearTask();
}

void PLSFileDownloader::OnRequestTimeout(const DownloadTaskData &taskData)
{
	if (stopped)
		return;

	TaskResponData responData;
	responData.taskData = taskData;
	responData.resultType = ResultStatus::ERROR_OCCUR;
	responData.subType = ErrorSubType::Error_Timeout;
	if (responData.taskData.callback) {
		responData.taskData.callback(responData);
	}
	emit downloadResult(responData);
}

bool PLSFileDownloader::saveToDisk(QString &filename, const QString &outputPath, const QByteArray &data, qint64 /*version*/) const
{
	QString path(outputPath);
	QDir dir(path);
	if (!dir.exists()) {
		dir.mkpath(path);
	}
	filename = path + "/" + filename;
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly)) {
		PLS_WARN(MAIN_FILE_DOWNLOADER, "Could not open %s for writing: %s\n", qUtf8Printable(getFileName(filename)), qUtf8Printable(file.errorString()));
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
		iter = taskDownloads.erase(iter);
	}
}
