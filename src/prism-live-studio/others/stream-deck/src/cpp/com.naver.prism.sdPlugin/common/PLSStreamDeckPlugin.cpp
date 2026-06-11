//==============================================================================
/**
@file       PLSStreamDeckPlugin.cpp

@brief      prism live studio plugin

@copyright  (c) 2018, Corsair Memory, Inc.
                        This source code is licensed under the MIT-style license
found in the LICENSE file.

**/
//==============================================================================

#if defined(_WIN32)
#include "../windows/pch.h"
#elif defined(__APPLE__)
#include "../mac/pch.h"
#endif
#include "PLSStreamDeckPlugin.h"
#include <atomic>
#include "ESDConnectionManager.h"
#include "PLSConnectionManager.h"

const static int MAX_MESSAGE_COUNT = 999;
bool isWindowVisible(int id, const json &windowsInfo);


PLSStreamDeckPlugin::~PLSStreamDeckPlugin() {}

void PLSStreamDeckPlugin::KeyDownForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) {}

void PLSStreamDeckPlugin::KeyUpForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	bool isConnect = mPLSConnectionManager->PrismDidConnect();
	if (!isConnect) {
		resetKeyState(inAction, inContext);
		showAlert(inContext, false);
		return;
	}
	std::string jsn = inPayload.dump();
	auto state = EPLJSONUtils::GetIntByName(inPayload, "state", -1);
	if (state == -1) {
		return;
	}
	json settings;
	if (!EPLJSONUtils::GetObjectByName(inPayload, "settings", settings)) {
		return;
	}

	// check if user is logined.
	if (mPrismLoginState.empty() || mPrismLoginState == LOGIN_STATE_LOGINING || mPrismLoginState == LOGIN_STATE_LOGIN_FAILED) {
		showAlert(inContext);
		return;
	}

	handleKeyUp(settings, inAction, inContext, state);
}

void PLSStreamDeckPlugin::WillAppearForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	// Remember the context
	mActionContext[inAction].insert(std::make_pair(inContext, inPayload));
	if (!mPrismConnected) {
		resetKeyState(inAction, inContext);
	} else {
		if (KEY_UUID_RECORD == inAction) {
			mConnectionManager->SetState(getRecordKeyState(mPrismRecordState), inContext);
		} else if (KEY_UUID_STREAM == inAction) {
			mConnectionManager->SetState(getStreamKeyState(mPrismStreamState), inContext);
		} else if (KEY_UUID_MUTE_ALL == inAction) {
			setAllMuteState(mAllMute, inContext);
		} else if (KEY_UUID_STUDIO_MODE == inAction) {
			mConnectionManager->SetState(mStudioModeState ? State_On : State_off, inContext);
		} else if (KEY_UUID_TOAST_MESSAGE == inAction) {
			mConnectionManager->SetState((mToastMessageCount > 0) ? State_On : State_off, inContext);
		} else {
			checkStateBySetting(inAction, inContext, inPayload);
		}
	}
}

void PLSStreamDeckPlugin::WillDisappearForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	// Remove the context
	mActionContext[inAction].erase(inContext);
}

void PLSStreamDeckPlugin::DeviceDidConnect(const std::string &inDeviceID, const json &inDeviceInfo)
{
	// Nothing to do
}

void PLSStreamDeckPlugin::DeviceDidDisconnect(const std::string &inDeviceID)
{
	// Nothing to do
}

void PLSStreamDeckPlugin::DidReceiveSettings(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	std::string payload = inPayload.dump();
	updateSettings(inAction, inContext, inPayload);
	checkStateBySetting(inAction, inContext, inPayload);
}

void PLSStreamDeckPlugin::SendToPlugin(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	if (nullptr != mPLSConnectionManager) {
		std::string data = inPayload.dump();
		json dataObj;
		if (EPLJSONUtils::GetObjectByName(inPayload, "data", dataObj)) {
			auto rpcId = EPLJSONUtils::GetIntByName(dataObj, "id");
			auto param = EPLJSONUtils::GetStringByName(dataObj, "param");
			if (rpcId > 0) {

				json result;
				result["id"] = rpcId;

				switch (rpcId) {
				case RPC_ID_getSources: {
					if (param.empty())
						return;
					do {
						if (mCollections.find(param) != mCollections.end()) {
							std::string collections = mCollections.dump();
							if (mCollections[param].find("sources") != mCollections[param].end()) {
								auto data = mCollections[param]["sources"];
								if (!data.is_null()) {
									result["data"] = mCollections[param]["sources"];
									mConnectionManager->SendToPropertyInspector(inAction, inContext, result);
									break;
								}
							}
						}
						getSources(param);
					} while (false);
				} break;
				case RPC_ID_getScenes: {
					if (param.empty())
						return;
					do {
						if (mCollections.find(param) != mCollections.end()) {
							std::string collections = mCollections.dump();
							std::string resString = result.dump();
							if (mCollections[param].find("scenes") != mCollections[param].end()) {
								auto data = mCollections[param]["scenes"];
								if (!data.is_null()) {
									result["data"] = mCollections[param]["scenes"];
									mConnectionManager->SendToPropertyInspector(inAction, inContext, result);
									break;
								}
							}
						}
						getSceneAndSource(param);
					} while (false);
				} break;
				case RPC_ID_getSideWindowInfo: {
					result["data"] = mWindowInfo;
					std::string resString = result.dump();
					mConnectionManager->SendToPropertyInspector(inAction, inContext, result);
				} break;
				case RPC_ID_getPrismConnectionState: {
					result["data"] = mPrismConnected;
					std::string resString = result.dump();
					mConnectionManager->SendToPropertyInspector(inAction, inContext, result);
				} break;
				case RPC_ID_fetchSceneCollectionsSchema: {
					json collections = json::object();
					collections["activeSceneCollection"] = mCurrentActiveCollection;
					collections["collectionList"] = mSceneCollections;
					result["data"] = collections;
					mConnectionManager->SendToPropertyInspector(inAction, inContext, result);
				} break;
				default:
					break;
				}
			}
		}
	}
}

