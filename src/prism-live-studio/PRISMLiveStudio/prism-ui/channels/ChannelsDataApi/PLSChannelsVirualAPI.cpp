#include "PLSChannelsVirualAPI.h"
#include <qobject.h>
#include <quuid.h>
#include <QDialog>
#include <QJsonObject>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include "ChannelCommonFunctions.h"
#include "ChannelsSettingsWidget.h"
#include "LoadingFrame.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelDataHandler.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSChannelsEntrance.h"
#include "PLSIPCHandler.h"
#include "PLSLaunchWizardView.h"
#include "PLSLiveEndDialog.h"
#include "PLSPlatformApi.h"
#include "PLSPlatformApi/PLSLiveInfoDialogs.h"
#include "PLSRtmpChannelView.h"
#include "PLSShareSourceItem.h"

#include "PLSMainView.hpp"
#include "PLSWatchers.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
#include "pls-shared-values.h"
#include "window-basic-main.hpp"

using namespace ChannelData;

void showLiveInfo(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	QString tmp = uuid;
	pls_exec_live_Info(info);

	auto newInfo = PLSCHANNELS_API->getChannelInfo(tmp);
	if (newInfo != info) {
		PLSCHANNELS_API->channelModified(tmp);
	}
}

void showChannelInfo(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	int infoType = getInfo(info, g_data_type, NoType);

	if (infoType == ChannelType) {
		showLiveInfo(uuid);
	} else if (infoType >= CustomType) {
		editRTMP(uuid);
	}
}

bool updateChannelInfoFromNet(const QString &uuid)
{
	Q_ASSERT(!uuid.isEmpty());
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return false;
	}
	int infoType = getInfo(info, g_data_type, ChannelType);
	if (infoType == ChannelType) {
		return updateChannelTypeFromNet(uuid, false);
	} else if (infoType >= CustomType) {
		return updateRTMPTypeFromNet(uuid);
	}
	return true;
}

void sortInfosByKey(InfosList &infos, const QString &sortKey)
{
	auto isLessThan = [&](const QVariantMap &left, const QVariantMap &right) { return (getInfo(left, sortKey).compare(getInfo(right, sortKey))) < 0; };
	std::sort(infos.begin(), infos.end(), isLessThan);
}

const QString showSettingTransation = "showSettingTransation";

void setShowSettingTransation(const QString &platformName)
{

	QVariantMap task;
	QVariant parameter = platformName;
	TaskFun func = [](const QVariant &para) {
		showChannelsSetting(para.toString());
		return;
	};
	task.insert(ChannelTransactionsKeys::g_functions, QVariant::fromValue(func));
	task.insert(ChannelTransactionsKeys::g_taskParameters, parameter);
	task.insert(ChannelTransactionsKeys::g_context, QVariant::fromValue(getMainWindow()));
	task.insert(ChannelTransactionsKeys::g_taskName, showSettingTransation);

	PLSCHANNELS_API->addTask(task);
}

void checkIfSettingNeedShow(int newInfoCount, int currentTransactionType, const QString &platformName)
{
	if (newInfoCount <= 1 || PLSCHANNELS_API->countTask(showSettingTransation) > 0) {
		return;
	}

	if (currentTransactionType == (int)ChannelTransactionsKeys::CMDTypeValue::AddChannelCMD) {
		setShowSettingTransation(platformName);
		return;
	}
	setShowSettingTransation("");
	return;
}

void removeNotExist(const InfosList &oldInfos, const InfosList &taskInfos)
{
	for (const auto &oldInfo : oldInfos) {
		auto oldSubId = getInfo(oldInfo, g_subChannelId);
		auto isChannelEqual = [&](const QVariantMap &info) { return getInfo(info, g_subChannelId) == oldSubId; };
		auto findIte = std::find_if(taskInfos.constBegin(), taskInfos.constEnd(), isChannelEqual);
		if (findIte == taskInfos.constEnd()) {
			bool isNotify = getInfo(oldInfo, g_isUpdated, false);
			PLSCHANNELS_API->removeChannelInfo(getInfo(oldInfo, g_channelUUID), isNotify, false);
		}
	}
}

