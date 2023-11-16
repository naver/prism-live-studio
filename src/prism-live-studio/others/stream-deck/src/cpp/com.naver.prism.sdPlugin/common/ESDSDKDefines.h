//==============================================================================
/**
@file       ESDSDKDefines.h

@brief      Defines used for the Stream Deck communication

@copyright  (c) 2018, Corsair Memory, Inc.
			This source code is licensed under the MIT-style license found in the LICENSE file.

**/
//==============================================================================

#pragma once

//
// Current version of the SDK
//

#define kESDSDKVersion 2

//
// Common base-interface
//

#define kESDSDKCommonAction "action"
#define kESDSDKCommonEvent "event"
#define kESDSDKCommonContext "context"
#define kESDSDKCommonPayload "payload"
#define kESDSDKCommonDevice "device"
#define kESDSDKCommonDeviceInfo "deviceInfo"

//
// Events
//

#define kESDSDKEventKeyDown "keyDown"
#define kESDSDKEventKeyUp "keyUp"
#define kESDSDKEventWillAppear "willAppear"
#define kESDSDKEventWillDisappear "willDisappear"
#define kESDSDKEventDeviceDidConnect "deviceDidConnect"
#define kESDSDKEventDeviceDidDisconnect "deviceDidDisconnect"
#define kESDSDKEventApplicationDidLaunch "applicationDidLaunch"
#define kESDSDKEventApplicationDidTerminate "applicationDidTerminate"
#define kESDSDKEventSystemDidWakeUp "systemDidWakeUp"
#define kESDSDKEventTitleParametersDidChange "titleParametersDidChange"
#define kESDSDKEventDidReceiveSettings "didReceiveSettings"
#define kESDSDKEventDidReceiveGlobalSettings "didReceiveGlobalSettings"
#define kESDSDKEventPropertyInspectorDidAppear "propertyInspectorDidAppear"
#define kESDSDKEventPropertyInspectorDidDisappear "propertyInspectorDidDisappear"

//
// Functions
//

#define kESDSDKEventSetTitle "setTitle"
#define kESDSDKEventSetImage "setImage"
#define kESDSDKEventShowAlert "showAlert"
#define kESDSDKEventShowOK "showOk"
#define kESDSDKEventGetSettings "getSettings"
#define kESDSDKEventSetSettings "setSettings"
#define kESDSDKEventGetGlobalSettings "getGlobalSettings"
#define kESDSDKEventSetGlobalSettings "setGlobalSettings"
#define kESDSDKEventSetState "setState"
#define kESDSDKEventSwitchToProfile "switchToProfile"
#define kESDSDKEventSendToPropertyInspector "sendToPropertyInspector"
#define kESDSDKEventSendToPlugin "sendToPlugin"
#define kESDSDKEventOpenURL "openUrl"
#define kESDSDKEventLogMessage "logMessage"

//
// Payloads
//

#define kESDSDKPayloadSettings "settings"
#define kESDSDKPayloadCoordinates "coordinates"
#define kESDSDKPayloadState "state"
#define kESDSDKPayloadUserDesiredState "userDesiredState"
#define kESDSDKPayloadTitle "title"
#define kESDSDKPayloadTitleParameters "titleParameters"
#define kESDSDKPayloadImage "image"
#define kESDSDKPayloadURL "url"
#define kESDSDKPayloadTarget "target"
#define kESDSDKPayloadProfile "profile"
#define kESDSDKPayloadApplication "application"
#define kESDSDKPayloadIsInMultiAction "isInMultiAction"
#define kESDSDKPayloadMessage "message"

#define kESDSDKPayloadCoordinatesColumn "column"
#define kESDSDKPayloadCoordinatesRow "row"

//
// Device Info
//

#define kESDSDKDeviceInfoID "id"
#define kESDSDKDeviceInfoType "type"
#define kESDSDKDeviceInfoSize "size"
#define kESDSDKDeviceInfoName "name"

#define kESDSDKDeviceInfoSizeColumns "columns"
#define kESDSDKDeviceInfoSizeRows "rows"

//
// Title Parameters
//

