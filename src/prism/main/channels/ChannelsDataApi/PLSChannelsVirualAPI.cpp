#include "PLSChannelsVirualAPI.h"
#include <QMessageBox>
#include "ChannelConst.h"
#include <QUUid>
#include "PLSChannelDataAPI.h"
#include "PLSShareSourceItem.h"
#include "PLSChannelsEntrance.h"
#include "frontend-api.h"
#include "LoadingFrame.h"
#include <QMainWindow>
#include "NetWorkAPI.h"
#include <QJsonObject>
#include "PLSChannelDataHandler.h"
#include "LogPredefine.h"
#include "ChannelCommonFunctions.h"
#include "PLSLiveEndDialog.h"
#include "PLSPlatformApi/PLSLiveInfoDialogs.h"
#include "PLSChatDialog.h"
#include "PLSRtmpChannelView.h"
#include "PLSChatDialog.h"
#include "alert-view.hpp"
#include "window-basic-main.hpp"
#include <QDialog>
#include "main-view.hpp"
#include <qobject.h>
#include "PLSPlatformApi.h"
#include "window-basic-main.hpp"
#include "pls-net-url.hpp"

using namespace ChannelData;

#define BOOL_To_STR(x) (x) ? "true" : "false"

void showLiveInfo(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	pls_exec_live_Info(info);
	auto newInfo = PLSCHANNELS_API->getChannelInfo(uuid);
	if (newInfo != info) {
		PLSCHANNELS_API->channelModified(uuid);
	}
}

void showChannelInfo(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	int infoType = getInfo(info, g_data_type, NoType);

	switch (infoType) {
	case ChannelType: {
		showLiveInfo(uuid);
	} break;
	case RTMPType:
		editRTMP(uuid);
		break;
	default:
		break;
	}
}

bool updateChannelInfoFromNet(const QString &uuid)
{
	Q_ASSERT(!uuid.isEmpty());
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return false;
	}
	int type = getInfo(info, g_data_type, ChannelType);
	switch (type) {
	case ChannelType:
		return updateChannelTypeFromNet(uuid);
		break;
	case RTMPType:
		PLSCHANNELS_API->channelAdded(uuid);
		break;
	default:
		break;
	}
	PLSCHANNELS_API->holdOnChannelArea(false);
	return true;
}

bool updateChannelTypeFromNet(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return false;
	}
	QString name = getInfo(info, g_channelName, QString());
	if (name == TWITCH) {
		createTwitchHandler(uuid);
		PLSCHANNELS_API->sigSendRequest(uuid);
		return true;
	}

	if (name == YOUTUBE) {
		createYouTubeHandler(uuid);
		PLSCHANNELS_API->sigSendRequest(uuid);
		return true;
	}
	// to be done add more
	return false;
}

void refreshChannel(const QString &uuid)
{
	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	int type = getInfo(info, g_data_type, ChannelType);
	switch (type) {
	case ChannelType:
		updateChannelTypeFromNet(uuid);
		break;
	case RTMPType:
		resetRTMPState(uuid);
		break;
	default:
		break;
	}
}

void resetRTMPState(const QString &uuid)
{
	PLSCHANNELS_API->setChannelStatus(uuid, Valid);
	PLSCHANNELS_API->channelModified(uuid);
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
		PLSCHANNELS_API->removeChannelInfo(uuid);
		break;
	default:
		PLSCHANNELS_API->channelModified(uuid);
		break;
	}
}

void resetExpiredChannel(const QString &uuid)
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}
	if (PLSCHANNELS_API->isLiving()) {
		PLSCHANNELS_API->setChannelStatus(uuid, Expired);
		return;
	}

	auto info = PLSCHANNELS_API->getChannelInfo(uuid);
	if (info.isEmpty()) {
		return;
	}
	auto name = getInfo(info, g_channelName);
	auto ret = PLSAlertView::question(getMainWindow(), CHANNELS_TR(Confirm), CHANNELS_TR(expiredQuestion),
					  {{PLSAlertView::Button::Yes, CHANNELS_TR(Yes)}, {PLSAlertView::Button::Cancel, CHANNELS_TR(Cancel)}}, PLSAlertView::Button::Cancel);

	PLSCHANNELS_API->removeChannelInfo(uuid);
	if (ret != PLSAlertView::Button::Yes) {

		return;
	}

	QTimer::singleShot(500, [=]() { runCMD(name); });
	//runCMD(name);
}

