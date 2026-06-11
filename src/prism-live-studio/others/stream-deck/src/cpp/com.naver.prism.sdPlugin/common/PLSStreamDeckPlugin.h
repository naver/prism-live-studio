#pragma once
#include <mutex>
#include "EPLJSONUtils.h"
#include "PLSPluginBase.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

class PLSStreamDeckPlugin : public PLSPluginBase {
public:
	PLSStreamDeckPlugin() : PLSPluginBase() {}
	virtual ~PLSStreamDeckPlugin();

	using ActionContext = std::map<std::string, std::map<std::string, json>>;
	void KeyDownForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;
	void KeyUpForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;

	void WillAppearForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;
	void WillDisappearForAction(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;

	void DeviceDidConnect(const std::string &inDeviceID, const json &inDeviceInfo) override;
	void DeviceDidDisconnect(const std::string &inDeviceID) override;

	void DidReceiveSettings(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;

	void SendToPlugin(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;
	void PropertyInspectorDidAppear(const std::string &inAction, const std::string &inContext, const json &inPayload, const std::string &inDeviceID) override;

	void PrismConnected() override;
	void PrismDisconnected() override;

	void UpdateSceneCollectionList(const json &payload) override;
	void SendToPropertyInspector(const json &payload) override;

	void DidReceiveFromPrism(const json &payload) override;

	void LogMessage(const std::string &message);
	void LogMessage(const char *format, ...) override;
	void StreamDeckDisconnected() override;

	static websocketpp::lib::asio::io_service *GetApp();

private:
	void updateSettings(const std::string &inAction, const std::string &inContext, const json &inPayload);
	void setState(const std::string &inAction, int state);
	void setSetings(const std::string &key, const std::string &value);
	void setSetings(const std::string &key, const json &value);
	void setSetings(const std::string &inAction, const std::string &key, const json &value);
	void sendToPropertyInspector(const std::string &action, const json &payload);
	void handleEvents(const json &payload);
	void handleRequests(int rpcID, const json &payload);
	void handleKeyUp(const json &settings, const std::string &inAction, const std::string &inContext, int state);

	void getAllMuteState();
	void getStreamingAndRecordState();
	void getSceneCollections();
	void getActiveSceneId();
	void getActiveCollection();
	void getSideWindowInfo();
	void getScenes(const std::string &collection);
	void getSources(const std::string &collection);
	void getSceneAndSource(const std::string &collection);
	void getStudioModeState();
	void getToastMessageCount();
	void getPrismLoginState();
	void getPrismStreamState();
	void getPrismRecordState();
	void openPrismSideWindow(int windowKey, bool visible, json &payload);
	void setPrismStudioModeState(bool open, json &payload);

	void updateSceneList(const std::string &collectionId);
	void updateSourceList(const std::string &collectionId);
	void checkSourceVisible(const std::string &collection, const json &sceneList);
	void checkAudioMuted(const std::string &collection, const json &audioSourceList);
	void checkSceneCollection(const std::string &collectionId);
	void broadcastPrismConnectionState(bool isConnected);
	void broadcastPrismLoadingFinished();
	void broadcastWindowVisibleChanged(int windowId, bool visible);
	void broadcastSceneSwitched(const std::string &sceneId);
	void broadcastMessageCount(int messageCount);
	void broadcastStudioModeChanged(bool studioModeOn);
	void broadcastCpuUasgeUpdate(const std::string &usage);
	void broadcastFrameDropUpdate(const std::string &frameDrop, const std::string &dropPercent);
	void broadcastBitrateUpdate(const std::string &bitrate);
	void broadcastSceneCollectionChanged(const json &sceneCollections);
	void broadcastStreamStateChanged(const std::string &state);
	void broadcastRecordStateChanged(const std::string &state);

	void showAlert(const std::string &inContext, bool resetState = true);
	void checkStateBySetting(const std::string &inAction, const std::string &inContext, const json &settings);
	int getStreamKeyState(const std::string &state);
	int getRecordKeyState(const std::string &state);
	void resetKeyState(const std::string &inAction, const std::string &inContext);
	void setAllMuteState(bool allMute, const std::string &inContext = "");
	void resetWindowInfo();

private:
	std::mutex mMutex;
	std::pair<std::string, std::string> mCurrentInspector;
	json mSettings;
	ActionContext mActionContext;
	std::map<int, bool> mWindows;
	std::string mCurrentActiveScene;
	std::string mCurrentActiveCollection;
	json mSceneCollections = json::object();
	json mWindowInfo = json::array();
	json mCollections = json::object();
	bool mAllMute = false;
	bool mPrismConnected = false;
	bool mStudioModeState = false;
	bool mIsStreaming = false;
	bool mIsRecording = false;

	int mToastMessageCount = 0;
	std::string mPrismLoginState = "";
	std::string mPrismStreamState = "";
	std::string mPrismRecordState = "";
};