#define kESDSDKTitleParametersShowTitle "showTitle"
#define kESDSDKTitleParametersTitleColor "titleColor"
#define kESDSDKTitleParametersTitleAlignment "titleAlignment"
#define kESDSDKTitleParametersFontFamily "fontFamily"
#define kESDSDKTitleParametersFontSize "fontSize"
#define kESDSDKTitleParametersCustomFontSize "customFontSize"
#define kESDSDKTitleParametersFontStyle "fontStyle"
#define kESDSDKTitleParametersFontUnderline "fontUnderline"

//
// Connection
//

#define kESDSDKConnectSocketFunction "connectElgatoStreamDeckSocket"
#define kESDSDKRegisterPlugin "registerPlugin"
#define kESDSDKRegisterPropertyInspector "registerPropertyInspector"
#define kESDSDKPortParameter "-port"
#define kESDSDKPluginUUIDParameter "-pluginUUID"
#define kESDSDKRegisterEventParameter "-registerEvent"
#define kESDSDKInfoParameter "-info"
#define kESDSDKRegisterUUID "uuid"

#define kESDSDKApplicationInfo "application"
#define kESDSDKPluginInfo "plugin"
#define kESDSDKDevicesInfo "devices"
#define kESDSDKColorsInfo "colors"
#define kESDSDKDevicePixelRatio "devicePixelRatio"

#define kESDSDKApplicationInfoVersion "version"
#define kESDSDKApplicationInfoLanguage "language"
#define kESDSDKApplicationInfoPlatform "platform"

#define kESDSDKApplicationInfoPlatformMac "mac"
#define kESDSDKApplicationInfoPlatformWindows "windows"

#define kESDSDKColorsInfoHighlightColor "highlightColor"
#define kESDSDKColorsInfoMouseDownColor "mouseDownColor"
#define kESDSDKColorsInfoDisabledColor "disabledColor"
#define kESDSDKColorsInfoButtonPressedTextColor "buttonPressedTextColor"
#define kESDSDKColorsInfoButtonPressedBackgroundColor "buttonPressedBackgroundColor"
#define kESDSDKColorsInfoButtonMouseOverBackgroundColor "buttonMouseOverBackgroundColor"
#define kESDSDKColorsInfoButtonPressedBorderColor "buttonPressedBorderColor"

//
// UUid
//

#define KEY_UUID_SCENE "com.naver.streamdeck.pls.scene"
#define KEY_UUID_SOURCE "com.naver.streamdeck.pls.source"
#define KEY_UUID_RECORD "com.naver.streamdeck.pls.record"
#define KEY_UUID_STREAM "com.naver.streamdeck.pls.stream"
#define KEY_UUID_AUDIO_MIXER "com.naver.streamdeck.pls.mixeraudio"
#define KEY_UUID_SIDE_WINDOW "com.naver.streamdeck.pls.sidewindow"
#define KEY_UUID_MUTE_ALL "com.naver.streamdeck.pls.muteall"

#define KEY_UUID_TOAST_MESSAGE "com.naver.streamdeck.pls.toastmessage"
#define KEY_UUID_STUDIO_MODE "com.naver.streamdeck.pls.studiomode"

#define KEY_UUID_CPU_USAGE "com.naver.streamdeck.pls.cpuusage"
#define KEY_UUID_FRAME_DROP "com.naver.streamdeck.pls.framedrop"
#define KEY_UUID_STREAM_BITERATE "com.naver.streamdeck.pls.bitrate"

//
// Prism Events
//

#define EVENT_STREAMING_STATUS_CHANGED "StreamingService.streamingStatusChange"
#define EVENT_RECORDING_STATUS_CHANGED "StreamingService.recordingStatusChange"
#define EVENT_SCENE_SWITCHED "ScenesService.sceneSwitched"
#define EVENT_SCENE_ADDED "ScenesService.sceneAdded"
#define EVENT_SCENECOLLECTION_SWITCHED "SceneCollectionsService.collectionSwitched"
#define EVENT_SCENECOLLECTION_ADDED "SceneCollectionsService.collectionAdded"
#define EVENT_STUDIOMODE_STATUS_CHANGED "StudioModeService.studioModeStatusChange"
#define EVENT_MUTE_ALL_STATUS_CHANGED "Mainview.audioMixerMuteChanged"
#define EVENT_SIDE_WINDOW_VISIBLE_CHANGED "Mainview.sideWindowVisibleChanged"
#define EVENT_SOURCE_ITEM_UPDATED "ScenesService.itemUpdated"
#define EVENT_SOURCE_UPDATED "SourcesService.sourceUpdated"
#define EVENT_SIDE_WINDOW_ALL_REGISTERD "Mainview.sideWindowAllRegisterd"
#define EVENT_WINDOW_LOADING_FINISHED "Mainview.windowLoadingFinished"
#define EVENT_WINDOW_UPDATE_MESSAGE_COUNT "Mainview.updateNoticeMessageCount"
#define EVENT_WINDOW_STUDIO_MODE_SWITCHED "StudioModeService.studioModeStatusChange"

