#pragma once

#include <QDateTime>
#include <QMutex>
#include <QObject>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QSemaphore>
#include <QSize>
#include <QStack>
#include <QTimer>
#include <QWriteLocker>
#include "ChannelDefines.h"
#include "PLSChannelPublicAPI.h"
#include "PLSDualOutputConst.h"
#include "PLSErrorHandler.h"
#include "pls-channel-const.h"

class QNetworkAccessManager;
class ChannelDataBaseHandler;
using PlatformHandlerPtrs = QSharedPointer<ChannelDataBaseHandler>;
using InfosList = QList<QVariantMap>;

using ImagesMap = QMap<QString, QVariantMap>;

using ImagesContainer = QHash<QSize, QPixmap>;
Q_DECLARE_METATYPE(ImagesContainer)

size_t qHash(const QSize &keySize, uint seed = 0);

using EndLivePair = QPair<QString, QPixmap>;
using EndLiveList = QList<EndLivePair>;

void updatePlatformViewerCount();

class PLSChannelDataAPI final : public QObject, public PLSChannelPublicAPI {
	Q_OBJECT
public:
	explicit PLSChannelDataAPI(QObject *parent = nullptr);
	~PLSChannelDataAPI() override = default;
	//add source path image to map,and scale default size image
	QPixmap addImage(const QString &srcPath, const QPixmap &pix, const QSize &defaultSize = QSize(100, 100));
	//get or load target size image
	QPixmap getImage(const QString &srcPath, const QSize &defaultSize = QSize(100, 100));

	QPixmap updateImage(const QString &oldPath, const QString &srcPath, const QSize &size = QSize(100, 100));
	QPixmap updateImage(const QString &srcPath, const QSize &size = QSize(100, 100));

	void runTaskInOtherThread(const ChannelData::TaskFunNoPara &task) const;

	bool isImageExists(const QString &srcPath) const;
	void removeImage(const QString &srcPath);

	void clearOldVersionImages() const;

	static PLSChannelDataAPI *getInstance();

	bool isOnlyRecordStop() const { return mIsOnlyStopRecording; };
	void setIsOnlyStopRecord(bool isOnly) { mIsOnlyStopRecording = isOnly; };

	void beginTransactions(const QVariantMap &varMap = QVariantMap());
	void addTransaction(const QString &key, const QVariant &value);

	//task will be called after refresh
	void addTask(const ChannelData::TaskFun &fun, QObject *context = nullptr);
	void addTask(const ChannelData::TaskFunNoPara &fun, QObject *context = nullptr);
	void addTask(const QVariant &function, const QVariant &parameters = QVariant(), QObject *context = nullptr, const QString &taskName = "");
	void addTask(const QVariantMap &task);

	int currentTransactionCMDType() const;
	int currentTaskQueueCount() const;
	int countTask(const QString &taskName) const;
	void endTransactions();

	QSemaphore &getSourceSemaphore() { return mSemap; }
	void release();
	void acquire();
	bool tryAcquire() { return mSemap.tryAcquire(1); }
	bool isEmptyToAcquire() const;

	QReadWriteLock &getLock() { return mInfosLocker; }

	/*replace  only one channel info */
	void setChannelInfos(const QVariantMap &Infos, bool notify = true, bool removeImage = false) override;

	bool replaceChannelInfo(const QString &channelUUID, QVariantMap &lastInfo, const QVariantMap &Infos);

	/*add one channel*/
	void addChannelInfo(const QVariantMap &Infos, bool notify = true) override;
	void updateChannelInfo(const QString &channelUUID, const QVariantMap &Infos);

	//for rollback data when failed
	void backupInfo(const QString &uuid);
	void recoverInfo(const QString &uuid);
	void clearBackup(const QString &uuid);

	//for hold expired channel and resolve later
	void recordExpiredInfo(const QVariantMap &info);

	/* check if channel exists */
	bool isChannelInfoExists(const QString &) override;

	void addError(const QVariantMap &errorMap);
	QVariantMap takeFirstError();
	bool hasError() const;
	void clearAllErrors();
	/* check if no channels */
	bool isEmpty() const override;

	/*count of channels */
	int count() const;

	/*current selected channels */
	ChannelsMap getCurrentSelectedChannels(int Type = 0);

	/*get current selected channels number*/
	int currentSelectedCount() const;
	//get current selected channel which can broadcast
	int currentSelectedValidCount() const;

	ChannelsMap getCurrentSelectedPlatformChannels(const QString &platform, int Type = 0) const;

	/*get the channel of given name */
	QVariantMap getChannelInfo(const QString &channelUUID) const override;

	/* get the channel ref of given name for no copy */
	QVariantMap &getChanelInfoRef(const QString &channelUUID);

	QVariantMap &getChanelInfoRefByPlatformName(const QString &channelName, int type = 0);
	const QVariantMap &getChanelInfoRefBySubChannelID(const QString &subChannelID, const QString &channelName);

