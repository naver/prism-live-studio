#include "PLSChannelDataAPI.h"
#include "ChannelConst.h"
#include "NetWorkAPI.h"
#include "CommonDefine.h"
#include "NetBaseInfo.h"
#include "LogPredefine.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataHandler.h"
#include "NetWorkCommonFunctions.h"
#include "NetWorkCommonDefines.h"
#include <QNetworkAccessManager>
#include <QFile>
#include "PLSChannelsVirualAPI.h"
#include <QUUid>
#include "frontend-api.h"
#include "PLSChannelsEntrance.h"
#include <QMainWindow>
#include <QPushButton>
#include <algorithm>
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include <QJsonDocument>
#include <qjsonarray.h>

using namespace ChannelData;
Q_DECLARE_METATYPE(obs_frontend_event)

PLSChannelDataAPI *PLSChannelDataAPI::mInstance = nullptr;

PLSChannelDataAPI *PLSChannelDataAPI::getInstance()
{
	if (mInstance == nullptr) {
		mInstance = new PLSChannelDataAPI();
	}
	return mInstance;
}

void PLSChannelDataAPI::release()
{
	//qDebug() << "---------------- release------------ " << mSemap.available();
	mSemap.release(1);
	//qDebug() << "---------------- release------------ " << mSemap.available();
}

void PLSChannelDataAPI::acquire()
{
	//qDebug() << " ------------------acquire--------------- " << mSemap.available();
	mSemap.acquire(1);
	//qDebug() << " ------------------acquire--------------- " << mSemap.available();
	if (mSemap.available() == 0) {
		emit lockerReleased();
	}
}

bool PLSChannelDataAPI::isEmptyToAcquire()
{
	return mSemap.available() == 0;
}
void PLSChannelDataAPI::connectNetwork()
{
	auto networkObj = convertToObejct(mNetWorkAPI);
	if (networkObj) {
		connect(networkObj, SIGNAL(replyResultData(const QString &)), this, SLOT(handleNetTaskFinished(const QString &)), Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
		connect(
			this, &PLSChannelDataAPI::networkInvalidOcurred, networkObj, [=]() { mNetWorkAPI->abortAll(); }, Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection));
	}
}
void PLSChannelDataAPI::tryToRefreshToken(const QString &uuid, bool isForce)
{
	bool result = false;
	if (isForce) {
		result = sendRefreshRequest(uuid);
	} else {
		result = checkAndUpdateToken(uuid);
	}
	bool isValid = isTokenValid(uuid);
	if (result && isValid) {
		emit tokenRefreshed(uuid, NoError_Valid);
		return;
	}

	if (!result && isValid) {
		emit tokenRefreshed(uuid, RequestError_Valid);
		return;
	}

	if (result && !isValid) {
		emit tokenRefreshed(uuid, NoError_Expired);
		return;
	}

	if (!result && !isValid) {
		emit tokenRefreshed(uuid, RequestError_Expired);
		return;
	}
}

PLSChannelDataAPI::PLSChannelDataAPI(QObject *parent)
	: QObject(parent),
	  mNetWorkAPI(createNetWorkAPI()),
	  mSemap(0),
	  mInfosLocker(QReadWriteLock::Recursive),
	  mBroadcastState(ReadyState),
	  mRecordState(mRecordState),
	  IsOnlyStopRecording(false),
	  Initilized(false)
{
	mInstance = this;
	connect(this, &PLSChannelDataAPI::sigSendRequest, this, &PLSChannelDataAPI::sendRequest, Qt::QueuedConnection);

	auto doRefreshAllChannels = []() { refreshAllChannels(); };
	connect(this, &PLSChannelDataAPI::sigRefreshAllChannels, this, doRefreshAllChannels, Qt::QueuedConnection);

	auto tryAddChannel = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToAddChannel, this, tryAddChannel, Qt::QueuedConnection);

	auto tryUpdate = [](const QString &uuid) { updateChannelInfoFromNet(uuid); };
	connect(this, &PLSChannelDataAPI::sigTryToUpdateChannel, this, tryUpdate, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::sigTryRemoveChannel, this, &PLSChannelDataAPI::removeChannelInfo, Qt::QueuedConnection);

	connect(this, &PLSChannelDataAPI::sigRefreshToken, this, &PLSChannelDataAPI::tryToRefreshToken, Qt::QueuedConnection);

	connect(this, &PLSChannelDataAPI::channelAdded, this, &PLSChannelDataAPI::saveData, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelModified, this, &PLSChannelDataAPI::saveData, Qt::QueuedConnection);
	connect(this, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelDataAPI::saveData, Qt::QueuedConnection);
	connect(
		this, &PLSChannelDataAPI::channelExpired, getMainWindow(), [](const QString &uuid) { resetExpiredChannel(uuid); }, Qt::QueuedConnection);

	connect(
		this, &PLSChannelDataAPI::prismTokenExpired, getMainWindow(), []() { reloginPrismExpired(); }, Qt::QueuedConnection);

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
				showEndView(false);
			}
		},
		Qt::DirectConnection);
	//refresh ui
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEndedForUi, this, [=]() { this->setBroadcastState(StreamEnd); }, Qt::DirectConnection);

	//stop living act
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveToStop, this, [=]() { this->setBroadcastState(CanBroadcastStop); }, Qt::QueuedConnection);

	auto gotoYoutubeAsk = [=](const QString &uuid) { gotuYoutube(uuid); };
	connect(this, &PLSChannelDataAPI::channelGoToInitialize, getMainWindow(), gotoYoutubeAsk, Qt::QueuedConnection);

	this->setBroadcastState(ReadyState);
	this->setRecordState(RecordReady);
	//qDebug() << " --------------init----------- " << mSemap.available();
	reloadData();
}