bool updateSingleInfo(const InfosList &oldInfos, const QString &platformName, int index, InfosList &sortedInfos, InfosList &returnInfos, int currentTransactionType, bool isMulti)
{
	int newInfoCount = static_cast<int>(sortedInfos.size());
	auto &newInfo = sortedInfos[index];
	newInfo[g_isLeader] = index == 0 ? true : false;

	//error
	if (auto nickname = getInfo(newInfo, g_nickName); nickname.isEmpty()) {
		PRE_LOG_MSG(QString("empty nick name:" + platformName).toStdString().c_str(), ERROR)
		newInfo.insert(g_channelStatus, Error);
		return false;
	}
	//error
	if (getInfo(newInfo, g_channelStatus, Error) == Error) {
		returnInfos.append(newInfo);
		return false;
	}

	auto doUpdateChannelInfo = [&](const QVariantMap &source) {
		PLSCHANNELS_API->setChannelInfos(source, false);
		returnInfos.append(source);
	};
	if (!isMulti) {
		doUpdateChannelInfo(newInfo);
		return true;
	}

	newInfo.remove(g_displayState);
	auto newSubId = getInfo(newInfo, g_subChannelId);
	auto isChannelEqual = [&](const QVariantMap &info) { return getInfo(info, g_subChannelId) == newSubId; };
	auto findIte = std::find_if(oldInfos.constBegin(), oldInfos.constEnd(), isChannelEqual);
	QVariantMap baseInfo;
	if (findIte == oldInfos.constEnd()) {
		baseInfo = createDefaultChannelInfoMap(platformName, ChannelType);
		newInfo[g_isUpdated] = false;

		if (index == 0 && currentTransactionType == (int)ChannelTransactionsKeys::CMDTypeValue::AddChannelCMD) {
			newInfo[g_displayState] = true;
		} else {
			newInfo[g_displayState] = false;
		}

		addToMap(baseInfo, newInfo);
		PLSCHANNELS_API->addChannelInfo(baseInfo, false);
		returnInfos.append(baseInfo);
		checkIfSettingNeedShow(newInfoCount, currentTransactionType, platformName);
		return true;
	}
	//update old
	baseInfo = *findIte;
	addToMap(baseInfo, newInfo);
	doUpdateChannelInfo(baseInfo);
	return true;
}

void handleUpdateRet(InfosList &sortedInfos)
{
	if (sortedInfos.isEmpty()) {
		endRefresh();
		return;
	}
	auto platformName = getInfo(sortedInfos.first(), g_platformName);

	bool isMulti = PLSCHANNELS_API->isPlatformMultiChildren(platformName);
	auto oldInfos = PLSCHANNELS_API->getChanelInfosByPlatformName(platformName, ChannelType);
	int currentTransactionType = PLSCHANNELS_API->currentTransactionCMDType();
	auto newInfoCount = sortedInfos.count();
	InfosList returnInfos;
	bool hasError = false;
	for (int i = 0; i < newInfoCount; ++i) {
		if (!updateSingleInfo(oldInfos, platformName, i, sortedInfos, returnInfos, currentTransactionType, isMulti)) {
			hasError = true;
			break;
		}
	} //end for

	//to do delete work if no error
	if (!hasError && (isMulti || oldInfos.size() != returnInfos.size())) {
		removeNotExist(oldInfos, returnInfos);
	}

	//refresh finish job
	PLSCHANNELS_API->sortAllChannels();
	for (const auto &info : returnInfos) {
		if (!handleChannelStatus(info)) {
			break;
		}
	}

	endRefresh();
}

bool handleChannelStatus(const QVariantMap &info)
{
	bool isContinue = false;
	int status = getInfo(info, g_channelStatus, Error);
	auto uuid = getInfo(info, g_channelUUID);

	switch (status) {

	case UnInitialized:
		handleUninitialState(uuid);
		break;
	case UnAuthorized:
	case Expired:
		PLSCHANNELS_API->channelExpired(uuid);
		break;
	case WaitingActive:
		handleWaitingState(uuid);
		break;
	case EmptyChannel:
		handleEmptyState(uuid);
		break;
	case LoginError:
		handleLoginErrorState(info, uuid);
		break;
	case Error:
		handleErrorState(info, uuid);
		break;
	case Valid:
		handleValidState(info, uuid, isContinue);
		break;
	default:
		isContinue = false;
		break;
	} //end switch

	return isContinue;
}

void handleErrorState(const QVariantMap &info, const QString &uuid)
{
	addErrorFromInfo(info);

	if (!getInfo(info, g_isUpdated, false)) {
		PLSCHANNELS_API->sigTryRemoveChannel(uuid, false, false);
		emit PLSCHANNELS_API->channelCreateError(uuid);
	}
}

void handleValidState(const QVariantMap &info, const QString &uuid, bool &isContinue)
{

	if (auto displayname = getInfo(info, g_nickName); displayname.isEmpty()) {
		auto platform = getInfo(info, g_platformName);
		PRE_LOG_MSG(QString("empty nick name:" + platform).toStdString().c_str(), ERROR)
		addErrorForType(NetWorkErrorType::UnknownError);

		return;
	}

	PLSCHANNELS_API->setValueOfChannel(uuid, g_isUserAsked, false);
	isContinue = true;
	FinishTaskReleaser releaser(uuid);
}

void handleLoginErrorState(const QVariantMap &info, const QString &uuid)
{

	QVariantMap errormap;
	errormap.insert(g_errorTitle, QObject::tr("Alert.Title"));
	errormap.insert(g_errorString, CHANNELS_TR(Check.Login.Error));
	PLSCHANNELS_API->addError(errormap);
	bool isUpdated = getInfo(info, g_isUpdated, false);
	PLSCHANNELS_API->sigTryRemoveChannel(uuid, isUpdated, false);
	if (!isUpdated) {
		emit PLSCHANNELS_API->channelCreateError(uuid);
	}
}