	InfosList getChanelInfosByPlatformName(const QString &channelName, int type = 0);

	bool isChannelSelectedDisplay(const QString &uuid);

	bool isPlatformHasCountForEndView(const QString &platformName) const;
	EndLiveList getEndLiveList(const QVariantMap &info) const;

	//to check if the platform is enbaled
	bool isPlatformEnabled(const QString &platformName) const;
	void updatePlatformsStates();

	void updateRtmpGpopInfos();

	std::tuple<bool, bool> isPlatformBeSurportedByCurrentVersion(const QString &platform) const;

	/*to check the status of channel */
	int getChannelStatus(const QString &channelUUID) override;
	void setChannelStatus(const QString &uuid, int state);

	bool updateChannelState(const QString &channelUuid, int state);

	/*to get channel user state if enabled */
	void setChannelUserStatus(const QString &channelUuid, int state, bool notify = true);
	bool updateChannelUserState(const QString &channelUuid, int isActive);
	int getChannelUserStatus(const QString &channelUUID);

	//to set all kind values of channel
	template<typename ValueType> inline bool setValueOfChannel(const QString &channelUUID, const QString &key, const ValueType &value)
	{
		QWriteLocker lokcer(&mInfosLocker);
		auto ite = mChannelInfos.find(channelUUID);
		if (ite != mChannelInfos.end()) {
			auto &info = ite.value();
			info[key] = QVariant::fromValue(value);

			if (key == ChannelData::g_viewers || key == ChannelData::g_displayLine1) {
				updatePlatformViewerCount();
			}
			return true;
		}
		return false;
	}

	//to get all kind values of channel
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
	ChannelsMap getAllChannelInfo() override;

	/* get all channels info for reference */
	ChannelsMap &getAllChannelInfoReference();

	//remove channel routered by platform
	void tryRemoveChannel(const QString &channelUUID, bool notify = true, bool notifyServer = true);
	/* delete channel info of name ,the widget will be deleted later  it maybe RTMP */
	void removeChannelInfo(const QString &channelUUID, bool notify = true, bool notifyServer = true) override;
	int removeInfo(const QString &channelUUID);
	//delte channel by platform ,it may be channel logined
	void removeChannelsByPlatformName(const QString &platformName, int type = 0, bool notify = true, bool notifyServer = true);

	/*get the sorted order of channels */
	QStringList getCurrentSortedChannelsUUID();

	InfosList sortAllChannels();

	/*delete all channels info and widgets will be deleted later */
	void clearAll() override;

	bool isExit() const { return mIsExit; }
	void setExitState(bool isExi = true) { mIsExit = isExi; }

	/* load channels data of uer */
	void reloadData();

	/* click stop record button to stop record*/
	bool getIsClickToStopRecord() const { return m_isClickToStopRecord; };
	void setIsClickToStopRecord(bool value) { m_isClickToStopRecord = value; };

	/*get channel active status*/
	bool isLivingOrRecording() const;
	bool isPrepareLiving() const;
	bool isLiving() const;
	bool isShifting() const;
	bool isInEndingStream() const;
	bool isRecording() const;
	void switchStreaming();
	void switchRecording();

	bool isInWholeBroadCasting() const;
	bool isInWholeRecording() const;

	/* check if recording */
	void setRecordState(int state);
	int currentReocrdState() const;

	void setRehearsal(bool isRehearsal = true);
	bool isRehearsaling() const { return mIsRehearsal; }

	/*set and get current broadcast */
	void setBroadcastState(int state);
	int currentBroadcastState() const;

	void resetInitializeState(bool);
	bool isInitilized() const { return mInitilized; }

	bool isResetNeed() const { return mIsResetNeed; }
	void setResetNeed(bool isNeed = false) { mIsResetNeed = isNeed; }

	/*get network*/
	QNetworkAccessManager *getNetWorkAPI() { return mNetWorkAPI; }

	void registerPlatformHandler(ChannelDataBaseHandler *handler);
	PlatformHandlerPtrs getPlatformHandler(const QString &platformName) const;

	bool isPlatformMultiChildren(const QString &platformName) const;
	bool isChannelMultiChildren(const QString &uuid) const;

	const QStringList &getRTMPsName();

	const QMap<QString, QString> &getRTMPInfos();

	QStringList getUuidListOfRISTandSRT();

	void addRISTandSRT2RtmpServer();

	void getChannelCountOfOutputDirection(QStringList &horOutputList, QStringList &verOutputList);
	void setChannelDefaultOutputDirection();
	void setOutputDirectionWhenAddChannel(const QString &uuid);
	bool isCanSetDualOutput(const QString &uuid) const;
	void clearDualOutput();

public slots:

