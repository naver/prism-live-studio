#ifndef PLSSERVERSTREAMHANDLER_H
#define PLSSERVERSTREAMHANDLER_H

#include <QObject>
#include <qhttpmultipart.h>

class PLSBasic;

constexpr auto NAVER_SHOPPING_HIGH_SLV_RESOLUTION_KEY = "navershoppingliveSlvHigh";
constexpr auto NAVER_SHOPPING_RESOLUTION_KEY = "navershoppinglive";

#define OUTRO_PATH QStringLiteral("outroPtah")
#define OUTRO_TEXT QStringLiteral("outrotext")

class PLSServerStreamHandler : public QObject {
	Q_OBJECT

public:
	static PLSServerStreamHandler *instance();
	~PLSServerStreamHandler() override;
	explicit PLSServerStreamHandler(QObject *parent = nullptr);
	QString getOutputResolution() const;
	QString getOutputFps() const;
	bool isSupportedResolutionFPS(QString &outTipString) const;
	QString getResolutionAndFpsInvalidTip(const QString& channeName) const;
	void checkChannelResolutionFpsValid(const QString &channelName, const QVariantMap &platformFPSMap, const QString &platformKey, bool &result, QList<QString> &platformList) const;
	void requestLiveDirectEnd() const;
	bool isValidWatermark(const QString &platFormName) const;
	bool isValidOutro(const QString &platFormName) const;

signals:
	void retriveImagefinished();

private:
	void startThumnailRequest() const;
	void uploadThumbnailToRemote() const;
	bool isLandscape() const;

	PLSBasic *main;
};

#endif // PLSSERVERSTREAMHANDLER_H