void PLSStreamDeckPlugin::PropertyInspectorDidAppear(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID)
{
	mMutex.lock();
	mCurrentInspector = std::make_pair(inContext, inAction);
	mMutex.unlock();
	if (nullptr != mPLSConnectionManager) {
		bool isConnect = mPLSConnectionManager->PrismDidConnect();
		json payload;
		payload["isConnected"] = isConnect;
		mConnectionManager->SendToPropertyInspector(inAction, inContext, payload);
	}
}

void PLSStreamDeckPlugin::UpdateSceneCollectionList(const json &inPayload)
{
	if (mConnectionManager != nullptr) {
		mConnectionManager->SendToPropertyInspector(mCurrentInspector.second, mCurrentInspector.first, inPayload);
	}
}

void PLSStreamDeckPlugin::SendToPropertyInspector(const json &payload)
{
	if (nullptr != mConnectionManager) {
		mConnectionManager->SendToPropertyInspector(mCurrentInspector.second, mCurrentInspector.first, payload);
	}
}

void PLSStreamDeckPlugin::DidReceiveFromPrism(const json &payload)
{
	int rpcID = EPLJSONUtils::GetIntByName(payload, "id", -1);
	//SendToPropertyInspector(payload);
	if (rpcID > 0) {
		handleRequests(rpcID, payload);
	} else {
		handleEvents(payload);
	}
}

void PLSStreamDeckPlugin::LogMessage(const std::string &message)
{
	if (nullptr != mConnectionManager) {
		#if defined(_WIN32)
		char pi[30];
		sprintf_s(pi, "[process id:%lu] ", GetCurrentProcessId());
		std::string result(pi);
		result.append(message);
		mConnectionManager->LogMessage(message);
		#elif defined (__APPLE__)
		mConnectionManager->LogMessage(message);
		#endif
	}
}

void PLSStreamDeckPlugin::StreamDeckDisconnected()
{
	if (mPLSConnectionManager) {
		mPLSConnectionManager->Disconnect();
		PLSStreamDeckPlugin::GetApp()->stop();
	}
}

extern websocketpp::lib::asio::io_service *app;
websocketpp::lib::asio::io_service *PLSStreamDeckPlugin::GetApp()
{
	return app;
}

void PLSStreamDeckPlugin::LogMessage(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int messageLength = vsnprintf(nullptr, 0, format, args);
	if (messageLength > 0) {
		char *buffer = (char *)malloc(messageLength + 1);
		if (buffer) {
			vsnprintf(buffer, messageLength, format, args);
			LogMessage(std::string(buffer));
			free(buffer);
		}
	}
	va_end(args);
}

void PLSStreamDeckPlugin::updateSettings(const std::string &inAction, const std::string &inContext, const json &inPayload)
{
	if (mActionContext.find(inAction) != mActionContext.end()) {
		auto &action = mActionContext[inAction];
		if (action.find(inContext) != action.end()) {
			action[inContext] = inPayload;
		}
	}
}

void PLSStreamDeckPlugin::setState(const std::string &inAction, int state)
{
	if (mConnectionManager != nullptr) {
		//mMutex.lock();
		if (mActionContext.find(inAction) != mActionContext.end()) {
			const auto &context = mActionContext[inAction];
			for (const auto &item : context) {
				mConnectionManager->SetState(state, item.first);
			}
		}
		//mMutex.unlock();
	}
}

void PLSStreamDeckPlugin::setSetings(const std::string &key, const std::string &value)
{
	if (mConnectionManager != nullptr) {
		//mMutex.lock();
		mSettings[key] = value;
		mConnectionManager->SetSettings(mSettings, mCurrentInspector.first);
		//mMutex.unlock();
	}
}

void PLSStreamDeckPlugin::setSetings(const std::string &key, const json &value)
{
	if (mConnectionManager != nullptr) {
		mSettings[key] = value;
		mConnectionManager->SetSettings(mSettings, mCurrentInspector.first);
	}
}

void PLSStreamDeckPlugin::setSetings(const std::string &inAction, const std::string &key, const json &value)
{
	if (mActionContext.find(inAction) != mActionContext.end()) {
		auto context = mActionContext[inAction];
		for (const auto &item : context) {
			json contextData = item.second;
			json settings = json::object();
			EPLJSONUtils::GetObjectByName(contextData, "settings", settings);
			settings[key] = value;
			mConnectionManager->SetSettings(settings, item.first);
		}
	}
}

void PLSStreamDeckPlugin::sendToPropertyInspector(const std::string &inAction, const json &payload)
{
	if (mConnectionManager != nullptr) {
		if (mActionContext.find(inAction) != mActionContext.end()) {
			const auto &context = mActionContext[inAction];
			for (const auto &item : context) {
				mConnectionManager->SendToPropertyInspector(inAction, item.first, payload);
			}
		}
	}
}

