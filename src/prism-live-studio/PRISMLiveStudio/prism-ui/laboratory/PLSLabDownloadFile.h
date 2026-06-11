#ifndef PLSLABDOWNLOADFILE_H
#define PLSLABDOWNLOADFILE_H

#include <QObject>
#include <QRunnable>
#include <qnetworkaccessmanager.h>
#include <QTimer>

class PLSLabDownloadFile : public QObject, public QRunnable {
	Q_OBJECT

public:
	explicit PLSLabDownloadFile(const QString &labId, QObject *parent = nullptr);
	~PLSLabDownloadFile() override;

private:
	bool onUncompress(const QString &path) const;
	bool writeDownloadCache();

protected:
	void run() override;

private:
	QString m_zipFilePath;
	QString m_unzipFolderPath;
	QString m_labId;
	QString m_title;
	QString m_version;
	QString m_type;
	QVariantMap m_downloadMap;

signals:
	void taskSucceeded(const QVariantMap &downloadMap);
	void taskFailed();
	void taskRelease();
};

#endif // PLSLABDOWNLOADFILE_H
