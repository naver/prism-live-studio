#ifndef ACTIONHELP_H
#define ACTIONHELP_H

#include <QObject>
#include <QHash>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <frontend-api.h>
#include <QTcpSocket>
#include <QtWebSockets/QWebSocket>
#include "JSONUtils.h"
#include "SourceInfo.h"

// ----------------------------------------------------------------------------
enum class OBS_SOURCE_TYPE {
	OBS_SOURCE_TYPE_UNKNOWN,
	OBS_SOURCE_TYPE_AUDIO_INPUT_CAPTURE,
	OBS_SOURCE_TYPE_AUDIO_OUTPUT_CAPTURE,
	OBS_SOURCE_TYPE_IMAGE,
	OBS_SOURCE_TYPE_VIDEO_CAPTURE_DEVICE,
	OBS_SOURCE_TYPE_GAME_CAPTURE,
	OBS_SOURCE_TYPE_MEDIA_SOURCE,
	OBS_SOURCE_TYPE_VLC_VIDEO_SOURCE,
	OBS_SOURCE_TYPE_TEXT,
	OBS_SOURCE_TYPE_WINDOW_CAPTURE,
	OBS_SOURCE_TYPE_BROWSER_SOURCE,
	OBS_SOURCE_TYPE_JACKIN_CLIENT,
	OBS_SOURCE_TYPE_NDI_SOURCE
};

struct _GroupItemInfo {
	std::string sourceName;
	bool isVisible;
	int64_t sceneItemId;
	obs_sceneitem_t *item;
};
using GroupItemInfo = _GroupItemInfo;

struct _SceneItemInfo {
	obs_source_t *source;
	std::string sourceName;
	bool isVisible;
	bool isGroup;
	int64_t sceneItemId;
	QList<_GroupItemInfo> groupSceneItems;
};
using SceneItemInfo = _SceneItemInfo;

struct _SceneInfo {
	obs_source_t *scene;
	std::string name;
	QList<SceneItemInfo> sceneItems;
};
using SceneInfo = _SceneInfo;

enum class ToggleInfo {
	Toggle,
	Activate,
	Deactivate,
};

static const int RPC_ID_startStreaming = 1;
static const int RPC_ID_stopStreaming = 2;
static const int RPC_ID_startRecording = 3;
static const int RPC_ID_stopRecording = 4;
static const int RPC_ID_makeCollectionActive = 6;
static const int RPC_ID_fetchSceneCollectionsSchema = 8;
static const int RPC_ID_getScenes = 9;
static const int RPC_ID_getSources = 10;
static const int RPC_ID_makeSceneActive = 11;
static const int RPC_ID_getActiveSceneId = 12;
static const int RPC_ID_muteMixerAudioSource = 13;
static const int RPC_ID_unmuteMixerAudioSource = 14;
static const int RPC_ID_hideScene = 15;
static const int RPC_ID_showScene = 16;
static const int RPC_ID_subscribeToSceneSwitched = 17;
static const int RPC_ID_subscribeToSceneAdded = 18;
static const int RPC_ID_subscribeToSceneRemoved = 19;
static const int RPC_ID_subscribeToSouceAdded = 20;
static const int RPC_ID_subscribeToSourceRemoved = 21;
static const int RPC_ID_subscribeToSourceUpdated = 22;
static const int RPC_ID_subscribeToItemAdded = 23;
static const int RPC_ID_subscribeToItemRemoved = 24;
static const int RPC_ID_subscribeToItemUpdated = 25;
static const int RPC_ID_subscribeToStreamingStatusChanged = 26;
static const int RPC_ID_getActiveCollection = 27;
static const int RPC_ID_subscribeToCollectionAdded = 28;
static const int RPC_ID_subscribeToCollectionRemoved = 29;
static const int RPC_ID_subscribeToCollectionSwitched = 30;
static const int RPC_ID_getRecordingAndStreamingState = 31;
static const int RPC_ID_subscribeToCollectionUpdated = 32;
static const int RPC_ID_subscribeToRecordingStatusChanged = 33;
static const int RPC_ID_setPushToProgramInStudioMode = 34;
static const int RPC_ID_showSideWindow = 35;
static const int RPC_ID_muteAll = 36;
static const int RPC_ID_unmuteAll = 37;
static const int RPC_ID_getMasterMuteState = 38;
static const int RPC_ID_getSideWindowInfo = 39;
static const int RPC_ID_getStudioModeState = 40;
static const int RPC_ID_setStudioModeState = 41;
static const int RPC_ID_getNoticeMessageCount = 42;
static const int RPC_ID_getPrismLoginState = 43;
static const int RPC_ID_getPrismStreamState = 44;
static const int RPC_ID_getPrismRecordState = 45;
static const int RPC_ID_getScenesAndSource = 46;

// ----------------------------------------------------------------------------
class ActionHelp : public QObject {
	Q_OBJECT
public:
	explicit ActionHelp(QObject *parent = nullptr);
	~ActionHelp() final;

	//used to prevent sending event messages to Stream Deck, when updating streaming and recording states on request of Stream Deck
	bool GetIsRespondingStreamingFlag() const;