void PLSStreamDeckPlugin::handleEvents(const json &payload)
{
	json resultObj;
	if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
		auto resourceId = EPLJSONUtils::GetStringByName(resultObj, "resourceId");
		auto data = EPLJSONUtils::GetStringByName(resultObj, "data");
		if (!data.empty()) {
			if (EVENT_MUTE_ALL_STATUS_CHANGED == resourceId) {
				mAllMute = (data == "mute");
				setAllMuteState(mAllMute);
			}
		} else {
			json dataObj = json::object();
			if (EPLJSONUtils::GetObjectByName(resultObj, "data", dataObj)) {
				if (EVENT_SIDE_WINDOW_VISIBLE_CHANGED == resourceId) {
					auto id = EPLJSONUtils::GetIntByName(dataObj, "id", -1);
					auto visible = EPLJSONUtils::GetBoolByName(dataObj, "visible");
					getSideWindowInfo();
					// to tell all the window instances to update thire state
					broadcastWindowVisibleChanged(id, visible);
				} else if (EVENT_SCENE_SWITCHED == resourceId) {
					auto id = EPLJSONUtils::GetStringByName(dataObj, "id");
					if (!id.empty()) {
						mCurrentActiveScene = id;
						broadcastSceneSwitched(mCurrentActiveScene);
					}
				} else if (EVENT_SCENECOLLECTION_SWITCHED == resourceId) {
					auto id = EPLJSONUtils::GetStringByName(dataObj, "id");
					if (!id.empty()) {
						mCurrentActiveCollection = id;
						//getScenes("");
						//getSources("");
						getSceneAndSource("");
						getSceneCollections();
						getAllMuteState();
					}
				} else if (EVENT_WINDOW_UPDATE_MESSAGE_COUNT == resourceId) {
					auto count = EPLJSONUtils::GetIntByName(dataObj, "messageCount");
					mToastMessageCount = count;
					broadcastMessageCount(count);
				} else if (EVENT_WINDOW_UPDATE_CPU_USAGE == resourceId) {
					auto cpuUsage = EPLJSONUtils::GetStringByName(dataObj, "cpuUsage");
					auto totalCpu = EPLJSONUtils::GetStringByName(dataObj, "totalCpu");
					char usageStr[64];
					int pos = cpuUsage.find(".");
					if (pos != std::string::npos) {
						cpuUsage = cpuUsage.substr(0, pos);
					}
					sprintf(usageStr, "%s%%", cpuUsage.c_str());
					broadcastCpuUasgeUpdate(std::string(usageStr));
				} else if (EVENT_WINDOW_UPDATE_BITRATE == resourceId) {
					auto mbitsPerSec = EPLJSONUtils::GetStringByName(dataObj, "mbitsPerSec");
					broadcastBitrateUpdate(mbitsPerSec);
				} else if (EVENT_WINDOW_UPDATE_FRAME_DROP == resourceId) {
					auto totalDrop = EPLJSONUtils::GetStringByName(dataObj, "totalDrop");
					auto dropPercent = EPLJSONUtils::GetStringByName(dataObj, "dropPercent");
					broadcastFrameDropUpdate(totalDrop, dropPercent);
				} else if (EVENT_WINDOW_LOGIN_STATE_CHANGED == resourceId) {
					auto state = EPLJSONUtils::GetStringByName(dataObj, "loginState");
					mPrismLoginState = state;
				} else if (EVENT_WINDOW_STREAM_STATE_CHANGED == resourceId) {
					auto state = EPLJSONUtils::GetStringByName(dataObj, "streamState");
					mPrismStreamState = state;
					broadcastStreamStateChanged(state);
				} else if (EVENT_WINDOW_RECORD_STATE_CHANGED == resourceId) {
					auto state = EPLJSONUtils::GetStringByName(dataObj, "recordState");
					mPrismRecordState = state;
					broadcastRecordStateChanged(state);
				}
			} else {
				if (EVENT_SOURCE_ITEM_UPDATED == resourceId) {
					getScenes("");
				} else if (EVENT_SOURCE_UPDATED == resourceId) {
					//getScenes("");
					//getSources("");
					getSceneAndSource("");
				} else if (EVENT_WINDOW_LOADING_FINISHED == resourceId) {
					getSideWindowInfo();
					getSceneCollections();
					getToastMessageCount();
					getPrismLoginState();
					getPrismStreamState();
					getPrismRecordState();
					getStudioModeState();
					broadcastPrismLoadingFinished();
					updateSceneList(mCurrentActiveCollection);
					//getSources("");
					getSceneAndSource("");
					getActiveSceneId();
					getAllMuteState();
				} else if (EVENT_WINDOW_STUDIO_MODE_SWITCHED == resourceId) {
					auto isStudioMode = EPLJSONUtils::GetBoolByName(resultObj, "enabled");
					mStudioModeState = isStudioMode;
					broadcastStudioModeChanged(isStudioMode);
				} else if (EVENT_SCENECOLLECTION_ADDED == resourceId) {
					getSceneCollections();
				} else if (EVENT_SCENE_ADDED == resourceId) {
					getSceneAndSource("");
					getActiveSceneId();
				}
			}
		}
	}
}