	void stopAll();
	void exitApi();
	void finishAdding(const QString &uuid);
	void connectUIsignals();
	/* save channels data*/
	void saveData();
	void delaySave();

	void onAllScheduleListUpdated() const;

	void startUpdateScheduleList() const;

signals:

	void channelAdded(const QString &channelUUID);
	void channelRemoved(const QString &channelUUID);
	void channelCreateError(const QString &channelUUID);
	void channelModified(const QString &channelUUID);
	void channelExpired(const QString &channelUUID, bool toAsk = true);
	void channelGoToInitialize(const QString &channelUUID);
	void addChannelForDashBord(const QString &uuid) const;
	void channelRemovedForCheckVideo(bool bLeader);
	void channelRemovedForChzzk();
	void channelRefreshEnd(const QString &platformName);
	/*recording sigs*/
	void toStartRecord();
	void toStopRecord();
	void recordingChanged(int);

	void sigTrySetRecordState(int);
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
	void toStopBroadcast(DualOutputType outputType = DualOutputType::All);
	void toStartBroadcastInInfoView();
	void toStartRehearsal();
	void toStopRehearsal();
	void rehearsalBegin();
	void liveTypeChanged();

	void sigTrySetBroadcastState(int state);

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

	void holdOnChannelArea(bool hold = true);

	void lockerReleased();

	void addingHold(bool hold = true);

	void holdOnGolive(bool);

	void sigAllClear();

	void toDoinitialize();

	void sigChannelAreaInialized();

	void networkInvalidOcurred();

	void prismTokenExpired(const PLSErrorHandler::RetData &data);

	void sigSendRequest(const QString &uuid);

	void sigTryToAddChannel(const QString &uuid);

	void sigTryToUpdateChannel(const QString &uuid);

	void sigTryRemoveChannel(const QString &uuid, bool nofify = true, bool nofityServer = true);

	void sigUpdateAllRtmp();

	void sigRefreshAllChannels();

	void sigRefreshToken(const QString &uuid, bool isForce = false);

	void tokenRefreshed(const QString &uuid, int returnCode);

	void sigOperationChannelDone();

	void startFailed();

	void sigB2BChannelEndLivingCheckExpired(const QString &channelName);

	void sigSetChannelDualOutput(const QString &uuid, ChannelData::ChannelDualOutput outputType);

private:
	void registerEnumsForStream() const;
	void connectSignals();
	void reCheckExpiredChannels();
	void resetData() const;
	ChannelsMap getMatchKeysInfos(const QVariantMap &keysMap);

	//delete function to copy
	PLSChannelDataAPI(const PLSChannelDataAPI &) = delete;
	PLSChannelDataAPI &operator=(const PLSChannelDataAPI &) = delete;

	//private:
	ChannelsMap mChannelInfos; // all curretn channels info
	ChannelsMap mBackupInfos;
	ChannelsMap mExpiredChannels;
	ImagesMap mImagesCache;

	mutable QReadWriteLock mErrorLock{QReadWriteLock::Recursive};
	QStack<QVariantMap> mErrorStack;

	QNetworkAccessManager *mNetWorkAPI;
	//flags to handle

	bool mIsOnlyStopRecording{false};  //when record is stopped by click button,the flag is set
	bool m_isClickToStopRecord{false}; //click stop record button to stop record
	bool mInitilized{false};           //to set data is initialized
	bool mIsExit{false};               //when app to be quit ,the flag is set
	bool mIsResetNeed{false};          //when refresh,the flag is set
	bool mIsRehearsal{false};          //when channel is rehearsal,the flag is set

	QSemaphore mSemap{0}; //semaphore for hold task count
	mutable QReadWriteLock mInfosLocker{QReadWriteLock::Recursive};
	QReadWriteLock mFileLocker{QReadWriteLock::Recursive};

	int mBroadcastState{ChannelData::ReadyState};
	int mRecordState{ChannelData::RecordReady};

	//update fro gpop
	QMap<QString, QString> mRtmps;
	QStringList mRtmpPlatforms;
	QMap<QString, bool> mPlatformStates;

	//use for update platform
	QMap<QString, PlatformHandlerPtrs> mPlatformHandler;

	QTimer *mSaveDelayTimer;
	QVariantMap mTransactions;
	static QThread *mPainterThread;
	static PLSChannelDataAPI *mInstance;
	static QThread *mDataThread;
};
#define PLSCHANNELS_API PLSChannelDataAPI::getInstance()

/***************************************************************/
/*
channels data store ChannelsMap map like json below

all keys is defined in file pls-channel-const.h

ChannelsMap {

	twitch:{
	"uuid:"......
	"name":"twitch"
	"url":"www.twitch......."
	........
	},

	facebook:{
	"name":"facebook"
	"url":"www.facebook......."
	........
	},
	......

	twitter:{
	"name":"twitter"
	"url":"www.twitter......."
	........
	}
}

*/
/*****************************************************************************/