	//used to prevent sending event messages to Stream Deck, when updating collections, scenes or sources on request of Stream Deck
	bool GetIsRespondingCollectionsSchemaFlag() const;

	void WriteToSocket(const std::string &inString);

	void UpdateSourcesList(QList<SourceInfo> &outSourceList) const;
	void UpdateScenesList(QList<SceneInfo> &outList) const;
	void StopSocket();
	void SetSceneCollectionChangingFlag(bool changing) { mIsSecneCollectionChanging = changing; }
	bool SceneCollectionChangingFlag() const { return mIsSecneCollectionChanging; }

public slots:
	void WriteToSocketInMainThread(QString inString);

	void NotifySceneSwitched();
	void NotifyCollectionChanged();
	void CheckStudioMode();

	//Socket connection
	void SDClientConnected();
	void ReadyRead(const QByteArray &data);
	void OnTextMessage(const QString &text);
	void Disconnected();

private:
	//scenes and collections
	QString GetCurrentSceneName() const;
	bool GetCurrentCollectionAndSceneName(QString &inCollectionName, QString &inSceneName) const;
	QString GetCurrentSceneCollectionName() const;

	void UpdateSceneCollectionList(QStringList &list);
	bool RequestSceneListUpdate(QString inCollectionName, QList<SceneInfo> &outSceneList) const;
	bool RequestSceneAndSourceListUpdate(QString inCollectionName, QList<SceneInfo> &outSceneList, QList<SourceInfo> &outSourceList) const;

	void UpdateScenesAsSourcesList(QList<SourceInfo> &outSet) const;

	bool SelectSceneCollection(QString inCollectionName) const;
	bool SelectScene(QString inSceneName) const;

	//sources
	bool RequestSourcesListUpdate(QString inCollectionName, QList<SourceInfo> &outSceneList) const;

	bool ToggleSourceVisibility(QString inSceneId, QString inSceneItemId, QString inSourceId, QString inGroupParentId, ToggleInfo inToggleInfo) const;
	bool MuteMixerSource(QString inSourceId, ToggleInfo inToggleInfo) const;

	bool IsSourceVisible(const obs_sceneitem_t *inItem) const;

	//streaming and recording
	bool RequestStartRecording() const;
	bool RequestStopRecording() const;

	bool RequestStartStreaming();
	bool RequestStopStreaming();

	//control side window
	bool SetSideWindowVisible(int key, bool visible) const;

	QWebSocket *mSocket = nullptr;

	//used to prevent sending event messages to Stream Deck, when updating streaming and recording states on request of Stream Deck
	bool mIsRespondingStreamingFlag = true;

	//used to prevent sending event messages to Stream Deck, when updating collections, scenes or sources on request of Stream Deck
	//so that we can be sure that SD doesn't get unnecessary messages
	bool mIsRespondingCollectionsSchemaFlag = true;

	//used to know when OBS is running in Studio Mode
	bool mIsStudioMode = false;

	//when a client connects, we want to force send the Studio Mode state even if the Studio Mode state did not change
	bool mForceSendStudioMode = true;

	//when changing the scene, push it to program if this bool is true
	bool mPushToProgramInStudioMode = false;

	bool mIsProcessing = false;

	bool mIsSecneCollectionChanging = false;

	// Handle request methods
	void StartStreaming(json &responseJson);
	void StopStreaming(json &responseJson);
	void StartRecording(json &responseJson);
	void StopRecording(json &responseJson);
	void GetActiveCollection(json &responseJson);
	void GetActiveSceneId(json &responseJson);
	void SetMuteAll(json &responseJson);
	void SetUnmuteAll(json &responseJson);
	void GetSideWindowInfo(json &responseJson);

	void GetRecordingAndStreamingState(json &result, json &responseJson);
	void FetchSceneCollectionsSchema(json &result, json &responseJson);
	void GetMasterMuteState(json &result, json &responseJson);
	void GetStudioModeState(json &result, json &responseJson);
	void GetNoticeMessageCount(json &result, json &responseJson);
	void GetPrismLoginState(json &result, json &responseJson);
	void GetPrismStreamState(json &result, json &responseJson);
	void GetPrismRecordState(json &result, json &responseJson);

	void SetPushToProgramInStudioMode(const json &receivedJson);
	void MakeCollectionActive(const json &receivedJson, const json &responseJson);
	void MakeSceneActive(const json &receivedJson, json &responseJson);
	void GetScenes(const json &receivedJson, json &responseJson);
	void GetSources(const json &receivedJson, json &responseJson);
	void GetScenesAndSource(const json &receivedJson, json &responseJson);

	void ShowSideWindow(const json &receivedJson, json &result, json &responseJson);
	void SetStudioModeState(const json &receivedJson, json &result, json &responseJson);

	void MakeSceneVisible(bool visible, const json &receivedJson, json &responseJson);
	void TriggerMixerAudioSource(bool mute, const json &receivedJson, json &responseJson);
};

#endif // ACTIONHELP_H
