#ifndef PLSMOTIONNETWORK_H
#define PLSMOTIONNETWORK_H

#include <QObject>
#include <functional>
#include <qnetworkaccessmanager.h>
#include <QNetworkReply>
#include <QPointer>
#include <QMap>
#include <QThread>
#include <QFile>
#include <QTimer>
#include <chrono>
#include "PLSMotionDefine.h"

class PLSMotionFileManager;

class PLSMotionNetwork : public QObject {
	Q_OBJECT

public:
	explicit PLSMotionNetwork(QObject *parent = nullptr);
	~PLSMotionNetwork() override;

	static PLSMotionNetwork *instance();
	void downloadResource(const QString &categoryJsonFilePath) const;
	void downloadResource(const QJsonObject &obj) const;
	void downloadPrismListRequest(bool force = false);
	void downloadFreeListRequest(bool force = false);
	QList<MotionData> getPrismCacheList() const;
	QList<MotionData> getFreeCacheList() const;
	bool prismRequestFinished() const;
	bool freeRequestFinished() const;

	bool isPrismOrFree(const MotionData &md) const;
	bool isPathEqual(const MotionData &md1, const MotionData &md2) const;
	bool findPrismOrFree(MotionData &md, const QString &itemId) const;
	int GetResourceNotCachedCount();

private:
	void downloadListRequest(const QVariantList &list, const QString &group);
	bool checkResourceCached(MotionData &md, const QString &group);
	void downloadResourcePrismFile(const MotionData &md, bool update = false, bool reDownload = false, const std::function<void(const MotionData &md)> &ok = nullptr,
				       const std::function<void(const MotionData &md)> &fail = nullptr);
	void downloadResourceFreeFile(const MotionData &md, bool update = false, bool reDownload = false, const std::function<void(const MotionData &md)> &ok = nullptr,
				      const std::function<void(const MotionData &md)> &fail = nullptr);
	void downloadResourceFile(const QString &group, const MotionData &md, bool update = false, bool reDownload = false, const std::function<void(const MotionData &md)> &ok = nullptr,
				  const std::function<void(const MotionData &md)> &fail = nullptr);
	void reDownloadResourceFile(MotionData &md, bool update = false, const std::function<void(const MotionData &md)> &ok = nullptr,
				    const std::function<void(const MotionData &md)> &fail = nullptr);
	bool uncompress(MotionData &md, const QString &resourcePath, const QString &thumbnailPath) const;
	void checkStatic(MotionData &md, const QStringList &files) const;
	void checkMotion(MotionData &md, const QStringList &files) const;
	void downloadResourceFileFinished(bool ok, const QString &group, const MotionData &md, bool update, bool reDownload);
	void checkListRequestState(const QString &groupType);
	void checkReDownloadListRequestState(const QString &groupType);
	bool isDownloading(const QString &group, const QString &itemId) const;
	void setDownloading(const QString &group, const QString &itemId);
	void loadList();
	void loadListRequest(const QVariantList &list, const QString &group);
	void loadCacheList();
	void saveCacheList();

signals:
	void prismResourceDownloadFinished(const MotionData &data, bool update = false);
	void freeResourceDownloadFinished(const MotionData &data, bool update = false);
	void prismListStartedSignal();
	void freeListStartedSignal();
	void prismListFinishedSignal();
	void freeListFinishedSignal();

private:
	QMap<QString, QList<MotionData>> m_cacheList;
	QMap<QString, QList<MotionData>> m_downloadCache;
	bool m_prismRequestFinished{false};
	bool m_freeRequestFinished{false};

	friend class PLSMotionFileManager;
};

#endif // PLSMOTIONNETWORK_H
