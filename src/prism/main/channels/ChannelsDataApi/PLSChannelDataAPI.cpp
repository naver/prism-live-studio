#include "PLSChannelDataAPI.h"
#include <QFile>
#include <QImageReader>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QUUid>
#include <algorithm>
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "LogPredefine.h"
#include "PLSChannelDataHandler.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSChannelsEntrance.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "pls-gpop-data.hpp"
#include "ui-config.h"
using namespace ChannelData;

PLSChannelDataAPI *PLSChannelDataAPI::mInstance = nullptr;

PLSChannelDataAPI *PLSChannelDataAPI::getInstance()
{
	if (mInstance == nullptr) {
		mInstance = new PLSChannelDataAPI();
	}
	return mInstance;
}

void PLSChannelDataAPI::beginTransactions(const QVariantMap &varMap)
{
	mTransactions = varMap;
}

void PLSChannelDataAPI::addTransaction(const QString &key, const QVariant &value)
{
	mTransactions.insert(key, value);
}

void PLSChannelDataAPI::addTask(const QVariant &function, const QVariant &parameters)
{
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();

	QVariantMap taskMap;
	taskMap.insert(ChannelTransactionsKeys::g_functions, function);
	taskMap.insert(ChannelTransactionsKeys::g_taskParameters, parameters);
	tasks.append(taskMap);

	mTransactions.insert(ChannelTransactionsKeys::g_taskQueue, tasks);
}

int PLSChannelDataAPI::currentTransactionCMDType()
{
	return getInfo(mTransactions, ChannelTransactionsKeys::g_CMDType, ChannelTransactionsKeys::NotDefineCMD);
}

int PLSChannelDataAPI::currentTaskQueueCount()
{
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	return tasks.count();
}

void PLSChannelDataAPI::endTransactions()
{
	if (mTransactions.isEmpty()) {
		return;
	}

	PLSCHANNELS_API->sigAllChannelRefreshDone();

	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	if (tasks.isEmpty()) {
		mTransactions.clear();
		return;
	}
	mTransactions.clear();
	for (const auto &var : tasks) {
		auto varMap = var.toMap();
		auto func = varMap.value(ChannelTransactionsKeys::g_functions).value<TaskFun>();
		if (func != nullptr) {
			auto parameters = varMap.value(ChannelTransactionsKeys::g_taskParameters);
			auto context = varMap.value(ChannelTransactionsKeys::g_context, QVariant::fromValue(this)).value<QObject *>();
			auto connectType = (context == this ? Qt::DirectConnection : Qt::QueuedConnection);
			QMetaObject::invokeMethod(
				context, [=]() { func(parameters); }, connectType);
		}
	}
}

void PLSChannelDataAPI::release()
{

	mSemap.release(1);
}

void PLSChannelDataAPI::acquire()
{

	mSemap.acquire(1);

	if (mSemap.available() == 0) {
		emit lockerReleased();
	}
}

bool PLSChannelDataAPI::isEmptyToAcquire()
{

	return mSemap.available() == 0;
}

void PLSChannelDataAPI::connectSignals()
{
	auto doUpdateAllRtmp = []() { updateAllRtmps(); };
	connect(this, &PLSChannelDataAPI::sigUpdateAllRtmp, this, doUpdateAllRtmp, Qt::QueuedConnection);
	auto doRefreshAllChannels = []() { refreshAllChannels(); };
	connect(this, &PLSChannelDataAPI::sigRefreshAllChannels, this, doRefreshAllChannels, Qt::QueuedConnection);

	auto tryAddChannel = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToAddChannel, this, tryAddChannel, Qt::QueuedConnection);

	auto tryUpdate = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToUpdateChannel, this, tryUpdate, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::sigTryRemoveChannel, this, &PLSChannelDataAPI::tryRemoveChannel, Qt::QueuedConnection);

	connect(this, &PLSChannelDataAPI::sigTrySetBroadcastState, this, &PLSChannelDataAPI::setBroadcastState, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::sigTrySetRecordState, this, &PLSChannelDataAPI::setRecordState, Qt::QueuedConnection);

	mSaveDelayTimer = new QTimer(this);
	mSaveDelayTimer->setSingleShot(true);
	connect(mSaveDelayTimer, &QTimer::timeout, this, &PLSChannelDataAPI::saveData, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelAdded, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelModified, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);

	auto handleExpired = [](const QString &uuid, bool toAsk) { resetExpiredChannel(uuid, toAsk); };
	connect(this, &PLSChannelDataAPI::channelExpired, qApp, handleExpired, Qt::QueuedConnection);
	auto handlePrismExpired = []() { reloginPrismExpired(); };
	connect(this, &PLSChannelDataAPI::prismTokenExpired, qApp, handlePrismExpired, Qt::QueuedConnection);

	//in shifting
	connect(PLS_PLATFORM_API, &PLSPlatformApi::livePrepared, this, [=](bool isOk) {
		if (isOk) {
			this->setBroadcastState(CanBroadcastState);
			return;
		}
		this->setBroadcastState(ReadyState);
		return;
	});
	//in obs living
	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, this, [=](bool isOk) {
		emit holdOnGolive(false);
		if (!isOk && this->isLiving()) {
			this->setBroadcastState(StopBroadcastGo);
		}
	});
	//in end view
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, PLS_PLATFORM_API,
		[=](bool, bool apiStarted) {
			if (apiStarted && !PLS_PLATFORM_API->isStopForExit()) {
				PLS_PLATFORM_API->showEndView(false);
			}
		},
		Qt::DirectConnection);
	//refresh ui
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEndedForUi, this, [=]() { this->setBroadcastState(StreamEnd); }, Qt::DirectConnection);

	//stop living act
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveToStop, this, [=]() { this->setBroadcastState(CanBroadcastStop); }, Qt::QueuedConnection);

	auto gotoAsk = [](const QString &uuid) { handleEmptyChannel(uuid); };
	connect(this, &PLSChannelDataAPI::channelGoToInitialize, qApp, gotoAsk, Qt::QueuedConnection);

	connect(
		this, &PLSChannelDataAPI::rehearsalBegin, this, [=]() { this->setRehearsal(true); }, Qt::QueuedConnection);
}

