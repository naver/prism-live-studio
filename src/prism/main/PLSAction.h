#pragma once
#include <QString>
#include <vector>
#include "obs.hpp"

namespace action {
// There is no name of selected item in property list, so we have to generate a kay to store selected text
#define PROPERTY_LIST_SELECTED_KEY QString("_selected_name")

//----------------------- source list -----------------------------
#define ACT_SRC_CAMERA "videoCaptureDevice"
#define ACT_SRC_AUDIO "audioCaptureDevice "
#define ACT_SRC_GAME "game"
#define ACT_SRC_MONITOR "monitor"
#define ACT_SRC_WINDOW "window"
#define ACT_SRC_MEDIA "video"
#define ACT_SRC_IMAGE "image"
#define ACT_SRC_IMAGESLIDE "imageSlide"
#define ACT_SRC_COLOR "colorPanel"
#define ACT_SRC_TEXT "text"
#define ACT_SRC_SCENE "scene"
#define ACT_SRC_WEB "web"
#define ACT_SRC_GROUP "group"
#define ACT_SRC_UNKNOWN "unknown"

//----------------------- event1 list -----------------------------
#define EVENT_MAIN_ADD "button"
#define EVENT_MAIN_EDIT "button"
#define EVENT_MAIN_STATUS "event"

//----------------------- event2 list -----------------------------
#define EVENT_SUB_SOURCE_ADDED "source"

//----------------------- event3 list -----------------------------
#define EVENT_TYPE_CONFIRMED "confirmed"
#define EVENT_TYPE_GAMEMODE "modeSelected"
#define EVENT_TYPE_GROUP_ADDSOURCE "Added"

//-----------------------------------------------------------------
struct ActionInfo {
	QString event1;
	QString event2;
	QString event3;
	QString target;

	ActionInfo();
	ActionInfo(QString e1, QString e2, QString e3, QString tgt);
};

void SendActionLog(ActionInfo act);
QString GetActionSourceID(const char *id);
void CheckPropertyAction(const char *id, OBSData previous, OBSData current, unsigned operationFlags);
void OnGroupChildAdded(std::vector<QString> &childs);

//---------------------- name space end --------------------------
};