PLSChannelDataAPI ::~PLSChannelDataAPI()
{
#ifdef DEBUG
	saveTranslations();
#endif // DEBUG
}

void PLSChannelDataAPI::addChannelInfo(const QVariantMap &Infos, bool notify)
{
	PRE_LOG_MSG("add channel", INFO);
	PRE_LOG_MSG(getInfo(Infos, g_channelName).toStdString().c_str(), INFO);
	QString channelUUID = getInfo(Infos, g_channelUUID);

	int dataType = getInfo(Infos, g_data_type, DownloadType);
	switch (dataType) {
	case ChannelType:
	case RTMPType: {
		QWriteLocker locker(&mInfosLocker);
		mChannelInfos[channelUUID] = Infos;

	} break;

	default:
		return;
		break;
	}

	if (PLSCHANNELS_API->isInitilized()) {
		emit addingHold(true);
	}
	if (notify) {
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

void PLSChannelDataAPI::exitApi()
{
	this->disconnect(PLSCHANNELS_API, 0, 0, 0);
	this->blockSignals(true);
	this->saveData();
	this->abortAll();
	this->thread()->exit();
}

void PLSChannelDataAPI::finishAdding(const QString &channelUUID)
{
	int currentSeleted = this->currentSelectedCount();
	if (currentSeleted < g_maxActiveChannels && this->getValueOfChannel(channelUUID, g_channelStatus, InValid) == Valid) {
		this->setChannelUserStatus(channelUUID, Enabled);
	} else {
		this->setChannelUserStatus(channelUUID, Disabled);
	}
	emit channelAdded(channelUUID);
}

void PLSChannelDataAPI::moveToNewThread(QThread *newThread)
{
	this->moveToThread(newThread);
	if (mNetWorkAPI)
		deleteAPI(mNetWorkAPI);
	mNetWorkAPI = createNetWorkAPI();
	dynamic_cast<QObject *>(mNetWorkAPI)->moveToThread(newThread);
	connectNetwork();
}

void PLSChannelDataAPI::setChannelInfos(const QVariantMap &Infos, bool notify)
{
	QString channelUUID = getInfo(Infos, g_channelUUID);
	{
		QWriteLocker locker(&mInfosLocker);
		int dataType = getInfo(Infos, g_data_type, NoType);
		auto ite = mChannelInfos.find(channelUUID);
		if (ite == mChannelInfos.end()) {
			return;
		}
		*ite = Infos;
	}
	if (notify) {
		emit channelModified(channelUUID);
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

bool PLSChannelDataAPI::isInWholeRecording()
{
	return (currentReocrdState() != RecordReady && currentReocrdState() != RecordStopped);
}

const ChannelsMap PLSChannelDataAPI::getCurrentSelectedChannels()
{
	ChannelsMap ret;
	auto ite = mChannelInfos.begin();

	for (; ite != mChannelInfos.end(); ++ite) {
		auto &info = ite.value();
		int status = getInfo(info, g_channelUserStatus, Disabled);
		if (status == Enabled) {
			ret.insert(ite.key(), info);
		}
	}

	return ret;
}

int PLSChannelDataAPI::currentSelectedCount()
{
	auto isSelected = [](const QVariantMap &info) { return (getInfo(info, g_channelUserStatus, Disabled) == Enabled); };
	return std::count_if(mChannelInfos.begin(), mChannelInfos.end(), isSelected);
}

int PLSChannelDataAPI::currentSelectedValidCount()
{
	auto isSelected = [](const QVariantMap &info) {
		return (getInfo(info, g_channelUserStatus, Disabled) == Enabled && (getInfo(info, g_channelStatus, Error) == Valid) || getInfo(info, g_channelStatus, Error) == Expired);
	};
	return std::count_if(mChannelInfos.begin(), mChannelInfos.end(), isSelected);
}

int PLSChannelDataAPI::getChannelStatus(const QString &channelUUID)
{
	return getValueOfChannel(channelUUID, g_channelStatus, InValid);
}

void PLSChannelDataAPI::setChannelStatus(const QString &channelUuid, int state)
{
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
}

void PLSChannelDataAPI::setChannelUserStatus(const QString &channelUuid, int isActive)
{
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
	}

	emit channelActiveChanged(channelUuid, isActive == Enabled);
	emit channelModified(channelUuid);
	emit currentSelectedChanged();
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
	auto isSameName = [&](const QVariantMap &info) {
		QString platFormName = getInfo(info, g_channelName);
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

void PLSChannelDataAPI::removeChannelInfo(const QString &channelUUID, bool notify, bool notifyServer)
{
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PRE_LOG_MSG("remove channel ", INFO);
	PRE_LOG_MSG(channelUUID.toStdString().c_str(), INFO);
	if (!channelUUID.isEmpty()) {

		int count = 0;
		{
			QWriteLocker locker(&mInfosLocker);
			count = mChannelInfos.remove(channelUUID);
		}

		if (count > 0 && notify) {
			emit channelRemoved(channelUUID);
		}
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

void PLSChannelDataAPI::clearAll()
{
	emit sigAllClear();
	mChannelInfos.clear();
	mHandlers.clear();
}

void PLSChannelDataAPI::clearAllRTMPs()
{
	auto ite = mChannelInfos.begin();
	while (ite != mChannelInfos.end()) {
		auto info = ite.value();
		int type = getInfo(info, g_data_type, NoType);
		if (type == RTMPType) {
			ite = mChannelInfos.erase(ite);
		} else {
			++ite;
		}
	}
}

void PLSChannelDataAPI::abortAll()
{
	mHandlers.clear();
	mNetWorkAPI->abortAll();
}

void PLSChannelDataAPI::saveData()
{
	if (this->isInitilized()) {
		saveDataXToFile(mChannelInfos, getChannelCacheFilePath());
	}
}

void verify(ChannelsMap &src)
{
	std::for_each(src.begin(), src.end(), [](QVariantMap &info) {
		int state = getInfo(info, g_channelStatus, Error);
		if (state == Error) {
			info[g_channelStatus] = Valid;
		}
	});
}

void PLSChannelDataAPI::reloadData()
{
	QString cachePath = getChannelCacheFilePath();
	if (QFile::exists(cachePath)) {
		ChannelsMap tmp;
		loadDataFromFile(tmp, cachePath);
		if (!tmp.isEmpty()) {
			mChannelInfos = tmp;
			verify(mChannelInfos);
		}
	}
}

void PLSChannelDataAPI::createDefaultData()
{
	QStringList nameList{TWITCH, VLIVE, FACEBOOK, YOUTUBE, WAV, NAVER_TV, AFREECATV};

	for (const QString &name : nameList) {
		QVariantMap info = createDefaultChannelInfoMap(name);
		QString channelUUID = getInfo(info, g_channelUUID);
		mChannelInfos.insert(channelUUID, info);
	}
}

void PLSChannelDataAPI::setRecordState(int state)
{
	PRE_LOG_MSG(QString("Record state change :%1").arg(RecordStatesLst[state]).toStdString().c_str(), INFO);

	if (mRecordState == state) {
		return;
	} else {
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
		if (!stopRecord()) {
			this->setRecordState(RecordStarted);
			return;
		}
		emit recordStopGo();
	} break;
	case RecordStopping: {

		emit recordStopping();
	} break;
	case RecordStopped: {
		if (!PLS_PLATFORM_API->isStopForExit()) {
			auto end = [=]() { showEndView(true); };
			QTimer::singleShot(100, getMainWindow(), end);
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
	PRE_LOG_MSG(QString("Broadcast state change :%1").arg(LiveStatesLst[event]).toStdString().c_str(), INFO);
	int lastState = mBroadcastState;
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
		emit inReady();
	} break;
	case BroadcastGo: {

		emit holdOnGolive(true);
		if (!checkChannelsState()) {
			this->setBroadcastState(ReadyState);
			return;
		}

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

		emit toBeBroadcasting();
	} break;
	case StreamStarted: {

		emit broadcasting();
	} break;
	case StopBroadcastGo: {

		emit holdOnGolive(true);
		if (!stopStreamingCheck()) {
			this->setBroadcastState(ReadyState);
			return;
		}
		emit stopBroadcastGo();
	} break;
	case CanBroadcastStop: {

		toStopLive();
	} break;
	case StreamStopping: {

		emit stopping();
	} break;
	case StreamStopped: {

		emit broadcastStopped();
		if (lastState != StreamStopping) {
			this->setBroadcastState(ReadyState);
		}
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

void PLSChannelDataAPI::resetInitializeState(bool toInitilized)
{
	Initilized = toInitilized;
	if (toInitilized) {
		this->setBroadcastState(ChannelData::ReadyState);
		emit toDoinitialize();
	}
}

void PLSChannelDataAPI::sendRequest(const QString &channelUUID)
{
	if (mHandlers.contains(channelUUID)) {
		auto handler = getInfo(mHandlers, channelUUID, ChannelDataHandlerPtr());
		if (!handler->sendRequest()) {
			this->removeChannelInfo(channelUUID);
			mHandlers.remove(channelUUID);
		}
	}
}
const QString PLSChannelDataAPI::getTokenOf(const QString &channelUUID)
{
	if (isChannelInfoExists(channelUUID)) {
		return mChannelInfos.value(channelUUID).value(g_channelToken).toString();
	}
	return QString();
}

QVariant PLSChannelDataAPI::getHandler(const QString &channelUUID)
{
	return mHandlers.value(channelUUID);
}

void PLSChannelDataAPI::addHandler(QVariant handler)
{
	auto handlerPtr = handler.value<ChannelDataHandlerPtr>();
	mHandlers.insert(handlerPtr->name(), handler);
}

void PLSChannelDataAPI::removeHandler(const QString &uuid)
{
	mHandlers.remove(uuid);
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
		}
		in.flush();
	}
}

#endif // DEBUG

void PLSChannelDataAPI::updateRtmpInfos()
{
	QFile rtmpFile(":/configs/configs/RTMPInfos.json");
	if (rtmpFile.open(QIODevice::ReadOnly)) {
		auto jsonData = rtmpFile.readAll();
		auto doc = QJsonDocument::fromJson(jsonData);
		auto jsArray = doc.object().value("rtmpDestination").toArray();
		mPlatforms.clear();

		for (const auto &rtmpInfo : jsArray) {
			auto obj = rtmpInfo.toObject();
			auto name = obj.value("streamName").toString();
			auto url = obj.value("rtmpUrl").toString();
			mRtmps.insert(name, url);
			mPlatforms.append(name);
		}
	}
}

void PLSChannelDataAPI::stopAll()
{
	if (currentBroadcastState() == StreamStarted) {
		toStopLive();
	}

	if (currentReocrdState() == RecordStarted) {
		stopRecord();
	}
}

void PLSChannelDataAPI::handleNetTaskFinished(const QString &taskID)
{
	auto taskMap = mNetWorkAPI->takeTaskMap(taskID);
	if (taskMap.isEmpty()) {
		return;
	}
	//PRE_LOG(taskMap);
	auto handlerUUID = getInfo(taskMap, g_handlerUUID);
	if (mHandlers.contains(handlerUUID)) {
		auto handler = takeInfo(mHandlers, handlerUUID, ChannelDataHandlerPtr());
		if (handler != nullptr) {
			handler->callback(taskMap);
			handler->feedbackToServer();
		}
	} else if (taskMap.contains(g_callback)) {
		auto callback = taskMap.value(g_callback).value<networkCallback>();
		if (callback) {
			callback(taskMap);
		}
	}
}