void handleEmptyState(const QString &uuid)
{

	PLSCHANNELS_API->setValueOfChannel(uuid, g_channelUserStatus, Disabled);
	PLSCHANNELS_API->setValueOfChannel(uuid, g_errorString, CHANNELS_TR(emptyChannelError));
	PLSCHANNELS_API->channelGoToInitialize(uuid);
	FinishTaskReleaser releaser(uuid);
}

void handleWaitingState(const QString &uuid)
{

	PLSCHANNELS_API->setValueOfChannel(uuid, g_channelUserStatus, Disabled);
	PLSCHANNELS_API->setValueOfChannel(uuid, g_errorString, CHANNELS_TR(initialChannelError));
	FinishTaskReleaser releaser(uuid);
}

void handleUninitialState(const QString &uuid)
{

	PLSCHANNELS_API->setValueOfChannel(uuid, g_channelUserStatus, int(Disabled));
	PLSCHANNELS_API->setValueOfChannel(uuid, g_errorString, CHANNELS_TR(emptyChannelError));
	PLSCHANNELS_API->channelGoToInitialize(uuid);
	FinishTaskReleaser releaser(uuid);
}

bool updateChannelTypeFromNet(const QString &uuid, bool bRefresh)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return false;
	}

	if (bool isLeader = getInfo(info, g_isLeader, true); !isLeader) {
		return true;
	}

	QString platformName = getInfo(info, g_platformName, QString());

	if (!PLSCHANNELS_API->isPlatformEnabled(platformName)) {
		PLSCHANNELS_API->removeChannelsByPlatformName(platformName, ChannelType, true, false);
		PLSCHANNELS_API->addingHold(false);
		return true;
	}

	if (auto handler = PLSCHANNELS_API->getPlatformHandler(platformName); handler) {
		PLSCHANNELS_API->release();
		bool isUpdated = getInfo(info, g_isUpdated, false);
		auto updateFun = [handler, bRefresh](const InfosList &srcList) {
			InfosList tmpList = srcList;

			// reset display order string ,reset display line1 line 2
			handler->updateDisplayInfo(tmpList);
			sortInfosByKey(tmpList, g_sortString);
			handleUpdateRet(tmpList);
			if (!bRefresh) {
				for (auto &src : tmpList) {
					auto platformName = getInfo(src, g_platformName);
					int status = getInfo(src, g_channelStatus, Error);
					if (status == ChannelData::ChannelStatus::Valid) {
						PLS_LOGEX(PLS_LOG_INFO, "Channels", {{"platformName", platformName.toUtf8().constData()}, {"loginStatus", "Success"}}, "%s channel login success",
							  platformName.toUtf8().constData());
					} else if (status == ChannelData::ChannelStatus::Expired) {
						PLS_LOGEX(PLS_LOG_INFO, "Channels",
							  {{"platformName", platformName.toUtf8().constData()}, {"loginStatus", "Failed"}, {"loginFailed", "channel token expired."}},
							  "%s channel login failed", platformName.toUtf8().constData());
					} else if (status == ChannelData::ChannelStatus::Error) {
						auto errStr = getInfo(src, g_channelSreLoginFailed);
						PLS_LOGEX(PLS_LOG_INFO, "Channels",
							  {{"platformName", platformName.toUtf8().constData()}, {"loginStatus", "Failed"}, {"loginFailed", errStr.toUtf8().constData()}},
							  "%s channel login failed", platformName.toUtf8().constData());
					}
				}
			}
		};
		//clear last data
		handler->resetData(ChannelDataBaseHandler::ResetReson::RefreshReset);
		info = PLSCHANNELS_API->getChannelInfo(uuid);
		PRE_LOG_MSG_STEP("try get channel info :" + platformName, isUpdated ? g_updateChannelStep : g_addChannelStep, INFO)
		if (!handler->tryToUpdate(info, updateFun)) {
			PRE_LOG_MSG(QString("Error on update platform:" + platformName).toStdString().c_str(), ERROR)
			endRefresh();
			return false;
		}
		return true;
	}

	PLSCHANNELS_API->removeChannelInfo(uuid, false, false);
	PLSCHANNELS_API->addingHold(false);
	return false;
}

bool updateRTMPTypeFromNet(const QString &uuid)
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return false;
	}
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	if (info.isEmpty()) {
		return false;
	}
	int type = getInfo(info, g_data_type, NoType);
	if (type == SRTType || type == RISTType) {
		FinishTaskReleaser finishAdd(uuid);
		return true;
	}
	QString seq = getInfo(info, g_rtmpSeq, QString());
	if (seq.isEmpty()) {
		return RTMPAddToPrism(uuid);
	} else {
		return RTMPUpdateToPrism(uuid);
	}
}