void PLSStreamDeckPlugin::handleRequests(int rpcID, const json &payload)
{
	bool error = EPLJSONUtils::GetBoolByName(payload, "error");
	std::string context, action;
	json extension;
	if (EPLJSONUtils::GetObjectByName(payload, "extension", extension)) {
		context = EPLJSONUtils::GetStringByName(extension, "context");
		action = EPLJSONUtils::GetStringByName(extension, "action");
	}
	switch (rpcID) {
	case RPC_ID_getRecordingAndStreamingState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto recordingState = EPLJSONUtils::GetStringByName(resultObj, "recordingStatus");
			auto streamingState = EPLJSONUtils::GetStringByName(resultObj, "streamingStatus");
			mIsRecording = (recordingState == "recording");
			mIsStreaming = (streamingState == "live");
			setState(KEY_UUID_RECORD, mIsRecording ? State_On : State_off);
			setState(KEY_UUID_STREAM, mIsStreaming ? State_On : State_off);
		}
	} break;
	case RPC_ID_getMasterMuteState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto muteState = EPLJSONUtils::GetStringByName(resultObj, "allMuteState");
			mAllMute = (muteState == "mute");
			setAllMuteState(mAllMute);
		}
	} break;
	case RPC_ID_getActiveSceneId: {
		auto sceneId = EPLJSONUtils::GetStringByName(payload, "result");
		if (!sceneId.empty()) {
			mCurrentActiveScene = sceneId;
			broadcastSceneSwitched(mCurrentActiveScene);
		}
	} break;
	case RPC_ID_getActiveCollection: {
		auto collectionId = EPLJSONUtils::GetStringByName(payload, "result");
		if (!collectionId.empty()) {
			mCurrentActiveCollection = collectionId;
			checkSceneCollection(collectionId);
		}
	} break;
	case RPC_ID_fetchSceneCollectionsSchema: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			json dataArray = json::array();
			if (EPLJSONUtils::GetArrayByName(resultObj, "data", dataArray)) {
				mSceneCollections = dataArray;
				for (auto &elment : dataArray) {
					if (mCollections.find(elment["id"].get<std::string>()) == mCollections.end()) {
						mCollections[elment["id"].get<std::string>()] = json::object();
					}
				}
				std::string collections = mCollections.dump();
				setSetings(KEY_UUID_SCENE, "collectionList", dataArray);
				setSetings(KEY_UUID_SOURCE, "collectionList", dataArray);
				setSetings(KEY_UUID_AUDIO_MIXER, "collectionList", dataArray);
				broadcastSceneCollectionChanged(dataArray);
			}
		}
	} break;
	case RPC_ID_getSources: {
		json resultArray = json::array();
		auto collection = EPLJSONUtils::GetStringByName(payload, "collection");
		if (EPLJSONUtils::GetArrayByName(payload, "result", resultArray)) {
			mCollections[collection]["sources"] = resultArray;
			std::string collections = mCollections.dump();
			if (mCurrentActiveCollection == collection) {
				checkAudioMuted(collection, resultArray);
			}
			updateSourceList(collection);
		}
	} break;
	case RPC_ID_getScenes: {
		json resultArray = json::array();
		auto collection = EPLJSONUtils::GetStringByName(payload, "collection");
		if (EPLJSONUtils::GetArrayByName(payload, "result", resultArray)) {
			mCollections[collection]["scenes"] = resultArray;
			std::string collections = mCollections.dump();
			if (mCurrentActiveCollection == collection) {
				checkSourceVisible(collection, resultArray);
			}
			updateSceneList(collection);
		}
	} break;
	case RPC_ID_getSideWindowInfo: {
		json resultArray = json::array();
		if (EPLJSONUtils::GetArrayByName(payload, "result", resultArray)) {
			mWindowInfo = resultArray;
			setSetings(KEY_UUID_SIDE_WINDOW, "sideWindowInfo", resultArray);
			if (mActionContext.find(KEY_UUID_SIDE_WINDOW) != mActionContext.end()) {
				auto context = mActionContext[KEY_UUID_SIDE_WINDOW];
				for (const auto &item : context) {
					json contextData = item.second;
					checkStateBySetting(KEY_UUID_SIDE_WINDOW, item.first, contextData);
				}
			}
		}
	} break;
	case RPC_ID_getNoticeMessageCount: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto messageCount = EPLJSONUtils::GetIntByName(resultObj, "messageCount");
			mToastMessageCount = messageCount;
			broadcastMessageCount(messageCount);
		}
	} break;
	case RPC_ID_getStudioModeState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto studioModeState = EPLJSONUtils::GetStringByName(resultObj, "studioModeState");
			if (!studioModeState.empty()) {
				mStudioModeState = (studioModeState == "on");
				broadcastStudioModeChanged(mStudioModeState);
			} else {
				broadcastStudioModeChanged(false);
			}
		}
	} break;
	case RPC_ID_getPrismLoginState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto loginState = EPLJSONUtils::GetStringByName(resultObj, "loginState");
			mPrismLoginState = loginState;
		}
	} break;
	case RPC_ID_getPrismStreamState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto streamState = EPLJSONUtils::GetStringByName(resultObj, "streamState");
			mPrismStreamState = streamState;
			broadcastStreamStateChanged(streamState);
		}
	} break;
	case RPC_ID_getPrismRecordState: {
		json resultObj;
		if (EPLJSONUtils::GetObjectByName(payload, "result", resultObj)) {
			auto recordState = EPLJSONUtils::GetStringByName(resultObj, "recordState");
			mPrismRecordState = recordState;
			broadcastRecordStateChanged(recordState);
		}
	} break;
	case RPC_ID_startStreaming:
	case RPC_ID_stopStreaming: {
		if (error) {
			if (!context.empty()) {
				int ret = getStreamKeyState(mPrismStreamState);
				mConnectionManager->SetState(ret, context);
				showAlert(context, false);
			}
		}
	} break;
	case RPC_ID_stopRecording:
	case RPC_ID_startRecording: {
		if (error) {
			if (!context.empty()) {
				int ret = getRecordKeyState(mPrismRecordState);
				mConnectionManager->SetState(ret, context);
				showAlert(context, false);
			}
		}
	} break;
	case RPC_ID_getScenesAndSource: {
		json sceneList = json::array();
		auto collection = EPLJSONUtils::GetStringByName(payload, "collection");
		if (EPLJSONUtils::GetArrayByName(payload, "sceneList", sceneList)) {
			mCollections[collection]["scenes"] = sceneList;
			std::string collections = mCollections.dump();
			if (mCurrentActiveCollection == collection) {
				checkSourceVisible(collection, sceneList);
			}
			updateSceneList(collection);
		}

		json sourceList = json::array();
		if (EPLJSONUtils::GetArrayByName(payload, "sourceList", sourceList)) {
			mCollections[collection]["sources"] = sourceList;
			std::string collections = mCollections.dump();
			if (mCurrentActiveCollection == collection) {
				checkAudioMuted(collection, sourceList);
			}
			updateSourceList(collection);
		}
	} break;
	case RPC_ID_hideScene:
	case RPC_ID_showScene:
	case RPC_ID_muteMixerAudioSource:
	case RPC_ID_unmuteMixerAudioSource:
	case RPC_ID_setStudioModeState:
	case RPC_ID_showSideWindow:
	case RPC_ID_unmuteAll:
	case RPC_ID_muteAll:
	case RPC_ID_makeSceneActive: {
		if (error) {
			if (!context.empty()) {
				mConnectionManager->SetState(State_off, context);
				showAlert(context, false);
			}
		}
	} break;
	default:
		break;
	}
}