void PLSChannelDataAPI::registerEnumsForStream()
{

	qRegisterMetaTypeStreamOperators<ChannelStatus>("ChannelStatus");
	qRegisterMetaTypeStreamOperators<ChannelUserStatus>("ChannelUserStatus");
	qRegisterMetaTypeStreamOperators<ChannelDataType>("ChannelDataType");
	qRegisterMetaTypeStreamOperators<LiveState>("LiveState");
	qRegisterMetaTypeStreamOperators<RecordState>("RecordState");

	qRegisterMetaType<TaskFun>();
}

PLSChannelDataAPI::PLSChannelDataAPI(QObject *parent)
	: QObject(parent),
	  mNetWorkAPI(new QNetworkAccessManager(this)),
	  mSemap(0),
	  mInfosLocker(QReadWriteLock::Recursive),
	  mFileLocker(QReadWriteLock::Recursive),
	  mBroadcastState(ReadyState),
	  mRecordState(mRecordState),
	  mIsOnlyStopRecording(false),
	  mInitilized(false),
	  mIsExit(false),
	  mIsResetNeed(false),
	  mIsRehearsal(false)
{
	mInstance = this;
	registerEnumsForStream();

	this->setBroadcastState(ReadyState);
	this->setRecordState(RecordReady);
}

PLSChannelDataAPI ::~PLSChannelDataAPI()
{
#ifdef DEBUG
	saveTranslations();
#endif // DEBUG
}