QVariantMap createErrorMap(int errorType)
{

	QVariantMap errormap;
	errormap.insert(g_errorType, errorType);
	switch (errorType) {

	case NetWorkErrorType::NetWorkNoStable:
		errormap.insert(g_errorTitle, QObject::tr("Alert.Title"));
		errormap.insert(g_errorString, QTStr("login.check.note.network")); //error
		errormap.insert(g_errorIsErrMsg, false);
		break;
	case NetWorkErrorType::RTMPNotExist:
		errormap.insert(g_errorTitle, QObject::tr("Alert.Title"));
		errormap.insert(g_errorString, QObject::tr("Channels.rtmp.notexist"));
		errormap.insert(g_errorIsErrMsg, true);
		break;
	case NetWorkErrorType::NeedRefresh: //for some api ,reply code can be any case ,refresh to confirm
		errormap.insert(g_errorTitle, QObject::tr("Alert.Title"));
		errormap.insert(g_errorString, QObject::tr("Channels.Refresh.Confirm"));
		errormap.insert(g_errorIsErrMsg, true);
		break;
	default:
		errormap.insert(g_errorTitle, QObject::tr("Alert.Title"));
		errormap.insert(g_errorString, QObject::tr("server.unknown.error")); //error
		errormap.insert(g_errorIsErrMsg, true);
		break;
	}

	return errormap;
}

void addErrorForType(int errorType)
{
	auto errorMap = createErrorMap(errorType);
	PLSCHANNELS_API->addError(errorMap);
}

void addErrorFromInfo(const QVariantMap &info)
{
	int errorType = getInfo(info, g_errorType, NetWorkErrorType::NetWorkNoStable);
	if (errorType == NetWorkErrorType::SpecializedError) {
		QVariantMap errormap;
		errormap.insert(g_errorType, errorType);
		errormap.insert(g_errorTitle, getInfo(info, g_errorTitle, QObject::tr("Alert.Title")));
		errormap.insert(g_errorString, getInfo(info, g_errorString, QObject::tr("server.unknown.error"))); //error
		errormap.insert(g_errorIsErrMsg, true);
		PLSCHANNELS_API->addError(errormap);
		return;
	}
	addErrorForType(errorType);
}

QVariantMap createScheduleGetError(const QString &platform, ChannelData::NetWorkErrorType errorType)
{
	auto content = createErrorMap(errorType);
	content.insert(ChannelData::g_platformName, platform);
	return content;
}

void refreshChannel(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	int type = getInfo(info, g_data_type, ChannelType);
	if (type == ChannelType) {
		updateChannelTypeFromNet(uuid, true);
	} else if (type >= CustomType) {
		resetChannel(uuid);
	}
}

void resetChannel(const QString &uuid)
{
	auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	int state = getInfo(info, g_channelStatus, Error);

	switch (state) {
	case Error:
		info[g_channelStatus] = Valid;
		PLSCHANNELS_API->channelModified(uuid);
		break;
	case Expired:
	case UnAuthorized:
		PLSCHANNELS_API->sigTryRemoveChannel(uuid, true, false);
		break;
	default:
		PLSCHANNELS_API->channelModified(uuid);
		break;
	}
}

void resetExpiredChannel(const QString &uuid, bool toAsk)
{
	if (!PLSCHANNELS_API->isInitilized()) {
		auto info = PLSCHANNELS_API->getChannelInfo(uuid);
		PLSCHANNELS_API->recordExpiredInfo(info);
		PLSCHANNELS_API->tryRemoveChannel(uuid, true, false);
		return;
	}

	if (PLSCHANNELS_API->isLiving() || PLSCHANNELS_API->isShifting()) {
		PLSCHANNELS_API->setChannelStatus(uuid, Expired);
		return;
	}

	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	PLSCHANNELS_API->tryRemoveChannel(uuid, true, false);

	auto name = getInfo(info, g_platformName);
	reloginChannel(name, toAsk);
}

void reloginChannel(const QString &platformName, bool toAsk)
{
	if (toAsk) {
		auto displayPlatform = translatePlatformName(platformName);
		auto ret = pls_alert_error_message(getMainWindow(), CHANNELS_TR(Confirm), CHANNELS_TR(expiredQuestion).arg(displayPlatform), {{PLSAlertView::Button::Yes, QObject::tr("OK")}});
		if (ret != PLSAlertView::Button::Yes) {
			return;
		}
	}

	QTimer::singleShot(500, [platformName]() { runCMD(platformName); });
}

int getReplyContentCode(const QByteArray &data)
{
	auto doc = QJsonDocument::fromJson(data);
	int code = 0;
	if (!doc.isNull()) {
		auto obj = doc.object();
		if (!obj.isEmpty()) {
			code = obj.value("code").toInt();
		}
	}
	return code;
}

void reloginPrismExpired()
{
	pls_alert_error_message(getMainWindow(), QObject::tr("Alert.Title"), QObject::tr("main.message.prism.login.session.expired"));
	//PLSBasic::Get()->setSessionExpired(true);
	pls_prism_change_over_login_view();
}

//reset channel state when end live ,try check channel state and delete
void resetAllChannels()
{
	auto channles = PLSCHANNELS_API->getAllChannelInfo();
	auto ite = channles.begin();
	while (ite != channles.end()) {
		int type = getInfo(*ite, g_data_type, NoType);
		if (type == ChannelType || type >= CustomType) {
			resetChannel(ite.key());
		}
		++ite;
	}
}

