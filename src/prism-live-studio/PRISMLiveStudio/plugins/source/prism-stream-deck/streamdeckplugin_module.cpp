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
#include "liblog.h"
#include "../../../prism-ui/log/module_names.h"

#include "JSONUtils.h"

OBS_DECLARE_MODULE()

const quint64 PLS_PORT = 28120;

namespace {
struct LocalGlobalVars {
	static ActionHelp *actionHelpPtr;
	static QWebSocketServer *webSocketServer;
	static ModuleHelper *moduleHelper;
};
ActionHelp *LocalGlobalVars::actionHelpPtr = nullptr;
QWebSocketServer *LocalGlobalVars ::webSocketServer = nullptr;
ModuleHelper *LocalGlobalVars::moduleHelper = nullptr;
}

QWebSocketServer *GetWebsocketServer()
{
	return LocalGlobalVars::webSocketServer;
}

// ----------------------------------------------------------------------------
// OBS Module Callback
// ----------------------------------------------------------------------------
void ItemMuted(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr)

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "SourcesService.sourceUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (LocalGlobalVars::actionHelpPtr)
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
}

void UpdateSources()
{
	if (LocalGlobalVars::actionHelpPtr) {
		if (LocalGlobalVars::actionHelpPtr->SceneCollectionChangingFlag())
			return;
	}
	QList<SourceInfo> list;
	if (LocalGlobalVars::actionHelpPtr)
		LocalGlobalVars::actionHelpPtr->UpdateSourcesList(list);

	for (int i = 0; i < list.count(); i++) {
		SourceInfo srcInfo = list.at(i);
		signal_handler_t *signalHandler = obs_source_get_signal_handler(srcInfo.source);

		if (signalHandler == nullptr) {
			continue;
		}

		signal_handler_connect(signalHandler, "mute", ItemMuted, nullptr);
	}
}

void ItemVisible(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr)

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "ScenesService.itemUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (LocalGlobalVars::actionHelpPtr)
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
}

void ItemsReorder(void *ptr, calldata_t *calldata)
{
	Q_UNUSED(ptr)

	//We could use the data, when introducing a new event, which may take them as parameter
	Q_UNUSED(calldata)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	result["resourceId"] = "ScenesService.itemUpdated";
	eventJson["result"] = result;

	std::string str = eventJson.dump() + "\n";

	if (LocalGlobalVars::actionHelpPtr)
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
}

void UpdateScenes()
{
	QList<SceneInfo> list;

	LocalGlobalVars::actionHelpPtr->UpdateScenesList(list);

	for (int i = 0; i < list.count(); i++) {
		SceneInfo srcInfo = list.at(i);

		for (int j = 0; j < srcInfo.sceneItems.count(); j++) {
			SceneItemInfo sceneItemInfo = srcInfo.sceneItems.at(j);
			if (!sceneItemInfo.isGroup)
				continue;

			signal_handler_t *sceneItemSignalHandler = obs_source_get_signal_handler(sceneItemInfo.source);

			if (sceneItemSignalHandler == nullptr) {
				continue;
			}

			signal_handler_connect(sceneItemSignalHandler, "item_visible", ItemVisible, nullptr);
		}

		signal_handler_t *signalHandler = obs_source_get_signal_handler(srcInfo.scene);

		if (signalHandler == nullptr) {
			continue;
		}

		signal_handler_connect(signalHandler, "item_visible", ItemVisible, nullptr);
		signal_handler_connect(signalHandler, "reorder", ItemsReorder, nullptr);
	}
}

