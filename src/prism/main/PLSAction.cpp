#include "PLSAction.h"
#include "liblog.h"
#include "log/module_names.h"
#include "window-basic-main.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QUrl>

namespace action {
//---------------------- keys list defined in plugins -------------
#define IMAGE_PATH "file"
#define MEDIA_IS_LOCAL "is_local_file"
#define MEDIA_LOCAL_FILE "local_file"
#define MEDIA_INPUT "input"
#define COLOR_ABGR "color"
#define URL_IS_LOCAL "is_local_file"
#define URL_LOCAL_FILE "local_file"
#define URL_WEB_URL "url"
#define TEXT_CONTENT "text"
#define WINDOW_WIN "window"
#define GAME_WIN "window"
#define GAME_SETTING_ANY_FULLSCREEN "capture_any_fullscreen" // deprecated
#define GAME_SETTING_MODE "capture_mode"
#define GAME_SETTING_MODE_ANY "any_fullscreen"
#define GAME_SETTING_MODE_WINDOW "window"
#define GAME_SETTING_MODE_HOTKEY "hotkey"
#define MONITOR_INDEX "monitor"
#define CAMERA_DEVICE_ID "video_device_id"
#define AUDIO_DEVICE_ID "device_id"

ActionInfo::ActionInfo() : event1(), event2(), event3(), target() {}

ActionInfo::ActionInfo(QString e1, QString e2, QString e3, QString tgt) : event1(e1), event2(e2), event3(e3), target(tgt) {}

QString GetTime()
{
	QDateTime cdt(QDateTime::currentDateTime());
	cdt.setOffsetFromUtc(cdt.offsetFromUtc());
	return cdt.toString(Qt::ISODateWithMs);
}

void SendActionLog(ActionInfo act)
{
	// TODO
}

QString GetActionSourceID(const char *id)
{
	if (!id)
		return ACT_SRC_UNKNOWN;

	if (strcmp(id, "scene") == 0)
		return ACT_SRC_SCENE;

	if (strcmp(id, "group") == 0)
		return ACT_SRC_GROUP;

	switch (obs_source_get_icon_type(id)) {
	case OBS_ICON_TYPE_IMAGE:
		return ACT_SRC_IMAGE;

	case OBS_ICON_TYPE_COLOR:
		return ACT_SRC_COLOR;

	case OBS_ICON_TYPE_SLIDESHOW:
		return ACT_SRC_IMAGESLIDE;

	case OBS_ICON_TYPE_AUDIO_INPUT:
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return ACT_SRC_AUDIO;

	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return ACT_SRC_MONITOR;

	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return ACT_SRC_WINDOW;

	case OBS_ICON_TYPE_GAME_CAPTURE:
		return ACT_SRC_GAME;

	case OBS_ICON_TYPE_CAMERA:
		return ACT_SRC_CAMERA;

	case OBS_ICON_TYPE_TEXT:
		return ACT_SRC_TEXT;

	case OBS_ICON_TYPE_MEDIA:
		return ACT_SRC_MEDIA;

	case OBS_ICON_TYPE_BROWSER:
		return ACT_SRC_WEB;

	case OBS_ICON_TYPE_CUSTOM:
	case OBS_ICON_TYPE_UNKNOWN:
	default:
		return id;
	}
}

// "." is included in the length
// Generally, file extension is like ".XXX" which includes 4 chars.
#define MIN_EXTENSION_SIZE 2
#define MAX_EXTENSION_SIZE 5

QString GetFileExtension(const QString &path)
{
	int i = path.lastIndexOf(".");
	if (i < 0 || i > (path.length() - 1))
		return path; // invalid index

	int sz = path.length() - i;
	if (sz < MIN_EXTENSION_SIZE || sz > MAX_EXTENSION_SIZE)
		return path; // invalid extension

	QString temp = path;
	temp.remove(0, i);

	return temp;
}

void CheckImage(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldFile = obs_data_get_string(previous, IMAGE_PATH);
	QString newFile = obs_data_get_string(current, IMAGE_PATH);

	if (oldFile != newFile && !newFile.isEmpty())
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_IMAGE, EVENT_TYPE_CONFIRMED, GetFileExtension(newFile)));
}

QString GetMediaPath(obs_data_t *settings)
{
	bool is_local_file = obs_data_get_bool(settings, MEDIA_IS_LOCAL);
	if (is_local_file) {
		return obs_data_get_string(settings, MEDIA_LOCAL_FILE);
	} else {
		return obs_data_get_string(settings, MEDIA_INPUT);
	}
}

void CheckMedia(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldFile = GetMediaPath(previous);
	QString newFile = GetMediaPath(current);
	if (oldFile != newFile && !newFile.isEmpty())
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_MEDIA, EVENT_TYPE_CONFIRMED, GetFileExtension(newFile)));
}

void CheckColorPanel(OBSData previous, OBSData current, unsigned operationFlags)
{
	unsigned oldRGB = (uint32_t)obs_data_get_int(previous, COLOR_ABGR);
	unsigned newRGB = (uint32_t)obs_data_get_int(current, COLOR_ABGR);
	if (operationFlags & OPERATION_ADD_SOURCE || oldRGB != newRGB) {
		char buf[40];
		sprintf(buf, "ABGR#%X", newRGB);
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_COLOR, EVENT_TYPE_CONFIRMED, buf));
	}
}

