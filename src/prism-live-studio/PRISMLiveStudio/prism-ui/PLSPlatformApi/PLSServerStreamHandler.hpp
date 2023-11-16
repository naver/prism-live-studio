#ifndef PLSSERVERSTREAMHANDLER_H
#define PLSSERVERSTREAMHANDLER_H

#include <QObject>
#include <qhttpmultipart.h>

class PLSBasic;

constexpr auto NAVER_SHOPPING_HIGH_SLV_RESOLUTION_KEY = "navershoppingliveSlvHigh";
constexpr auto NAVER_SHOPPING_RESOLUTION_KEY = "navershoppinglive";

class PLSServerStreamHandler : public QObject {
	Q_OBJECT

public:
	static PLSServerStreamHandler *instance();
	~PLSServerStreamHandler() override;
	explicit PLSServerStreamHandler(QObject *parent = nullptr);
	QString getOutputResolution() const;
	QString getOutputFps() const;
	bool isSupportedResolutionFPS(QString &outTipString) const;
	void checkChannelResolutionFpsValid(const QString &channelName, const QVariantMap &platformFPSMap, const QString &platformKey, bool &result, QList<QString> &platformList) const;
	void requestLiveDirectEnd() const;

signals:
	void retriveImagefinished();

private:
	void startThumnailRequest() const;
	void uploadThumbnailToRemote() const;
	bool isLandscape() const;

	PLSBasic *main;
};

#endif // PLSSERVERSTREAMHANDLER_H
