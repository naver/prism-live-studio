#ifndef PLSSERVERSTREAMHANDLER_H
#define PLSSERVERSTREAMHANDLER_H

#include <QObject>
#include "obs.h"
#include <qhttpmultipart.h>

class PLSBasic;

class PLSServerStreamHandler : public QObject {
	Q_OBJECT

public:
	static PLSServerStreamHandler *instance();
	~PLSServerStreamHandler();
	explicit PLSServerStreamHandler(QObject *parent = nullptr);
	bool getOutroInfo(QString &path, uint *timeout);
	QString getOutputResolution();
	QString getOutputFps();
	bool isSupportedResolutionFPS(QString &outTipString);
	void requestLiveDirectEnd();
	void getWatermarkInfo(QString &guide, bool &enabled, bool &selected);
	bool isValidWatermark();
	bool isValidOutro();

signals:
	void retriveImagefinished();

private:
	QVariantMap getWatermarkMap(QString &platformKey);
	QVariantMap getOutroInfoMap(QString &platformKey);
	void updateWatermark();
	void clearWatermark();
	obs_watermark_policy showTypeByString(const QString &typeString);
	void startThumnailRequest();
	void uploadThumbnailToRemote();
	QHttpPart getHttpPart(const QString &key, const QString &body);
	void onReplyErrorData(int statusCode, const QString &url, const QString &body, const QString &errorInfo);
	bool isLandscape();

private:
	PLSBasic *main;
	QString m_thumbnailURL;
};

#endif // PLSSERVERSTREAMHANDLER_H
