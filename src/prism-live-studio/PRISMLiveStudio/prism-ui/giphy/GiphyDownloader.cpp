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
	pls_network_state_monitor([this, this_guard = QPointer<GiphyDownloader>(this)](bool accessible) {
		if (pls_is_app_exiting())
			return;

		if (!this_guard)
			return;

		// Put the time-consuming or background logic into the child thread for execution
		this_guard->performBackgroundNetworkCheck(accessible);
	});
}

void GiphyDownloader::performBackgroundNetworkCheck(bool accessible)
{
	QMutexLocker locker(&m_mutex);
	if (accessible) {

		if (tasksRetry.isEmpty())
			return;

		qDebug("Retry status: retry task size=%d, GiphyDownloader::performBackgroundNetworkCheck retry download...", tasksRetry.size());
		auto sizeTask = tasksRetry.size();
		QList<DownloadTaskData> tasksToExecute;
		for (size_t i = 0; i < sizeTask; ++i) {
			DownloadTaskData task = tasksRetry.dequeue();
			tasksToExecute.append(task);
		}

		// Release the lock before executing the task
		locker.unlock();

		// Execute tasks (without holding locks)
		for (const auto &task : tasksToExecute) {
			excuteTask(task);
		}

	} else {
		qDebug("Retry status: download task size=%d, tasksRetry task size=%d, GiphyDownloader::performBackgroundNetworkCheck retry download...", taskDownloads.size(), tasksRetry.size());
		auto iter = taskDownloads.begin();
		while (iter != taskDownloads.end()) {
			if (iter->needRetry)
				tasksRetry.enqueue(iter.value());
			iter = taskDownloads.erase(iter);
		}
	}
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

void GiphyDownloader::addRetryTask(const DownloadTaskData &taskData)
{
	if (taskData.needRetry) {
		QMutexLocker locker(&m_mutex);
		tasksRetry.enqueue(taskData);
	}
	qDebug("Retry status: Network is not ok, add task to retry list, current retry task size is:%llu", tasksRetry.size());
}

void GiphyDownloader::excuteTask(const DownloadTaskData &taskData)
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
	QString filename = saveFileName(url, taskData.uniqueId, tail);
	{
		QMutexLocker locker(&m_mutex);
		taskDownloads.insert(url, taskData);
	}

	pls::rsm::getDownloader()->download(pls::rsm::UrlAndHowSave() //
						    .keyPrefix(QStringLiteral("giphy-") + filename)
						    .fileName(filename)                                    //
						    .saveDir(pls_get_user_path(GIPHY_STICKERS_CACHE_PATH)) //
						    .url(url)                                              //
						    .done([](const pls::rsm::UrlAndHowSave &urlAndHowSave, bool ok, const QString &, pls::rsm::PathFrom pathFrom, bool) {
							    // handle done callback
							    PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "Download '%s' done. result: %s", qUtf8Printable(urlAndHowSave.fileName()),
								     ok ? "Succeeded" : "Failed");
						    }),
					    [this, url, taskData](const pls::rsm::DownloadResult &result) {
						    // handle download result
						    if (pls_get_app_exiting())
							    return;

						    {
							    QMutexLocker locker(&m_mutex);
							    taskDownloads.remove(url);
						    }

						    if (result.timeout()) {
							    downloadTimeout(taskData);
						    } else {
							    downloadFinished(result, taskData);
						    }
					    });
}

void GiphyDownloader::downloadFinished(const pls::rsm::DownloadResult &result, const DownloadTaskData &taskData)
{
	TaskResponData responData;
	responData.taskData = taskData;
	if (!result.isOk()) {
		auto url = result.m_urlAndHowSave.url();
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Failed to download '%s'.", url.toEncoded().constData());
		responData.errorString = QString::asprintf("Failed to download '%s'.", url.toEncoded().constData());
		responData.resultType = ResultStatus::ERROR_OCCUR;
		addRetryTask(responData.taskData);
		emit downloadResult(responData);
	} else {
		responData.fileName = result.m_urlAndHowSave.savedFilePath();
		emit downloadResult(responData);
	}
}

void GiphyDownloader::downloadTimeout(const DownloadTaskData &taskData)
{
	TaskResponData responData;
	responData.taskData = taskData;
	addRetryTask(responData.taskData);
	responData.errorString = QString("Request was timeout.");
	responData.resultType = ResultStatus::ERROR_OCCUR;
	responData.subType = ErrorSubType::Error_Timeout;
	emit downloadResult(responData);
}

QString GiphyDownloader::saveFileName(const QUrl &url, const QString &id, const QString &tail)
{
	QString path = url.path();
	QString basename = QFileInfo(path).fileName();

	if (!id.isEmpty())
		basename = id + "_" + tail;
	return basename;
}

void GiphyDownloader::ClearTask()
{
	QMutexLocker locker(&m_mutex);
	auto iter = taskDownloads.begin();
	while (iter != taskDownloads.end()) {
		iter = taskDownloads.erase(iter);
	}
}
