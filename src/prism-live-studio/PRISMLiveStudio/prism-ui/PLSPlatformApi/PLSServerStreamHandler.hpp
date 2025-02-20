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
	QString getOutputResolution(bool bVertical) const;
	QString getOutputFps() const;
	bool isSupportedResolutionFPS(QString &outTipString) const;
	QString getResolutionAndFpsInvalidTip(const QString& channeName) const;
	void checkChannelResolutionFpsValid(const QString &channelName, const QVariantMap &platformFPSMap, const QString &platformKey, bool &result, QList<QString> &platformList,
					    bool bVertical) const;
	bool isValidWatermark(const QString &platFormName) const;
	bool isValidOutro(const QString &platFormName) const;

private:
	PLSBasic *main;
};

#endif // PLSSERVERSTREAMHANDLER_H