bool isPrismReplyExpired(ReplyPtrs reply, const QByteArray &data)
{
	int netCode = getReplyStatusCode(reply);
	auto doc = QJsonDocument::fromJson(data);
	int code = 0;
	if (!doc.isNull()) {
		auto obj = doc.object();
		if (!obj.isEmpty()) {
			code = obj.value("code").toInt();
		}
	}
	if (netCode == 401 && code == 3000) {
		return true;
	}
	return false;
}

void reloginPrismExpired()
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}
	PLSAlertView::warning(getMainWindow(), QObject::tr("live.toast.title"), QObject::tr("main.message.prism.login.session.expired"));
	PLSBasic::Get()->setSessionExpired(true);
	pls_prism_change_over_login_view();
}

void resetAllChannels()
{
	auto channles = PLSCHANNELS_API->getAllChannelInfo();
	auto ite = channles.begin();
	while (ite != channles.end()) {
		int type = getInfo(*ite, g_data_type, NoType);
		switch (type) {
		case RTMPType:
			resetRTMPState(ite.key());
			break;
		case ChannelType:
			resetChannel(ite.key());
			break;
		default:
			break;
		}
		++ite;
	}
}

void refreshAllChannels()
{
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	auto channels = PLSCHANNELS_API->getAllChannelInfo();
	if (!channels.isEmpty()) {
		SemaphoreHolder holder(PLSCHANNELS_API->getSourceSemaphore());
		auto ite = channels.begin();
		while (ite != channels.end()) {
			//QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			if (PLSCHANNELS_API->hasError()) {
				return;
			}
			int dataState = getInfo(ite.value(), g_channelStatus, Error);
			if (dataState == Expired) {
				PLSCHANNELS_API->removeChannelInfo(ite.key());
			} else {
				refreshChannel(ite.key());
			}
			++ite;
		}
	}
	PLSCHANNELS_API->sigUpdateAllRtmp();
	if (!PLSCHANNELS_API->isInitilized()) {
		PLSCHANNELS_API->resetInitializeState(true);
	}
}

void gotuYoutube(const QString &uuid)
{
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}
	auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(GotoYoutubetoSet),
					  {{PLSAlertView::Button::Yes, CHANNELS_TR(GotoYoutubeOk)}, {PLSAlertView::Button::Cancel, CHANNELS_TR(Cancel)}}, PLSAlertView::Button::Cancel);

	auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	info[g_isUserAsked] = true;
	if (ret != PLSAlertView::Button::Yes) {
		return;
	}
	QDesktopServices::openUrl(QUrl(g_yoububeLivePage));
}

void addRTMP()
{
	auto rtmpInfo = createDefaultChannelInfoMap(SELECT, RTMPType);
	PLSRtmpChannelView *rtmpView = new PLSRtmpChannelView(rtmpInfo, getMainWindow());
	if (rtmpView->exec() == QDialog::Accepted) {
		rtmpInfo = rtmpView->SaveResult();
		rtmpInfo[g_channelStatus] = Valid;
		nowCheckAndVerify(rtmpInfo);
		PLSCHANNELS_API->addChannelInfo(rtmpInfo);
	}
	rtmpView->deleteLater();
}

void editRTMP(const QString &uuid)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(uuid);
	PLSRtmpChannelView *rtmpView = new PLSRtmpChannelView(channelInfo, getMainWindow());
	if (rtmpView->exec() == QDialog::Accepted) {
		channelInfo = rtmpView->SaveResult();
		PLSCHANNELS_API->backupInfo(uuid);
		PLSCHANNELS_API->setChannelInfos(channelInfo, false);
		PLSCHANNELS_API->sigTryToUpdateChannel(uuid);
	}
	rtmpView->deleteLater();
}

