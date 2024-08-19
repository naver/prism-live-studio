#pragma once
#include <QString>
#include <vector>
#include "obs.hpp"

namespace action {
// There is no name of selected item in property list, so we have to generate a kay to store selected text
const auto PROPERTY_LIST_SELECTED_KEY = QString("_selected_name");

//----------------------- source list -----------------------------
const auto ACT_SRC_CAMERA = "videoCaptureDevice";
const auto ACT_SRC_AUDIO = "audioCaptureDevice";
const auto ACT_SRC_GAME = "game";
const auto ACT_SRC_MONITOR = "monitor";
const auto ACT_SRC_WINDOW = "window";
const auto ACT_SRC_MEDIA = "video";
const auto ACT_SRC_IMAGE = "image";
const auto ACT_SRC_IMAGESLIDE = "imageSlide";
const auto ACT_SRC_COLOR = "colorPanel";
const auto ACT_SRC_TEXT = "text";
const auto ACT_SRC_SCENE = "scene";
const auto ACT_SRC_WEB = "web";
const auto ACT_SRC_GROUP = "group";
const auto ACT_SRC_BGM = "bgm";
const auto ACT_SRC_PRISM_LENS = "prismLens";
const auto ACT_SRC_PRISM_MOBILE = "prismMobile";
const auto ACT_SRC_PRISM_MOBILE2 = "prismMobile2";
const auto ACT_SRC_TIMER = "timer";
const auto ACT_SRC_BGT = "Background Template";
const auto ACT_SRC_AUDIO_VISUAL = "Audio Visualizer";
const auto ACT_SRC_CHAT = "PRISM Chat";
const auto ACT_SRC_TEXT_MOTION = "Text Template";
const auto ACT_SRC_NDI = "NDI™ Source";
const auto ACT_SRC_OUTPUT_AUDIO = "Audio Output Capture";
const auto ACT_SRC_STICKER = "PRISM Sticker";
const auto ACT_SRC_GIPHY_STICKER = "Gihpy Sticker";
const auto ACT_SRC_REGION_CAPTURER = "Region capture";
const auto ACT_SRC_VIEWER_COUNT = "Viewer Count";
const auto ACT_SRC_DECKLINK_INPUT = "Decklink Input";
const auto ACT_SRC_UNKNOWN = "unknown";
const auto ACT_SRC_CHATV2 = "chatv2";
const auto ACT_SRC_CHZZK_SPONSOR_SOURCE = "prismChzzkSponsor";

//----------------------- event list (to NELO) -----------------------------
const auto ACTION_ADD_EVENT = "addEvent";
const auto ACTION_EDIT_EVENT = "editEvent";
const auto ACTION_MODE_EVENT = "modeEvent";

//----------------------- event1 list -----------------------------
const auto EVENT_MAIN_ADD = "button";
const auto EVENT_MAIN_EDIT = "button";
const auto EVENT_MAIN_STATUS = "event";
const auto EVENT_SENSE_AR_INIT = "init";
const auto EVENT_MAIN_STATISTICS_INIT = "init";
const auto EVENT_APP = "app";
const auto EVENT_CRASH = "crash";
const auto EVENT_EXTERNAL_PLUGIN_INIT = "init";
const auto EVENT_PLAY = "play";
const auto EVENT_USE = "use";

//----------------------- event2 list -----------------------------
const auto EVENT_SUB_SOURCE_ADDED = "source";
const auto EVENT_SUB_EFFECT = "effect";
const auto EVENT_SENSE_AR_NAME = "SenseAR";
const auto EVENT_SUB_STATISTICS_DEVICE = "device";
const auto EVENT_SUB_STATISTICS_VERSION = "version";
const auto EVENT_SUB_STATISTICS_ID = "id";
const auto EVENT_APP_INIT = "init";
const auto EVENT_CRASH_GENERAL = "general";
const auto EVENT_CRASH_DEVICE = "device";
const auto EVENT_CRASH_PLUGIN = "plugin";
const auto EVENT_CRASH_PROGRAM = "program";
const auto EVENT_PLUGIN = "plugin";
const auto EVENT_PRISM_MUSIC = "prismMusic";
const auto EVENT_APP_LIVING = "live";
const auto EVENT_APP_RECORDING = "record";
const auto EVENT_USE_USAGE = "usage";

//----------------------- event3 list -----------------------------
const auto EVENT_TYPE_CONFIRMED = "confirmed";
const auto EVENT_TYPE_MODE = "modeSelected";
const auto EVENT_TYPE_GROUP_ADDSOURCE = "Added";
const auto EVENT_TYPE_BEAUTY = "beauty";
const auto EVENT_TYPE_SMOOTH = "smooth";
const auto EVENT_SENSE_AR_VERSION = "v.7.3.2";
const auto EVENT_TYPE_STATISTICS_CPU = "cpu";
const auto EVENT_TYPE_STATISTICS_GPU = "gpu";
const auto EVENT_TYPE_STATISTICS_MEM = "mem";
const auto EVENT_TYPE_STATISTICS_WIN_VER = "win_ver";
const auto EVENT_TYPE_STATISTICS_PRISM_VER = "prism_ver";
const auto EVENT_TYPE_STATISTICS_DIRECTX_VER = "directx_ver";
const auto EVENT_APP_INIT_RESULT_SUCCESS = "success";
const auto EVENT_APP_INIT_RESULT_FAIL = "fail";
const auto EVENT_CRASH_UNKNOWN_DEVICE = "unknown_device";
const auto EVENT_CRASH_CAM_DEVICE = "cam_device";
const auto EVENT_CRASH_AUDIO_DEVICE = "audio_interface";
const auto EVENT_CRASH_GRAPHIC_CARD = "graphic_card";
const auto EVENT_CRASH_EXTERNAL_PLUGIN = "external_plugin";
const auto EVENT_CRASH_VST_PLUGIN = "vst";
const auto EVENT_CRASH_GENERAL_TYPE = "other_type";
const auto EVENT_CRASH_THIRD_PROGRAM = "third_program";
const auto EVENT_EXTERNAL_PLUGIN_NAME = "3rdPlugin";
const auto EVENT_PLAYED = "played";

//----------------------- event4 list -----------------------------
const auto EVENT_APP_INIT_DX_ERROR = "dxError";
const auto EVENT_APP_INIT_API_ERROR = "apiError";

//-----------------------------------------------------------------

#define SENSEAR_API_PATH QStringLiteral("/prismpc/license/sensetime")
const auto SENSEAR_VER_KEY = "sensetimeVer";

struct ActionInfo {
	QString event1;
	QString event2;
	QString event3;
	QString target;
	QString resourceID;
	QString time;
	bool setUserId;

	ActionInfo() = default;
	ActionInfo(QString e1, QString e2, QString e3, QString tgt);
	ActionInfo(QString e1, QString e2, QString e3, QString tgt, QString resID, bool setUserId_ = false);
};

void NotifyActionLogReady();
void NotifyUserIdReady();

void SendActionToNelo(const char *plugin, const char *event, const char *value);
void SendActionLog(ActionInfo act);
QString GetActionSourceID(const char *id);
void CheckPropertyAction(OBSSource source, OBSData previous, OBSData current, unsigned operationFlags);
void OnGroupChildAdded(std::vector<QString> &childs);
void SendPropToNelo(const char *plugin, const char *proptype, const char *propContent);
void SendPropsToNelo(const char *plugin, const std::vector<std::pair<std::string, std::string>> &fields);

//---------------------- name space end --------------------------
};
