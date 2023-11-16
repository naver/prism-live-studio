#include "GiphyDownloader.h"
#include <QThreadPool>
#include "GiphyWebHandler.h"
#include <pls-common-define.hpp>
#include <frontend-api/frontend-api.h>
#include "log/module_names.h"
#include "liblog.h"
#include "network-state.h"

using namespace common;
constexpr auto CONTENT_TYPE = "image/gif";

static void GiphyDownloaderThread()
{
	PLS_LOG(PLS_LOG_INFO, MAIN_GIPHY_STICKER_MODULE, "[%s] Thread started.", __FUNCTION__);
}

GiphyDownloader::GiphyDownloader(QObject *parent) : QObject(parent)
{
	pls_network_state_monitor([this](bool accessible) {
		std::lock_guard<std::recursive_mutex> locker(m_mutex);
		if (accessible) {
			if (tasksRetry.isEmpty())
				return;
			qDebug("retry task size=%llu,retry download...", tasksRetry.size());
			auto sizeTask = tasksRetry.size();
			for (size_t i = 0; i < sizeTask; ++i) {
				DownloadTaskData task = tasksRetry.dequeue();
				excuteTask(task);
				QThread::msleep(50);
			}
		} else {
			auto iter = taskDownloads.begin();
			while (iter != taskDownloads.end()) {
				if (iter->needRetry)
					tasksRetry.enqueue(iter.value());
				iter = taskDownloads.erase(iter);
			}
		}
	});
}

GiphyDownloader::~GiphyDownloader()
{
	threadDownload.quit();
	threadDownload.wait();

	if (nullptr != manager)
		manager->deleteLater();
	ClearTask();
}

GiphyDownloader * ::GiphyDownloader::instance()
{
	static GiphyDownloader downloader;
	return &downloader;
}

void GiphyDownloader::Get(const DownloadTaskData &taskData)
{
	QMetaObject::invokeMethod(this, "excuteTask", Qt::QueuedConnection, Q_ARG(const DownloadTaskData &, taskData));
}

void GiphyDownloader::Start()
{
	if (running)
		return;
	running = true;
	this->moveToThread(&threadDownload);
	connect(&threadDownload, &QThread::started, GiphyDownloaderThread);
	threadDownload.start();
}

bool GiphyDownloader::IsRunning() const
{
	return running;
}

bool GiphyDownloader::IsDownloadFileExsit(const DownloadTaskData &taskData, QString &outputFileName)
{
	QUrl url = QUrl::fromEncoded(taskData.url.toLocal8Bit());
	QString tail;
	switch (taskData.type) {
	case StickerDownloadType::THUMBNAIL:
		tail = "thumbnail";
		break;
	case StickerDownloadType::ORIGINAL:
		tail = "original";
		break;
	default:
		break;
	}
	QString fileName = saveFileName(url, taskData.uniqueId, tail);
	outputFileName = pls_get_user_path(GIPHY_STICKERS_CACHE_PATH) + fileName;
	return QFile::exists(outputFileName);
}

void GiphyDownloader::excuteTask(const DownloadTaskData &taskData)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	QUrl url = QUrl::fromEncoded(taskData.url.toLocal8Bit());
	QString userFileName;
	if (IsDownloadFileExsit(taskData, userFileName)) {
		TaskResponData responData;
		responData.taskData = taskData;
		responData.fileName = userFileName;
		emit downloadResult(responData);
		return;
	}

	if (!pls_get_network_state()) {
		if (taskData.needRetry) {
			qDebug("Network is not accessible,add task to retry list, current retry task size is:%llu", tasksRetry.size());
			tasksRetry.enqueue(taskData);
		}
		QString errorString("Network is not accessible");
		TaskResponData responData;
		responData.taskData = taskData;
		responData.errorString = errorString;
		responData.resultType = ResultStatus::ERROR_OCCUR;
		emit downloadResult(responData);
		return;
	}

	pls::http::request(pls::http::Request()
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, "")               //
				   .forDownload(false)             //
				   .saveFilePath("")               //
				   .withLog()                      //
				   .receiver(qApp)                 //
				   .progress([this, taskData, userFileName](const pls::http::Reply &reply) {
					   if (pls_get_app_exiting())
						   return;

					   TaskResponData responData;
					   responData.taskData = taskData;
					   responData.fileName = userFileName;
					   emit downloadProgress(responData, reply.downloadedBytes(), reply.downloadTotalBytes());
				   })
				   .result([this](const pls::http::Reply &reply) {
					   if (pls_get_app_exiting())
						   return;

					   if (reply.isTimeout()) {
						   downloadTimeout(reply);
					   } else {
						   downloadFinished(reply);
					   }
				   }));

	taskDownloads.insert(url, taskData);
}

