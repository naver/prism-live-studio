#include "PLSChannelDataAPI.h"
#include <qnetworkaccessmanager.h>
#include <quuid.h>
#include <QFile>
#include <QImageReader>
#include <QMainWindow>
#include <QPushButton>
#include <algorithm>
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSChannelDataHandler.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSChannelsEntrance.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSIPCHandler.h"
#include "PLSSyncServerManager.hpp"
#include "frontend-api.h"
#include "pls-gpop-data.hpp"
#include "pls/pls-dual-output.h"
#include "prism-version.h"
#include "ui-config.h"

using namespace ChannelData;

PLSChannelDataAPI *PLSChannelDataAPI::mInstance = nullptr;

QThread *PLSChannelDataAPI::mDataThread = nullptr;

PLSChannelDataAPI *PLSChannelDataAPI::getInstance()
{
	if (mInstance == nullptr) {
		mInstance = new PLSChannelDataAPI();
		mDataThread = new QThread();
		mInstance->moveToThread(mDataThread);
		mDataThread->start();
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

void PLSChannelDataAPI::addTask(const ChannelData::TaskFun &fun, QObject *context)
{
	QVariantMap taskMap;
	taskMap.insert(ChannelTransactionsKeys::g_functions, QVariant::fromValue(fun));
	taskMap.insert(ChannelTransactionsKeys::g_context, QVariant::fromValue(context));
	addTask(taskMap);
}

void PLSChannelDataAPI::addTask(const ChannelData::TaskFunNoPara &fun, QObject *context)
{
	QVariantMap taskMap;
	auto warpF = wrapFun(fun);
	taskMap.insert(ChannelTransactionsKeys::g_functions, QVariant::fromValue(warpF));
	taskMap.insert(ChannelTransactionsKeys::g_context, QVariant::fromValue(context));
	addTask(taskMap);
}

void PLSChannelDataAPI::addTask(const QVariant &function, const QVariant &parameters, QObject *context, const QString &taskName)
{
	if (context == nullptr) {
		context = this;
	}
	QVariantMap taskMap;
	taskMap.insert(ChannelTransactionsKeys::g_functions, function);
	taskMap.insert(ChannelTransactionsKeys::g_taskParameters, parameters);
	taskMap.insert(ChannelTransactionsKeys::g_context, QVariant::fromValue(context));
	taskMap.insert(ChannelTransactionsKeys::g_taskName, taskName);
	addTask(taskMap);
}

void PLSChannelDataAPI::addTask(const QVariantMap &taskMap)
{
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	tasks.append(taskMap);
	mTransactions.insert(ChannelTransactionsKeys::g_taskQueue, tasks);
}

int PLSChannelDataAPI::currentTransactionCMDType() const
{
	return (int)getInfo(mTransactions, ChannelTransactionsKeys::g_CMDType, ChannelTransactionsKeys::CMDTypeValue::NotDefineCMD);
}

int PLSChannelDataAPI::currentTaskQueueCount() const
{
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	return static_cast<int>(tasks.count());
}

int PLSChannelDataAPI::countTask(const QString &taskName) const
{
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	auto isTaskX = [&taskName](const QVariant &task) {
		auto taskMap = task.toMap();
		auto taskNameTmp = taskMap.value(ChannelTransactionsKeys::g_taskName).toString();
		return taskName == taskNameTmp;
	};
	return int(std::count_if(tasks.cbegin(), tasks.cend(), isTaskX));
}

void PLSChannelDataAPI::endTransactions()
{
	PLS_INFO("Channel", "end transactions");
	if (mTransactions.isEmpty()) {
		updatePlatformViewerCount();
		return;
	}

	PLSCHANNELS_API->sigOperationChannelDone();
	PLS_INFO("Channel", "Operation Channel Done");
	QVariantList tasks = mTransactions.value(ChannelTransactionsKeys::g_taskQueue).toList();
	if (tasks.isEmpty()) {
		mTransactions.clear();
		return;
	}
	mTransactions.clear();
	for (const auto &var : tasks) {
		auto varMap = var.toMap();
		auto func = varMap.value(ChannelTransactionsKeys::g_functions).value<TaskFun>();
		if (func == nullptr) {
			continue;
		}
		auto parameters = varMap.value(ChannelTransactionsKeys::g_taskParameters);
		auto context = varMap.value(ChannelTransactionsKeys::g_context, QVariant::fromValue(this)).value<QObject *>();
		if (context == nullptr) {
			context = this;
		}
		auto connectType = (context == this ? Qt::DirectConnection : Qt::QueuedConnection);
		QMetaObject::invokeMethod(
			context, [func, parameters]() { func(parameters); }, connectType);
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

bool PLSChannelDataAPI::isEmptyToAcquire() const
{

	return mSemap.available() == 0;
}

void PLSChannelDataAPI::connectSignals()
{
	auto doUpdateAllRtmp = []() { updateAllRtmps(); };
	connect(this, &PLSChannelDataAPI::sigUpdateAllRtmp, this, doUpdateAllRtmp, Qt::QueuedConnection);
	auto doRefreshAllChannels = []() { refreshAllChannels(); };
	connect(this, &PLSChannelDataAPI::sigRefreshAllChannels, this, doRefreshAllChannels, Qt::QueuedConnection);

	connect(PLS_PLATFORM_API, &PLSPlatformApi::allScheduleListUpdated, this, &PLSChannelDataAPI::onAllScheduleListUpdated, Qt::QueuedConnection);
	auto tryAddChannel = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToAddChannel, this, tryAddChannel, Qt::QueuedConnection);

	auto tryUpdate = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToUpdateChannel, this, tryUpdate, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::sigTryRemoveChannel, this, &PLSChannelDataAPI::tryRemoveChannel, Qt::QueuedConnection);

	connect(this, &PLSChannelDataAPI::sigTrySetBroadcastState, this, &PLSChannelDataAPI::setBroadcastState, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::sigTrySetRecordState, this, &PLSChannelDataAPI::setRecordState, Qt::QueuedConnection);

	connect(mSaveDelayTimer, &QTimer::timeout, this, &PLSChannelDataAPI::saveData, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelAdded, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelAdded, this, &PLSChannelDataAPI::startUpdateScheduleList, Qt::QueuedConnection);

	connect(this, &PLSChannelDataAPI::channelModified, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelDataAPI::startUpdateScheduleList, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::broadcastStopped, this, &PLSChannelDataAPI::startUpdateScheduleList, Qt::QueuedConnection);

	auto handleExpired = [](const QString &uuid, bool toAsk) { resetExpiredChannel(uuid, toAsk); };
	connect(this, &PLSChannelDataAPI::channelExpired, qApp, handleExpired, Qt::DirectConnection);
	auto handlePrismExpired = [](const PLSErrorHandler::RetData &data) { reloginPrismExpired(data); };
	connect(this, &PLSChannelDataAPI::prismTokenExpired, qApp, handlePrismExpired, Qt::QueuedConnection);

	auto handleB2BExpired = [](const QString &channelName) {
		PLS_INFO("Channel", "b2b token expired when in live, end living will logout");
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::CHANNEL_NCP_B2B_401_PREPARELIVE, channelName, "");
		pls_prism_change_over_login_view();
	};
	connect(this, &PLSChannelDataAPI::sigB2BChannelEndLivingCheckExpired, qApp, handleB2BExpired, Qt::QueuedConnection);
	//in shifting
	connect(PLS_PLATFORM_API, &PLSPlatformApi::livePrepared, this, [this](bool isOk) {
		if (isOk) {
			this->setBroadcastState(CanBroadcastState);
			return;
		}
		this->setBroadcastState(StreamEnd);
		return;
	});
	//in obs living
	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveStarted, this, [this](bool isOk) {
		emit holdOnGolive(false);
		if (!isOk && this->isLiving()) {
			this->setBroadcastState(StopBroadcastGo);
		}
	});
	//in end view
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, PLS_PLATFORM_API,
		[](bool, bool apiStarted) {
			PLS_INFO("Channels", "liveEnded signals parameter is apiStarted:%d, isStopForExit:%d", apiStarted, PLS_PLATFORM_API->isStopForExit());
			if (apiStarted && !PLS_PLATFORM_API->isStopForExit()) {
				PLS_PLATFORM_API->showEndView(false);
			}
			if (!apiStarted && !PLS_PLATFORM_API->isRecording()) {
				pls_toast_clear();
			}
		},
		Qt::DirectConnection);
	//refresh ui
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEndedForUi, this, [this]() { this->setBroadcastState(StreamEnd); }, Qt::DirectConnection);

	//stop living act
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveToStop, this, [this]() { this->setBroadcastState(CanBroadcastStop); }, Qt::QueuedConnection);

	auto gotoAsk = [](const QString &uuid) { handleEmptyChannel(uuid); };
	connect(this, &PLSChannelDataAPI::channelGoToInitialize, qApp, gotoAsk, Qt::QueuedConnection);

	connect(
		this, &PLSChannelDataAPI::rehearsalBegin, this, [this]() { this->setRehearsal(true); }, Qt::QueuedConnection);

	pls_connect(this, &PLSChannelDataAPI::sigSetChannelDualOutput, this, &PLSChannelDataAPI::delaySave, Qt::QueuedConnection);
}

