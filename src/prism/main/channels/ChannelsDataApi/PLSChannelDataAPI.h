#pragma once

#include <QObject>
#include "PLSChannelPublicAPI.h"
#include <QStack>
#include <QSemaphore>
#include <QMutex>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QDateTime>

class NetWorkAPI;

class PLSChannelDataAPI final : public QObject, public PLSChannelPublicAPI {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "PLSChannelPublicAPI.plugin")
	Q_INTERFACES(PLSChannelPublicAPI)
public:
	explicit PLSChannelDataAPI(QObject *parent = nullptr);
	~PLSChannelDataAPI();

	static PLSChannelDataAPI *getInstance();

	bool isOnlyRecordStop() { return IsOnlyStopRecording; };
	void setIsOnlyStopRecord(bool isOnly) { IsOnlyStopRecording = isOnly; };

	QSemaphore &getSourceSemaphore() { return mSemap; }
	void release();
	void acquire();
	bool tryAcquire() { return mSemap.tryAcquire(1); }
	bool isEmptyToAcquire();

	QReadWriteLock &getLock() { return mInfosLocker; }

	/*replace  only one channel info */
	void setChannelInfos(const QVariantMap &Infos, bool notify = true);

	/*add one channel*/
	void addChannelInfo(const QVariantMap &Infos, bool notify = true) override;

	void backupInfo(const QString &uuid);

	void recoverInfo(const QString &uuid);

	void clearBackup(const QString &uuid);

	/* check if channel exists */
	bool isChannelInfoExists(const QString &);

	void addError(const QVariantMap &errorMap);
	const QVariantMap takeFirstError();
	bool hasError() { return !mErrorStack.isEmpty(); }
	void clearAllErrors() { mErrorStack.clear(); }
	/* check if no channels */
	bool isEmpty() const;

	/*count of channels */
	int count() const;

	/*current selected channels */
	const ChannelsMap getCurrentSelectedChannels();

	/*get current selected channels number*/
	int currentSelectedCount();

	int currentSelectedValidCount();

	/*get the channel of given name */
	const QVariantMap getChannelInfo(const QString &channelUUID);

	/* get the channel ref of given name for no copy */
	QVariantMap &getChanelInfoRef(const QString &channelUUID);

	QVariantMap &getChanelInfoRefByPlatformName(const QString &channelName, int type = 0);

	/*to check the status of channel */
	int getChannelStatus(const QString &channelUUID);
	void setChannelStatus(const QString &uuid, int state);

	/*to get channel user state if enabled */
	void setChannelUserStatus(const QString &channelUuid, int state);
	int getChannelUserStatus(const QString &channelUUID);

	template<typename ValueType> inline bool setValueOfChannel(const QString &channelUUID, const QString &key, const ValueType &value)
	{
		QWriteLocker lokcer(&mInfosLocker);
		auto ite = mChannelInfos.find(channelUUID);
		if (ite != mChannelInfos.end()) {
			(*ite)[key] = value;
			return true;
		}
		return false;
	}

	template<typename ValueType> inline auto getValueOfChannel(const QString &channelUUID, const QString &key, const ValueType &defaultValue = ValueType()) -> ValueType
	{
		QReadLocker lokcer(&mInfosLocker);
		auto ite = mChannelInfos.find(channelUUID);
		if (ite != mChannelInfos.end()) {
			return getInfo(*ite, key, defaultValue);
		}
		return defaultValue;
	}

	/*get all channels info for copy */
	const ChannelsMap getAllChannelInfo() override;

	/* get all channels info for reference */
	ChannelsMap &getAllChannelInfoReference();

	/* delete channel info of name ,the widget will be deleted later */
	void removeChannelInfo(const QString &channelUUID, bool notify = true, bool notifyServer = true);

	/*get the sorted order of channels */
	QStringList getCurrentSortedChannelsUUID();

	/*delete all channels info and widgets will be deleted later */
	void clearAll();

	void clearAllRTMPs();

	void abortAll();

	/* save channels data*/
	void saveData();

	/* load channels data of uer */
	void reloadData();

	/*just for  using default data */
	void createDefaultData();

	//click stop record button to stop record;
	uint getIsClickToStopRecord() { return m_isClickToStopRecord; };
	void setIsClickToStopRecord(bool value) { m_isClickToStopRecord = value; };

	/*get channel active status*/
	bool isLivingOrRecording();
	bool isLiving();
	bool isShifting();
	bool isRecording();

	bool isInWholeBroadCasting();
	bool isInWholeRecording();

	/* check if recording */
	void setRecordState(int state);
	int currentReocrdState() const;

	/*set and get current broadcast */
	void setBroadcastState(int state);
	int currentBroadcastState() const;

	void resetInitializeState(bool);
	bool isInitilized() { return Initilized; }

	/*send request of handler */
	void sendRequest(const QString &channelUUID);

	/*get network*/
	NetWorkAPI *getNetWorkAPI() { return mNetWorkAPI; }

	/*to get token of channel*/
	const QString getTokenOf(const QString &channelUUID);

	/* get channel network handler */
	QVariant getHandler(const QString &channelUUID);

	/*add channel network handler */
	void addHandler(QVariant handler);
	void removeHandler(const QString &uuid);

	void updateRtmpInfos();

	const QStringList &getRTMPsName() { return mPlatforms; }

	const QMap<QString, QString> &getRTMPInfos() { return mRtmps; }