//add source path image to map,and scale default size image
QPixmap PLSChannelDataAPI::addImage(const QString &srcPath, const QPixmap &tmp, const QSize &defaultSize)
{
	QVariantMap imageMap;

	ImagesContainer images;
	QPixmap target;
	auto srcSize = tmp.size();
	//if source image size is not equal to target size ,scale to
	if (srcSize != defaultSize) {
		target = tmp.scaled(defaultSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		images.insert(defaultSize, target);
	} else {
		target = tmp;
	}
	images.insert(tmp.size(), tmp);

	imageMap.insert(g_imageSize, tmp.size());
	imageMap.insert(g_imageCache, QVariant::fromValue(images));

	auto fileKey = QFileInfo(srcPath).fileName();
	mImagesCache.insert(fileKey, imageMap);
	return target;
}

//get or load target size image
QPixmap PLSChannelDataAPI::getImage(const QString &srcPath, const QSize &defaultSize)
{
	auto fileKey = QFileInfo(srcPath).fileName();
	QVariantMap imageMap = mImagesCache.value(fileKey);
	auto images = imageMap.value(g_imageCache).value<ImagesContainer>();

	auto tmp = images.value(defaultSize);
	if (!tmp.isNull()) {
		return tmp;
	}

	//to find original image  return null if source image not exists!
	auto srcSize = imageMap.value(g_imageSize).value<QSize>();
	auto srcImage = images.value(srcSize);
	if (srcImage.isNull()) {
		return tmp;
	}

	//scale to target size
	tmp = srcImage.scaled(defaultSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	images.insert(defaultSize, tmp);
	imageMap.insert(g_imageCache, QVariant::fromValue(images));
	mImagesCache.insert(fileKey, imageMap);

	return tmp;
}

QPixmap PLSChannelDataAPI::updateImage(const QString &oldPath, const QString &srcPath, const QSize &size)
{
	if (srcPath.isEmpty()) {
		return QPixmap();
	}
	if (!oldPath.isEmpty() && oldPath != srcPath) {
		removeImage(oldPath);
	}

	return updateImage(srcPath, size);
}

// update and scale to target size image ,null image returned  if load failed
QPixmap PLSChannelDataAPI::updateImage(const QString &srcPath, const QSize &size)
{
	QFileInfo fInfo(srcPath);
	QVariantMap imageMap = mImagesCache.value(fInfo.fileName());
	QPixmap tmp;
	if (!imageMap.isEmpty()) {
		if (fInfo.isWritable()) {
			QFile::remove(srcPath);
		}
		// to get target size image
		return getImage(srcPath, size);
	}

	//try to load source image
	loadPixmap(tmp, srcPath, size);

	if (fInfo.isWritable()) {
		QFile::remove(srcPath);
	}
	//if load fail return null
	if (tmp.isNull()) {
		return tmp;
	}
	//add and return loaded image
	return addImage(srcPath, tmp, size);
}

bool PLSChannelDataAPI::isImageExists(const QString &srcPath)
{
	QFileInfo info(srcPath);
	return mImagesCache.contains(info.fileName());
}

void PLSChannelDataAPI::removeImage(const QString &srcPath)
{
	QFileInfo info(srcPath);
	mImagesCache.remove(info.fileName());
	QFile::remove(srcPath);
}

void PLSChannelDataAPI::addChannelInfo(const QVariantMap &Infos, bool notify)
{
	//QString msg = " platform " +getInfo(Infos, g_platformName) + " ,display name :" + getInfo(Infos, g_displayLine1);
	//PRE_LOG_MSG_STEP(msg, g_addChannelStep, INFO);
	QString channelUUID = getInfo(Infos, g_channelUUID);

	int dataType = getInfo(Infos, g_data_type, DownloadType);
	switch (dataType) {
	case ChannelType:
	case RTMPType: {
		QWriteLocker locker(&mInfosLocker);
		mChannelInfos[channelUUID] = Infos;
		mChannelInfos[channelUUID][g_isPresetRTMP] = PLSGpopData::instance()->isPresetRTMP(getInfo(Infos, g_channelRtmpUrl));

	} break;

	default:
		return;
		break;
	}
	auto imagePath = getInfo(Infos, g_userIconCachePath);
	if (!imagePath.isEmpty()) {
		updateImage(imagePath);
	}
	if (notify) {
		if (PLSCHANNELS_API->isInitilized()) {
			emit addingHold(true);
		}
		emit sigTryToAddChannel(channelUUID);
	}
}

void PLSChannelDataAPI::backupInfo(const QString &uuid)
{
	auto ite = mChannelInfos.find(uuid);
	if (ite != mChannelInfos.end()) {
		mBackupInfos.insert(uuid, mChannelInfos.value(uuid));
	}
}

void PLSChannelDataAPI::recoverInfo(const QString &uuid)
{
	auto ite = mBackupInfos.find(uuid);
	if (ite != mBackupInfos.end()) {
		mChannelInfos[uuid] = ite.value();
	}
}

void PLSChannelDataAPI::clearBackup(const QString &uuid)
{
	mBackupInfos.remove(uuid);
}

void PLSChannelDataAPI::recordExpiredInfo(const QVariantMap &info)
{

	auto infoId = getInfo(info, g_channelUUID);
	if (infoId.isEmpty()) {
		return;
	}
	mExpiredChannels.insert(infoId, info);
}

void PLSChannelDataAPI::reCheckExpiredChannels()
{
	if (mExpiredChannels.isEmpty()) {
		return;
	}
	auto ite = mExpiredChannels.begin();
	while (ite != mExpiredChannels.end()) {
		const auto &info = ite.value();
		auto platformName = getInfo(info, g_platformName);
		this->removeChannelsByPlatformName(platformName, ChannelDataType::ChannelType, true, false);
		QMetaObject::invokeMethod(
			getMainWindow(), [=]() { reloginChannel(platformName, true); }, Qt::QueuedConnection);
		++ite;
	}
	mExpiredChannels.clear();
}

void PLSChannelDataAPI::resetData()
{
	for (auto handler : mPlatformHandler) {
		handler->resetData();
	}
}

void PLSChannelDataAPI::exitApi()
{
	resetData();
	mTransactions.clear();
	this->setExitState(true);
	this->blockSignals(true);
	this->saveData();
#ifdef DEBUG
	saveTranslations();
#endif // DEBUG
	this->thread()->exit();
}

void PLSChannelDataAPI::finishAdding(const QString &channelUUID)
{
	const auto info = this->getChannelInfo(channelUUID);
	const int myType = getInfo(info, g_data_type, NoType);
	bool isLeader = getInfo(info, g_isLeader, true);

	if (getInfo(info, g_channelStatus, InValid) == Valid && isLeader) {
		// exclusive band rtmp,v live, Now etc....
		exclusiveChannelCheckAndVerify(info);
		int childrenSelected = 0;
		if (myType == ChannelType) {
			childrenSelected = this->getCurrentSelectedPlatformChannels(getInfo(info, g_platformName), ChannelType).count();
			showResolutionTips(getInfo(info, g_platformName));
		} else {
			showResolutionTips(CUSTOM_RTMP);
		}

		if (childrenSelected == 0) {
			int currentSeleted = this->currentSelectedCount();
			if (currentSeleted < g_maxActiveChannels) {
				this->setChannelUserStatus(channelUUID, Enabled);
				this->setValueOfChannel(channelUUID, g_displayState, true);
			}
		}
		this->sortAllChannels();
	}

	emit channelAdded(channelUUID);
}

void PLSChannelDataAPI::moveToNewThread(QThread *newThread)
{
	connectSignals();
	this->moveToThread(newThread);
}

void PLSChannelDataAPI::setChannelInfos(const QVariantMap &Infos, bool notify)
{
	QVariantMap lastInfo;
	QString channelUUID = getInfo(Infos, g_channelUUID);
	{
		QWriteLocker locker(&mInfosLocker);
		auto ite = mChannelInfos.find(channelUUID);
		if (ite == mChannelInfos.end()) {
			return;
		}
		lastInfo = ite.value();
		*ite = Infos;
		(*ite)[g_isPresetRTMP] = PLSGpopData::instance()->isPresetRTMP(getInfo(Infos, g_channelRtmpUrl));
	}
	updateImage(getInfo(lastInfo, g_userIconCachePath), getInfo(Infos, g_userIconCachePath));
	if (notify) {
		emit channelModified(channelUUID);
	}
	int newUserStatus = getInfo(Infos, g_channelUserStatus, NotExist);
	if (getInfo(lastInfo, g_channelUserStatus, NotExist) != newUserStatus) {
		emit channelActiveChanged(channelUUID, newUserStatus == Enabled);
	}
}

bool PLSChannelDataAPI::isChannelInfoExists(const QString &channelUUID)
{
	QReadLocker locker(&mInfosLocker);
	return mChannelInfos.contains(channelUUID);
}

void PLSChannelDataAPI::addError(const QVariantMap &errorMap)
{
	mErrorStack.push(errorMap);
}

const QVariantMap PLSChannelDataAPI::takeFirstError()
{
	if (hasError()) {
		return mErrorStack.pop();
	}
	return QVariantMap();
}

bool PLSChannelDataAPI::isEmpty() const
{
	return mChannelInfos.isEmpty();
}

int PLSChannelDataAPI::count() const
{
	return mChannelInfos.count();
}

bool PLSChannelDataAPI::isLivingOrRecording()
{
	return (isRecording() || isLiving());
}

bool PLSChannelDataAPI::isLiving()
{
	return (currentBroadcastState() == StreamStarting) || (currentBroadcastState() == StreamStarted) || (currentBroadcastState() == StreamStopping);
}

bool PLSChannelDataAPI::isInWholeBroadCasting()
{
	return (currentBroadcastState() != ReadyState && currentBroadcastState() != RecordStopped);
}

bool PLSChannelDataAPI::isShifting()
{
	int liveState = this->currentBroadcastState();
	if (liveState == StopBroadcastGo || liveState == BroadcastGo || liveState == CanBroadcastState || liveState == StreamStarting || liveState == StreamStopping) {
		return true;
	}
	return false;
}

bool PLSChannelDataAPI::isRecording()
{
	return (currentReocrdState() == RecordStarting) || (currentReocrdState() == RecordStarted) || (currentReocrdState() == RecordStopping);
}

void PLSChannelDataAPI::switchStreaming()
{
	if (isLiving()) {
		emit toStopBroadcast();
		return;
	}
	emit toStartBroadcast();
}

void PLSChannelDataAPI::switchRecording()
{
	if (isRecording()) {
		emit toStopRecord();
		return;
	}
	emit toStartRecord();
}

bool PLSChannelDataAPI::isInWholeRecording()
{
	return (currentReocrdState() != RecordReady && currentReocrdState() != RecordStopped);
}

const ChannelsMap PLSChannelDataAPI::getCurrentSelectedChannels(int Type)
{
	QVariantMap searchmap;
	searchmap.insert(g_channelUserStatus, ChannelData::Enabled);
	if (Type != ChannelData::NoType) {
		searchmap.insert(g_data_type, Type);
	}

	return getMatchKeysInfos(searchmap);
}

int PLSChannelDataAPI::currentSelectedCount()
{
	auto isSelected = [](const QVariantMap &info) { return (getInfo(info, g_channelUserStatus, Disabled) == Enabled); };
	return std::count_if(mChannelInfos.begin(), mChannelInfos.end(), isSelected);
}

int PLSChannelDataAPI::currentSelectedValidCount()
{
	auto isSelected = [](const QVariantMap &info) {
		int channelState = getInfo(info, g_channelStatus, Error);
		bool isCanBroadcasting = channelState == Valid || channelState == Expired;
		return (getInfo(info, g_channelUserStatus, Disabled) == Enabled && isCanBroadcasting);
	};
	return std::count_if(mChannelInfos.begin(), mChannelInfos.end(), isSelected);
}

const ChannelsMap PLSChannelDataAPI::getCurrentSelectedPlatformChannels(const QString &platform, int srcType)
{
	ChannelsMap retMap;
	auto isMatched = [&](const QVariantMap &info) {
		int myType = getInfo(info, g_data_type, NoType);
		bool isTypeMatch = ((srcType == NoType) ? true : (myType == srcType));
		return getInfo(info, g_platformName).contains(platform) && getInfo(info, g_channelUserStatus, Disabled) == Enabled && isTypeMatch;
	};

	for (const auto &info : mChannelInfos) {
		if (isMatched(info)) {
			retMap.insert(getInfo(info, g_platformName), info);
		}
	}
	return retMap;
}

int PLSChannelDataAPI::getChannelStatus(const QString &channelUUID)
{
	return getValueOfChannel(channelUUID, g_channelStatus, InValid);
}

void PLSChannelDataAPI::setChannelStatus(const QString &channelUuid, int state)
{
	auto fun = [=]() {
		{
			QWriteLocker lokcer(&mInfosLocker);
			auto ite = mChannelInfos.find(channelUuid);
			if (ite == mChannelInfos.end()) {
				return;
			}
			if (state == getInfo(*ite, g_channelStatus, Error)) {
				return;
			}
			(*ite)[g_channelStatus] = state;
		}

		emit channelModified(channelUuid);
	};
	QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}

void PLSChannelDataAPI::setChannelUserStatus(const QString &channelUuid, int isActive, bool notify)
{
	auto fun = [=]() {
		{
			QWriteLocker lokcer(&mInfosLocker);
			auto ite = mChannelInfos.find(channelUuid);
			if (ite == mChannelInfos.end()) {
				return;
			}
			if (isActive == getInfo(*ite, g_channelUserStatus, NotExist)) {
				return;
			}

			(*ite)[g_channelUserStatus] = isActive;
			//QString msg = getInfo(*ite, g_platformName) + QString("channel to  ") + (isActive == Enabled ? " ON " : " OFF ");
			//PRE_LOG_MSG(msg.toStdString().c_str(), INFO);
		}
		if (notify) {
			emit channelActiveChanged(channelUuid, isActive == Enabled);
			emit channelModified(channelUuid);
		}
	};

	QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}

int PLSChannelDataAPI::getChannelUserStatus(const QString &channelUUID)
{
	return getValueOfChannel(channelUUID, g_channelUserStatus, NotExist);
}

const QVariantMap PLSChannelDataAPI::getChannelInfo(const QString &channelUUID)
{
	QReadLocker locker(&mInfosLocker);
	auto ite = mChannelInfos.find(channelUUID);
	if (ite != mChannelInfos.end()) {
		return *ite;
	}
	return QVariantMap();
}

QVariantMap &PLSChannelDataAPI::getChanelInfoRef(const QString &channelUUID)
{
	QReadLocker locker(&mInfosLocker);
	auto ite = mChannelInfos.find(channelUUID);
	if (ite != mChannelInfos.end()) {
		return *ite;
	}

	static QVariantMap empty;
	empty.clear();
	return empty;
}

QVariantMap &PLSChannelDataAPI::getChanelInfoRefByPlatformName(const QString &channelName, int type)
{
	QReadLocker locker(&mInfosLocker);
	auto isSameName = [&](const QVariantMap &info) {
		QString platFormName = getInfo(info, g_platformName);
		int channelT = getInfo(info, g_data_type, NoType);
		return (platFormName == channelName) && (channelT == type);
	};

	auto ret = std::find_if(mChannelInfos.begin(), mChannelInfos.end(), isSameName);
	if (ret != mChannelInfos.end()) {
		return ret.value();
	}

	static QVariantMap empty;
	empty.clear();
	return empty;
}

const QVariantMap &PLSChannelDataAPI::getChanelInfoRefBySubChannelID(const QString &subChannelID, const QString &channelName)
{
	QReadLocker locker(&mInfosLocker);
	auto isSameID = [&](const QVariantMap &info) {
		QString platFormName = getInfo(info, g_platformName);
		QString channelID = getInfo(info, ChannelData::g_subChannelId);
		return (platFormName == channelName) && (channelID == subChannelID);
	};

	auto ret = std::find_if(mChannelInfos.begin(), mChannelInfos.end(), isSameID);
	if (ret != mChannelInfos.end()) {
		return ret.value();
	}

	static QVariantMap empty;
	empty.clear();
	return empty;
}

InfosList PLSChannelDataAPI::getChanelInfosByPlatformName(const QString &channelName, int type)
{
	QReadLocker locker(&mInfosLocker);
	InfosList retLst;

	for (auto &info : mChannelInfos) {
		bool isTypeMatch = true;

		if (type != NoType) {
			int myType = getInfo(info, g_data_type, NoType);
			isTypeMatch = (type == myType);
		}
		if (getInfo(info, g_platformName) == channelName && isTypeMatch) {
			retLst << info;
		}
	}
	return retLst;
}

bool PLSChannelDataAPI::isChannelSelectedDisplay(const QString &uuid)
{
	auto ite = mChannelInfos.find(uuid);
	if (ite != mChannelInfos.end()) {
		return getInfo(ite.value(), g_displayState, true);
	}
	return true;
}

bool PLSChannelDataAPI::isPlatformEnabled(const QString &platformName)
{
	return mPlatformStates.value(platformName, true);
}

void PLSChannelDataAPI::updatePlatformsStates()
{
	QStringList platforms = getDefaultPlatforms();
	for (auto platform : platforms) {
		bool isEnbaled = true;
		if (platform.contains(VLIVE, Qt::CaseInsensitive)) {
			auto notice = PLSGpopData::instance()->getVliveNotice();
			isEnbaled = !notice.isNeedNotice;
		}
		mPlatformStates.insert(platform, isEnbaled);
	}
}

void PLSChannelDataAPI::updateRtmpGpopInfos()
{
	mRtmpPlatforms.clear();
	auto rtmps = PLSGpopData::instance()->getRtmpDestination();
	for (const auto &rtmpInfo : rtmps) {
		if (rtmpInfo.exposure) {
			mRtmps.insert(rtmpInfo.streamName, rtmpInfo.rtmpUrl);
			mRtmpPlatforms.append(rtmpInfo.streamName);
		}
	}
}

std::tuple<bool, bool> PLSChannelDataAPI::isPlatformBeSurportedByCurrentVersion(const QString &platform)
{
	std::tuple<bool, bool> surportInfo{true, false};

	const auto &tmps = PLSGpopData::instance()->getPlatformVersionInfo();
	auto it = findMatchKeyFromMap(tmps, platform, isStringEqual);
	if (it != tmps.cend()) {
		auto tmp = it->toMap();
		auto minVer = tmp.value("version").toString();
		if (isVersionLessthan(PLS_VERSION, minVer)) {
			std::get<0>(surportInfo) = false;
			std::get<1>(surportInfo) = tmp.value("needForceUpdate").toBool();
		}
	}
	return surportInfo;
}

const ChannelsMap PLSChannelDataAPI::getAllChannelInfo()
{
	QReadLocker locker(&mInfosLocker);
	return mChannelInfos;
}

ChannelsMap &PLSChannelDataAPI::getAllChannelInfoReference()
{
	QReadLocker locker(&mInfosLocker);
	return mChannelInfos;
}

void PLSChannelDataAPI::tryRemoveChannel(const QString &channelUUID, bool notify, bool notifyServer)
{
	PRE_LOG_MSG_STEP("begin remove channel", g_removeChannelStep, INFO);
	auto info = this->getChannelInfo(channelUUID);
	if (info.isEmpty()) {
		PRE_LOG_MSG_STEP("channel has been removed ", g_removeChannelStep, WARN);
		return;
	}
	auto platformName = getInfo(info, g_platformName);
	bool isMultiChildren = this->isPlatformMultiChildren(platformName);
	int myType = getInfo(info, g_data_type, NoType);
	if (isMultiChildren && myType == ChannelType) {

		this->removeChannelsByPlatformName(platformName, myType, notify, notifyServer);
	} else {
		this->removeChannelInfo(channelUUID, notify, notifyServer);
	}
}

void PLSChannelDataAPI::removeChannelInfo(const QString &channelUUID, bool notify, bool notifyServer)
{
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	if (channelUUID.isEmpty()) {
		return;
	}

	QVariantMap tmpInfo;
	{
		QReadLocker lokcer(&mInfosLocker);
		tmpInfo = mChannelInfos.value(channelUUID);
	}

	int channelT = getInfo(tmpInfo, g_data_type, NoType);
	if (channelT == RTMPType && notifyServer) {

		RTMPDeleteToPrism(channelUUID);
		return;
	}

	int count = 0;
	{
		QWriteLocker locker(&mInfosLocker);
		count = mChannelInfos.remove(channelUUID);
	}
	auto platformName = getInfo(tmpInfo, g_platformName, QString(" empty "));

	QString msg = " End remmove channel " + channelUUID + " platform: " + platformName;
	PRE_LOG_MSG_STEP(msg, g_removeChannelStep, INFO);
	if (count > 0 && notify) {
		emit channelRemoved(channelUUID);
	}

	if (channelT == ChannelType) {
		auto imagePath = getInfo(tmpInfo, g_userIconCachePath);
		removeImage(imagePath);
	}
}

void PLSChannelDataAPI::removeChannelsByPlatformName(const QString &platformName, int type, bool notify, bool notifyServer)
{
	auto infos = getChanelInfosByPlatformName(platformName, type);
	for (auto &info : infos) {
		this->removeChannelInfo(getInfo(info, g_channelUUID), notify, notifyServer);
	}
}

QStringList PLSChannelDataAPI::getCurrentSortedChannelsUUID()
{
	auto lessThan = [&](const QString &left, const QString &right) {
		int leftIndex = getValueOfChannel(left, g_displayOrder, -1);
		int rightIndex = getValueOfChannel(right, g_displayOrder, -1);
		return leftIndex < rightIndex;
	};
	QStringList retLst = mChannelInfos.keys();
	std::sort(retLst.begin(), retLst.end(), lessThan);

	return retLst;
}

InfosList PLSChannelDataAPI::sortAllChannels()
{
	auto infos = mChannelInfos.values();

	auto isLessThan = [](const QVariantMap &left, const QVariantMap &right) {
		int ltype = getInfo(left, g_data_type, NoType);
		int rtype = getInfo(right, g_data_type, NoType);

		if (ltype != rtype) {
			return ltype < rtype;
		}

		if (ltype == RTMPType) {
			auto ltime = getInfo(left, g_createTime, QDateTime::currentDateTime());
			auto rtime = getInfo(right, g_createTime, QDateTime::currentDateTime());
			return ltime > rtime;
		}

		QString lplatform = getInfo(left, g_platformName);
		QString rplatform = getInfo(right, g_platformName);
		if (lplatform != rplatform) {
			return isPlatformOrderLessThan(lplatform, rplatform);
		}

		auto lname = getInfo(left, g_displayLine1);
		auto rname = getInfo(right, g_displayLine1);
		return QString::localeAwareCompare(lname, rname) < 0;
	};

	std::sort(infos.begin(), infos.end(), isLessThan);

	InfosList ret;
	for (int i = 0; i < infos.count(); ++i) {
		const auto &info = infos[i];
		auto uuid = getInfo(info, g_channelUUID);
		ret << info;
		this->setValueOfChannel(uuid, g_displayOrder, i);
	}

	return ret;
}

void PLSChannelDataAPI::clearAll()
{
	emit sigAllClear();
	mChannelInfos.clear();
	mImagesCache.clear();
}

void PLSChannelDataAPI::saveData()
{
	if (this->isInitilized()) {
		QReadLocker infoLokcer(&mInfosLocker);
		QWriteLocker lokcer(&mFileLocker);
		try {
			saveDataXToFile(mChannelInfos, getChannelCacheFilePath());
		} catch (...) {
			PRE_LOG(Save error, ERROR);
		}
	}
}

void PLSChannelDataAPI::delaySave()
{
	if (this->isInitilized()) {
		mSaveDelayTimer->start(1000);
	}
}

void verifyRename(ChannelsMap &src)
{
	std::for_each(src.begin(), src.end(), [](QVariantMap &info) {
		int state = getInfo(info, g_channelStatus, Error);
		if (state != Valid) {
			info[g_channelStatus] = Valid;
		}
		info.remove(g_shareUrlTemp);
	});
}

void PLSChannelDataAPI::reloadData()
{
	QString cachePath = getChannelCacheFilePath();
	if (QFile::exists(cachePath)) {
		ChannelsMap tmp;
		loadDataFromFile(tmp, cachePath);
		if (!tmp.isEmpty()) {
			mChannelInfos = std::move(tmp);
			verifyRename(mChannelInfos);
		}
	}
}

void PLSChannelDataAPI::setRecordState(int state)
{
	PRE_LOG_MSG(QString("Record state change from %1  to %2").arg(RecordStatesMap[mRecordState]).arg(RecordStatesMap[state]), INFO);

	if (mRecordState == state) {
		return;
	}
	auto lastState = mRecordState;
	{
		mRecordState = state;
		emit recordingChanged(state);
	}

	switch (state) {
	case RecordReady: {

		emit recordReady();
	} break;
	case CanRecord: {

		if (!startRecordCheck()) {
			this->setRecordState(RecordReady);
			return;
		}
		if (!toRecord()) {
			this->setRecordState(RecordReady);
			return;
		}
		emit canRecord();
	} break;
	case RecordStarting: {

		emit recordStarting();
	} break;
	case RecordStarted: {

		emit recordStarted();
	} break;
	case RecordStopGo: {

		if (!stopRecordCheck()) {
			this->setRecordState(RecordStarted);
			return;
		}
		if (!toTryStopRecord()) {
			this->setRecordState(RecordStarted);
			return;
		}
		emit recordStopGo();
	} break;
	case RecordStopping: {
		emit recordStopping();
	} break;
	case RecordStopped: {

		if (lastState == RecordStopping && !PLS_PLATFORM_API->isStopForExit()) {
			auto end = [=]() { PLS_PLATFORM_API->showEndView(true); };
			QTimer::singleShot(100, qApp, end);
		}
		this->setRecordState(RecordReady);
		emit recordStopped();
	} break;
	default:
		break;
	} //end switch
}

int PLSChannelDataAPI::currentReocrdState() const
{
	return mRecordState;
}

void PLSChannelDataAPI::setBroadcastState(int event)
{
	int lastState = mBroadcastState;
	PRE_LOG_MSG(QString("Broadcast state change from %1  to %2").arg(LiveStatesMap[lastState]).arg(LiveStatesMap[event]), INFO);

	if (event == mBroadcastState) {
		return;
	} else {
		mBroadcastState = event;
		emit liveStateChanged(event);
	}

	switch (event) {
	case ReadyState: {

		emit holdOnGolive(false);
		emit holdOnChannelArea(false);
		this->setRehearsal(false);
	} break;
	case BroadcastGo: {

		emit holdOnGolive(true);

		if (!startStreamingCheck()) {
			this->setBroadcastState(ReadyState);
			return;
		}

		emit broadcastGo();
	} break;
	case CanBroadcastState: {

		if (!toGoLive()) {
			this->setBroadcastState(ReadyState);
			return;
		}
		emit canBroadcast();
	} break;
	case StreamStarting: {
		if (lastState != CanBroadcastState) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit toBeBroadcasting();
	} break;
	case StreamStarted: {
		emit holdOnGolive(false);
		emit holdOnChannelArea(false);
		emit broadcasting();
	} break;
	case StopBroadcastGo: {
		emit holdOnGolive(true);
		if (!stopStreamingCheck()) {
			this->setBroadcastState(StreamStarted);
			return;
		}
		emit stopBroadcastGo();
	} break;
	case CanBroadcastStop: {

		toStopLive();
	} break;
	case StreamStopping: {
		if (lastState != CanBroadcastStop) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit stopping();
	} break;
	case StreamStopped: {

		if (lastState != StreamStopping) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit broadcastStopped();
	} break;
	case StreamEnd: {
		emit broadCastEnded();
		resetAllChannels();
		//saveData();
		this->setBroadcastState(ReadyState);
	} break;
	default:
		break;
	}
}

int PLSChannelDataAPI::currentBroadcastState() const
{
	return mBroadcastState;
}
void PLSChannelDataAPI::clearOldVersionImages()
{
	QDir dir(getChannelCacheDir());
	auto nameFilter = QImageReader::supportedImageFormats();
	QStringList nameFilterS;
	for (const auto &filter : nameFilter) {
		QString tmp = "*." + filter;
		nameFilterS << tmp;
	}
	auto files = dir.entryInfoList(nameFilterS, QDir::Files | QDir::NoDotAndDotDot);
	for (auto &info : files) {
		QFile::remove(info.absoluteFilePath());
	}
}

void PLSChannelDataAPI::resetInitializeState(bool toInitilized)
{
	mInitilized = toInitilized;
	if (toInitilized) {
		this->reCheckExpiredChannels();
		this->setBroadcastState(ChannelData::ReadyState);
		saveData();
		emit toDoinitialize();
		QMetaObject::invokeMethod(PLS_PLATFORM_API, "doChannelInitialized", Qt::QueuedConnection);
	}
}

void PLSChannelDataAPI::registerPlatformHandler(ChannelDataBaseHandler *handler)
{
	mPlatformHandler.insert(handler->getPlatformName().toUpper(), PlatformHandlerPtrs(handler));
}

PlatformHandlerPtrs PLSChannelDataAPI::getPlatformHandler(const QString &platformName)
{
	return mPlatformHandler.value(platformName.toUpper());
}

bool PLSChannelDataAPI::isPlatformMultiChildren(const QString &platformName)
{
	auto hanlder = getPlatformHandler(platformName);
	if (hanlder) {
		return hanlder->isMultiChildren();
	}
	return false;
}

bool PLSChannelDataAPI::isChannelMultiChildren(const QString &uuid)
{
	auto info = getChannelInfo(uuid);
	return isPlatformMultiChildren(getInfo(info, g_platformName));
}

#ifdef DEBUG
/*for develop use for get what to translate when debug*/

static QSet<QString> debugTranslations;
void PLSChannelDataAPI::RegisterStr(const QString &src)
{
	debugTranslations.insert(src);
}

void PLSChannelDataAPI::saveTranslations()
{
	QFile file(getChannelCacheDir() + "/trans.txt");
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream in(&file);
		for (const auto &tran : debugTranslations) {
			in << tran;
			in >> endl;
		}
		in.flush();
	}
}

#endif // DEBUG

void PLSChannelDataAPI::stopAll()
{
	if (currentBroadcastState() == StreamStarted) {
		toStopLive();
	}

	if (currentReocrdState() == RecordStarted) {
		toStopRecord();
	}
}

uint qHash(const QSize &keySize, uint seed)
{
	return qHash(keySize.width() + 1.0 / keySize.height(), seed);
}