void refreshAllChannels()
{
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);

	if (auto channels = PLSCHANNELS_API->getAllChannelInfo(); !channels.isEmpty()) {
		SemaphoreHolder holder(PLSCHANNELS_API->getSourceSemaphore());
		sendLoadingState(true);
		PLSCHANNELS_API->addTransaction(ChannelTransactionsKeys::g_CMDType, (int)ChannelTransactionsKeys::CMDTypeValue::RefreshChannelsCMD);
		PLSCHANNELS_API->addTask([]() { PLSCHANNELS_API->startUpdateScheduleList(); }, getMainWindow());
		auto ite = channels.begin();
		while (ite != channels.end()) {

			const auto &info = ite.value();
			const auto &uuid = ite.key();
			++ite;

			if (bool isLeader = getInfo(info, g_isLeader, true); !isLeader) {
				continue;
			}
			refreshChannel(uuid);
		}
	}
	PLSCHANNELS_API->sigUpdateAllRtmp();

#ifdef DEBUG
	testError();
#endif // DEBUG
}

void handleEmptyChannel(const QString &uuid)
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}
	auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);

	if (bool isAsked = getInfo(info, g_isUserAsked, false); isAsked) {
		return;
	}
	info[g_isUserAsked] = true;

	if (auto platformName = getInfo(info, g_platformName); platformName == YOUTUBE) {

		if (auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(GotoYoutubetoSet),
						      {{PLSAlertView::Button::Yes, CHANNELS_TR(GotoYoutubeOk)}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}}, PLSAlertView::Button::Cancel);
		    ret != PLSAlertView::Button::Yes) {
			return;
		}
		QDesktopServices::openUrl(QUrl(g_yoububeLivePage));
		return;
	}

	pls_alert_error_message(getMainWindow(), QObject::tr("Alert.Title"), CHANNELS_TR(EmptyChannelMessage));
	return;
}

void addRTMP()
{
	auto rtmpInfo = createDefaultChannelInfoMap(SELECT_TYPE, RTMPType);
	auto rtmpView = std::make_shared<PLSRtmpChannelView>(rtmpInfo, getMainWindow());
	if (rtmpView->exec() == QDialog::Accepted) {
		rtmpInfo = rtmpView->SaveResult();
		rtmpInfo[g_displayLine1] = rtmpInfo[g_nickName];
		rtmpInfo[g_channelStatus] = QVariant::fromValue(Valid);
		PLSCHANNELS_API->addChannelInfo(rtmpInfo);
	}
}

void editRTMP(const QString &uuid)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(uuid);
	auto rtmpView = std::make_shared<PLSRtmpChannelView>(channelInfo, getMainWindow());
	if (rtmpView->exec() == QDialog::Accepted) {
		channelInfo = rtmpView->SaveResult();
		channelInfo[g_displayLine1] = channelInfo[g_nickName];
		PLSCHANNELS_API->backupInfo(uuid);
		PLSCHANNELS_API->setChannelInfos(channelInfo, false);
		PLSCHANNELS_API->sigTryToUpdateChannel(uuid);
	}
}

void runCMD(const QString &cmdStr)
{
	static bool isBegin = false;
	if (isBegin) {
		PRE_LOG_MSG_STEP(QString("run add channel twice......" + cmdStr), g_addChannelStep, WARN)
		return;
	}

	isBegin = true;

	if (PLSCHANNELS_API->count() >= 100) {
		PLSAlertView::warning(getMainWindow(), QObject::tr("Alert.Title"), CHANNELS_TR(MaxChannels));
		isBegin = false;
		return;
	}

	if (cmdStr == CUSTOM_RTMP) {
		PLSCHANNELS_API->addTransaction(ChannelTransactionsKeys::g_CMDType, (int)ChannelTransactionsKeys::CMDTypeValue::AddChannelCMD);
		PRE_LOG_MSG_STEP(QString(" show UI add RTMP "), g_addChannelStep, INFO)
		addRTMP();
		isBegin = false;
		return;
	}

	if (!PLSCHANNELS_API->getChanelInfoRefByPlatformName(cmdStr, ChannelData::ChannelType).isEmpty()) {
		isBegin = false;
		return;
	}

	if (!isCurrentVersionCanDoNext(cmdStr, nullptr)) {
		isBegin = false;
		return;
	}

	if (gDefaultPlatform.contains(cmdStr, Qt::CaseInsensitive)) {
		PLSCHANNELS_API->addTransaction(ChannelTransactionsKeys::g_CMDType, (int)ChannelTransactionsKeys::CMDTypeValue::AddChannelCMD);
		auto handler = PLSCHANNELS_API->getPlatformHandler(cmdStr);
		if (handler) {
			handler->beginLogin();
		}
	}

	isBegin = false;
}