void runCMD(const QString &cmdStr)
{
	PRE_LOG_MSG(QString("run add channel:" + cmdStr).toStdString().c_str(), INFO);
	if (cmdStr == CUSTOM_RTMP) {
		addRTMP();
	} else if (gDefaultPlatform.contains(cmdStr)) {
		QVariantMap channelInfo = createDefaultChannelInfoMap(cmdStr);
		QJsonObject retObj = QJsonObject::fromVariantMap(channelInfo);
		if (pls_channel_login(retObj, cmdStr, getMainWindow())) {
			addLoginChannel(retObj);
		}
	}
}

bool checkChannelsState()
{
	if (PLSCHANNELS_API->currentSelectedCount() == 0) {
		PLSAlertView::warning(getMainWindow(), CHANNELS_TR(SelectOneErrorTitle), CHANNELS_TR(ErrorEmptyChannelMessage));
		return false;
	}
	return true;
}

bool startStreamingCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "startStreamingCheck", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :startStreamingCheck", ERROR);
		return false;
	}
	return retVal;
}

bool toGoLive()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool isok = QMetaObject::invokeMethod(mainw, "StartStreaming");
	if (!isok) {
		PRE_LOG("error :StartStreaming", ERROR);
	}
	return isok;
}

bool stopStreamingCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "stopStreamingCheck", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :startStreamingCheck", ERROR);
		return false;
	}
	return retVal;
}

bool toStopLive()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool isok = QMetaObject::invokeMethod(mainw, "StopStreaming");
	if (!isok) {
		PRE_LOG("error :StopStreaming", ERROR);
	}
	return isok;
}

bool startRecordCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "startRecordCheck", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :startRecordCheck ", ERROR);
	}
	return retVal && isok;
}

bool toRecord()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "StartRecording", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :StartRecording ", ERROR);
	}
	return isok && retVal;
}

bool stopRecordCheck()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "stopRecordCheck", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok) {
		PRE_LOG("error :stopRecordCheck ", ERROR);
	}
	return retVal && isok;
}

bool stopRecord()
{
	auto mainw = getMainWindow();
	Q_ASSERT(mainw);
	bool retVal = false;
	bool isok = QMetaObject::invokeMethod(mainw, "StopRecording", Qt::DirectConnection, Q_RETURN_ARG(bool, retVal));
	if (!isok || !retVal) {
		PRE_LOG("error :StopRecording ", ERROR);
	}
	return isok && retVal;
}

bool isNowChannel(const QString &uuid)
{
	auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	return isNowChannel(info);
}

bool isNowChannel(const QVariantMap &info)
{
	auto platName = getInfo(info, g_channelName);
	return platName.contains(NOW);
}

bool isEnabledNowExist()
{
	return !findExistEnabledNow().isEmpty();
}

QString findExistEnabledNow()
{
	QString ret;
	const auto &channels = PLSCHANNELS_API->getAllChannelInfoReference();
	auto isEnabledNow = [](const QVariantMap &info) { return isNowChannel(info) && getInfo(info, g_channelUserStatus, Disabled) == Enabled; };
	auto retIte = std::find_if(channels.cbegin(), channels.cend(), isEnabledNow);
	if (retIte != channels.cend()) {
		ret = retIte.key();
	}
	return ret;
}