void PLSChannelDataAPI::registerEnumsForStream() const
{
	qRegisterMetaType<ChannelStatus>("ChannelStatus");
	qRegisterMetaType<ChannelUserStatus>("ChannelUserStatus");
	qRegisterMetaType<ChannelDataType>("ChannelDataType");
	qRegisterMetaType<LiveState>("LiveState");
	qRegisterMetaType<RecordState>("RecordState");

	qRegisterMetaType<TaskFun>();
}

PLSChannelDataAPI::PLSChannelDataAPI(QObject *parent) : QObject(parent), mNetWorkAPI(new QNetworkAccessManager(this))

{
	mInstance = this;
	mSaveDelayTimer = new QTimer(this);
	mSaveDelayTimer->setSingleShot(true);
	registerEnumsForStream();

	this->setBroadcastState(ReadyState);
	this->setRecordState(RecordReady);
}

//add source path image to map,and scale default size image
QPixmap PLSChannelDataAPI::addImage(const QString &srcPath, const QPixmap &tmp, const QSize &defaultSize)
{
	QVariantMap imageMap;

	ImagesContainer images;
	QPixmap target;
	//if source image size is not equal to target size ,scale to
	if (tmp.size() != defaultSize) {
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
		//removeImage(oldPath)
	}

	return updateImage(srcPath, size);
}

// update and scale to target size image ,null image returned  if load failed
QPixmap PLSChannelDataAPI::updateImage(const QString &srcPath, const QSize &size)
{
	QFileInfo fInfo(srcPath);

	//don't remove this folder
	bool bRemove = true;
	QString tmpPath = QString("PRISMLiveStudio/resources/library/library_Policy_PC/images");
	tmpPath = pls_get_user_path(tmpPath);
	if (srcPath.contains(tmpPath, Qt::CaseInsensitive)) {
		bRemove = false;
	}

	QVariantMap imageMap = mImagesCache.value(fInfo.fileName());
	QPixmap tmp;
	if (!imageMap.isEmpty()) {
		if (fInfo.isWritable() && bRemove) {
			QFile::remove(srcPath);
		}
		// to get target size image
		return getImage(srcPath, size);
	}

	//try to load source image
	loadPixmap(tmp, srcPath, size);

	if (fInfo.isWritable() && bRemove) {
		QFile::remove(srcPath);
	}
	//if load fail return null
	if (tmp.isNull()) {
		return tmp;
	}
	//add and return loaded image
	return addImage(srcPath, tmp, size);
}

QThread *PLSChannelDataAPI::mPainterThread = nullptr;

void PLSChannelDataAPI::runTaskInOtherThread(const ChannelData::TaskFunNoPara &task) const
{
	if (mPainterThread == nullptr) {
		mPainterThread = new QThread();
		mPainterThread->moveToThread(mPainterThread);
		mPainterThread->start();
	}
	QMetaObject::invokeMethod(mPainterThread, task, Qt::QueuedConnection);
}