void PLSStreamDeckPlugin::handleKeyUp(const json &settings, const std::string &inAction, const std::string &inContext, int state)
{
	json param = json::object();
	json payload;

	json extension = json::object();
	extension["context"] = inContext;
	extension["action"] = inAction;
	payload["extension"] = extension;

	if (inAction == KEY_UUID_SCENE) {
		auto currentSceneId = EPLJSONUtils::GetStringByName(settings, "select_scene_id", "");
		auto currentCollection = EPLJSONUtils::GetStringByName(settings, "select_collection_id", "");
		if (!currentSceneId.empty() && !currentCollection.empty() && currentCollection == mCurrentActiveCollection) {
			payload["id"] = RPC_ID_makeSceneActive;
			payload["method"] = "makeSceneActive";
			json args = json::array();
			args.push_back(currentSceneId);
			param["args"] = args;
			param["resource"] = "ScenesService";
			payload["params"] = param;
			mPLSConnectionManager->SendToPrism(payload);
		} else {
			LogMessage("show alert! \naction: %s \nscene:%s\nsceneCollection:%s\ncurrent scene collection:%s\n", inAction.c_str(), currentSceneId.c_str(), currentCollection.c_str(),
				   mCurrentActiveCollection.c_str());
			showAlert(inContext);
		}
	} else if (inAction == KEY_UUID_SOURCE) {
		auto currentSourceId = EPLJSONUtils::GetStringByName(settings, "select_source_id", "");
		auto currentSceneId = EPLJSONUtils::GetStringByName(settings, "select_scene_id", "");
		auto currentSceneItemId = EPLJSONUtils::GetIntByName(settings, "select_sceneItem_id", -1);
		auto groupSourceId = EPLJSONUtils::GetStringByName(settings, "select_groupSource_id", "");
		auto currentCollection = EPLJSONUtils::GetStringByName(settings, "select_collection_id", "");
		if (!currentSourceId.empty() && !currentSceneId.empty() && currentSceneItemId != -1 && !currentCollection.empty() && currentCollection == mCurrentActiveCollection) {
			payload["id"] = (state == State_On) ? RPC_ID_hideScene : RPC_ID_showScene;
			param["groupSourceId"] = groupSourceId;
			param["sceneId"] = currentSceneId;
			param["sourceId"] = currentSourceId;
			param["sceneItemId"] = std::to_string(currentSceneItemId);
			payload["params"] = param;
			mPLSConnectionManager->SendToPrism(payload);
		} else {
			LogMessage("show alert! \naction: %s \n,source:%s\nscene:%s\nsceneCollection:%s\ncurrent scene collection:%s\n", inAction.c_str(), currentSourceId.c_str(),
				   currentSceneId.c_str(), currentCollection.c_str(), mCurrentActiveCollection.c_str());
			showAlert(inContext);
		}

	} else if (inAction == KEY_UUID_RECORD) {

		// check recording state.
		if (mPrismRecordState.empty()) {
			showAlert(inContext);
			setState(KEY_UUID_RECORD, State_off);
			return;
		}
		int ret = getRecordKeyState(mPrismRecordState);
		setState(KEY_UUID_RECORD, ret);
		// prism is on progressing, cannot control.
		if (ret == State_Disable)
			return;

		int id = -1;
		if (RECORD_STATE_RECORD_STARTED == mPrismRecordState) {
			id = RPC_ID_stopRecording;
		}
		if (RECORD_STATE_RECORD_READY == mPrismRecordState) {
			id = RPC_ID_startRecording;
		}
		if (id == -1)
			return;
		bool startRecord = (RECORD_STATE_RECORD_READY == mPrismRecordState);
		payload["id"] = startRecord ? RPC_ID_startRecording : RPC_ID_stopRecording;
		payload["method"] = startRecord ? "startRecording" : "stopRecording";
		param["resource"] = "StreamingService";
		payload["params"] = param;
		mPLSConnectionManager->SendToPrism(payload);

	} else if (inAction == KEY_UUID_STREAM) {
		// check streaming state.
		if (mPrismStreamState.empty()) {
			showAlert(inContext);
			setState(KEY_UUID_STREAM, State_off);
			return;
		}

		int ret = getStreamKeyState(mPrismStreamState);
		setState(KEY_UUID_STREAM, ret);
		// prism is on progressing, cannot control.
		if (ret == State_Disable)
			return;

		int id = -1;
		if (STREAM_STATE_STREAM_STARTED == mPrismStreamState) {
			id = RPC_ID_stopStreaming;
		}
		if (STREAM_STATE_READY_STATE == mPrismStreamState) {
			id = RPC_ID_startStreaming;
		}
		if (id == -1)
			return;
		bool startStream = (STREAM_STATE_READY_STATE == mPrismStreamState);
		payload["id"] = startStream ? RPC_ID_startStreaming : RPC_ID_stopStreaming;
		payload["method"] = startStream ? "startStreaming" : "stopStreaming";
		param["resource"] = "StreamingService";
		payload["params"] = param;
		mPLSConnectionManager->SendToPrism(payload);
	} else if (inAction == KEY_UUID_AUDIO_MIXER) {
		auto sourceId = EPLJSONUtils::GetStringByName(settings, "audioSourceId", "");
		auto currentCollection = EPLJSONUtils::GetStringByName(settings, "select_collection_id", "");
		if (sourceId.empty() || currentCollection != mCurrentActiveCollection) {
			LogMessage("show alert! \naction: %s \nsource:%s\nscene collection:%s\ncurrent scene collection:%s\n", inAction.c_str(), sourceId.c_str(), currentCollection.c_str(),
				   mCurrentActiveCollection.c_str());
			showAlert(inContext);
			return;
		}
		payload["id"] = (state == State_off) ? RPC_ID_unmuteMixerAudioSource : RPC_ID_muteMixerAudioSource;
		param["sourceId"] = sourceId;
		payload["params"] = param;
		mPLSConnectionManager->SendToPrism(payload);
	} else if (inAction == KEY_UUID_SIDE_WINDOW) {
		auto key = EPLJSONUtils::GetStringByName(settings, "selectSideWindowInfo", "");
		if (key.empty()) {
			showAlert(inContext);
			return;
		}
		openPrismSideWindow(atoi(key.c_str()), !(state == State_On), payload);
	} else if (inAction == KEY_UUID_MUTE_ALL) {
		payload["id"] = mAllMute ? RPC_ID_unmuteAll : RPC_ID_muteAll;
		mPLSConnectionManager->SendToPrism(payload);
	} else if (inAction == KEY_UUID_TOAST_MESSAGE) {
		mConnectionManager->SetState(mToastMessageCount > 0 ? State_On : State_off, inContext);
		openPrismSideWindow(LivingMsgView, !isWindowVisible(LivingMsgView, mWindowInfo), payload);
	} else if (inAction == KEY_UUID_STUDIO_MODE) {
		setPrismStudioModeState((state == State_off), payload);
	} else if (inAction == KEY_UUID_CPU_USAGE) {
	}
}

void PLSStreamDeckPlugin::broadcastWindowVisibleChanged(int windowId, bool visible)
{
	if (windowId <= 0)
		return;
	mMutex.lock();
	if (mActionContext.find(KEY_UUID_SIDE_WINDOW) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_SIDE_WINDOW];
		for (const auto &item : context) {
			json contextData = item.second;
			json settings = json::object();
			if (EPLJSONUtils::GetObjectByName(contextData, "settings", settings)) {
				auto key = EPLJSONUtils::GetStringByName(settings, "selectSideWindowInfo");
				if (!key.empty() && atoi(key.c_str()) == windowId) {
					mConnectionManager->SetState(visible ? State_On : State_off, item.first);
				}
			}
		}
	}
	mMutex.unlock();
}