void nowCheckAndVerify(const QVariantMap &Newinfo)
{
	if (isNowChannel(Newinfo)) {
		disableAll();
	} else {
		auto nowId = findExistEnabledNow();
		if (!nowId.isEmpty()) {
			PLSCHANNELS_API->setChannelUserStatus(nowId, Disabled);
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

void addLoginChannel(const QJsonObject &retJson)
{
	auto newInfo = retJson.toVariantMap();
	nowCheckAndVerify(newInfo);
	PLSCHANNELS_API->addChannelInfo(newInfo);
}

void showShareView(const QString &channelUUID)
{
	if (PLSCHANNELS_API->isChannelInfoExists(channelUUID)) {
		auto &info = PLSCHANNELS_API->getChanelInfoRef(channelUUID);
		auto *dialog = new PLSShareSourceItem(getMainWindow());
		dialog->setWindowFlag(Qt::Popup);
		dialog->initInfo(info);
		moveWidgetToParentCenter(dialog);
		dialog->show();
	}
}

void showEndView(bool isRecord)
{
	bool isClickToStopRecord = PLSCHANNELS_API->getIsClickToStopRecord();
	bool isLivingAndRecording = false;
	if (isRecord && PLS_PLATFORM_API->isLiving()) {
		isLivingAndRecording = true;
	} else if (!isRecord && PLS_PLATFORM_API->isRecording()) {
		isLivingAndRecording = true;
	}

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	bool keepRecordingWhenStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	//when stop streaming will stop record sametime.
	bool isStreamingRecordStopAuto = recordWhenStreaming && !keepRecordingWhenStreamStops;

	//when live and record stop sametime, will ignore record end page show and toast.
	static bool isIgnoreNextRecordShow = false;
	if (isStreamingRecordStopAuto && !isClickToStopRecord) {
		if (isRecord) {
			isIgnoreNextRecordShow = true;
		} else if (!isRecord && isLivingAndRecording) {
			isIgnoreNextRecordShow = true;
		}
	}

	PLS_INFO(END_MODULE, "Show end with parameter \nisLivingAndRecording:%s, \nisShowRecordEnd:%s, \nisClickToStopRecord:%s, \nisStreamingRecordStopAuto:%s, \nisIgnoreNextRecordShow:%s",
		 BOOL_To_STR(isLivingAndRecording), BOOL_To_STR(isRecord), BOOL_To_STR(isClickToStopRecord), BOOL_To_STR(isStreamingRecordStopAuto), BOOL_To_STR(isIgnoreNextRecordShow));

	if (isRecord) {
		if (isIgnoreNextRecordShow) {
			//ignore this end page;
			PLS_INFO(END_MODULE, "Show end with parameter ignore this record end page");
			isIgnoreNextRecordShow = false;
		} else if (isLivingAndRecording) {
			//record stoped, but live still streaming
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("main.message.error.recordwhenbroadcasting"));
		} else {
			//only record and live all stoped, to clear toast.
			if (PLSBasic::Get()->getMainView() != nullptr && PLSBasic::Get()->getMainView()->isVisible()) {
				auto mainwindw = getMainWindow();
				PLSLiveEndDialog *dialog = new PLSLiveEndDialog(isRecord, mainwindw);
				dialog->exec();
				delete dialog;
			}
			pls_toast_clear();
		}
		PLSCHANNELS_API->setIsClickToStopRecord(false);
		return;
	}

	if (isLivingAndRecording && !isStreamingRecordStopAuto) {
		//live ended, but still recording
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QObject::tr("broadcast.end.live"));
		return;
	}

	//only record and live all stoped, to clear toast.
	if (PLSBasic::Get()->getMainView() != nullptr && PLSBasic::Get()->getMainView()->isVisible()) {
		auto mainwindw = getMainWindow();
		PLSLiveEndDialog *dialog = new PLSLiveEndDialog(isRecord, mainwindw);
		dialog->exec();
		delete dialog;
	}
	pls_toast_clear();
}

int showSummaryView()
{
	return 0;
}

void showChatView(bool isRebackLogin, bool isOnlyShow, bool isOnlyInit) {}

LoadingFrame *createBusyFrame(QWidget *parent)
{
	LoadingFrame *loadView = new LoadingFrame(parent);
	loadView->initialize(":/Images/skin/loading-1.png", 500);
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

long getMaxRTMPcount()
{
	auto allChannels = PLSCHANNELS_API->getAllChannelInfoReference();
	long max = -1;
	for (const auto &channel : allChannels) {
		int type = getInfo(channel, g_data_type, ChannelType);
		if (type == ChannelType) {
			long count = getInfo(channel, g_displayOrder, 0L);
			if (count >= max) {
				max = count;
			}
		}
	}
	return max;
}

void showNetworkErrorAlert()
{
	auto errorMap = PLSCHANNELS_API->takeFirstError();
	if (!PLSCHANNELS_API->isInitilized()) {
		return;
	}

	if (!errorMap.isEmpty() && PLSCHANNELS_API->isEmptyToAcquire()) {
		SemaphoreHolder holder(PLSCHANNELS_API->getSourceSemaphore());
		PLSAlertView::warning(getMainWindow(), getInfo(errorMap, g_errorTitle), getInfo(errorMap, g_errorString));
		PLSCHANNELS_API->clearAllErrors();
	}
	PLSCHANNELS_API->holdOnChannelArea(false);
}