bool PLSChannelDataAPI::isImageExists(const QString &srcPath) const
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

	QString channelUUID = getInfo(Infos, g_channelUUID);

	switch (getInfo(Infos, g_data_type, DownloadType)) {
	case ChannelType:
	case RTMPType:
	case RISTType:
	case SRTType:
		updateChannelInfo(channelUUID, Infos);
		break;

	default:
		return;
	}

	if (auto imagePath = getInfo(Infos, g_userIconCachePath); !imagePath.isEmpty()) {
		updateImage(imagePath);
	}
	if (notify) {
		if (PLSCHANNELS_API->isInitilized()) {
			emit addingHold(true);
		}
		emit sigTryToAddChannel(channelUUID);
	}
}

void PLSChannelDataAPI::updateChannelInfo(const QString &channelUUID, const QVariantMap &Infos)
{

	QWriteLocker locker(&mInfosLocker);
	auto platName = getInfo(Infos, g_channelName);
	platName = toPlatformCodeID(platName, true);
	auto tmp = Infos;
	tmp[g_channelName] = platName;
	mChannelInfos[channelUUID] = tmp;
	mChannelInfos[channelUUID][g_isPresetRTMP] = PLSSyncServerManager::instance()->isPresetRTMP(getInfo(Infos, g_channelRtmpUrl));
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
		auto platformName = getInfo(info, g_channelName);
		auto retData = getInfo<PLSErrorHandler::RetData>(info, g_errorRetdata);
		this->removeChannelsByPlatformName(platformName, ChannelDataType::ChannelType, false, false);
		QMetaObject::invokeMethod(
			getMainWindow(), [platformName, retData]() { reloginChannel(platformName, true, retData); }, Qt::QueuedConnection);
		++ite;
	}
	mExpiredChannels.clear();
}

void PLSChannelDataAPI::resetData() const
{
	for (auto handler : mPlatformHandler) {
		handler->resetData(ChannelDataBaseHandler::ResetReson::EndLiveReset);
	}
}

void PLSChannelDataAPI::exitApi()
{
	resetData();
	mTransactions.clear();
	this->setExitState(true);
	this->blockSignals(true);
	this->saveData();

	if (mPainterThread) {
		mPainterThread->exit();
		mPainterThread->wait(10000);
	}
	if (mDataThread) {
		mDataThread->exit();
		mDataThread->wait(10000);
	}
}

void PLSChannelDataAPI::finishAdding(const QString &channelUUID)
{
	const auto info = this->getChannelInfo(channelUUID);
	const int myType = getInfo(info, g_data_type, NoType);

	if (bool isLeader = getInfo(info, g_isLeader, true); getInfo(info, g_channelStatus, InValid) == Valid && isLeader) {
		// exclusive band rtmp,v live, Now etc....
		if (!pls_is_dual_output_on())
			exclusiveChannelCheckAndVerify(info);
		int childrenSelected = 0;
		if (myType == ChannelType) {
			childrenSelected = static_cast<int>(this->getCurrentSelectedPlatformChannels(getInfo(info, g_channelName), ChannelType).count());
			showResolutionTips(getInfo(info, g_channelName));
		} else {
			showResolutionTips(CUSTOM_RTMP);
		}

		if (childrenSelected == 0) {
			int currentSeleted = this->currentSelectedCount();
			if (currentSeleted < g_maxActiveChannels) {
				if (pls_is_dual_output_on()) {
					pls_async_call(this, [this, channelUUID]() { setOutputDirectionWhenAddChannel(channelUUID); });
				} else {
					this->setChannelUserStatus(channelUUID, Enabled);
				}
				this->setValueOfChannel(channelUUID, g_displayState, true);
			}
		}
		this->sortAllChannels();
	}

	emit channelAdded(channelUUID);
}

void PLSChannelDataAPI::connectUIsignals()
{
	connectSignals();
}

void PLSChannelDataAPI::setChannelInfos(const QVariantMap &Infos, bool notify, bool removeImage)
{
	QVariantMap lastInfo;
	QString channelUUID = getInfo(Infos, g_channelUUID);

	if (!replaceChannelInfo(channelUUID, lastInfo, Infos)) {
		return;
	}

	updatePlatformViewerCount();

	if (removeImage) {
		updateImage(getInfo(lastInfo, g_userIconCachePath), getInfo(Infos, g_userIconCachePath));
	}

	if (notify) {
		emit channelModified(channelUUID);
	}
	int newUserStatus = getInfo(Infos, g_channelUserStatus, NotExist);
	int orgUserStatus = getInfo(lastInfo, g_channelUserStatus, NotExist);
	if (orgUserStatus != newUserStatus) {
		emit channelActiveChanged(channelUUID, newUserStatus == Enabled);
	}
}

bool PLSChannelDataAPI::replaceChannelInfo(const QString &channelUUID, QVariantMap &lastInfo, const QVariantMap &Infos)
{

	QWriteLocker locker(&mInfosLocker);
	auto ite = mChannelInfos.find(channelUUID);
	if (ite == mChannelInfos.end()) {
		return false;
	}
	lastInfo = ite.value();
	*ite = Infos;
	(*ite)[g_isPresetRTMP] = PLSSyncServerManager::instance()->isPresetRTMP(getInfo(Infos, g_channelRtmpUrl));
	auto platName = getInfo(Infos, g_channelName);
	platName = toPlatformCodeID(platName, true);
	(*ite)[g_channelName] = platName;
	return true;
}

bool PLSChannelDataAPI::isChannelInfoExists(const QString &channelUUID)
{
	QReadLocker locker(&mInfosLocker);
	return mChannelInfos.contains(channelUUID);
}

void PLSChannelDataAPI::addError(const QVariantMap &errorMap)
{
	QWriteLocker locker(&mErrorLock);
	mErrorStack.push(errorMap);
}

QVariantMap PLSChannelDataAPI::takeFirstError()
{
	if (hasError()) {
		QWriteLocker locker(&mErrorLock);
		return mErrorStack.pop();
	}
	return QVariantMap();
}

