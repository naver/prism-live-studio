#include "streamdeckplugin_module.h"
#include "actionhelp.h"
#include <QMainWindow>
#include <QAction>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QtNetwork>
#include <QLocalSocket>
#include <QtWebSockets/QtWebSockets>
#include "log/log.h"

#include "JSONUtils.h"

OBS_DECLARE_MODULE()

#define OBS_PORT 28195
#define PLS_PORT 28120
#define PLS_PLUGIN_PORT 9002

ActionHelp *actionHelpPtr = NULL;

#if !FOR_PRISM
QTcpServer *tcpServer = nullptr;
#else
QWebSocketServer *webSocketServer = nullptr;
#endif

ModuleHelper *moduleHelper = nullptr;

// ----------------------------------------------------------------------------
// OBS Module Callback
// ----------------------------------------------------------------------------
void ItemMuted(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr);

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata);

	//obs_source_t* source = (obs_source_t*)calldata_ptr(calldata, "source");
	//std::string name = obs_source_get_name(source);
	//std::string id = obs_source_get_id(source);
	//bool muted = calldata_bool(calldata, "muted");

	//    qDebug() << __FUNCTION__ << muted << QString::fromStdString(name) << type << QString::fromStdString(id) << QString::fromStdString(displayName);

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "SourcesService.sourceUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (actionHelpPtr)
		actionHelpPtr->WriteToSocket(str);
}

void UpdateSources()
{
	if (actionHelpPtr) {
		if (actionHelpPtr->SceneCollectionChangingFlag())
			return;
	}
	QList<SourceInfo> list;
	if (actionHelpPtr)
		actionHelpPtr->UpdateSourcesList(list);

	for (int i = 0; i < list.count(); i++) {
		SourceInfo srcInfo = list.at(i);
		signal_handler_t *signalHandler = obs_source_get_signal_handler(srcInfo.source);

		if (signalHandler == NULL) {
			continue;
		}

		signal_handler_connect(signalHandler, "mute", ItemMuted, nullptr);
	}
}

void ItemVisible(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr);

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata);

	//obs_sceneitem_t* sceneItem = (obs_sceneitem_t*)calldata_ptr(calldata, "item");
	//int64_t sceneItemId = obs_sceneitem_get_id(sceneItem);
	//obs_source_t* source = obs_sceneitem_get_source(sceneItem);
	//std::string name = obs_source_get_name(source);
	//std::string id = obs_source_get_id(source);
	//bool visible = calldata_bool(calldata, "visible");

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "ScenesService.itemUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (actionHelpPtr)
		actionHelpPtr->WriteToSocket(str);
}

void ItemsReorder(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr);

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata);

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "ScenesService.itemUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (actionHelpPtr)
		actionHelpPtr->WriteToSocket(str);
}

void UpdateScenes()
{
	QList<SceneInfo> list;

	actionHelpPtr->UpdateScenesList(list);

	for (int i = 0; i < list.count(); i++) {
		SceneInfo srcInfo = list.at(i);

		for (int j = 0; j < srcInfo.sceneItems.count(); j++) {
			SceneItemInfo sceneItemInfo = srcInfo.sceneItems.at(j);
			if (sceneItemInfo.isGroup) {

				signal_handler_t *sceneItemSignalHandler = obs_source_get_signal_handler(sceneItemInfo.source);

				if (sceneItemSignalHandler == NULL) {
					continue;
				}

				signal_handler_connect(sceneItemSignalHandler, "item_visible", ItemVisible, nullptr);
			}
		}

		signal_handler_t *signalHandler = obs_source_get_signal_handler(srcInfo.scene);

		if (signalHandler == NULL) {
			continue;
		}

		signal_handler_connect(signalHandler, "item_visible", ItemVisible, nullptr);
		signal_handler_connect(signalHandler, "reorder", ItemsReorder, nullptr);
	}
}