#define EVENT_WINDOW_UPDATE_CPU_USAGE "Mainview.updateCPUUsage"
#define EVENT_WINDOW_UPDATE_FRAME_DROP "Mainview.updateFramedrop"
#define EVENT_WINDOW_UPDATE_BITRATE "Mainview.updateBitrate"
#define EVENT_WINDOW_LOGIN_STATE_CHANGED "Mainview.loginStateChanged"
#define EVENT_WINDOW_STREAM_STATE_CHANGED "Mainview.streamStateChanged"
#define EVENT_WINDOW_RECORD_STATE_CHANGED "Mainview.recordStateChanged"

//
// Prism streaming state
//
#define STREAM_STATE_READY_STATE "readyState"
#define STREAM_STATE_BROADCAST_GO "broadcastGo"
#define STREAM_STATE_CAN_BROADCAST_STATE "canBroadcastState"
#define STREAM_STATE_STREAM_STARTING "streamStarting"
#define STREAM_STATE_STREAM_STARTED "streamStarted"
#define STREAM_STATE_STOP_BROADCAST_GO "stopBroadcastGo"
#define STREAM_STATE_CAN_BROADCAST_STOP "canBroadcastStop"
#define STREAM_STATE_STREAM_STOPPING "streamStopping"
#define STREAM_STATE_STREAM_STOPPED "streamStopped"
#define STREAM_STATE_STREAM_END "streamEnd"

//
// Prism recording state
//
#define RECORD_STATE_RECORD_READY "recordReady"
#define RECORD_STATE_CAN_RECORD "canRecord"
#define RECORD_STATE_RECORD_STARTING "recordStarting"
#define RECORD_STATE_RECORD_STARTED "recordStarted"
#define RECORD_STATE_RECORD_STOPPING "recordStopping"
#define RECORD_STATE_RECORD_STOPGO "recordStopGo"
#define RECORD_STATE_RECORD_STOPPED "recordStopped"

//
// Prism login state
//
#define LOGIN_STATE_LOGINED "logined"
#define LOGIN_STATE_LOGINING "logining"
#define LOGIN_STATE_LOGIN_FAILED "loginFailed"

typedef int ESDSDKTarget;
enum { kESDSDKTarget_HardwareAndSoftware = 0, kESDSDKTarget_HardwareOnly = 1, kESDSDKTarget_SoftwareOnly = 2 };

typedef int ESDSDKDeviceType;
enum { kESDSDKDeviceType_StreamDeck = 0, kESDSDKDeviceType_StreamDeckMini = 1, kESDSDKDeviceType_StreamDeckXL = 2, kESDSDKDeviceType_StreamDeckMobile = 3 };

typedef int StateType;
enum { State_off = 0, State_On = 1, State_Disable = 2 };

typedef int SideWindowType;
enum { None = -1, BeautyConfig = 1, GiphyStickersConfig, BgmConfig, LivingMsgView, ChatConfig, WiFiConfig, VirtualbackgroundConfig };

static const int RPC_ID_startStreaming = 1;
static const int RPC_ID_stopStreaming = 2;
static const int RPC_ID_startRecording = 3;
static const int RPC_ID_stopRecording = 4;
//static const int RPC_ID_getCollections = 5;
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

static const int RPC_ID_updateSceneList = 1000;
static const int RPC_ID_updateSourceList = 1001;
static const int RPC_ID_getPrismConnectionState = 1002;
static const int RPC_ID_prismLoadingFinished = 1003;