bool PLSChannelDataAPI::hasError() const
{
	QReadLocker locker(&mErrorLock);
	return !mErrorStack.isEmpty();
}

void PLSChannelDataAPI::clearAllErrors()
{
	QWriteLocker locker(&mErrorLock);
	mErrorStack.clear();
}

bool PLSChannelDataAPI::isEmpty() const
{
	QReadLocker locker(&mInfosLocker);
	return mChannelInfos.isEmpty();
}

int PLSChannelDataAPI::count() const
{
	QReadLocker locker(&mInfosLocker);
	return static_cast<int>(mChannelInfos.count());
}

bool PLSChannelDataAPI::isLivingOrRecording() const
{
	return (isRecording() || isLiving());
}

bool PLSChannelDataAPI::isPrepareLiving() const
{
	return (currentBroadcastState() == BroadcastGo) || (currentBroadcastState() == CanBroadcastState) || (currentBroadcastState() == StreamStarting);
}

bool PLSChannelDataAPI::isLiving() const
{
	return (currentBroadcastState() == StreamStarting) || (currentBroadcastState() == StreamStarted) || (currentBroadcastState() == StreamStopping);
}

bool PLSChannelDataAPI::isInWholeBroadCasting() const
{
	return (currentBroadcastState() != ReadyState && currentBroadcastState() != RecordStopped);
}

bool PLSChannelDataAPI::isShifting() const
{

	if (int liveState = this->currentBroadcastState();
	    liveState == BroadcastGo || liveState == CanBroadcastState || liveState == StreamStarting || liveState == StopBroadcastGo || liveState == CanBroadcastStop || liveState == StreamStopping) {
		return true;
	}
	return false;
}

bool PLSChannelDataAPI::isInEndingStream() const
{
	if (int liveState = this->currentBroadcastState(); liveState == StopBroadcastGo || liveState == CanBroadcastStop || liveState == StreamStopping) {
		return true;
	}
	return false;
}

bool PLSChannelDataAPI::isRecording() const
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

bool PLSChannelDataAPI::isInWholeRecording() const
{
	return (currentReocrdState() != RecordReady && currentReocrdState() != RecordStopped);
}

ChannelsMap PLSChannelDataAPI::getCurrentSelectedChannels(int Type)
{
	QVariantMap searchmap;
	searchmap.insert(g_channelUserStatus, ChannelData::Enabled);
	if (Type != ChannelData::NoType) {
		searchmap.insert(g_data_type, Type);
	}

	return getMatchKeysInfos(searchmap);
}

int PLSChannelDataAPI::currentSelectedCount() const
{
	QReadLocker locker(&mInfosLocker);
	auto isSelected = [](const QVariantMap &info) { return (getInfo(info, g_channelUserStatus, Disabled) == Enabled); };
	return int(std::count_if(mChannelInfos.cbegin(), mChannelInfos.cend(), isSelected));
}

int PLSChannelDataAPI::currentSelectedValidCount() const
{
	QReadLocker locker(&mInfosLocker);
	auto isSelected = [](const QVariantMap &info) {
		int channelState = getInfo(info, g_channelStatus, Error);
		bool isCanBroadcasting = channelState == Valid || channelState == Expired;
		return (getInfo(info, g_channelUserStatus, Disabled) == Enabled && isCanBroadcasting);
	};
	return int(std::count_if(mChannelInfos.cbegin(), mChannelInfos.cend(), isSelected));
}