void OBSEvent(enum obs_frontend_event event, void *data)
{
	Q_UNUSED(data)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "starting";
			result["resourceId"] = "StreamingService.streamingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_STREAMING_STARTED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "live";
			result["resourceId"] = "StreamingService.streamingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPING: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "stopping";
			result["resourceId"] = "StreamingService.streamingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "offline";
			result["resourceId"] = "StreamingService.streamingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_RECORDING_STARTING: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "starting";
			result["resourceId"] = "StreamingService.recordingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_RECORDING_STARTED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "recording";
			result["resourceId"] = "StreamingService.recordingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPING: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "stopping";
			result["resourceId"] = "StreamingService.recordingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["data"] = "offline";
			result["resourceId"] = "StreamingService.recordingStatusChange";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			QMetaObject::invokeMethod(actionHelpPtr, "NotifySceneSwitched");
		}
	} break;

	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			QMetaObject::invokeMethod(actionHelpPtr, "NotifySceneSwitched");
		}
	} break;

	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED: {
		UpdateScenes();

		//"sceneAdded"-signal used as a generic changed signal. SD will request all collections then
		result["resourceId"] = "ScenesService.sceneAdded";
		eventJson["result"] = result;

		if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED: {
	} break;

	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED: {
	} break;

	case OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED: {
	} break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED: {
		if (actionHelpPtr) {
			actionHelpPtr->SetSceneCollectionChangingFlag(false);
		}
		UpdateSources();
		UpdateScenes();

		if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			QMetaObject::invokeMethod(actionHelpPtr, "NotifyCollectionChanged");
		}
	} break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED: {
		//Added signal used as "generic" changed signal. SD will request all collections then
		result["resourceId"] = "SceneCollectionsService.collectionAdded";
		eventJson["result"] = result;

		if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}

	} break;

	case OBS_FRONTEND_EVENT_PROFILE_CHANGED: {
	} break;

	case OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED: {
	} break;

	case OBS_FRONTEND_EVENT_EXIT: {
	} break;

	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED: {
		QMetaObject::invokeMethod(actionHelpPtr, "CheckStudioMode");
	} break;

	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED: {
		QMetaObject::invokeMethod(actionHelpPtr, "CheckStudioMode");
	} break;

	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["resourceId"] = "Mainview.windowLoadingFinished";
			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;

	default: {
	} break;
	}

	if (event == OBS_FRONTEND_EVENT_EXIT)
		FreeStreamDeckPlugin();

	// needed to reset the transition, but not working properly now (only some transitions work)
	// if (event == OBS_FRONTEND_EVENT_TRANSITION_STOPPED){
	//	SwitcherData *test = (SwitcherData*)switcher;
	//	test->transitionWaitMutex2.lock();
	//	this_thread::sleep_for(chrono::milliseconds(100)); //seems necesssary
	// since the transition is not always done
	//	test->transitionCv.notify_one();
	//	test->transitionWaitMutex2.unlock();
	//
	//}
}

void SourceDestroyed(void * /*data*/, calldata_t * /*calldata*/)
{
	if (moduleHelper)
		QMetaObject::invokeMethod(moduleHelper, "DefferUpdate", Qt::QueuedConnection);
}

void SourceCreate(void * /*data*/, calldata_t * /*calldata*/)
{
	if (moduleHelper)
		QMetaObject::invokeMethod(moduleHelper, "DefferUpdate", Qt::QueuedConnection);
}

void PLSEvent(pls_frontend_event event, const QVariantList &params, void *switcher)
{
	Q_UNUSED(switcher)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_SIGNOUT:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_VOLUME_MONTY_BACK:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END:
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_ALL_MUTE: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				result["data"] = params.at(0).toBool() ? "mute" : "unmute";
				result["resourceId"] = "Mainview.audioMixerMuteChanged";

				eventJson["result"] = result;

				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 1) {
				json data = json::object();
				data["id"] = static_cast<int>(params.at(0).value<ConfigId>());
				data["visible"] = params.at(1).toBool();
				result["data"] = data;
				result["resourceId"] = "Mainview.sideWindowVisibleChanged";

				eventJson["result"] = result;

				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_ALL_REGISTERD: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			result["resourceId"] = "Mainview.sideWindowAllRegisterd";
			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			actionHelpPtr->WriteToSocket(str);
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				PrismStatus info = params.at(0).value<PrismStatus>();
				result["resourceId"] = "Mainview.updateCPUUsage";
				json status = json::object();
				status["cpuUsage"] = std::string(QString::number(info.cpuUsage, 'f', 1).toUtf8().data());
				status["totalCpu"] = std::string(QString::number(info.totalCpu, 'f', 1).toUtf8().data());
				result["data"] = status;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				PrismStatus info = params.at(0).value<PrismStatus>();
				result["resourceId"] = "Mainview.updateFramedrop";
				json status = json::object();
				status["totalDrop"] = std::to_string(info.totalDrop);
				status["dropPercent"] = std::string(QString::number(info.dropPercent, 'f', 0).toUtf8().data());
				result["data"] = status;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				PrismStatus info = params.at(0).value<PrismStatus>();
				result["resourceId"] = "Mainview.updateBitrate";
				json status = json::object();
				status["kbitsPerSec"] = std::string(QString::number(info.kbitsPerSec, 'f', 0).toUtf8().data());
				status["mbitsPerSec"] = std::string(QString::number(info.kbitsPerSec / 1024.0, 'f', 1).toUtf8().data());
				result["data"] = status;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				int messageCount = params.at(0).toInt();
				json data = json::object();
				data["messageCount"] = messageCount;
				result["resourceId"] = "Mainview.updateNoticeMessageCount";
				result["data"] = data;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN_STATE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				auto loginState = params.at(0).toString();
				json data = json::object();
				data["loginState"] = std::string(loginState.toUtf8().data());
				result["resourceId"] = "Mainview.loginStateChanged";
				result["data"] = data;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				auto streamState = params.at(0).toString();
				json data = json::object();
				data["streamState"] = std::string(streamState.toUtf8().data());
				result["resourceId"] = "Mainview.streamStateChanged";
				result["data"] = data;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED: {
		if (actionHelpPtr && actionHelpPtr->GetIsRespondingStreamingFlag()) {
			if (params.size() > 0) {
				auto streamState = params.at(0).toString();
				json data = json::object();
				data["recordState"] = std::string(streamState.toUtf8().data());
				result["resourceId"] = "Mainview.recordStateChanged";
				result["data"] = data;
				eventJson["result"] = result;
				std::string str = eventJson.dump() + "\n";
				actionHelpPtr->WriteToSocket(str);
			}
		}
	} break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED: {
		if (actionHelpPtr) {
			actionHelpPtr->SetSceneCollectionChangingFlag(true);
		}
	} break;
	default:
		break;
	}
}

