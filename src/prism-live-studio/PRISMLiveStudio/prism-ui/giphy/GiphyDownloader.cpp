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

void GiphyDownloader::excuteTask(const DownloadTaskData &taskData)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
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
	taskDownloads.insert(url, taskData);
	pls::rsm::getDownloader()->download(pls::rsm::UrlAndHowSave()                                      //
						    .keyPrefix(QStringLiteral("giphy-") + filename)
						    .fileName(filename)                                    //
						    .saveDir(pls_get_user_path(GIPHY_STICKERS_CACHE_PATH)) //
						    .url(url)                                              //
						    .done([](const pls::rsm::UrlAndHowSave &urlAndHowSave, bool ok, const QString &, pls::rsm::PathFrom pathFrom, bool) {
							    // handle done callback
							    PLS_INFO(MAIN_GIPHY_STICKER_MODULE, "Download '%s' done. result: %s", qUtf8Printable(urlAndHowSave.fileName()),
								     ok ? "Succeeded" : "Failed");
						    }),
					    [this](const pls::rsm::DownloadResult &result) {
						    // handle download result
						    if (pls_get_app_exiting())
							    return;

						    if (result.timeout()) {
							    downloadTimeout(result);
						    } else {
							    downloadFinished(result);
						    }
					    });
}

void GiphyDownloader::downloadFinished(const pls::rsm::DownloadResult &result)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	auto url = result.m_urlAndHowSave.url();
	TaskResponData responData;
	responData.taskData = taskDownloads[url];
	if (!result.isOk()) {
		if (responData.taskData.needRetry)
			tasksRetry.enqueue(responData.taskData);
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Failed to download '%s'.", url.toEncoded().constData());
		responData.errorString = QString::asprintf("Failed to download '%s'.", url.toEncoded().constData());
		responData.resultType = ResultStatus::ERROR_OCCUR;
		emit downloadResult(responData);
	} else {
		responData.fileName = result.m_urlAndHowSave.savedFilePath();
		emit downloadResult(responData);
	}
	taskDownloads.remove(url);
}

void GiphyDownloader::downloadTimeout(const pls::rsm::DownloadResult &result)
{
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	auto url = result.m_urlAndHowSave.url();
	TaskResponData responData;
	responData.taskData = taskDownloads[url];
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
	std::lock_guard<std::recursive_mutex> locker(m_mutex);
	auto iter = taskDownloads.begin();
	while (iter != taskDownloads.end()) {
		iter = taskDownloads.erase(iter);
	}
}