ChannelsMap PLSChannelDataAPI::getCurrentSelectedPlatformChannels(const QString &platform, int srcType) const
{
	ChannelsMap retMap;
	auto isMatched = [&](const QVariantMap &info) {
		int myType = getInfo(info, g_data_type, NoType);
		bool isTypeMatch = ((srcType == NoType) ? true : (myType == srcType));
		return getInfo(info, g_channelName).contains(platform) && getInfo(info, g_channelUserStatus, Disabled) == Enabled && isTypeMatch;
	};

	for (const auto &info : mChannelInfos) {
		if (isMatched(info)) {
			retMap.insert(getInfo(info, g_channelName), info);
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
	auto fun = [this, channelUuid, state]() {
		if (!updateChannelState(channelUuid, state)) {
			return;
		}
		emit channelModified(channelUuid);
	};
	QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}

bool PLSChannelDataAPI::updateChannelState(const QString &channelUuid, int state)
{

	QWriteLocker lokcer(&mInfosLocker);
	auto ite = mChannelInfos.find(channelUuid);
	if (ite == mChannelInfos.end()) {
		return false;
	}
	if (state == getInfo(*ite, g_channelStatus, Error)) {
		return false;
	}
	(*ite)[g_channelStatus] = state;

	return true;
}

void PLSChannelDataAPI::setChannelUserStatus(const QString &channelUuid, int isActive, bool notify)
{
	auto fun = [this, channelUuid, isActive, notify]() {
		if (!updateChannelUserState(channelUuid, isActive)) {
			return;
		}

		auto outputDir = getValueOfChannel(channelUuid, ChannelData::g_channelDualOutput, NoSet);
		if (isActive == Disabled && outputDir != NoSet) {
			setValueOfChannel(channelUuid, ChannelData::g_channelDualOutput, NoSet);
			sigSetChannelDualOutput(channelUuid, NoSet);
			if (!pls_is_dual_output_on()) {
				clearDualOutput();
			}
		}
		if (notify) {
			emit channelActiveChanged(channelUuid, isActive == Enabled);
			emit channelModified(channelUuid);
		}
	};

	QMetaObject::invokeMethod(this, fun, Qt::QueuedConnection);
}

bool PLSChannelDataAPI::updateChannelUserState(const QString &channelUuid, int isActive)
{

	QWriteLocker lokcer(&mInfosLocker);
	auto ite = mChannelInfos.find(channelUuid);
	if (ite == mChannelInfos.end()) {
		return false;
	}
	if (isActive == getInfo(*ite, g_channelUserStatus, NotExist)) {
		return false;
	}

	(*ite)[g_channelUserStatus] = isActive;
	return true;
}

int PLSChannelDataAPI::getChannelUserStatus(const QString &channelUUID)
{
	return getValueOfChannel(channelUUID, g_channelUserStatus, NotExist);
}

QVariantMap PLSChannelDataAPI::getChannelInfo(const QString &channelUUID) const
{
	QReadLocker locker(&mInfosLocker);

	if (auto ite = mChannelInfos.find(channelUUID); ite != mChannelInfos.cend()) {
		return *ite;
	}
	return QVariantMap();
}

QVariantMap &PLSChannelDataAPI::getChanelInfoRef(const QString &channelUUID)
{
	QReadLocker locker(&mInfosLocker);

	if (auto ite = mChannelInfos.find(channelUUID); ite != mChannelInfos.end()) {
		return *ite;
	}

	static QVariantMap empty;
	empty.clear();
	return empty;
}

QVariantMap &PLSChannelDataAPI::getChanelInfoRefByPlatformName(const QString &channelName, int type)
{
	QReadLocker locker(&mInfosLocker);
	auto tempName = NCB2BConvertChannelName(channelName);
	auto isSameName = [&](const QVariantMap &info) {
		QString platFormName = getInfo(info, g_channelName);
		int channelT = getInfo(info, g_data_type, NoType);
		return (platFormName == tempName) && (channelT == type);
	};

	if (auto ret = std::find_if(mChannelInfos.begin(), mChannelInfos.end(), isSameName); ret != mChannelInfos.end()) {
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
		QString platFormName = getInfo(info, g_channelName);
		QString channelID = getInfo(info, ChannelData::g_subChannelId);
		return (platFormName == channelName) && (channelID == subChannelID);
	};

	if (auto ret = std::find_if(mChannelInfos.begin(), mChannelInfos.end(), isSameID); ret != mChannelInfos.end()) {
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

	auto tempName = NCB2BConvertChannelName(channelName);

	for (const auto &info : mChannelInfos) {
		bool isTypeMatch = true;

		if (type != NoType) {
			int myType = getInfo(info, g_data_type, NoType);
			isTypeMatch = (type == myType);
		}
		if (getInfo(info, g_channelName) == tempName && isTypeMatch) {
			retLst << info;
		}
	}
	return retLst;
}

bool PLSChannelDataAPI::isChannelSelectedDisplay(const QString &uuid)
{
	QReadLocker locker(&mInfosLocker);
	if (auto ite = mChannelInfos.find(uuid); ite != mChannelInfos.end()) {
		return getInfo(ite.value(), g_displayState, true);
	}
	return true;
}

bool PLSChannelDataAPI::isPlatformHasCountForEndView(const QString &platformName) const
{
	auto handler = this->getPlatformHandler(platformName);
	if (handler) {
		return handler->hasCountsForLiveEnd();
	}

	return false;
}

EndLiveList PLSChannelDataAPI::getEndLiveList(const QVariantMap &info) const
{
	EndLiveList ret;
	auto platform = getInfo(info, g_channelName);
	auto type = getInfo(info, g_data_type, NoType);
	if (type != ChannelType) {
		return ret;
	}
	auto handler = this->getPlatformHandler(platform);
	if (handler == nullptr) {
		return ret;
	}
	ret = handler->getEndLiveViewList(info);
	return ret;
}

bool PLSChannelDataAPI::isPlatformEnabled(const QString &platformName) const
{
	return mPlatformStates.value(platformName, true);
}

void PLSChannelDataAPI::updatePlatformsStates()
{
	QStringList platforms = getDefaultPlatforms();
	for (auto platform : platforms) {
		auto platformName = channleNameConvertFixPlatformName(platform);
		mPlatformStates.insert(platformName, true);
	}
}

void PLSChannelDataAPI::updateRtmpGpopInfos()
{
	mRtmpPlatforms.clear();
	mRtmps.clear();
	auto rtmps = PLSSyncServerManager::instance()->getRtmpDestination();
	for (const auto &rtmpInfo : rtmps) {
		if (rtmpInfo.exposure) {
			mRtmps.insert(rtmpInfo.streamName, rtmpInfo.rtmpUrl);
			mRtmpPlatforms.append(rtmpInfo.streamName);
		}
	}
}

const QStringList &PLSChannelDataAPI::getRTMPsName()
{
	updateRtmpGpopInfos();
	return mRtmpPlatforms;
}

const QMap<QString, QString> &PLSChannelDataAPI::getRTMPInfos()
{
	updateRtmpGpopInfos();
	return mRtmps;
}

QStringList PLSChannelDataAPI::getUuidListOfRISTandSRT()
{
	QStringList uuidList;
	QReadLocker locker(&mInfosLocker);
	for (const auto &info : mChannelInfos) {
		int myType = getInfo(info, g_data_type, NoType);
		if (myType == SRTType || myType == RISTType) {
			auto uuid = getInfo(info, g_channelUUID);
			uuidList << uuid;
		}
	}
	PLS_INFO("Channels", "get size of SRT or RIST is %d", uuidList.count());
	return uuidList;
}

void PLSChannelDataAPI::addRISTandSRT2RtmpServer()
{
	QStringList uuidList = getUuidListOfRISTandSRT();
	for (const auto &uuid : uuidList) {
		PLS_INFO("Channels", "add SRT or RIST to new api,uuid is %s", uuid.toUtf8().constData());
		AddOrgDataToNewApi(uuid, false);
	}
}

std::tuple<bool, bool> PLSChannelDataAPI::isPlatformBeSurportedByCurrentVersion(const QString &platform) const
{
	std::tuple<bool, bool> surportInfo{true, false};

	const auto &tmps = PLSSyncServerManager::instance()->getPlatformVersionInfo();

	if (auto it = findMatchKeyFromMap(tmps, platform, isStringEqual); it != tmps.cend()) {
		auto tmp = it->toMap();
		auto minVer = tmp.value("version").toString();
		if (isVersionLessthan(PLS_VERSION, minVer)) {
			std::get<0>(surportInfo) = false;
			std::get<1>(surportInfo) = tmp.value("needForceUpdate").toBool();
		}
	}
	return surportInfo;
}

ChannelsMap PLSChannelDataAPI::getAllChannelInfo()
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
	PRE_LOG_MSG_STEP("begin remove channel", g_removeChannelStep, INFO)
	auto info = this->getChannelInfo(channelUUID);
	if (info.isEmpty()) {
		PRE_LOG_MSG_STEP("channel has been removed ", g_removeChannelStep, WARN)
		return;
	}
	auto platformName = getInfo(info, g_channelName);
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

	QVariantMap tmpInfo = getChannelInfo(channelUUID);
	if (tmpInfo.isEmpty()) {
		return;
	}

	int channelT = getInfo(tmpInfo, g_data_type, NoType);
	if (channelT >= RTMPType && notifyServer) {

		RTMPDeleteToPrism(channelUUID);
		return;
	}

	int count = removeInfo(channelUUID);
	if (!pls_is_dual_output_on() && getInfo(tmpInfo, g_channelDualOutput, NoSet) != NoSet) {
		clearDualOutput();
	}
	auto channelName = getInfo(tmpInfo, g_channelName, QString(" empty "));
	auto bLeader = getInfo(tmpInfo, g_isLeader, false);
	QString msg = " End remmove channel " + channelUUID + " channel: " + channelName;
	PRE_LOG_MSG_STEP(msg, g_removeChannelStep, INFO)
	if (count > 0 && notify) {
		auto platformName = getInfo(tmpInfo, g_fixPlatformName);
		PLSBasic::instance()->ClearService(platformName);
		emit channelRemoved(channelUUID);
		channelRemovedForCheckVideo(bLeader);
		if (platformName == CHZZK) {
			channelRemovedForChzzk();
		}
	}

	if (channelT == ChannelType) {
		auto imagePath = getInfo(tmpInfo, g_userIconCachePath);
		removeImage(imagePath);
	}
}

int PLSChannelDataAPI::removeInfo(const QString &channelUUID)
{

	QWriteLocker locker(&mInfosLocker);
	return static_cast<int>(mChannelInfos.remove(channelUUID));
}

void PLSChannelDataAPI::removeChannelsByPlatformName(const QString &platformName, int type, bool notify, bool notifyServer)
{
	auto infos = getChanelInfosByPlatformName(platformName, type);
	for (const auto &info : infos) {
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

		if (ltype >= CustomType && rtype >= CustomType) {
			auto ltime = getInfo(left, g_createTime, QDateTime::currentDateTime());
			auto rtime = getInfo(right, g_createTime, QDateTime::currentDateTime());
			return ltime > rtime;
		}

		if (ltype != rtype) {
			return ltype < rtype;
		}

		QString lplatform = getInfo(left, g_channelName);

		if (QString rplatform = getInfo(right, g_channelName); lplatform != rplatform) {
			return isPlatformOrderLessThan(lplatform, rplatform);
		}

		auto lname = getInfo(left, g_displayLine1);
		auto rname = getInfo(right, g_displayLine1);
		return lname.compare(rname) < 0;
	};

	std::sort(infos.begin(), infos.end(), isLessThan);

	InfosList ret;
	auto infoCount = infos.count();
	for (int i = 0; i < infoCount; ++i) {
		const auto &info = infos[i];
		auto uuid = getInfo(info, g_channelUUID);
		ret << info;
		this->setValueOfChannel(uuid, g_displayOrder, i);
		emit channelModified(uuid);
	}

	return ret;
}

void PLSChannelDataAPI::clearAll()
{
	PLS_INFO("Channels", "prism logout will clear all channels");
	QWriteLocker locker(&mInfosLocker);
	mChannelInfos.clear();
	mImagesCache.clear();
}

void PLSChannelDataAPI::saveData()
{
	if (this->isInitilized()) {
		QReadLocker infoLokcer(&mInfosLocker);
		QWriteLocker lokcer(&mFileLocker);
		saveDataXToFile(mChannelInfos, getChannelCacheFilePath());
	}
}

void PLSChannelDataAPI::delaySave()
{
	if (this->isInitilized()) {
		mSaveDelayTimer->start(1000);
	}
}

int countError(const QVariantList &errorsList, int errorType)
{
	auto isMatchedType = [errorType](const QVariant &error) {
		auto errorTmp = error.toMap();
		auto errorActType = errorTmp.value(g_errorRetdata).value<PLSErrorHandler::RetData>().prismCode;
		return errorActType == errorType;
	};

	return (int)std::count_if(errorsList.cbegin(), errorsList.cend(), isMatchedType);
}

void PLSChannelDataAPI::onAllScheduleListUpdated() const
{
	auto ret = PLS_PLATFORM_API->getAllScheduleList();
	auto currentTimeStamp = QDateTime::currentSecsSinceEpoch();

	auto isPassed = [currentTimeStamp](const QVariant &lValue) {
		auto lvarMap = lValue.toMap();
		auto lTime = lvarMap.value(ChannelData::g_timeStamp).toLongLong();
		return currentTimeStamp > lTime;
	};
	auto it = std::remove_if(ret.begin(), ret.end(), isPassed);
	ret.erase(it, ret.end());

	auto isLatest = [currentTimeStamp](const QVariant &lValue, const QVariant &rValue) {
		auto lvarMap = lValue.toMap();
		auto rvarMap = rValue.toMap();

		auto lTime = lvarMap.value(ChannelData::g_timeStamp).toLongLong() / 60;
		auto rTime = rvarMap.value(ChannelData::g_timeStamp).toLongLong() / 60;

		auto rDiffer = rTime - currentTimeStamp;

		if (auto lDiffer = lTime - currentTimeStamp; lDiffer != rDiffer) {
			return lDiffer < rDiffer;
		}

		auto lUuid = getInfo(lvarMap, ChannelData::g_channelUUID);
		auto rUuid = getInfo(rvarMap, ChannelData::g_channelUUID);
		if (lUuid == rUuid) {
			return false;
		}
		auto lOrder = PLSCHANNELS_API->getValueOfChannel(lUuid, g_displayOrder, -1);
		auto rOrder = PLSCHANNELS_API->getValueOfChannel(rUuid, g_displayOrder, -1);
		return lOrder < rOrder;
	};

	std::sort(ret.begin(), ret.end(), isLatest);

	QVariantMap first;
	if (!ret.isEmpty()) {
		first = ret.first().toMap();
	}
	if (first.isEmpty()) {
		auto errors = PLS_PLATFORM_API->getAllLastErrors();
		if (!errors.isEmpty() && countError(errors, PLSErrorHandler::COMMON_NETWORK_ERROR) > 0) {
			first = errors.first().toMap();
		}
	}
	sendChannelData(first);
	sendLoadingState(false);
}

void PLSChannelDataAPI::startUpdateScheduleList() const
{
	if (PLSCHANNELS_API->isLiving()) {
		return;
	}
	sendLoadingState(true);
	PLS_PLATFORM_API->updateAllScheduleList();
}

void verifyRename(ChannelsMap &src)
{
	std::for_each(src.begin(), src.end(), [](QVariantMap &info) {
		if (getInfo(info, g_channelStatus, Error) != Valid) {
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
		bool bExistCHzzk = false;
		if (!tmp.isEmpty()) {
			mChannelInfos = tmp;
			verifyRename(mChannelInfos);
			std::for_each(tmp.begin(), tmp.end(), [this, &bExistCHzzk](QVariantMap &info) {
				auto type = getInfo(info, g_data_type, NoType);
				if (type == ChannelType) {
					auto channelName = getInfo(info, g_channelName, QString());
					auto supportPlatforms = getDefaultPlatforms();
					auto uuid = getInfo(info, g_channelUUID, QString());
					if (!supportPlatforms.contains(channelName)) {
						removeInfo(uuid);
						return;
					}
					auto platform = channleNameConvertFixPlatformName(channelName);
					setValueOfChannel(uuid, g_fixPlatformName, platform);
					if (!bExistCHzzk && platform == CHZZK) {
						bExistCHzzk = true;
					}
				}
			});
		}
		if (!bExistCHzzk) {
			channelRemovedForChzzk();
		}
	}
}

void PLSChannelDataAPI::setRecordState(int state)
{
	PRE_LOG_MSG(QString("Record state change from %1  to %2").arg(RecordStatesMap[mRecordState]).arg(RecordStatesMap[state]), INFO)

	if (mRecordState == state) {
		return;
	}
	auto lastState = mRecordState;

	mRecordState = state;
	emit recordingChanged(state);

	switch (state) {
	case RecordReady:

		emit recordReady();
		break;
	case CanRecord:

		if (!startRecordCheck()) {
			this->setRecordState(RecordReady);
			return;
		}

		if (!toRecord()) {
			this->setRecordState(RecordReady);
			return;
		}
		emit canRecord();
		break;
	case RecordStarting:

		emit recordStarting();
		break;
	case RecordStarted:

		emit recordStarted();
		break;
	case RecordStopGo:

		if (!stopRecordCheck()) {
			this->setRecordState(RecordStarted);
			return;
		}

		if (!toTryStopRecord()) {
			this->setRecordState(RecordStarted);
			return;
		}
		emit recordStopGo();
		break;
	case RecordStopping:
		emit recordStopping();
		break;
	case RecordStopped:
		if (!PLS_PLATFORM_API->isStopForExit()) {
			auto isShowEnd = lastState == RecordStopping;
			auto end = [isShowEnd]() {
				PLS_INFO("Channel", "singleShot setRecordState");
				PLS_PLATFORM_API->showEndView(true, isShowEnd);
			};
			QTimer::singleShot(100, qApp, end);
		}

		this->setRecordState(RecordReady);
		emit recordStopped();
		break;
	default:
		break;
	} //end switch
}

int PLSChannelDataAPI::currentReocrdState() const
{
	return mRecordState;
}

void PLSChannelDataAPI::setRehearsal(bool isRehearsal)
{
	if (isRehearsal != mIsRehearsal) {
		mIsRehearsal = isRehearsal;
		emit liveTypeChanged();
	}
}

void PLSChannelDataAPI::setBroadcastState(int event)
{
	int lastState = mBroadcastState;
	PRE_LOG_MSG(QString("Broadcast state change from %1  to %2").arg(LiveStatesMap[lastState]).arg(LiveStatesMap[event]), INFO)

	if (event == mBroadcastState) {
		return;
	} else {
		mBroadcastState = event;
		emit liveStateChanged(event);
	}

	switch (event) {
	case ReadyState:

		emit holdOnGolive(false);
		emit holdOnChannelArea(false);
		this->setRehearsal(false);
		break;
	case BroadcastGo:

		emit holdOnGolive(true);

		if (!startStreamingCheck()) {
			this->setBroadcastState(ReadyState);
			return;
		}
		emit broadcastGo();
		break;
	case CanBroadcastState:

		if (!toGoLive()) {
			emit startFailed();
			this->setBroadcastState(ReadyState);
			return;
		}
		emit canBroadcast();
		break;
	case StreamStarting:
		emit holdOnGolive(true);
		emit holdOnChannelArea(true);
		if (lastState != CanBroadcastState) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit toBeBroadcasting();
		break;
	case StreamStarted:
		emit holdOnGolive(false);
		emit holdOnChannelArea(false);
		emit broadcasting();
		break;
	case StopBroadcastGo:
		emit holdOnGolive(true);
		if (!stopStreamingCheck()) {
			this->setBroadcastState(StreamStarted);
			return;
		}
		emit stopBroadcastGo();
		break;
	case CanBroadcastStop:

		toStopLive();
		break;
	case StreamStopping:
		if (lastState != CanBroadcastStop) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit stopping();
		break;
	case StreamStopped:

		if (lastState != StreamStopping) {
			this->setBroadcastState(ReadyState);
			break;
		}
		emit broadcastStopped();
		break;
	case StreamEnd:
		emit broadCastEnded();
		if (lastState == StreamStopping || lastState == StreamStopped || lastState == StreamStarted) {
			this->resetData();
		}
		resetAllChannels();
		this->setBroadcastState(ReadyState);
		break;
	default:
		break;
	}
}

int PLSChannelDataAPI::currentBroadcastState() const
{
	return mBroadcastState;
}

void PLSChannelDataAPI::clearOldVersionImages() const
{
	QDir dir(getChannelCacheDir());
	auto nameFilter = QImageReader::supportedImageFormats();
	QStringList nameFilterS;
	for (const auto &filter : nameFilter) {
		QString tmp = "*." + filter;
		nameFilterS << tmp;
	}
	auto files = dir.entryInfoList(nameFilterS, QDir::Files | QDir::NoDotAndDotDot);
	for (const auto &info : files) {
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
	handler->initialization();
	mPlatformHandler.insert(handler->getPlatformName().toUpper(), PlatformHandlerPtrs(handler));
}

PlatformHandlerPtrs PLSChannelDataAPI::getPlatformHandler(const QString &platformName) const
{
	auto name = channleNameConvertFixPlatformName(platformName);
	return mPlatformHandler.value(name.toUpper());
}

bool PLSChannelDataAPI::isPlatformMultiChildren(const QString &platformName) const
{

	if (auto hanlder = getPlatformHandler(platformName); hanlder) {
		return hanlder->isMultiChildren();
	}
	return false;
}

bool PLSChannelDataAPI::isChannelMultiChildren(const QString &uuid) const
{
	auto info = getChannelInfo(uuid);
	return isPlatformMultiChildren(getInfo(info, g_channelName));
}

ChannelsMap PLSChannelDataAPI::getMatchKeysInfos(const QVariantMap &keysMap)
{
	ChannelsMap ret;
	auto isMatched = [&](const QVariantMap &info) {
		for (auto srcIte = keysMap.cbegin(); srcIte != keysMap.cend(); ++srcIte) {
			if (srcIte.value() != getInfo(info, srcIte.key(), QVariant())) {
				return false;
			}
		}
		return true;
	};

	QReadLocker locker(&mInfosLocker);
	for (auto ite = mChannelInfos.cbegin(); ite != mChannelInfos.cend(); ++ite) {
		const auto &info = ite.value();
		if (isMatched(info)) {
			ret.insert(ite.key(), info);
		}
	}

	return ret;
}

void PLSChannelDataAPI::stopAll()
{
	if (currentBroadcastState() == StreamStarted) {
		toStopLive();
	}

	if (currentReocrdState() == RecordStarted) {
		toStopRecord();
	}
}

void PLSChannelDataAPI::getChannelCountOfOutputDirection(QStringList &horOutputList, QStringList &verOutputList)
{
	QReadLocker locker(&mInfosLocker);
	for (const QVariantMap info : mChannelInfos) {
		auto outputDirection = getInfo(info, g_channelDualOutput, NoSet);
		auto uuid = getInfo(info, g_channelUUID, QString());
		if (outputDirection == HorizontalOutput) {
			horOutputList.append(uuid);
		} else if (outputDirection == VerticalOutput) {
			verOutputList.append(uuid);
		}
	}
}

void PLSChannelDataAPI::setChannelDefaultOutputDirection()
{
	QStringList horOutputList, verOutputList;
	getChannelCountOfOutputDirection(horOutputList, verOutputList);
	int horOutputCount = horOutputList.count();
	int verOutputCount = verOutputList.count();

	auto uuidList = PLSChannelDataAPI::getCurrentSortedChannelsUUID();
	for (const QString uuid : uuidList) {
		auto outputDirection = getValueOfChannel(uuid, g_channelDualOutput, NoSet);
		auto userStatus = getValueOfChannel(uuid, g_channelUserStatus, NotExist);
		if (outputDirection == NoSet && userStatus == Enabled) {
			if (isExclusiveChannel(uuid)) {
				//dont use setChannelUserStatus
				// the setChannelUserStatus function is an asynchronous modification
				//This requires synchronous modification
				setValueOfChannel(uuid, g_channelUserStatus, Disabled);
				continue;
			}
			if (horOutputCount == 0) {
				setValueOfChannel(uuid, g_channelDualOutput, HorizontalOutput);
				++horOutputCount;
				sigSetChannelDualOutput(uuid, HorizontalOutput);
				continue;
			}
			if (verOutputCount == 0) {
				setValueOfChannel(uuid, g_channelDualOutput, VerticalOutput);
				++verOutputCount;
				sigSetChannelDualOutput(uuid, VerticalOutput);
				continue;
			}
			//dont use setChannelUserStatus
			// the setChannelUserStatus function is an asynchronous modification
			//This requires synchronous modification
			setValueOfChannel(uuid, g_channelUserStatus, Disabled);
		}
	}
}

void PLSChannelDataAPI::setOutputDirectionWhenAddChannel(const QString &uuid)
{
	QStringList horOutputList, verOutputList;
	getChannelCountOfOutputDirection(horOutputList, verOutputList);
	int horOutputCount = horOutputList.count();
	int verOutputCount = verOutputList.count();
	if ((horOutputCount >= 1 && verOutputCount >= 1) || isExclusiveChannel(uuid)) {
		setChannelUserStatus(uuid, Disabled);
		return;
	}
	setChannelUserStatus(uuid, Enabled);
	if (horOutputCount == 0) {
		setValueOfChannel(uuid, ChannelData::g_channelDualOutput, HorizontalOutput);
		return;
	}
	if (verOutputCount == 0) {
		setValueOfChannel(uuid, ChannelData::g_channelDualOutput, VerticalOutput);
		return;
	}
}

bool PLSChannelDataAPI::isCanSetDualOutput(const QString &uuid) const
{
	if (/*!isExclusiveChannel(uuid) && */ PLSCHANNELS_API->getChannelUserStatus(uuid) == Enabled) {
		return true;
	}
	return false;
}

void PLSChannelDataAPI::clearDualOutput()
{
	QWriteLocker locker(&mInfosLocker);
	for (QVariantMap &info : mChannelInfos) {
		auto outputDirection = getInfo(info, g_channelDualOutput, NoSet);
		if (outputDirection != NoSet) {
			info.remove(g_channelDualOutput);
		}
	}
}

size_t qHash(const QSize &keySize, uint seed)
{
	return qHash(keySize.width() + 1.0 / keySize.height(), seed);
}
