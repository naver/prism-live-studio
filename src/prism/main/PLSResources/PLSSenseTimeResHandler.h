#ifndef PLSSENSETIMERESHANDLER_H
#define PLSSENSETIMERESHANDLER_H

#include "PLSResourceHandler.h"

class PLSSenseTimeResHandler : public PLSResourceHandler {
public:
	PLSSenseTimeResHandler(const QStringList &resUrls, QObject *parent = nullptr, bool isUseSubThread = false);
	~PLSSenseTimeResHandler();

	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString()) override;
	/**
	 * synchronized check and download sensetime resource file if needed.
	 */
	static void checkAndDownloadSensetimeResource();

private:
	void getSensetime();

private:
	QMap<QString, QString> m_libraryResUrls;
};

#endif // PLSSENSETIMERESHANDLER_H
