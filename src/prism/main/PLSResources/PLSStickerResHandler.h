#ifndef PLSSTICKERRESHANDLER_H
#define PLSSTICKERRESHANDLER_H

#include <QMutex>
#include "PLSResourceHandler.h"

class PLSStickerResHandler : public PLSResourceHandler {
	Q_OBJECT

	struct CacheData {
		qint64 version = 0LL;
		QString id;
	};

public:
	PLSStickerResHandler(const QStringList &resUrls, QObject *parent = nullptr);
	~PLSStickerResHandler();
	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;

private:
	bool saveResource(const QByteArray &data, const QString &path);
	void updateDownloadCache(const QString &id, qint64 newVerison);
	bool CacheDataToLocalJson();

private:
	QMap<QString, CacheData> cachesVersion;
	QMutex cache_mutex;
};

#endif // PLSSTICKERRESHANDLER_H
