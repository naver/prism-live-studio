#ifndef PLSFILEDOWNLOADER_H
#define PLSFILEDOWNLOADER_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QMutex>
#include <QTimer>
#include <QQueue>
#include "giphy/GiphyDefine.h"
#include "libhttp-client.h"

using retryCallback_t = bool (*)(const DownloadTaskData &taskData);

namespace downloader {
static inline QString getFileName(const QString &file)
{
	QFileInfo info(file);
	return info.fileName();
}
}

class PLSFileDownloader : public QObject {
	Q_OBJECT

public:
	static PLSFileDownloader *instance()
	{
		static PLSFileDownloader downloader;
		return &downloader;
	}
	void Get(const DownloadTaskData &taskData, bool forceDownload = false);
	void Start();
	void Stop();
	bool IsRunning() const;
	void Retry(retryCallback_t callback = nullptr);
	static bool IsDownloadFileExsit(const DownloadTaskData &task, QString &fileName);

private slots:
	void excuteTask(const DownloadTaskData &taskData, bool forceDownload);
	void downloadFinished(const pls::http::Reply &reply, const DownloadTaskData &responData);
	void OnNetworkAccessChanged(bool accessible);
	void StopInner();
	void OnRequestTimeout(const DownloadTaskData &taskData);

signals:
	void downloadResult(const TaskResponData &result);
	void downloadProgress(const TaskResponData &result, qint64 bytesReceived, qint64 bytesTotal);

private:
	bool saveToDisk(QString &filename, const QString &outputPath, const QByteArray &data, qint64 version = -1) const;
	void ClearTask();
	void DownloadSuccess(TaskResponData responData, const pls::http::Reply &reply);
	bool UseCacheFile(const DownloadTaskData &taskData, const QString &userFileName);

	void DownloadFlow(const DownloadTaskData &taskData);
	void StartRequest(const DownloadTaskData &taskData);

	explicit PLSFileDownloader(QObject *parent = nullptr);
	~PLSFileDownloader() override;

	QMap<QUrl, DownloadTaskData> taskDownloads;
	QQueue<DownloadTaskData> tasksRetry;
	QMutex mutex;
	void *callbackParam = nullptr;
	bool running{false};
	std::atomic_bool stopped = false;
};

#endif // PLSFILEDOWNLOADER_H