void PLSStreamDeckPlugin::broadcastSceneSwitched(const std::string &sceneId)
{
	if (sceneId.empty())
		return;
	if (mActionContext.find(KEY_UUID_SCENE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_SCENE];
		for (const auto &item : context) {
			json contextData = item.second;
			json settings = json::object();
			if (EPLJSONUtils::GetObjectByName(contextData, "settings", settings)) {
				auto key = EPLJSONUtils::GetStringByName(settings, "select_scene_id");
				if (!key.empty() && key == sceneId) {
					mConnectionManager->SetState(State_On, item.first);
				} else {
					mConnectionManager->SetState(State_off, item.first);
				}
			}
		}
	}
}

void PLSStreamDeckPlugin::broadcastMessageCount(int messageCount)
{
	if (mActionContext.find(KEY_UUID_TOAST_MESSAGE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_TOAST_MESSAGE];
		for (const auto &item : context) {
			mConnectionManager->SetState((messageCount > 0) ? State_On : State_off, item.first);
			//mConnectionManager->SetTitle((messageCount > MAX_MESSAGE_COUNT) ? std::string("999+") : std::to_string(messageCount), item.first, kESDSDKTarget_HardwareAndSoftware);
		}
	}
}

void PLSStreamDeckPlugin::broadcastStudioModeChanged(bool studioModeOn)
{
	if (mActionContext.find(KEY_UUID_STUDIO_MODE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_STUDIO_MODE];
		for (const auto &item : context) {
			mConnectionManager->SetState(studioModeOn ? State_On : State_off, item.first);
		}
	}
}

void PLSStreamDeckPlugin::broadcastCpuUasgeUpdate(const std::string &usage)
{
	if (mActionContext.find(KEY_UUID_CPU_USAGE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_CPU_USAGE];
		for (const auto &item : context) {
			mConnectionManager->SetTitle(usage, item.first, kESDSDKTarget_HardwareAndSoftware);
		}
	}
}

void PLSStreamDeckPlugin::broadcastFrameDropUpdate(const std::string &frameDrop, const std::string &dropPercent)
{
	if (mActionContext.find(KEY_UUID_FRAME_DROP) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_FRAME_DROP];
		char titleStr[64];
		sprintf(titleStr, "%s%%", dropPercent.c_str());
		for (const auto &item : context) {
			mConnectionManager->SetTitle(std::string(titleStr), item.first, kESDSDKTarget_HardwareAndSoftware);
		}
	}
}

void PLSStreamDeckPlugin::broadcastBitrateUpdate(const std::string &bitrate)
{
	if (mActionContext.find(KEY_UUID_STREAM_BITERATE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_STREAM_BITERATE];
		int pos = bitrate.find(".");
		int mb = 0;
		if (pos != std::string::npos) {
			mb = atoi(bitrate.substr(0, pos).c_str());
		}
		char titleStr[64];
		if (mb > 9) {
			sprintf(titleStr, "%dmb", mb);
		} else {
			sprintf(titleStr, "%smb", bitrate.c_str());
		}

		for (const auto &item : context) {
			mConnectionManager->SetTitle(std::string(titleStr), item.first, kESDSDKTarget_HardwareAndSoftware);
		}
	}
}

void PLSStreamDeckPlugin::broadcastSceneCollectionChanged(const json &sceneCollections)
{
	json collections = json::object();
	json result = json::object();
	result["id"] = RPC_ID_fetchSceneCollectionsSchema;
	collections["activeSceneCollection"] = mCurrentActiveCollection;
	collections["collectionList"] = sceneCollections;
	result["data"] = collections;
	for (auto &context : mActionContext) {
		if (context.first == KEY_UUID_SCENE || context.first == KEY_UUID_SOURCE || context.first == KEY_UUID_AUDIO_MIXER) {
			for (auto &item : context.second) {
				mConnectionManager->SendToPropertyInspector(context.first, item.first, result);
			}
		}
	}
}

void PLSStreamDeckPlugin::broadcastStreamStateChanged(const std::string &state)
{
	LogMessage("%s current state:%s", __FUNCTION__, state.c_str());
	int keyState = getStreamKeyState(state);
	if (mActionContext.find(KEY_UUID_STREAM) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_STREAM];
		for (const auto &item : context) {
			mConnectionManager->SetState(keyState, item.first);
		}
	}
}

void PLSStreamDeckPlugin::broadcastRecordStateChanged(const std::string &state)
{
	LogMessage("%s current state:%s", __FUNCTION__, state.c_str());
	int keyState = getRecordKeyState(state);
	if (mActionContext.find(KEY_UUID_RECORD) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_RECORD];
		for (const auto &item : context) {
			mConnectionManager->SetState(keyState, item.first);
		}
	}
}

void PLSStreamDeckPlugin::showAlert(const std::string &inContext, bool resetState)
{
	if (resetState) {
		mConnectionManager->SetState(State_off, inContext);
	}
	mConnectionManager->ShowAlertForContext(inContext);
}

bool isWindowVisible(int id, const json &windowsInfo)
{
	size_t size = windowsInfo.size();
	for (size_t i = 0; i < size; ++i) {
		if (windowsInfo[i]["id"].get<int>() == id) {
			return windowsInfo[i]["visible"].get<bool>();
		}
	}
	return false;
}

void PLSStreamDeckPlugin::checkStateBySetting(const std::string &inAction, const std::string &inContext, const json &inPayload)
{
	json settings;
	if (!EPLJSONUtils::GetObjectByName(inPayload, "settings", settings)) {
		LogMessage("%s get settings failed. action: %s", __FUNCTION__, inAction.c_str());
		return;
	}
	if (KEY_UUID_SIDE_WINDOW == inAction) {
		auto key = EPLJSONUtils::GetStringByName(settings, "selectSideWindowInfo");
		if (!key.empty()) {
			int id = atoi(key.c_str());
			bool visible = isWindowVisible(id, mWindowInfo);
			mConnectionManager->SetState(visible ? State_On : State_off, inContext);
		}
	} else if (KEY_UUID_SCENE == inAction) {
		auto key = EPLJSONUtils::GetStringByName(settings, "select_scene_id");
		if (!key.empty()) {
			mConnectionManager->SetState((key == mCurrentActiveScene) ? State_On : State_off, inContext);
		}
	} else if (KEY_UUID_AUDIO_MIXER == inAction) {
		if (!mCurrentActiveCollection.empty()) {
			checkAudioMuted(mCurrentActiveCollection, mCollections[mCurrentActiveCollection]["sources"]);
		}

	} else if (KEY_UUID_SOURCE == inAction) {
		if (!mCurrentActiveCollection.empty()) {
			checkSourceVisible(mCurrentActiveCollection, mCollections[mCurrentActiveCollection]["scenes"]);
		}
	} else {
		mConnectionManager->SetState(State_off, inContext);
	}
}