template<typename Container> bool isCurrentVersionCanDoNextImp(const Container &platforms, QWidget *parent)
{

	bool isSurport = true;
	bool isForce = false;

	//to find not surported platform selected,and count if force update platform exists!
	QStringList msg;
	for (const auto &platform : platforms) {
		auto [isSurportT, isForceT] = PLSCHANNELS_API->isPlatformBeSurportedByCurrentVersion(platform);
		isSurport = isSurport && isSurportT;
		isForce = (isForce || isForceT);
		if (!isSurportT) {
			msg.append(translatePlatformName(platform));
		}
	}

	if (isSurport) {
		return true;
	}

	std::sort(msg.begin(), msg.end(), isPlatformOrderLessThan);
	//to ask user
	auto question = [parent](const QString &tipInfo) {
		return (PLSAlertView::Button::Yes == PLSAlertView::question(parent, QObject::tr("Alert.Title"), tipInfo, PLSAlertView::Button::Yes | PLSAlertView::Button::Cancel));
	};

	QString tipmsg = isForce ? CHANNELS_TR(needUpdate).arg(msg.join(',')) : CHANNELS_TR(suggestUpdate).arg(msg.join(','));

	//user agree to update
	if (auto isToUpdate = question(tipmsg); isToUpdate) {
		goto DOUPDATE;
	}
	//user ignore update
	if (isForce) {
		return false;
	}
	//suggestion ignore
	return true;
DOUPDATE:
	auto basic = PLSBasic::instance();
	basic->CheckAppUpdate();
	basic->startDownloading();
	return false;
}

bool isCurrentVersionCanDoNext(const QStringList &platforms, QWidget *parent)
{
	return isCurrentVersionCanDoNextImp<QStringList>(platforms, parent);
}

bool isCurrentVersionCanDoNext(const QString &platfrom, QWidget *parent)
{
	return isCurrentVersionCanDoNextImp(QStringList{platfrom}, parent);
}

void showResolutionTips(const QString &platform)
{
	QMetaObject::invokeMethod(pls_get_main_view(), "showResolutionTips", Qt::QueuedConnection, Q_ARG(QString, platform));
}
/****************streaming ***************************/
Qt::ConnectionType getInvokeType()
{
	return (QThread::currentThread() != getMainWindow()->thread() ? Qt::BlockingQueuedConnection : Qt::AutoConnection);
}

bool checkVersion()
{
	auto platformsInfo = PLSCHANNELS_API->getCurrentSelectedChannels(ChannelData::ChannelType);
	QStringList platfroms;
	auto getPlatforms = [](const QVariantMap &info) { return getInfo(info, g_platformName); };
	std::transform(platformsInfo.begin(), platformsInfo.end(), std::back_insert_iterator(platfroms), getPlatforms);
	if (!isCurrentVersionCanDoNext(platfroms, nullptr)) {
		return false;
	}

	return true;
}

bool checkChannelsState()
{
	bool isTocontinue = true;
	QMetaObject::invokeMethod(getMainWindow(), checkVersion, getInvokeType(), &isTocontinue);
	if (!isTocontinue) {
		return false;
	}
	if (PLSCHANNELS_API->currentSelectedCount() == 0) {
		auto showWarning = []() { PLSAlertView::warning(getMainWindow(), QObject::tr("Alert.Title"), CHANNELS_TR(ErrorEmptyChannelMessage)); };
		QMetaObject::invokeMethod(getMainWindow(), showWarning, getInvokeType());
		return false;
	}
	return true;
}

bool startStreamingCheck()
{
	if (!checkChannelsState()) {
		return false;
	}

	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;

	if (bool isok = QMetaObject::invokeMethod(mainw, "startStreamingCheck", getInvokeType(), Q_RETURN_ARG(bool, retVal)); !isok) {
		PRE_LOG("error :startStreamingCheck", ERROR)
		return false;
	}

	if (!retVal) {
		return false;
	}

	if (auto *main = dynamic_cast<PLSBasic *>(mainw); !main->CheckHEVC()) {
		return false;
	}

	return true;
}

bool toGoLive()
{
	bool isok = QMetaObject::invokeMethod(PLSBasic::instance(), "StartStreaming", getInvokeType());
	if (!isok || !PLSBasic::instance()->getState(OBS_FRONTEND_EVENT_STREAMING_STARTING)) {
		PRE_LOG("error :StartStreaming", ERROR)
		isok = false;
	}

	return isok;
}

bool stopStreamingCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "stopStreamingCheck", getInvokeType(), Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :startStreamingCheck", ERROR)
	}
	return isok && retVal;
}

bool toStopLive()
{
	bool isok = QMetaObject::invokeMethod(PLSBasic::instance(), "StopStreaming", getInvokeType());

	return isok;
}
/****************streaming ** end *************************/

/**************** recording  ***************************/

bool startRecordCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "startRecordCheck", getInvokeType(), Q_RETURN_ARG(bool, retVal));

	if (!isok) {
		PRE_LOG("error :startRecordCheck ", ERROR)
	}
	return retVal && isok;
}

