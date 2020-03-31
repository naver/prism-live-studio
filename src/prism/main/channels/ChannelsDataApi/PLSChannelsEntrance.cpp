#include <obs.h>
#include <obs-module.h>
#include "pls-app.hpp"
#include <QDebug>
#include <QDockWidget>
#include <QLabel>
#include "LogPredefine.h"
#include "signal.h"
#include "PLSChannelsEntrance.h"
#include "PLSChannelDataAPI.h"
#include <QFile>
#include <QToolBar>
#include <QPushButton>
#include <QFileDialog>
#include "ChannelCapsule.h"
#include "ChannelCommonFunctions.h"
#include "frontend-api.h"
#include "PLSChannelsArea.h"
#include "log.h"
#include "window-basic-main.hpp"
#include <QHBoxLayout>
#include "PLSChannelsVirualAPI.h"
#include <QThread>

void onLiveStateChanged(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		PLSCHANNELS_API->setBroadcastState(ChannelData::StreamStarting);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		PLSCHANNELS_API->setBroadcastState(ChannelData::StreamStarted);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		PLSCHANNELS_API->setBroadcastState(ChannelData::StreamStopping);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		//PLSCHANNELS_API->setBroadcastState(ChannelData::StreamStopped);
		break;
		/*recording       start       */
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		PLSCHANNELS_API->setRecordState(ChannelData::RecordStarting);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		PLSCHANNELS_API->setRecordState(ChannelData::RecordStarted);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		PLSCHANNELS_API->setRecordState(ChannelData::RecordStopping);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		PLSCHANNELS_API->setRecordState(ChannelData::RecordStopped);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		break;
		/*recording    end          */
	case OBS_FRONTEND_EVENT_EXIT:
		PLSCHANNELS_API->exitApi();
		break;
	default:
		break;
	}
};

void onUserLoginStateChanged(pls_frontend_event event, const QVariantList &, void *)
{
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT:
		//PLSCHANNELS_API->stopAll();
		PLSCHANNELS_API->clearAll();
		PLSCHANNELS_API->saveData();
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN:
		initChannelsData();
	default:
		break;
	}
}

//init channel ui
bool initChannelUI()
{
	auto window = PLSBasic::Get();
	if (window) {
		PRE_LOG(Initialize, INFO);
		auto thread = new QThread;
		//start a thread for data handle
		PLSCHANNELS_API->moveToNewThread(thread);
		thread->start();
		QHBoxLayout *layout = new QHBoxLayout(window->getChannelsContainer());
		layout->setMargin(0);
		layout->setSpacing(0);

		auto channelArea = new PLSChannelsArea;
		layout->addWidget(channelArea);

		setStyleSheetFromFile(channelArea, ":/Style/PLSChannelsArea.css", ":/Style/ChannelCapsule.css", ":/Style/ChannelConfigPannel.css", ":/Style/GoLivePannel.css",
				      ":/Style/PLSRTMPConfig.css", ":/Style/DefaultPlatformAddList.css", ":/Style/ChannelsAddWin.css");
		obs_frontend_add_event_callback(onLiveStateChanged, nullptr);
		pls_frontend_add_event_callback(onUserLoginStateChanged, nullptr);

		return true;
	}
	return false;
}

void initChannelsData()
{
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PLSCHANNELS_API->resetInitializeState(false);
	PLSCHANNELS_API->updateRtmpInfos();
	PLSCHANNELS_API->sigRefreshAllChannels();
}

QMainWindow *getMainWindow()
{
	return PLSBasic::Get();
}
