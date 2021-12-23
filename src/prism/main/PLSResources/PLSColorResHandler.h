#ifndef PLSCOLORRESHANDLER_H
#define PLSCOLORRESHANDLER_H

#include "PLSResourceHandler.h"

class PLSColorResHandler : public PLSResourceHandler {

public:
	PLSColorResHandler(const QStringList &resUrls, QObject *parent = nullptr);
	~PLSColorResHandler();
	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;

private:
	void onGetColorResourcesSuccess(const QByteArray &array);
	bool colorFilterDirIsExisted(const QString &path);
	void downloadThumbnailRequest();
	bool SaveToLocalThumbnailImage(const QByteArray &array, const QString &path);

private:
	QVariantMap m_resourceUrls;
	QVariantMap m_thumbnailUrls;
	QVariantMap thumbailImages;
	QVector<QNetworkReply *> m_replys;
};

#endif // PLSCOLORRESHANDLER_H