bool toRecord()
{

	bool isok = QMetaObject::invokeMethod(PLSBasic::instance(), "StartRecording", getInvokeType());
	if (!PLSBasic::instance()->getState(OBS_FRONTEND_EVENT_RECORDING_STARTING)) {
		PRE_LOG("error :StartRecording ", ERROR)
		isok = false;
	}
	return isok;
}

bool stopRecordCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "stopRecordCheck", getInvokeType(), Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :stopRecordCheck ", ERROR)
	}
	return retVal && isok;
}

bool toTryStopRecord()
{
	bool isok = QMetaObject::invokeMethod(PLSBasic::instance(), "StopRecording", getInvokeType());

	return isok;
}

/**************** recording  end ***************************/

void childExclusive(const QString &channelID)
{
	auto platformName = PLSCHANNELS_API->getValueOfChannel(channelID, g_platformName, QString());
	if (PLSCHANNELS_API->isPlatformMultiChildren(platformName)) {
		int myType = PLSCHANNELS_API->getValueOfChannel(channelID, g_data_type, NoType);
		auto allChildren = PLSCHANNELS_API->getChanelInfosByPlatformName(platformName, myType);
		for (const auto &child : allChildren) {
			auto uuid = getInfo(child, g_channelUUID);
			int userStatus = getInfo(child, g_channelUserStatus, Disabled);
			if (userStatus == Enabled && uuid != channelID) {
				PLSCHANNELS_API->setChannelUserStatus(uuid, Disabled);
			}
		}
	}
}

bool isExclusiveChannel(const QString &uuid)
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	return isExclusiveChannel(info);
}

bool isExclusiveChannel(const QVariantMap &info)
{
	auto platName = getInfo(info, g_platformName);
	auto rtmpUrl = getInfo(info, g_channelRtmpUrl);
	return g_exclusivePlatform.contains(platName.trimmed()) || rtmpUrl.contains(QRegularExpression(g_exclusiveRtmpUrl, QRegularExpression::CaseInsensitiveOption));
}

bool isEnabledExclusiveChannelExist()
{
	return !findExistEnabledExclusiveChannel().isEmpty();
}

QString findExistEnabledExclusiveChannel()
{
	QString ret;
	const auto &channels = PLSCHANNELS_API->getAllChannelInfoReference();
	auto isEnabledNow = [](const QVariantMap &info) { return isExclusiveChannel(info) && getInfo(info, g_channelUserStatus, Disabled) == Enabled; };

	if (auto retIte = std::find_if(channels.cbegin(), channels.cend(), isEnabledNow); retIte != channels.cend()) {
		ret = retIte.key();
	}
	return ret;
}

void exclusiveChannelCheckAndVerify(const QVariantMap &Newinfo)
{
	if (isExclusiveChannel(Newinfo)) {
		disableAll();
	} else {
		auto exclusiveID = findExistEnabledExclusiveChannel();
		if (!exclusiveID.isEmpty()) {
			PLSCHANNELS_API->setChannelUserStatus(exclusiveID, Disabled);
		}
	}
}

void disableAll()
{
	const auto &channels = PLSCHANNELS_API->getAllChannelInfoReference();
	auto ite = channels.cbegin();
	while (ite != channels.cend()) {
		PLSCHANNELS_API->setChannelUserStatus(ite.key(), Disabled);
		++ite;
	}
}

void showShareView(const QString &channelUUID)
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(channelUUID);
	if (info.isEmpty()) {
		return;
	}

	if (getInfo(info, g_shareUrl).isEmpty() && getInfo(info, g_shareUrlTemp).isEmpty()) {
		auto platformName = getInfo(info, g_platformName);
		auto displayPlatformName = translatePlatformName(platformName);
		pls_alert_error_message(getMainWindow(), QObject::tr("Alert.Title"), CHANNELS_TR(EmptyShareUrl).arg(displayPlatformName));
		return;
	}

	auto *dialog = pls_new<PLSShareSourceItem>(App()->getMainView());
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	//dialog->setWindowFlags(dialog->windowFlags() | Qt::Popup);
	dialog->show();
	dialog->initInfo(info);
}

int showSummaryView()
{
	return 0;
}

LoadingFrame *createBusyFrame(QWidget *parent)
{
	QPointer<LoadingFrame> loadView = new LoadingFrame(parent);
	loadView->initialize(":/resource/images/loading/loading-1.svg", 500);
	loadView->show();
	QPoint position;
	if (parent) {
		position = parent->mapToGlobal(QPoint(parent->rect().center())) - loadView->rect().center();
	} else {
		position = QCursor::pos();
	}
	loadView->move(position);
	return loadView;
}