void SaveCallback(obs_data_t *save_data, bool saving, void *)
{
	Q_UNUSED(save_data)

	static bool first = true;
	if (first) {
		// connect source changing state signal.
		first = false;

		UpdateSources();
		UpdateScenes();
	} else if (saving) {

		//QMetaObject::invokeMethod(&moduleHelper, "UpdateSourceList", Qt::QueuedConnection);
	}
}

void FreeStreamDeckPlugin()
{
	obs_frontend_remove_save_callback(SaveCallback, nullptr);
	obs_frontend_remove_event_callback(OBSEvent, nullptr);
	pls_frontend_remove_event_callback(PLSEvent, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_destroy", SourceDestroyed, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create", SourceCreate, nullptr);

	if (moduleHelper) {
		delete moduleHelper;
		moduleHelper = nullptr;
	}

	if (actionHelpPtr) {
		QObject::disconnect(webSocketServer, &QWebSocketServer::newConnection, actionHelpPtr, &ActionHelp::SDClientConnected);
		actionHelpPtr->StopSocket();
		qDebug() << __FUNCTION__ << "Delete Action Helper";
		delete actionHelpPtr;
		actionHelpPtr = NULL;
	}

	if (webSocketServer) {
		delete webSocketServer;
		webSocketServer = NULL;
	}
}

void InitStreamDeckPlugin()
{
	// action helper, some action must in main thread.
	actionHelpPtr = new ActionHelp;
	moduleHelper = new ModuleHelper;

	webSocketServer = new QWebSocketServer(QStringLiteral("prism websocket server"), QWebSocketServer::NonSecureMode);

	QObject::connect(webSocketServer, &QWebSocketServer::newConnection, actionHelpPtr, &ActionHelp::SDClientConnected);

	// Setup event handler to start listen.
	auto eventCallback = [](enum obs_frontend_event event, void *param) {
		if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			if (webSocketServer) {
				if (!webSocketServer->listen(QHostAddress::LocalHost, PLS_PORT)) {
					PLS_ERROR(FRONTEND_PLUGINS_STREAM_DECK, "[STREAM-DECK]start listen port %d faild", PLS_PORT);
					return;
				}
				// setup obs event callback
				obs_frontend_add_save_callback(SaveCallback, nullptr);
				obs_frontend_add_event_callback(OBSEvent, nullptr);
				pls_frontend_add_event_callback(PLSEvent, nullptr);
				signal_handler_connect_ref(obs_get_signal_handler(), "source_destroy", SourceDestroyed, nullptr);
				signal_handler_connect_ref(obs_get_signal_handler(), "source_create", SourceCreate, nullptr);
			}
			obs_frontend_remove_event_callback((obs_frontend_event_cb)param, nullptr);
		}
	};
	obs_frontend_add_event_callback(eventCallback, (void *)(obs_frontend_event_cb)eventCallback);
}

// ----------------------------------------------------------------------------
bool obs_module_load(void)
{
	InitStreamDeckPlugin();
	return true;
}

void obs_module_unload(void)
{
	FreeStreamDeckPlugin();
}

void ModuleHelper::UpdateSourceList()
{
	UpdateSources();

	if (actionHelpPtr && actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
		json eventJson;
		eventJson["jsonrpc"] = "2.0";
		json result = json::object();
		result["_type"] = "EVENT";
		eventJson["id"] = nullptr;

		result["resourceId"] = "SourcesService.sourceUpdated";
		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		actionHelpPtr->WriteToSocket(str);
	}
}

void ModuleHelper::DefferUpdate()
{
	timerUpdate.start(200);
}