int PLSStreamDeckPlugin::getStreamKeyState(const std::string &state)
{
	if (state.empty())
		return State_off;

	if (STREAM_STATE_READY_STATE == state) {
		return State_off;
	}

	if (STREAM_STATE_BROADCAST_GO == state || STREAM_STATE_STREAM_STARTING == state || STREAM_STATE_CAN_BROADCAST_STATE == state || STREAM_STATE_STREAM_STOPPING == state ||
	    STREAM_STATE_STOP_BROADCAST_GO == state || STREAM_STATE_CAN_BROADCAST_STOP == state) {
		return State_Disable;
	}

	if (STREAM_STATE_STREAM_STARTED == state || STREAM_STATE_STREAM_STOPPED == state || STREAM_STATE_STREAM_END == state) {
		return State_On;
	}

	return State_off;
}

int PLSStreamDeckPlugin::getRecordKeyState(const std::string &state)
{
	if (state.empty())
		return State_off;

	if (RECORD_STATE_RECORD_READY == state) {
		return State_off;
	}

	if (RECORD_STATE_CAN_RECORD == state || RECORD_STATE_RECORD_STARTING == state || RECORD_STATE_RECORD_STOPPING == state) {
		return State_Disable;
	}

	if (RECORD_STATE_RECORD_STARTED == state || RECORD_STATE_RECORD_STOPGO == state || RECORD_STATE_RECORD_STOPPED == state) {
		return State_On;
	}

	return State_off;
}

void PLSStreamDeckPlugin::resetKeyState(const std::string &inAction, const std::string &inContext)
{
	if (KEY_UUID_RECORD == inAction || KEY_UUID_STREAM == inAction) {
		mConnectionManager->SetState(State_Disable, inContext);
	} else {
		mConnectionManager->SetState(State_off, inContext);
	}
}

void PLSStreamDeckPlugin::setAllMuteState(bool allMute, const std::string &inContext)
{
	if (inContext.empty()) {
		setState(KEY_UUID_MUTE_ALL, allMute ? State_off : State_On);
	} else {
		mConnectionManager->SetState(allMute ? State_off : State_On, inContext);
	}
}

void PLSStreamDeckPlugin::resetWindowInfo()
{
	size_t size = mWindowInfo.size();
	for (size_t i = 0; i < size; ++i) {
		mWindowInfo[i]["visible"] = false;
	}
}