void showNetworkErrorAlert()
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}

	if (PLSCHANNELS_API->hasError() && PLSCHANNELS_API->isEmptyToAcquire()) {
		auto errorMap = PLSCHANNELS_API->takeFirstError();
		SemaphoreHolder holder(PLSCHANNELS_API->getSourceSemaphore());
		if (getInfo<bool>(errorMap, g_errorIsErrMsg, false)) {
			pls_alert_error_message(getMainWindow(), getInfo(errorMap, g_errorTitle), getInfo(errorMap, g_errorString));
		} else {
			auto title = getInfo(errorMap, g_errorTitle);
			auto text = getInfo(errorMap, g_errorString);
			if (title == QTStr("Alert.Title") && text == QTStr("login.check.note.network")) {
				showNetworkAlertOnlyOnce();
			} else {
				PLSAlertView::warning(getMainWindow(), title, text);
			}
		}
		PLSCHANNELS_API->clearAllErrors();
	}
	PLSCHANNELS_API->holdOnChannelArea(false);
}

void showErrorAlert(int errorType)
{
	QMetaObject::invokeMethod(
		pls_get_main_view(),
		[errorType]() {
			addErrorForType(errorType);
			showNetworkErrorAlert();
		},
		Qt::QueuedConnection);
}

void showNetworkAlertOnlyOnce()
{
	static bool bNetworkAlertShow = false;

	if (bNetworkAlertShow) {
		return;
	}

	auto title = QTStr("Alert.Title");
	auto text = QTStr("login.check.note.network");
	bNetworkAlertShow = true;
	PLSLaunchWizardView *banner = dynamic_cast<PLSLaunchWizardView *>(pls_get_banner_widget());
	if (banner && !banner->getIsShowFlag()) {
		auto *watcher = pls_new<PLSShowWatcher>(banner);
		QObject::connect(
			watcher, &PLSShowWatcher::signalShow, banner,
			[watcher, banner, title, text]() {
				watcher->deleteLater();
				PLSAlertView::warning(banner, title, text);
				bNetworkAlertShow = false;
				banner->activateWindow();
				banner->raise();
			},
			Qt::QueuedConnection);
	} else {
		PLSAlertView::warning(getMainWindow(), title, text);
		bNetworkAlertShow = false;
	}
}

template<typename ArgsType> void showChannelsSettingImpl(const ArgsType &platform)
{
	auto dialog = std::make_unique<ChannelsSettingsWidget>(getMainWindow());
	dialog->setChannelsData(PLSCHANNELS_API->getAllChannelInfo(), platform);
	dialog->exec();
}

void showChannelsSetting(int index)
{
	showChannelsSettingImpl(index);
}

void showChannelsSetting(const QString &platform)
{
	showChannelsSettingImpl(platform);
}

void sendInfoToWidzard(const QVariantMap &info, int type)
{

	QMetaObject::invokeMethod(
		PLSLaunchWizardView::instance(),
		[type, info]() {
			QVariantHash hashData;
			hashData.insert("type", type);
			hashData.insert("msg", QJsonObject::fromVariantMap(info));
			PLSLaunchWizardView::instance()->onPrismMessageCome(hashData);
		},
		Qt::QueuedConnection);
}

void sendLoadingState(bool isBussy)
{
	QVariantMap obj;
	auto state = isBussy ? int(ChannelData::PrismState::Bussy) : int(ChannelData::PrismState::Free);
	obj.insert(ChannelData::g_prismState, state);
	sendInfoToWidzard(obj, int(MessageType::AppState));
}

void sendChannelData(const QVariantMap &info)
{
	sendInfoToWidzard(info, int(MessageType::ChannelsModuleMsg));
}
void handleLauncherMsg(int type, const QJsonObject &data)
{

	switch (MessageType(type)) {
	// update schedule list
	case MessageType::GetScheduleList:
		PLSCHANNELS_API->startUpdateScheduleList();
		break;
	//clicked// active prism// switch on// show live info
	case MessageType::SelectChanel:
		if (PLSCHANNELS_API->isLiving() || PLSCHANNELS_API->isShifting()) {
			break;
		}
		activateWindow();
		if (!hasModalityWindow()) {
			selectChannel(data);
		}

		break;
	default:
		break;
	}
}

bool hasModalityWindow()
{
	if (QApplication::modalWindow()) {
		return true;
	}

	return false;
}

void activateWindow()
{
	auto win = PLSBasic::Get();
	if (win) {
		//win->SetShowing(true);
	}
}

void selectChannel(const QJsonObject &data)
{
	auto uuid = data.value(ChannelData::g_channelUUID).toString();
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	info.insert(ChannelData::g_displayState, true);
	info.insert(ChannelData::g_channelUserStatus, ChannelUserStatus::Enabled);
	info.insert(ChannelData::g_selectedSchedule, data);
	PLSCHANNELS_API->setChannelInfos(info);
	showLiveInfo(uuid);
	// reset
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_selectedSchedule, QJsonObject());
}

void testError()
{
	static bool isTo = true;
	QVariantMap info;
	if (isTo) {
		info.insert(shared_values::errorTitle, "Crash!(test)");
		info.insert(shared_values::errorContent, "Crash happenned!");
		info.insert(shared_values::errorTime, "time: " + QDateTime::currentDateTime().toString());
	}
	isTo = !isTo;
	sendInfoToWidzard(info, int(MessageType::ErrorInfomation));
}
