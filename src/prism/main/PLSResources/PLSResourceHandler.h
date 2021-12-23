#ifndef PLSRESOURCEHANDLER_H
#define PLSRESOURCEHANDLER_H

#include <QString>
#include <QObject>
#include <qvector.h>
#include <qthread.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <qjsonvalue.h>
#include <qdir.h>
#include <qeventloop.h>

#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSHttpApi/PLSHmacNetworkReplyBuilder.h"
#include "PLSHttpApi/PLSNetworkReplyBuilder.h"
#include "frontend-api.h"
#include "log/module_names.h"
#include "log.h"
#include "pls-common-define.hpp"
#include "json-data-handler.hpp"

typedef struct ResFileInfo {
	QString fileName;
	QString resUrl;
	QString savePath;
} ResFileInfo;
using fileHandlerCallback = function<void(const ResFileInfo &fileInfo)>;
using resourceCallback = function<void(const QMap<QString, bool> &resourceStatus)>;
enum class DownLoadResourceMode { All, Part };

class PLSResourceHandler : public QObject {
	Q_OBJECT

public:
	explicit PLSResourceHandler(const QStringList &resUrls, QObject *parent = nullptr, bool isUseSubThread = true);
	virtual ~PLSResourceHandler();

	virtual void doWorks(const QByteArray &data, DownLoadResourceMode downMode = DownLoadResourceMode::All, const QString &resPath = QString());
	void startDownLoadRes(DownLoadResourceMode downMode = DownLoadResourceMode::All, const QMap<QString, QString> &resourceUrlsPath = QMap<QString, QString>());
	void setItemName(const QString &itemName);
	void setRelativeResPath(const QString &relativeResPath);
	QString getRelativeResPath();
	void saveResource(const QByteArray &data, const QString &path);
	void setResUrls(const QStringList &urls);
	void setResFileName(const QString &fileName);
	static bool saveFile(const QString &filePath, const QString &fileName, const QByteArray &data);
	static bool unCompress(const QString &filePath, const QString &compressName, bool isRemoveFile = true);
	static void removeAllJsonFile(const QString &path);
	static bool moveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove = false);
	static QStringList getLanguageKey();
	void abort();
	void appendReply(QNetworkReply *reply);

public slots:
	void reDownloadResources(const ResFileInfo &file, fileHandlerCallback callback);
signals:
	void requestReDownload(const ResFileInfo &, fileHandlerCallback callback);

	void downPartResourceSignal(QMap<QString, bool> resouceStatus);
	void threadHandlerSignal(const QStringList &urls);

private:
	void downPartResourceCallback();

private slots:
	void threadHandler(const QStringList &urls);

protected:
	QString m_resFileName;
	QThread m_workThread;
	QNetworkAccessManager *m_manager;
	QString m_resItemName;
	QString m_relativeResPath;
	QStringList m_resUrls;
	QVector<ResFileInfo> m_ReDownLoadRes;
	int m_reDownloadCount = 3;
	DownLoadResourceMode m_downMode;
	QMap<QString, bool> m_resourceDownStatus;
	QMap<QString, QString> m_resourceUrlToPaths;
	bool m_isUseSubThread = true;
	QEventLoop m_eventLoop;
	QVector<QPointer<QNetworkReply>> m_replys;
};

#endif // PLSRESOURCEHANDLER_H