void PLSStreamDeckPlugin::getAllMuteState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getMasterMuteState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getMasterMuteState";
	json param = json::object();
	param["resource"] = "SourceService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getStreamingAndRecordState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getRecordingAndStreamingState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getModel";
	json param = json::object();
	param["resource"] = "StreamingService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getSceneCollections()
{
	json payload = json::object();
	payload["id"] = RPC_ID_fetchSceneCollectionsSchema;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "fetchSceneCollectionsSchema";
	json param = json::object();
	param["resource"] = "SceneCollectionsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getActiveSceneId()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getActiveSceneId;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "activeSceneId";
	json param = json::object();
	param["resource"] = "ScenesService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getActiveCollection()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getActiveCollection;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "activeCollection";
	json param = json::object();
	param["resource"] = "SceneCollectionsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getSideWindowInfo()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getSideWindowInfo;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getSideWindowInfo";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getScenes(const std::string &collection)
{
	json payload = json::object();
	payload["id"] = RPC_ID_getScenes;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getScenes";

	json param = json::object();
	param["resource"] = "ScenesService";
	json args = json::array();
	args.push_back(collection);
	param["args"] = args;
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getSources(const std::string &collection)
{
	json payload = json::object();
	payload["id"] = RPC_ID_getSources;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getSources";

	json param = json::object();
	param["resource"] = "SourcesService";
	json args = json::array();
	args.push_back(collection);
	param["args"] = args;
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getSceneAndSource(const std::string &collection)
{
	json payload = json::object();
	payload["id"] = RPC_ID_getScenesAndSource;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getScenesAndSource";

	json param = json::object();
	param["resource"] = "SourcesService";
	json args = json::array();
	args.push_back(collection);
	param["args"] = args;
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getStudioModeState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getStudioModeState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getStudioModeState";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getToastMessageCount()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getNoticeMessageCount;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getStudioModeState";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getPrismLoginState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getPrismLoginState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getPrismLoginState";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getPrismStreamState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getPrismStreamState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getPrismStreamState";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::getPrismRecordState()
{
	json payload = json::object();
	payload["id"] = RPC_ID_getPrismRecordState;
	payload["jsonrpc"] = "2.0";
	payload["method"] = "getPrismRecordState";
	json param = json::object();
	param["resource"] = "WindowsService";
	payload["params"] = param;
	mPLSConnectionManager->SendToPrism(payload);
}

void PLSStreamDeckPlugin::openPrismSideWindow(int windowKey, bool visible, json &payload)
{
	if (nullptr != mPLSConnectionManager) {
		json param = json::object();
		payload["id"] = RPC_ID_showSideWindow;
		param["windowId"] = windowKey;
		param["visible"] = visible;
		payload["params"] = param;
		mPLSConnectionManager->SendToPrism(payload);
	}
}

void PLSStreamDeckPlugin::setPrismStudioModeState(bool open, json &payload)
{
	if (nullptr != mPLSConnectionManager) {
		json param = json::object();
		payload["id"] = RPC_ID_setStudioModeState;
		param["studioModeState"] = open ? "on" : "off";
		payload["params"] = param;
		mPLSConnectionManager->SendToPrism(payload);
	}
}

void PLSStreamDeckPlugin::updateSceneList(const std::string &collectionId)
{
	if (collectionId.empty())
		return;
	json result;
	result["id"] = RPC_ID_updateSceneList;
	result["param"] = collectionId;
	sendToPropertyInspector(KEY_UUID_AUDIO_MIXER, result);
	sendToPropertyInspector(KEY_UUID_SOURCE, result);
	sendToPropertyInspector(KEY_UUID_SCENE, result);
}

void PLSStreamDeckPlugin::updateSourceList(const std::string &collectionId)
{
	if (collectionId.empty())
		return;
	json result;
	result["id"] = RPC_ID_updateSourceList;
	result["param"] = collectionId;
	sendToPropertyInspector(KEY_UUID_AUDIO_MIXER, result);
}

bool isSourceVisible(const std::string &sceneId, const std::string &sourceId, const json &sceneList)
{
	size_t size = sceneList.size();
	for (size_t i = 0; i < size; ++i) {
		if (sceneList[i]["id"].get<std::string>() == sceneId) {
			json sourceList = json::array();
			if (EPLJSONUtils::GetArrayByName(sceneList[i], "nodes", sourceList)) {
				size_t sizeSource = sourceList.size();
				for (size_t n = 0; n < sizeSource; ++n) {
					if (sourceList[n]["sourceId"].get<std::string>() == sourceId) {
						return sourceList[n]["visible"].get<bool>();
					}
				}
				return false;
			}
			return false;
		}
	}
	return false;
}

void PLSStreamDeckPlugin::checkSourceVisible(const std::string &collection, const json &sceneList)
{
	if (mActionContext.find(KEY_UUID_SOURCE) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_SOURCE];
		for (const auto &item : context) {
			json contextData = item.second;
			json settings = json::object();
			if (EPLJSONUtils::GetObjectByName(contextData, "settings", settings)) {
				auto collectionId = EPLJSONUtils::GetStringByName(settings, "select_collection_id");
				auto sceneId = EPLJSONUtils::GetStringByName(settings, "select_scene_id");
				auto sourceId = EPLJSONUtils::GetStringByName(settings, "select_source_id");
				if (collectionId == collection) {
					bool visible = isSourceVisible(sceneId, sourceId, sceneList);
					mConnectionManager->SetState(visible ? State_On : State_off, item.first);
				} else {
					mConnectionManager->SetState(State_off, item.first);
				}
			}
		}
	}
}

bool isAudioMuted(const std::string &sourceId, const json &audioSourceList)
{
	size_t size = audioSourceList.size();
	for (size_t i = 0; i < size; ++i) {
		if (audioSourceList[i]["id"].get<std::string>() == sourceId) {
			return audioSourceList[i]["muted"].get<bool>();
		}
	}
	return true;
}

void PLSStreamDeckPlugin::checkAudioMuted(const std::string &collection, const json &audioSourceList)
{
	if (mActionContext.find(KEY_UUID_AUDIO_MIXER) != mActionContext.end()) {
		auto context = mActionContext[KEY_UUID_AUDIO_MIXER];
		for (const auto &item : context) {
			json contextData = item.second;
			json settings = json::object();
			if (EPLJSONUtils::GetObjectByName(contextData, "settings", settings)) {
				auto collectionId = EPLJSONUtils::GetStringByName(settings, "select_collection_id");
				auto sourceId = EPLJSONUtils::GetStringByName(settings, "audioSourceId");
				if (collectionId == collection) {
					bool isMuted = isAudioMuted(sourceId, audioSourceList);
					mConnectionManager->SetState(isMuted ? State_off : State_On, item.first);
				} else {
					mConnectionManager->SetState(State_off, item.first);
				}
			}
		}
	}
}

void PLSStreamDeckPlugin::checkSceneCollection(const std::string &collectionId)
{
	if (collectionId.empty())
		return;
	for (auto &context : mActionContext) {
		if (context.first == KEY_UUID_SCENE || context.first == KEY_UUID_SOURCE || context.first == KEY_UUID_AUDIO_MIXER) {
			for (auto &item : context.second) {
				json contextData = item.second;
				json settings = json::object();
				if (EPLJSONUtils::GetObjectByName(contextData, "settings", settings)) {
					auto key = EPLJSONUtils::GetStringByName(settings, "select_collection_id");
					if (key != collectionId) {
						mConnectionManager->SetState(State_off, item.first);
					}
				}
			}
		}
	}
}

void PLSStreamDeckPlugin::broadcastPrismConnectionState(bool isConnected)
{
	json result;
	result["id"] = RPC_ID_getPrismConnectionState;
	result["data"] = mPrismConnected;
	for (auto &context : mActionContext) {
		for (auto &item : context.second) {
			if (!isConnected) {
				resetKeyState(context.first, item.first);
				std::string title;
				if (context.first == KEY_UUID_CPU_USAGE) {
					title = "0%";
				} else if (context.first == KEY_UUID_FRAME_DROP) {
					title = "0%";
				} else if (context.first == KEY_UUID_STREAM_BITERATE) {
					title = "0mb";
				}
				if (!title.empty()) {
					mConnectionManager->SetTitle(title, item.first, kESDSDKTarget_HardwareAndSoftware);
				}
			}
			mConnectionManager->SendToPropertyInspector(context.first, item.first, result);
		}
	}
}

void PLSStreamDeckPlugin::broadcastPrismLoadingFinished()
{
	json result;
	result["id"] = RPC_ID_prismLoadingFinished;
	result["data"] = mCurrentActiveCollection;
	for (auto &context : mActionContext) {
		for (auto &item : context.second) {
			mConnectionManager->SendToPropertyInspector(context.first, item.first, result);
		}
	}
}

void PLSStreamDeckPlugin::PrismConnected()
{
	mPrismConnected = true;
	broadcastPrismConnectionState(true);
	getActiveCollection();
	getSceneCollections();
	getSideWindowInfo();
	//getScenes("");
	//getSources("");
	getSceneAndSource("");
	getToastMessageCount();
	getStudioModeState();
	getPrismLoginState();
	getPrismStreamState();
	getPrismRecordState();
	getActiveSceneId();
	getAllMuteState();
}

void PLSStreamDeckPlugin::PrismDisconnected()
{
	mPrismConnected = false;
	resetWindowInfo();
	broadcastPrismConnectionState(false);
}
