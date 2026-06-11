#ifndef CHANNELDATAHANDLER_H
#define CHANNELDATAHANDLER_H

#include <qjsonobject.h>
#include <QNetworkReply>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QVariantMap>
#include "liblog.h"
using ChannelsMap = QMap<QString, QVariantMap>;

using InfosList = QList<QVariantMap>;
using UpdateCallback = std::function<void(const InfosList &)>;
Q_DECLARE_METATYPE(UpdateCallback)

struct FinishTaskReleaser {
private:
	Q_DISABLE_COPY(FinishTaskReleaser)
public:
	explicit FinishTaskReleaser(const QString &srcUUID) : mSrcUUID(srcUUID) {};
	~FinishTaskReleaser();

private:
	QString mSrcUUID;
};

class ChannelDataBaseHandler : public QObject {
	Q_OBJECT
public:
	explicit ChannelDataBaseHandler();
	~ChannelDataBaseHandler() override = default;
	//platform name ,eg. twitch,youtube,just key ,only string define in pls-channel-const.h
	virtual QString getPlatformName() { return ""; }
	virtual bool isMultiChildren() { return false; }
	// for some platfrom ,get mapper for json data
	virtual void initialization() { return; };
	//update data for channel when refresh
	virtual bool tryToUpdate(const QVariantMap &, const UpdateCallback &) { return false; };

	//to set how to display on channel UI
	virtual void updateDisplayInfo(InfosList &srcList);

	enum class ResetReson { All, EndLiveReset, RefreshReset };
	// remove or reset data before app exit,before refresh,endlive
	virtual void resetData(const ResetReson &reson = ResetReson::All);
	virtual void resetWhenLiveEnd();
	virtual void resetWhenRefresh();

	//login
	virtual void loginWithWebPage(const QString &cmdStr);

	virtual void showLiveInfo(const QString & /*uuid*/) {};

	virtual bool hasCountsForLiveEnd() { return false; };

	virtual QList<QPair<QString, QPixmap>> getEndLiveViewList(const QVariantMap &sourceData);

	//donwload image
	virtual void downloadImage();

signals:
	void preloginFinished(const QString &cmdStr);
	void loginFinished();
	void updateFinished();

	void liveInfoFinished(const QString &uuid, bool agree = true);

protected:
	//get data
	QVariantMap &myLastInfo() { return mLastInfo; }
	UpdateCallback &myCallback() { return mCallBack; }

	//must't call in construct fun
	void loadConfig();
	QVariantMap getMyDataMaper();

private:
	UpdateCallback mCallBack;
	QVariantMap mLastInfo;
	static ChannelsMap mDataMaper;
};

/* example */
class TwitchDataHandler : public ChannelDataBaseHandler {

public:
	TwitchDataHandler() = default;
	void initialization() override;
	QString getPlatformName() override;
	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback) override;
	virtual bool downloadHeaderImage(const QString &pixUrl);

private:
	bool getChannelsInfo();
};

class YoutubeHandler : public TwitchDataHandler {

public:
	YoutubeHandler() = default;
	QString getPlatformName() override;
	bool tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &callback) override;

	bool refreshToken();

	bool runTasks();

	void resetWhenLiveEnd() override { return; };

protected:
	bool getRealToken();
	bool getBasicInfo();
	void finishUpdateBasicInfo(const QVariantMap &jsonMap);
	bool getheaderImage();

	void handleError(int code, QByteArray data, QNetworkReply::NetworkError error, const QString &logFrom);
	void resetWhenRefresh() override;

private:
	QMap<long long, QVariant> mTaskMap;
};

#endif // ! CHANNELDATAHANDLER_H