#ifdef DEBUG
	/*for develop use for get what to translate when debug*/
	void RegisterStr(const QString &);

	void saveTranslations();

#endif // DEBUG

public slots:

	void stopAll();
	void exitApi();
	void finishAdding(const QString &uuid);
	void moveToNewThread(QThread *newThread);

signals:

	void channelAdded(const QString &channelUUID);
	void channelRemoved(const QString &channelUUID);
	void channelModified(const QString &channelUUID);
	void channelExpired(const QString &channelUUID);
	void channelGoToInitialize(const QString &channelUUID);

	/*recording sigs*/
	void toStartRecord();
	void toStopRecord();
	void recordingChanged(int);

	void recordReady();
	void canRecord();
	void recordStarting();
	void recordStarted();
	void recordStopGo();
	void recordStopping();
	void recordStopped();
	/*recording end*/

	/*broadcast sigs*/
	void toStartBroadcast();
	void toStopBroadcast();

	void inReady();
	void broadcastGo(); //
	void canBroadcast();
	void toBeBroadcasting();
	void broadcasting();
	void stopBroadcastGo();
	void stopping();
	void broadcastStopped();
	void broadCastEnded();
	void liveStateChanged(int state);
	/*broadcast sigs end*/

	void channelActiveChanged(const QString &channelUUID, bool enable);

	void holdOnChannel(const QString &channelUUID, bool hold = true);

	void holdOnChannelArea(bool hold = true);

	void lockerReleased();

	void addingHold(bool hold = true);

	void holdOnGolive(bool);

	void sigAllClear();

	void toDoinitialize();

	void currentSelectedChanged();

	void networkInvalidOcurred();

	void prismTokenExpired();

	void sigSendRequest(const QString &uuid);

	void sigTryToAddChannel(const QString &uuid);

	void sigTryToUpdateChannel(const QString &uuid);

	void sigTryRemoveChannel(const QString &uuid, bool nofify = true, bool nofityServer = true);

	void sigUpdateAllRtmp();

	void sigRefreshAllChannels();

	//void sigSendRequest(const QString &uuid);

	void sigRefreshToken(const QString &uuid, bool isForce = false);

	void tokenRefreshed(const QString &uuid, int returnCode);

private slots:

	void handleNetTaskFinished(const QString &taskID);

	void tryToRefreshToken(const QString &uuid, bool isForce);

private:
	void connectNetwork();
	PLSChannelDataAPI(const PLSChannelDataAPI &) = delete;
	PLSChannelDataAPI &operator=(const PLSChannelDataAPI &) = delete;

private:
	ChannelsMap mChannelInfos; // all curretn channels info
	ChannelsMap mBackupInfos;

	QStack<QVariantMap> mErrorStack;
	QVariantMap mHandlers;
	NetWorkAPI *mNetWorkAPI;
	static PLSChannelDataAPI *mInstance;
	bool m_isClickToStopRecord = false; //click stop record button to stop record;
	bool Initilized;
	bool isRTMPUpdateOk;
	QSemaphore mSemap;
	QReadWriteLock mInfosLocker;
	int mBroadcastState;
	int mRecordState;

	QMap<QString, QString> mRtmps;
	QStringList mPlatforms;

	bool IsOnlyStopRecording;
};
#define PLSCHANNELS_API PLSChannelDataAPI::getInstance()

/***************************************************************/
/*
channels data store ChannelsMap map like json below

all keys is defined in file ChannelConst.h

ChannelsMap {

	twitch:{
	"uuid:"......
	"name":"twitch";
	"url":"www.twitch.......";
	........
	},

	facebook:{
	"name":"facebook";
	"url":"www.facebook.......";
	........
	},
	......

	twitter:{
	"name":"twitter";
	"url":"www.twitter.......";
	........
	}
}

*/
/*****************************************************************************/