QString GetURL(obs_data_t *settings)
{
	bool isLocal = obs_data_get_bool(settings, URL_IS_LOCAL);
	return obs_data_get_string(settings, isLocal ? URL_LOCAL_FILE : URL_WEB_URL);
}

void CheckBrowser(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldURL = GetURL(previous);
	QString newURL = GetURL(current);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldURL != newURL) && !newURL.isEmpty()) {
		QString encodedURL(QUrl::toPercentEncoding(newURL));
		encodedURL = QString(QUrl::toPercentEncoding(encodedURL));

		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_WEB, EVENT_TYPE_CONFIRMED, encodedURL));
	}
}

void CheckText(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldText = obs_data_get_string(previous, TEXT_CONTENT);
	QString newText = obs_data_get_string(current, TEXT_CONTENT);
	if (oldText != newText && !newText.isEmpty())
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_TEXT, EVENT_TYPE_CONFIRMED, newText));
}

void CheckWindow(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldWindow = obs_data_get_string(previous, WINDOW_WIN);
	QString newWindow = obs_data_get_string(current, WINDOW_WIN);
	if (oldWindow != newWindow && !newWindow.isEmpty())
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_WINDOW, EVENT_TYPE_CONFIRMED, newWindow));
}

static inline bool using_older_non_mode_format(obs_data_t *settings)
{
	return obs_data_has_user_value(settings, GAME_SETTING_ANY_FULLSCREEN) && !obs_data_has_user_value(settings, GAME_SETTING_MODE);
}

QString GetGameMode(OBSData settings)
{
	if (using_older_non_mode_format(settings)) {
		bool any = obs_data_get_bool(settings, GAME_SETTING_ANY_FULLSCREEN);
		return (any ? GAME_SETTING_MODE_ANY : GAME_SETTING_MODE_WINDOW);
	} else {
		return obs_data_get_string(settings, GAME_SETTING_MODE);
	}
}

void CheckGame(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldMode = GetGameMode(previous);
	QString newMode = GetGameMode(current);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldMode != newMode) && !newMode.isEmpty())
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_GAMEMODE, newMode));

	if (newMode == GAME_SETTING_MODE_WINDOW) {
		QString oldWindow = obs_data_get_string(previous, GAME_WIN);
		QString newWindow = obs_data_get_string(current, GAME_WIN);
		if (oldWindow != newWindow && !newWindow.isEmpty())
			SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_CONFIRMED, newWindow));
	}
}

void CheckMonitor(OBSData previous, OBSData current, unsigned operationFlags)
{
	long long oldID = obs_data_get_int(previous, MONITOR_INDEX);
	long long newID = obs_data_get_int(current, MONITOR_INDEX);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && newID >= 0) {
		QString key = QString(MONITOR_INDEX) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_MONITOR, EVENT_TYPE_CONFIRMED, value));
	}
}

void CheckCamera(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldID = obs_data_get_string(previous, CAMERA_DEVICE_ID);
	QString newID = obs_data_get_string(current, CAMERA_DEVICE_ID);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && !newID.isEmpty()) {
		QString key = QString(CAMERA_DEVICE_ID) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_CAMERA, EVENT_TYPE_CONFIRMED, value));
	}
}

void CheckAudio(OBSData previous, OBSData current, unsigned operationFlags)
{
	QString oldID = obs_data_get_string(previous, AUDIO_DEVICE_ID);
	QString newID = obs_data_get_string(current, AUDIO_DEVICE_ID);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && !newID.isEmpty()) {
		QString key = QString(AUDIO_DEVICE_ID) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_AUDIO, EVENT_TYPE_CONFIRMED, value));
	}
}

void CheckPropertyAction(const char *id, OBSData previous, OBSData current, unsigned operationFlags)
{
	if (!id || !previous || !current)
		return;

	switch (obs_source_get_icon_type(id)) {
	case OBS_ICON_TYPE_CAMERA:
		CheckCamera(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_AUDIO_INPUT:
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		CheckAudio(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		CheckMonitor(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_MEDIA:
		CheckMedia(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_IMAGE:
		CheckImage(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_COLOR:
		CheckColorPanel(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_BROWSER:
		CheckBrowser(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_TEXT:
		CheckText(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		CheckWindow(previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_GAME_CAPTURE:
		CheckGame(previous, current, operationFlags);
		break;

	default:
		break;
	}
}

void OnGroupChildAdded(std::vector<QString> &childs)
{
	QString strList = "(";
	unsigned count = childs.size();
	for (unsigned i = 0; i < count; ++i) {
		if (i != 0)
			strList += ",";
		strList += childs[i];
	}
	strList += ")";

	action::SendActionLog(action::ActionInfo(EVENT_MAIN_STATUS, ACT_SRC_GROUP, EVENT_TYPE_GROUP_ADDSOURCE, strList));
}

//---------------------- name space end --------------------------
}
