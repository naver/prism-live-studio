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
#include "GiphyDefine.h"

typedef bool (*retryCallback_t)(const DownloadTaskData &taskData);

class PLSFileDownloader : public QObject {
	Q_OBJECT
	using NetworkAccessible = QNetworkAccessManager::NetworkAccessibility;
	using CheckFileVersionCallback = bool (*)(const DownloadTaskData &task, QString &fileName, void *param);

public:
	static PLSFileDownloader *instance()
	{
		static PLSFileDownloader downloader;
		return &downloader;
	}
	void Get(const DownloadTaskData &taskData, bool forceDownload = false);
	void Start();
	void Stop();
	bool IsRunning();
	void Retry(retryCallback_t callback = nullptr);
	void SetCheckVersionCallback(CheckFileVersionCallback checkversion, void *param);
	void AbortTask(const DownloadTaskData &taskData);
	static bool IsDownloadFileExsit(const DownloadTaskData &task, QString &fileName);

private slots:
	void excuteTask(const DownloadTaskData &taskData, bool forceDownload);
	void sslErrors(QList<QSslError>);
	void downloadFinished(QNetworkReply *reply);
	void OnNetworkAccessChanged(NetworkAccessible accessible);
	void abortTaskInner(const DownloadTaskData &taskData);
	void StopInner();

signals:
	void downloadResult(const TaskResponData &result);
	void downloadProgress(const TaskResponData &result, qint64 bytesReceived, qint64 bytesTotal);

private:
	bool saveToDisk(QString &filename, const QString &outputPath, const QByteArray &data, qint64 version = -1);
	void ClearTask();
	void DownloadSuccess(TaskResponData responData, QNetworkReply *reply);

private:
	explicit PLSFileDownloader(QObject *parent = nullptr);
	~PLSFileDownloader() override;

private:
	QNetworkAccessManager *manager{nullptr};
	QMap<QNetworkReply *, DownloadTaskData> taskDownloads;
	QMap<QNetworkReply *, QTimer *> timerMap;
	QQueue<DownloadTaskData> tasksRetry;
	QThread threadDownload;
	QMutex mutex;
	CheckFileVersionCallback checkversionCallback = nullptr;
	void *callbackParam = nullptr;
	bool running{false};
	volatile std::atomic_bool stopped = false;
};

#endif // PLSFILEDOWNLOADER_H
