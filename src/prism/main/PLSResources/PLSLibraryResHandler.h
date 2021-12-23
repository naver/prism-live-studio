#ifndef PLSLIBRARYRESHANDLER_H
#define PLSLIBRARYRESHANDLER_H

#include "PLSResourceHandler.h"

class PLSLibraryResHandler : public PLSResourceHandler {
public:
public:
	PLSLibraryResHandler(const QStringList &resUrls, QObject *parent = nullptr);
	~PLSLibraryResHandler();
	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;

private:
	void getLibraryResProcess();
	void getWatermarkOutroRes();
	QString getPolicyFileNameByURL(const QString &url);

private:
	QMap<QString, QString> m_libraryResUrls;
	QMap<QString, QVariantMap> m_outroMap;
};

#endif // PLSLIBRARYRESHANDLER_H
