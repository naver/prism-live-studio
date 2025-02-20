#ifndef GIPHYDOWNLOADER_H
#define GIPHYDOWNLOADER_H

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
#include "libhttp-client.h"
#include <libresource.h>

class GiphyDownloader : public QObject {
	Q_OBJECT

public:
	static GiphyDownloader *instance();
	void Get(const DownloadTaskData &taskData);
	void Start();
	bool IsRunning() const;

private slots:
	void excuteTask(const DownloadTaskData &taskData);
	void downloadFinished(const pls::rsm::DownloadResult &result);
	void downloadTimeout(const pls::rsm::DownloadResult &result);

signals:
	void downloadResult(const TaskResponData &result);
	void downloadProgress(const TaskResponData &result, qint64 bytesReceived, qint64 bytesTotal);

private:
	static QString saveFileName(const QUrl &url, const QString &id, const QString &tail);
	void ClearTask();

	explicit GiphyDownloader(QObject *parent = nullptr);
	~GiphyDownloader() override;

	QNetworkAccessManager *manager{nullptr};
	QMap<QUrl, DownloadTaskData> taskDownloads;
	QQueue<DownloadTaskData> tasksRetry;
	QThread threadDownload;
	std::recursive_mutex m_mutex;
	bool running{false};
};
#endif // GIPHYDOWNLOADER_H