// OBS events handler
void obs_event_streaming_starting(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "starting";
		result["resourceId"] = "StreamingService.streamingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_streaming_started(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "live";
		result["resourceId"] = "StreamingService.streamingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_streaming_stopping(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "stopping";
		result["resourceId"] = "StreamingService.streamingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_streaming_stopped(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "offline";
		result["resourceId"] = "StreamingService.streamingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_recording_starting(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "starting";
		result["resourceId"] = "StreamingService.recordingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_recording_started(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "recording";
		result["resourceId"] = "StreamingService.recordingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_recording_stopping(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "stopping";
		result["resourceId"] = "StreamingService.recordingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}
void obs_event_recording_stopped(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["data"] = "offline";
		result["resourceId"] = "StreamingService.recordingStatusChange";

		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_scene_list_changed(json &result, json &eventJson)
{
	UpdateScenes();

	//"sceneAdded"-signal used as a generic changed signal. SD will request all collections then
	result["resourceId"] = "ScenesService.sceneAdded";
	eventJson["result"] = result;

	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_scene_collection_changed(const json &result, const json &eventJson)
{
	Q_UNUSED(result)
	Q_UNUSED(eventJson)
	if (LocalGlobalVars::actionHelpPtr) {
		LocalGlobalVars::actionHelpPtr->SetSceneCollectionChangingFlag(false);
	}
	UpdateSources();
	UpdateScenes();

	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
		QMetaObject::invokeMethod(LocalGlobalVars::actionHelpPtr, "NotifyCollectionChanged");
	}
}

void obs_event_scene_collection_list_changed(json &result, json &eventJson)
{
	//Added signal used as "generic" changed signal. SD will request all collections then
	result["resourceId"] = "SceneCollectionsService.collectionAdded";
	eventJson["result"] = result;

	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void obs_event_loading_finished(json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["resourceId"] = "Mainview.windowLoadingFinished";
		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

static void OBSEvent(enum obs_frontend_event event, void *data)
{
	Q_UNUSED(data)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		obs_event_streaming_starting(result, eventJson);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		obs_event_streaming_started(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		obs_event_streaming_stopping(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		obs_event_streaming_stopped(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		obs_event_recording_starting(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		obs_event_recording_started(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		obs_event_recording_stopping(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		obs_event_recording_stopped(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			QMetaObject::invokeMethod(LocalGlobalVars::actionHelpPtr, "NotifySceneSwitched");
		}
		break;

	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
			QMetaObject::invokeMethod(LocalGlobalVars::actionHelpPtr, "NotifySceneSwitched");
		}
		break;
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		obs_event_scene_list_changed(result, eventJson);
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		obs_event_scene_collection_changed(result, eventJson);
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED:
		obs_event_scene_collection_list_changed(result, eventJson);
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
		QMetaObject::invokeMethod(LocalGlobalVars::actionHelpPtr, "CheckStudioMode");
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		QMetaObject::invokeMethod(LocalGlobalVars::actionHelpPtr, "CheckStudioMode");
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		obs_event_loading_finished(result, eventJson);
		break;

	default:
		break;
	}

	if (event == OBS_FRONTEND_EVENT_EXIT)
		FreeStreamDeckPlugin();
}

void SourceDestroyed(void *data, calldata_t *calldata)
{
	pls_unused(data, calldata);
	if (LocalGlobalVars::moduleHelper)
		QMetaObject::invokeMethod(LocalGlobalVars::moduleHelper, "DefferUpdate", Qt::QueuedConnection);
}

void SourceCreate(void *data, calldata_t *calldata)
{
	pls_unused(data, calldata);
	if (LocalGlobalVars::moduleHelper)
		QMetaObject::invokeMethod(LocalGlobalVars::moduleHelper, "DefferUpdate", Qt::QueuedConnection);
}

// Handle PRISM frontend events
void pls_event_all_mute(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			result["data"] = params.at(0).toBool() ? "mute" : "unmute";
			result["resourceId"] = "Mainview.audioMixerMuteChanged";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_side_window_visible_changed(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 1) {
			json data = json::object();
			data["id"] = static_cast<int>(params.at(0).value<ConfigId>());
			data["visible"] = params.at(1).toBool();
			result["data"] = data;
			result["resourceId"] = "Mainview.sideWindowVisibleChanged";

			eventJson["result"] = result;

			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_side_window_all_registered(const QVariantList &params, json &result, json &eventJson)
{
	Q_UNUSED(params)
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		result["resourceId"] = "Mainview.sideWindowAllRegisterd";
		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void pls_event_update_cpu_usage(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			PrismStatus info = params.at(0).value<PrismStatus>();
			result["resourceId"] = "Mainview.updateCPUUsage";
			json status = json::object();
			status["cpuUsage"] = std::string(QString::number(info.cpuUsage, 'f', 1).toUtf8().data());
			status["totalCpu"] = std::string(QString::number(info.totalCpu, 'f', 1).toUtf8().data());
			result["data"] = status;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_update_frame_drop(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			PrismStatus info = params.at(0).value<PrismStatus>();
			result["resourceId"] = "Mainview.updateFramedrop";
			json status = json::object();
			status["totalDrop"] = std::to_string(info.totalDrop);
			status["dropPercent"] = std::string(QString::number(info.dropPercent, 'f', 0).toUtf8().data());
			result["data"] = status;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_update_bitrate(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			PrismStatus info = params.at(0).value<PrismStatus>();
			result["resourceId"] = "Mainview.updateBitrate";
			json status = json::object();
			status["kbitsPerSec"] = std::string(QString::number(info.kbitsPerSec, 'f', 0).toUtf8().data());
			status["mbitsPerSec"] = std::string(QString::number(info.kbitsPerSec / 1024.0, 'f', 1).toUtf8().data());
			result["data"] = status;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_update_notice_message(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			int messageCount = params.at(0).toInt();
			json data = json::object();
			data["messageCount"] = messageCount;
			result["resourceId"] = "Mainview.updateNoticeMessageCount";
			result["data"] = data;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}
void pls_event_update_login_state_changed(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			auto loginState = params.at(0).toString();
			json data = json::object();
			data["loginState"] = std::string(loginState.toUtf8().data());
			result["resourceId"] = "Mainview.loginStateChanged";
			result["data"] = data;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}

void pls_event_stream_state_changed(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			auto streamState = params.at(0).toString();
			json data = json::object();
			data["streamState"] = std::string(streamState.toUtf8().data());
			result["resourceId"] = "Mainview.streamStateChanged";
			result["data"] = data;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}
void pls_event_record_state_changed(const QVariantList &params, json &result, json &eventJson)
{
	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingStreamingFlag()) {
		if (params.size() > 0) {
			auto streamState = params.at(0).toString();
			json data = json::object();
			data["recordState"] = std::string(streamState.toUtf8().data());
			result["resourceId"] = "Mainview.recordStateChanged";
			result["data"] = data;
			eventJson["result"] = result;
			std::string str = eventJson.dump() + "\n";
			LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
		}
	}
}
void pls_event_scene_collection_about_to_change(const QVariantList &params, const json &, const json &)
{
	Q_UNUSED(params)
	if (LocalGlobalVars::actionHelpPtr) {
		LocalGlobalVars::actionHelpPtr->SetSceneCollectionChangingFlag(true);
	}
}

static void PLSEvent(pls_frontend_event event, const QVariantList &params, void *switcher)
{
	Q_UNUSED(switcher)

	json eventJson;
	eventJson["jsonrpc"] = "2.0";
	json result = json::object();
	result["_type"] = "EVENT";
	eventJson["id"] = nullptr;

	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_ALL_MUTE:
		pls_event_all_mute(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_VISIBLE_CHANGED:
		pls_event_side_window_visible_changed(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SIDE_WINDOW_ALL_REGISTERD:
		pls_event_side_window_all_registered(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_CPU_USAGE:
		pls_event_update_cpu_usage(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_FRAME_DROP:
		pls_event_update_frame_drop(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_BITRATE:
		pls_event_update_bitrate(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_UPDATE_NOTICE_MESSAGE:
		pls_event_update_notice_message(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED:
		pls_event_stream_state_changed(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED:
		pls_event_record_state_changed(params, result, eventJson);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED:
		pls_event_scene_collection_about_to_change(params, result, eventJson);
		break;
	default:
		break;
	}
}

static void SaveCallback(obs_data_t *save_data, bool saving, void *)
{
	Q_UNUSED(save_data)
	Q_UNUSED(saving)

	static bool first = true;
	if (first) {
		// connect source changing state signal.
		first = false;

		UpdateSources();
		UpdateScenes();
	}
}

void FreeStreamDeckPlugin()
{
	obs_frontend_remove_save_callback(SaveCallback, nullptr);
	obs_frontend_remove_event_callback(OBSEvent, nullptr);
	pls_frontend_remove_event_callback(PLSEvent, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_destroy", SourceDestroyed, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create", SourceCreate, nullptr);

	if (LocalGlobalVars::moduleHelper) {
		pls_delete(LocalGlobalVars::moduleHelper, nullptr);
	}

	if (LocalGlobalVars::actionHelpPtr) {
		QObject::disconnect(LocalGlobalVars::webSocketServer, &QWebSocketServer::newConnection, LocalGlobalVars::actionHelpPtr, &ActionHelp::SDClientConnected);
		LocalGlobalVars::actionHelpPtr->StopSocket();
		qDebug() << __FUNCTION__ << "Delete Action Helper";
		pls_delete(LocalGlobalVars::actionHelpPtr, nullptr);
	}

	if (LocalGlobalVars::webSocketServer) {
		pls_delete(LocalGlobalVars::webSocketServer, nullptr);
	}
}

void InitStreamDeckPlugin()
{
	// action helper, some action must in main thread.
	LocalGlobalVars::actionHelpPtr = pls_new<ActionHelp>();
	LocalGlobalVars::moduleHelper = pls_new<ModuleHelper>();

	LocalGlobalVars::webSocketServer = pls_new<QWebSocketServer>(QStringLiteral("prism websocket server"), QWebSocketServer::NonSecureMode);

	QObject::connect(LocalGlobalVars::webSocketServer, &QWebSocketServer::newConnection, LocalGlobalVars::actionHelpPtr, &ActionHelp::SDClientConnected);

	// Setup event handler to start listen.
	auto eventCallback = [](enum obs_frontend_event event, void *param) {
		if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			if (LocalGlobalVars::webSocketServer) {
				if (!LocalGlobalVars::webSocketServer->listen(QHostAddress::LocalHost, PLS_PORT)) {
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

void ModuleHelper::UpdateSourceList() const
{
	UpdateSources();

	if (LocalGlobalVars::actionHelpPtr && LocalGlobalVars::actionHelpPtr->GetIsRespondingCollectionsSchemaFlag()) {
		json eventJson;
		eventJson["jsonrpc"] = "2.0";
		json result = json::object();
		result["_type"] = "EVENT";
		eventJson["id"] = nullptr;

		result["resourceId"] = "SourcesService.sourceUpdated";
		eventJson["result"] = result;

		std::string str = eventJson.dump() + "\n";
		LocalGlobalVars::actionHelpPtr->WriteToSocket(str);
	}
}

void ModuleHelper::DefferUpdate()
{
	timerUpdate.start(200);
}
