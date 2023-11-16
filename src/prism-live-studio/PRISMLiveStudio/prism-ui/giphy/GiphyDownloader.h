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

class GiphyDownloader : public QObject {
	Q_OBJECT

public:
	static GiphyDownloader *instance();
	void Get(const DownloadTaskData &taskData);
	void Start();
	bool IsRunning() const;
	static bool IsDownloadFileExsit(const DownloadTaskData &task, QString &fileName);

private slots:
	void excuteTask(const DownloadTaskData &taskData);
	void sslErrors(QList<QSslError>) const;
	void downloadFinished(const pls::http::Reply &reply);
	void downloadTimeout(const pls::http::Reply &reply);

signals:
	void downloadResult(const TaskResponData &result);
	void downloadProgress(const TaskResponData &result, qint64 bytesReceived, qint64 bytesTotal);

private:
	bool saveToDisk(QString &filename, const pls::http::Reply &reply) const;
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