void GiphyDownloader::sslErrors(QList<QSslError>) const
{
	qDebug("ssl error occured");
}

void GiphyDownloader::downloadFinished(const pls::http::Reply &reply)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	QUrl url = reply.reply()->url();
	TaskResponData responData;
	responData.taskData = taskDownloads[url];
	if (QNetworkReply::NoError != reply.error()) {
		if (responData.taskData.needRetry)
			tasksRetry.enqueue(responData.taskData);
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Download of %s failed: %s\n", url.toEncoded().constData(), qUtf8Printable(reply.errors()));
		responData.errorString = QString::asprintf("Download of %s failed: %s", url.toEncoded().constData(), qUtf8Printable(reply.errors()));
		responData.resultType = ResultStatus::ERROR_OCCUR;
		emit downloadResult(responData);
	} else {
		if (GiphyWebHandler::isHttpRedirect(reply.reply())) {
			PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Request was redirected.url=%s", url.toEncoded().constData());
			responData.errorString = QString("Request was redirected.");
			responData.resultType = ResultStatus::ERROR_OCCUR;
			emit downloadResult(responData);
		} else {
			QString tail;
			switch (responData.taskData.type) {
			case StickerDownloadType::THUMBNAIL:
				tail = "thumbnail";
				break;
			case StickerDownloadType::ORIGINAL:
				tail = "original";
				break;
			default:
				break;
			}
			QString filename = saveFileName(url, responData.taskData.uniqueId, tail);
			if (saveToDisk(filename, reply)) {
				responData.fileName = filename;
				emit downloadResult(responData);

			} else {
				PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Save file to disk failed.File name=%s", filename.toUtf8().constData());
				responData.errorString = QString("Save file to disk failed");
				responData.resultType = ResultStatus::ERROR_OCCUR;
				emit downloadResult(responData);
			}
		}
	}
	taskDownloads.remove(url);
}

void GiphyDownloader::downloadTimeout(const pls::http::Reply &reply)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	QUrl url = reply.reply()->url();
	TaskResponData responData;
	responData.taskData = taskDownloads[url];
	responData.errorString = QString("Request was timeout.");
	responData.resultType = ResultStatus::ERROR_OCCUR;
	responData.subType = ErrorSubType::Error_Timeout;
	emit downloadResult(responData);
}

bool GiphyDownloader::saveToDisk(QString &filename, const pls::http::Reply &reply) const
{
	QString contentType = reply.header(QNetworkRequest::ContentTypeHeader).toString();
	if (0 == contentType.compare(CONTENT_TYPE, Qt::CaseInsensitive)) {
		QString path(pls_get_user_path(GIPHY_STICKERS_CACHE_PATH));
		QDir dir(path);
		if (!dir.exists()) {
			dir.mkpath(path);
		}
		QString fileNameTmp = filename;
		filename = path + "/" + filename;
		QFile file(filename);
		if (!file.open(QIODevice::WriteOnly)) {
			PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Could not open file:%s for writing: %s\n", qUtf8Printable(fileNameTmp), qUtf8Printable(file.errorString()));
			return false;
		}

		file.write(reply.data());
		file.close();
		return true;
	}

	return false;
}

QString GiphyDownloader::saveFileName(const QUrl &url, const QString &id, const QString &tail)
{
	QString path = url.path();
	QString basename = QFileInfo(path).fileName();

	if (!id.isEmpty())
		basename = id + "_" + tail + "." + QFileInfo(path).suffix();
	return basename;
}

void GiphyDownloader::ClearTask()
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	auto iter = taskDownloads.begin();
	while (iter != taskDownloads.end()) {
		iter = taskDownloads.erase(iter);
	}
}
