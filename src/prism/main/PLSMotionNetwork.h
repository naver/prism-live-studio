#ifndef PLSMOTIONNETWORK_H
#define PLSMOTIONNETWORK_H

#include <QObject>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QMap>
#include <QThread>
#include <QFile>
#include <QTimer>
#include <chrono>
#include "PLSMotionDefine.h"

using namespace std;

using MotionRequestSuccessFunction = function<void(QList<MotionData> list)>;
using MotionRequestFailFunction = function<void()>;

class PLSMotionDownloadFile : public QObject {
	Q_OBJECT

public:
	PLSMotionDownloadFile(QNetworkAccessManager *namanager, const QString &group, const MotionData &md);
	~PLSMotionDownloadFile();

public:
	struct Url {
		QString url;
		QString &filePath;
		Url(const QString &url, QString &filePath);
	};

public:
	void start();
	void abort();

private:
	void downloadSuccess();
	void downloadFailed();
	void startTimer(int timeout);
	void stopTimer();

signals:
	void finished(bool ok, const QString &group, const MotionData &md);

private slots:
	void onStart();
	void onDownload();
	void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void onWriteData();
	void onDownloadFinished();
	void onUncompress();
	void onTimeout();

public:
	QNetworkAccessManager *namanager;
	QString group;
	MotionData md;
	QList<std::shared_ptr<Url>> urls;
	QNetworkReply *reply = nullptr;
	QTimer *timer = nullptr;
	QFile file;
	QString resourcePath;
	QString thumbnailPath;
	qint64 bytesReceived = 0;
	qint64 bytesTotal = 0;
	std::chrono::time_point<std::chrono::steady_clock> lastWriteTime;
	bool aborting = false;
};

class PLSMotionDownloadThread : public QThread {
	Q_OBJECT

public:
	PLSMotionDownloadThread();
	~PLSMotionDownloadThread();

public:
	bool downloadResourceFile(const QString &group, const MotionData &md);
	void abort();

private:
	void downloadResourceFileFinished(bool ok, const QString &group, const MotionData &md);

signals:
	void finished(bool ok, const QString &group, const MotionData &md);
	void aborting();

private:
	static const int MaxTotalCount = 3;

	QNetworkAccessManager *namanager;
	int totalCount = 0;
	int finishedCount = 0;
};

class PLSMotionNetwork : public QObject {
	Q_OBJECT

public:
	explicit PLSMotionNetwork(QObject *parent = nullptr);
	~PLSMotionNetwork();

	static PLSMotionNetwork *instance();
	void downloadResource(const QJsonObject &obj);
	void downloadPrismListRequest(bool force = false);
	void downloadFreeListRequest(bool force = false);
	QList<MotionData> getPrismCacheList();
	QList<MotionData> getFreeCacheList();
	bool prismRequestFinished();
	bool freeRequestFinished();

	bool isPrismOrFree(const MotionData &md) const;
	bool isPathEqual(const MotionData &md1, const MotionData &md2) const;
	bool findPrismOrFree(MotionData &md, const QString &itemId) const;
	void abort();

private:
	void downloadListRequest(const QVariantList &list, const QString &group);
	bool checkResourceCached(MotionData &md, const QString &group);
	void downloadResourceFile(PLSMotionDownloadThread *&downloadThread, const QString &group, const MotionData &md);
	void downloadResourceFileFinished(bool ok, const QString &group, const MotionData &md);
	void checkListRequestState(const QString &groupType);
	void loadList();
	void loadListRequest(const QVariantList &list, const QString &group);
	void loadCacheList();
	void saveCacheList();

signals:
	void prismResourceDownloadFinished(const MotionData &data);
	void freeResourceDownloadFinished(const MotionData &data);
	void prismListStartedSignal();
	void freeListStartedSignal();
	void prismListFinishedSignal();
	void freeListFinishedSignal();

private:
	QMap<QString, QList<MotionData>> m_cacheList;
	QMap<QString, QList<MotionData>> m_downloadCache;
	bool m_prismRequestFinished;
	bool m_freeRequestFinished;
	QList<QPointer<PLSMotionDownloadThread>> m_downloadThreads;
};

#endif // PLSMOTIONNETWORK_H
