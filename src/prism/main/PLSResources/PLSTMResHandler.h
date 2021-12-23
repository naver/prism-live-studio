#ifndef PLSTMRESHANDLER_H
#define PLSTMRESHANDLER_H

#include "PLSResourceHandler.h"

class PLSTMResHandler : public PLSResourceHandler {
public:
	explicit PLSTMResHandler(const QStringList &resUrls, QObject *parent = nullptr);
	~PLSTMResHandler();
	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;

private:
	void onGetTMResourcesProcess(const QByteArray &data);
	void downLoadGif(const QString &urlStr, const QString &language, const QString &id);
};

#endif // PLSTMRESHANDLER_H
