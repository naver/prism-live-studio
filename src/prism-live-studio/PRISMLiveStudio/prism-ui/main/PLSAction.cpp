#include "PLSAction.h"
#include "liblog.h"
#include "log/module_names.h"
#include "window-basic-main.hpp"
#include "prism/PLSPlatformPrism.h"
#include "login-user-info.hpp"
#include "pls/pls-source.h"

#include <QDateTime>
#include <QJsonArray>
#include <QUrl>

namespace action {
//---------------------- keys list defined in plugins -------------
const auto IMAGE_PATH = "file";
const auto MEDIA_IS_LOCAL = "is_local_file";
const auto MEDIA_LOCAL_FILE = "local_file";
const auto MEDIA_INPUT = "input";
const auto COLOR_ABGR = "color";
const auto URL_IS_LOCAL = "is_local_file";
const auto URL_LOCAL_FILE = "local_file";
const auto URL_WEB_URL = "url";
const auto TEXT_CONTENT = "text";
const auto WINDOW_WIN = "window";
const auto GAME_WIN = "window";
const auto GAME_SETTING_ANY_FULLSCREEN = "capture_any_fullscreen"; // deprecated
const auto GAME_SETTING_MODE = "capture_mode";
const auto GAME_SETTING_MODE_ANY = "any_fullscreen";
const auto GAME_SETTING_MODE_WINDOW = "window";
const auto GAME_SETTING_MODE_HOTKEY = "hotkey";
#if defined(_WIN32)
const auto CAMERA_DEVICE_ID = "video_device_id";
#else
const auto CAMERA_DEVICE_ID = "device";
const auto GAME_SOURCE = "uuid";
const auto GAME_ALLOW_TRANSPARENCY = "allow_transparency";
#endif
const auto USE_CUSTOM_AUDIO = "use_custom_audio_device";
const auto CUSTOM_AUDIO_DEVICE_ID = "audio_device_id";
const auto AUDIO_DEVICE_ID = "device_id";
const auto TRANSITION = "transition";
const auto SLIDE_TIME = "slide_time";
const auto TRANSITION_SPEED = "transition_speed";
const auto DECKLINK_DEVICE_HASH = "device_hash";

#define CHECK_SOURCE_ID(target)                        \
	if (pluginID && strcmp(pluginID, target) != 0) \
	return

ActionInfo::ActionInfo(QString e1, QString e2, QString e3, QString tgt) : event1(e1), event2(e2), event3(e3), target(tgt) {}

ActionInfo::ActionInfo(QString e1, QString e2, QString e3, QString tgt, QString resID, bool setUserId_) : event1(e1), event2(e2), event3(e3), target(tgt), resourceID(resID), setUserId(setUserId_) {}

QString GetTime()
{
	QDateTime cdt(QDateTime::currentDateTime());
	cdt.setOffsetFromUtc(cdt.offsetFromUtc());
	return cdt.toString(Qt::ISODateWithMs);
}

namespace {
struct LocalGlobalVars {
	static std::recursive_mutex actionLock;
	static std::vector<QString> actionList;
	static std::vector<ActionInfo> actionListWithUserId;
	static std::atomic<bool> actionLogReady;
	static std::atomic<bool> actionLogUserIdReady;
};
}

std::recursive_mutex LocalGlobalVars::actionLock;
std::vector<QString> LocalGlobalVars::actionList;
std::vector<ActionInfo> LocalGlobalVars::actionListWithUserId;
std::atomic<bool> LocalGlobalVars::actionLogReady = false;
std::atomic<bool> LocalGlobalVars::actionLogUserIdReady = false;

using FieldInfo = std::vector<std::pair<std::string, std::string>>;

void SendActionCache()
{
	assert(LocalGlobalVars::actionLogReady);
	if (!pls_get_gpop_connection().url.isEmpty()) {
		std::lock_guard<std::recursive_mutex> auto_lock(LocalGlobalVars::actionLock);
		for (auto item : LocalGlobalVars::actionList) {
			PLSPlatformPrism::instance()->sendAction(item);
		}
		LocalGlobalVars::actionList.clear();
	}
}

void SendActionWithUserIdCache()
{
	assert(LocalGlobalVars::actionLogUserIdReady);
	std::lock_guard<std::recursive_mutex> auto_lock(LocalGlobalVars::actionLock);
	for (const auto &act : LocalGlobalVars::actionListWithUserId) {
		QJsonObject obj;
		obj["eventAt"] = act.time;
		obj["event1"] = act.event1;
		obj["event2"] = act.event2;
		obj["event3"] = act.event3;
		obj["targetId"] = act.target;
		obj["resourceId"] = PLSLoginUserInfo::getInstance()->getUserCodeWithEncode();
		QJsonArray array;
		array.push_back(obj);

		QJsonDocument doc;
		doc.setArray(array);

		QString text(doc.toJson());
		PLSPlatformPrism::instance()->sendAction(text);
	}
	LocalGlobalVars::actionListWithUserId.clear();
}

void NotifyUserIdReady()
{
	LocalGlobalVars::actionLogUserIdReady = true;
	SendActionWithUserIdCache();
}

void NotifyActionLogReady()
{
	LocalGlobalVars::actionLogReady = true;
	SendActionCache();
}

void SendActionLog(ActionInfo act)
{
	if (act.setUserId && !LocalGlobalVars::actionLogUserIdReady) {
		std::lock_guard<std::recursive_mutex> auto_lock(LocalGlobalVars::actionLock);
		act.time = GetTime();
		LocalGlobalVars::actionListWithUserId.push_back(act);
		return;
	}

	QJsonObject obj;
	obj["eventAt"] = GetTime();
	obj["event1"] = act.event1;
	obj["event2"] = act.event2;
	obj["event3"] = act.event3;
	obj["targetId"] = act.target;
	if (!act.resourceID.isEmpty()) {
		obj["resourceId"] = act.resourceID;
	}

	QJsonArray array;
	array.push_back(obj);

	QJsonDocument doc;
	doc.setArray(array);

	QString text(doc.toJson());

	if (!LocalGlobalVars::actionLogReady) {
		std::lock_guard<std::recursive_mutex> auto_lock(LocalGlobalVars::actionLock);
		LocalGlobalVars::actionList.push_back(text);
	} else {
		SendActionCache();
		//PLSPlatformPrism::instance()->sendAction(text);
	}
}

void SendActionToNelo(const char *plugin, const char *event, const char *value)
{
	PLS_LOGEX(PLS_LOG_INFO, MAIN_ACTION_LOG, {{"plugin", plugin}, {event, value}}, "Nelo action log. <%s> [%s : %s]", plugin, event, value);
}

void SendPropToNelo(const char *plugin, const char *proptype, const char *propContent)
{
	PLS_LOGEX(PLS_LOG_INFO, MAIN_ACTION_LOG, {{"plugin", plugin}, {"proptype", proptype}, {"propContent", propContent}}, "Source Content.<%s>", plugin);
}

void SendPropsToNelo(const char *plugin, const std::vector<std::pair<std::string, std::string>> &fields)
{
	for (const auto &field : fields) {
		SendPropToNelo(plugin, field.first.c_str(), field.second.c_str());
	}
}

QString GetActionSourceID(const char *id)
{
	if (!id)
		return ACT_SRC_UNKNOWN;

	if (strcmp(id, "scene") == 0)
		return ACT_SRC_SCENE;

	if (strcmp(id, "group") == 0)
		return ACT_SRC_GROUP;

	switch (static_cast<int>(obs_source_get_icon_type(id))) {
	case OBS_ICON_TYPE_CAMERA:
		return ACT_SRC_CAMERA;
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return ACT_SRC_AUDIO;
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return "audioOutputCaptureDevice";
	case PLS_ICON_TYPE_NDI:
		return ACT_SRC_NDI;
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return ACT_SRC_GAME;
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return "monitorWhole";
	case PLS_ICON_TYPE_REGION:
		return "monitorPart";
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return ACT_SRC_WINDOW;
	case OBS_ICON_TYPE_BROWSER:
		return ACT_SRC_WEB;
	case OBS_ICON_TYPE_MEDIA:
		return ACT_SRC_MEDIA;
	case OBS_ICON_TYPE_IMAGE:
		return ACT_SRC_IMAGE;
	case OBS_ICON_TYPE_SLIDESHOW:
		return ACT_SRC_IMAGESLIDE;
	case OBS_ICON_TYPE_COLOR:
		return ACT_SRC_COLOR;
	case OBS_ICON_TYPE_TEXT:
		return ACT_SRC_TEXT;
	case PLS_ICON_TYPE_BGM:
		return ACT_SRC_BGM;
	case PLS_ICON_TYPE_TEXT_TEMPLATE:
		return ACT_SRC_TEXT_MOTION;
	case PLS_ICON_TYPE_CHAT:
		return ACT_SRC_CHAT;
	case PLS_ICON_TYPE_PRISM_MOBILE:
		return ACT_SRC_PRISM_MOBILE;
	case PLS_ICON_TYPE_PRISM_TIMER:
		return ACT_SRC_TIMER;
	case PLS_ICON_TYPE_VIRTUAL_BACKGROUND:
		return ACT_SRC_BGT;
	case PLS_ICON_TYPE_SPECTRALIZER:
		return ACT_SRC_AUDIO_VISUAL;
	case PLS_ICON_TYPE_VIEWER_COUNT:
		return ACT_SRC_VIEWER_COUNT;
	case PLS_ICON_TYPE_DECKLINK_INPUT:
		return ACT_SRC_DECKLINK_INPUT;
	case PLS_ICON_TYPE_CHAT_TEMPLATE:
		return ACT_SRC_CHATV2;
	case PLS_ICON_TYPE_PRISM_LENS:
		return ACT_SRC_PRISM_LENS;
	case PLS_ICON_TYPE_CHZZK_SPONSOR:
		return ACT_SRC_CHZZK_SPONSOR_SOURCE;
	default:
		return id;
	}
}

// "." is included in the length
// Generally, file extension is like ".XXX" which includes 4 chars.
const auto MIN_EXTENSION_SIZE = 2;
const auto MAX_EXTENSION_SIZE = 5;

QString GetFileExtension(const QString &path)
{
	auto i = path.lastIndexOf(".");
	if (i < 0 || i > (path.length() - 1))
		return path; // invalid index

	auto sz = path.length() - i;
	if (sz < MIN_EXTENSION_SIZE || sz > MAX_EXTENSION_SIZE)
		return path; // invalid extension

	QString temp = path;
	temp.remove(0, i);

	return temp;
}

void CheckImage(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	CHECK_SOURCE_ID(common::IMAGE_SOURCE_ID);

	QString oldFile = obs_data_get_string(previous, IMAGE_PATH);
	QString newFile = obs_data_get_string(current, IMAGE_PATH);

	if (oldFile != newFile && !newFile.isEmpty()) {
		std::string ext = GetFileExtension(newFile).toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, ext.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_IMAGE, EVENT_TYPE_CONFIRMED, GetFileExtension(newFile)));
		SendPropToNelo(pluginID, IMAGE_PATH, ext.c_str());
	}
}

void CheckSlideShow(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	CHECK_SOURCE_ID(common::SLIDESHOW_SOURCE_ID);

	QString oldTransitionType = obs_data_get_string(previous, TRANSITION);
	QString newTransitionType = obs_data_get_string(current, TRANSITION);
	if (oldTransitionType.compare(newTransitionType, Qt::CaseInsensitive) != 0 && !newTransitionType.isEmpty()) {
		SendPropToNelo(pluginID, TRANSITION, qUtf8Printable(newTransitionType));
	}

	auto oldTrSpeed = obs_data_get_int(previous, TRANSITION_SPEED);
	auto newTrSpeed = obs_data_get_int(current, TRANSITION_SPEED);
	if (oldTrSpeed != newTrSpeed && newTrSpeed != 0) {
		SendPropToNelo(pluginID, "transitionSpeed", qUtf8Printable(QString::number(newTrSpeed)));
	}

	auto oldDuration = obs_data_get_int(previous, SLIDE_TIME);
	auto newDuration = obs_data_get_int(current, SLIDE_TIME);
	if (oldDuration != newDuration && newDuration != 0) {
		SendPropToNelo(pluginID, "timeBetweenSlides", qUtf8Printable(QString::number(newDuration)));
	}
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

void CheckMedia(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	CHECK_SOURCE_ID(common::MEDIA_SOURCE_ID);

	QString oldFile = GetMediaPath(previous);
	QString newFile = GetMediaPath(current);
	if (oldFile != newFile && !newFile.isEmpty()) {
		std::string ext = GetFileExtension(newFile).toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, ext.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_MEDIA, EVENT_TYPE_CONFIRMED, GetFileExtension(newFile)));
		SendPropToNelo(pluginID, "mediaFile", ext.c_str());
	}
}

void CheckColorPanel(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::COLOR_SOURCE_ID);

	auto oldRGB = (uint32_t)obs_data_get_int(previous, COLOR_ABGR);
	auto newRGB = (uint32_t)obs_data_get_int(current, COLOR_ABGR);
	if (operationFlags & OPERATION_ADD_SOURCE || oldRGB != newRGB) {
		std::array<char, 40> buf;
		snprintf(buf.data(), buf.size(), "ABGR#%X", newRGB);
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, buf.data());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_COLOR, EVENT_TYPE_CONFIRMED, buf.data()));
		SendPropToNelo(pluginID, COLOR_ABGR, buf.data());
	}
}

QString GetURL(obs_data_t *settings)
{
	bool isLocal = obs_data_get_bool(settings, URL_IS_LOCAL);
	return obs_data_get_string(settings, isLocal ? URL_LOCAL_FILE : URL_WEB_URL);
}

QString GetAudioVisulTempName(OBSData data)
{
	long long index = obs_data_get_int(data, "visualizer_template_list");
	switch (index) {
	case 0:
		return "basic bar";

	case 1:
		return "fillet bar";

	case 2:
		return "linear bar";

	case 3:
		return "gradient bar";

	default:
		return "";
	}
}

QString GetTextTempName(OBSData data)
{
	auto id = (int)obs_data_get_int(data, "textmotion.template.list");
	return QString("%1").arg(id);
}

QString GetTimerTempName(OBSData data)
{
	auto tab = (int)obs_data_get_int(data, "template_list");
	switch (tab) {
	case 0:
		return "basic";

	case 1:
		return "round";

	case 2:
		return "flip";

	case 3:
		return "message";

	default:
		return "";
	}
}

QString GetMobileOutput(OBSData data)
{
	auto output = (int)obs_data_get_int(data, "output");
	switch (output) {
	case 0:
		return "all";

	case 1:
		return "video";

	case 2:
		return "audio";

	default:
		return "";
	}
}

QString GetChatTempName(OBSData data)
{
	long long index = obs_data_get_int(data, "ChatSource.Template");
	if (index != 0) {
		return QString("Style %1").arg(index);
	} else {
		return "";
	}
}

void CheckBrowser(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::BROWSER_SOURCE_ID);

	QString oldURL = GetURL(previous);
	QString newURL = GetURL(current);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldURL != newURL) && !newURL.isEmpty()) {
		QString encodedURL(QUrl::toPercentEncoding(newURL));
		encodedURL = QString(QUrl::toPercentEncoding(encodedURL));

		std::string temp = encodedURL.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_WEB, EVENT_TYPE_CONFIRMED, encodedURL));
		SendPropToNelo(pluginID, URL_WEB_URL, temp.c_str());
	}
}

void CheckText(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	CHECK_SOURCE_ID(common::GDIP_TEXT_SOURCE_ID);

	QString oldText = obs_data_get_string(previous, TEXT_CONTENT);
	QString newText = obs_data_get_string(current, TEXT_CONTENT);

	QString fontFamily;
	auto oldFontObj = obs_data_get_obj(previous, "font");
	auto newFontObj = obs_data_get_obj(current, "font");
	if (newFontObj) {
		if (!oldFontObj) {
			const char *font_face = obs_data_get_string(newFontObj, "face");
			fontFamily = (font_face && *font_face) ? font_face : "";
		} else {
			const char *new_font_face = obs_data_get_string(newFontObj, "face");
			const char *old_font_face = obs_data_get_string(oldFontObj, "face");
			if (new_font_face && old_font_face && 0 != strcmp(new_font_face, old_font_face)) {
				fontFamily = new_font_face;
			}
		}
	}
	if (oldText != newText && !newText.isEmpty()) {
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_TEXT, EVENT_TYPE_CONFIRMED, newText));
		SendPropToNelo(pluginID, "text", newText.toStdString().c_str());
	}

	if (!fontFamily.isEmpty()) {
		SendPropToNelo(pluginID, "fontFamily", fontFamily.toStdString().c_str());
	}
}

QString GetWindowMode(OBSData settings)
{
	// it is defined in window plugin
	enum class window_capture_method {
		METHOD_AUTO,
		METHOD_BITBLT,
		METHOD_WGC,
	};

	auto type = static_cast<window_capture_method>(obs_data_get_int(settings, "method"));
	switch (type) {
	case window_capture_method::METHOD_AUTO:
		return "AUTO";
	case window_capture_method::METHOD_BITBLT:
		return "BITBLT";
	case window_capture_method::METHOD_WGC:
		return "WGC";
	default:
		return "";
	}
}

void CheckWindow(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::WINDOW_SOURCE_ID);

#if defined(_WIN32)
	QString oldMode = GetWindowMode(previous);
	QString newMode = GetWindowMode(current);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldMode != newMode) && !newMode.isEmpty()) {
		std::string temp = newMode.toStdString();
		SendActionToNelo(pluginID, ACTION_MODE_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_WINDOW, EVENT_TYPE_MODE, newMode));
		SendPropToNelo(pluginID, "captureMethod", temp.c_str());
	}
#endif

#if defined(_WIN32)
	QString oldWindow = obs_data_get_string(previous, WINDOW_WIN);
	QString newWindow = obs_data_get_string(current, WINDOW_WIN);
#else
	QString oldWindow = QString::number(obs_data_get_int(previous, WINDOW_WIN));
	QString newWindow = QString::number(obs_data_get_int(current, WINDOW_WIN));
#endif
	if (oldWindow != newWindow && !newWindow.isEmpty()) {
		std::string temp = newWindow.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_WINDOW, EVENT_TYPE_CONFIRMED, newWindow));
		SendPropToNelo(pluginID, "window", temp.c_str());
	}
}

void CheckAudioVirual(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = GetAudioVisulTempName(previous);
	QString newValue = GetAudioVisulTempName(current);
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_AUDIO_VISUAL, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "templateName", temp.c_str());
	}
}

void CheckChatSource(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = GetChatTempName(previous);
	QString newValue = GetChatTempName(current);
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_CHAT, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "templateName", temp.c_str());
	}
}

void CheckTextTemp(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = GetTextTempName(previous);
	QString newValue = GetTextTempName(current);
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_TEXT_MOTION, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "templateId", temp.c_str());
	}
}

void CheckTimer(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = GetTimerTempName(previous);
	QString newValue = GetTimerTempName(current);
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_TIMER, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "templateName", temp.c_str());
	}
}

void CheckMobileOutput(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = GetMobileOutput(previous);
	QString newValue = GetMobileOutput(current);
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_PRISM_MOBILE, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "outputType", temp.c_str());
	}
}

void CheckNDI(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = obs_data_get_string(previous, "ndi_source_name");
	QString newValue = obs_data_get_string(current, "ndi_source_name");
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_NDI, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "sourceName", temp.c_str());
	}
}

void CheckBackgroundTemp(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	QString oldValue = obs_data_get_string(previous, "item_id");
	QString newValue = obs_data_get_string(current, "item_id");
	if (oldValue != newValue && !newValue.isEmpty()) {
		std::string temp = newValue.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_BGT, EVENT_TYPE_CONFIRMED, newValue));
		SendPropToNelo(pluginID, "templateId", temp.c_str());
	}
}

void CheckViewerCount(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	auto oldValue = obs_data_get_int(previous, "viewercount.template");
	auto newValue = obs_data_get_int(current, "viewercount.template");
	if (oldValue != newValue) {
		std::string temp = std::to_string(newValue);
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_VIEWER_COUNT, EVENT_TYPE_CONFIRMED, QString::number(newValue)));
		SendPropToNelo(pluginID, "templateId", temp.c_str());
	}
}

static inline bool using_older_non_mode_format(obs_data_t *settings)
{
	return obs_data_has_user_value(settings, GAME_SETTING_ANY_FULLSCREEN) && !obs_data_has_user_value(settings, GAME_SETTING_MODE);
}

QString GetGameMode(OBSData settings)
{
	if (using_older_non_mode_format(settings)) {
		bool any = obs_data_get_bool(settings, GAME_SETTING_ANY_FULLSCREEN);
		return any ? GAME_SETTING_MODE_ANY : GAME_SETTING_MODE_WINDOW;
	} else {
		return obs_data_get_string(settings, GAME_SETTING_MODE);
	}
}

void CheckGame(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::GAME_SOURCE_ID);

#if defined(_WIN32)
	QString oldMode = GetGameMode(previous);
	QString newMode = GetGameMode(current);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldMode != newMode) && !newMode.isEmpty()) {
		std::string temp = newMode.toStdString();
		SendActionToNelo(pluginID, ACTION_MODE_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_MODE, newMode));
		SendPropToNelo(pluginID, "captureMode", temp.c_str());
	}

	if (newMode == GAME_SETTING_MODE_WINDOW) {
		QString oldWindow = obs_data_get_string(previous, GAME_WIN);
		QString newWindow = obs_data_get_string(current, GAME_WIN);
		if (oldWindow != newWindow && !newWindow.isEmpty()) {
			std::string temp = newWindow.toStdString();
			SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
			SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_CONFIRMED, newWindow));
			SendPropToNelo(pluginID, "captureWindow", temp.c_str());
		}
	}
#else
	QString oldSource = obs_data_get_string(previous, GAME_SOURCE);
	QString newSource = obs_data_get_string(current, GAME_SOURCE);

	if ((operationFlags & OPERATION_ADD_SOURCE || oldSource != newSource) && !newSource.isEmpty()) {
		std::string temp = newSource.toStdString();
		SendActionToNelo(pluginID, ACTION_MODE_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_MODE, newSource));
		SendPropToNelo(pluginID, "source", temp.c_str());
	}

	auto oldTransparency = obs_data_get_bool(previous, GAME_ALLOW_TRANSPARENCY);
	auto newTransparency = obs_data_get_bool(current, GAME_ALLOW_TRANSPARENCY);
	if (oldTransparency != newTransparency) {
		std::string temp = newTransparency ? "true" : "false";
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_GAME, EVENT_TYPE_CONFIRMED, newTransparency ? "true" : "false"));
		SendPropToNelo(pluginID, "allowTransparency", temp.c_str());
	}
#endif
}
QString GetMonitorMode(OBSData settings)
{
	// it is defined in monitor plugin
	enum class monitor_capture_method {
		METHOD_NONE = 0,
		METHOD_D3D,
		METHOD_WGC,
		METHOD_BITBLT,
	};

	switch ((enum monitor_capture_method)obs_data_get_int(settings, "method")) {
	case monitor_capture_method::METHOD_D3D:
		return "D3D11";
	case monitor_capture_method::METHOD_BITBLT:
		return "BITBLT";
	case monitor_capture_method::METHOD_WGC:
		return "WGC";
	default:
		return "";
	}
}

QString GetDiplayCropMode(OBSData settings)
{
	enum class display_crop_mode {
		CROP_NONE = 0,
		CROP_MANUAL,
		CROP_TO_WINDOW,
		CROP_TO_WINDOW_AND_MANUAL,
	};

	switch ((enum display_crop_mode)obs_data_get_int(settings, "crop_mode")) {
	case display_crop_mode::CROP_NONE:
		return "None";
	case display_crop_mode::CROP_MANUAL:
		return "Manual";
	case display_crop_mode::CROP_TO_WINDOW:
		return "To window";
	case display_crop_mode::CROP_TO_WINDOW_AND_MANUAL:
		return "To window and manual";
	default:
		return "";
	}
}

std::tuple<QString, int> GetScreenCaptureType(OBSData settings)
{
	auto type = obs_data_get_int(settings, "type");
	switch (type) {
	case 0:
		return {"Display Capture", 0};
	case 1:
		return {"Window Capture", 1};
	case 2:
		return {"Application Capture", 2};
	default:
		return {};
	}
}

void CheckMonitor(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::PRISM_MONITOR_SOURCE_ID);

#if defined(_WIN32)
	QString oldMode = GetMonitorMode(previous);
	QString newMode = GetMonitorMode(current);
	const char *method = "captureMethod";
	const char *display_key = "monitor_id";
	QString oldID = obs_data_get_string(previous, display_key);
	QString newID = obs_data_get_string(current, display_key);
#else
	QString oldMode = GetDiplayCropMode(previous);
	QString newMode = GetDiplayCropMode(current);
	const char *method = "cropMethod";
	const char *display_key = "display";
	QString oldID = QString::number(obs_data_get_int(previous, display_key));
	QString newID = QString::number(obs_data_get_int(current, display_key));
#endif

	if ((operationFlags & OPERATION_ADD_SOURCE || oldMode != newMode) && !newMode.isEmpty()) {
		std::string temp = newMode.toStdString();
		SendActionToNelo(pluginID, ACTION_MODE_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_MONITOR, EVENT_TYPE_MODE, newMode));
		SendPropToNelo(pluginID, method, temp.c_str());
	}

	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && !newID.isEmpty()) {
		QString key = QString(display_key) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		if (!value.isEmpty()) {
			SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_MONITOR, EVENT_TYPE_CONFIRMED, value));
			SendPropToNelo(pluginID, "display", value.toStdString().c_str());
		}
	}
}

void CheckScreenCapture(const char *pluginID, OBSData previous, OBSData current, unsigned)
{
	CHECK_SOURCE_ID(common::OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID);

	enum class capture_type {
		Display_capture = 0,
		Window_capture,
		Application_capture,
	};

	auto oldCaptureType = GetScreenCaptureType(previous);
	auto newCaptureType = GetScreenCaptureType(current);

	if (std::get<0>(newCaptureType) != std::get<0>(oldCaptureType) && !std::get<0>(newCaptureType).isEmpty()) {
		SendPropToNelo(pluginID, "captureMethod", qUtf8Printable(std::get<0>(newCaptureType)));
	}

	auto type = static_cast<capture_type>(std::get<1>(newCaptureType));
	if (capture_type::Display_capture == type) {
		auto oldDisplay = obs_data_get_int(previous, "display");
		auto newDisplay = obs_data_get_int(current, "display");
		if (oldDisplay != newDisplay && newDisplay >= 0) {
			QString key = QString("display") + PROPERTY_LIST_SELECTED_KEY;
			QString value = obs_data_get_string(current, key.toUtf8());
			SendPropToNelo(pluginID, "display", qUtf8Printable(value));
		}
	} else if (capture_type::Window_capture == type) {
		auto oldWindow = obs_data_get_int(previous, "window");
		auto newWindow = obs_data_get_int(current, "window");
		if (oldWindow != newWindow && newWindow >= 0) {
			QString key = QString("window") + PROPERTY_LIST_SELECTED_KEY;
			QString value = obs_data_get_string(current, key.toUtf8());
			SendPropToNelo(pluginID, "window", qUtf8Printable(value));
		}
	} else if (capture_type::Application_capture == type) {
		QString oldApp = obs_data_get_string(previous, "application");
		QString newApp = obs_data_get_string(current, "application");
		if (oldApp != newApp && !newApp.isEmpty()) {
			SendPropToNelo(pluginID, "application", qUtf8Printable(newApp));
		}
	}
}

void CheckCamera(OBSSource source, const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
#if _WIN32
	CHECK_SOURCE_ID(common::OBS_DSHOW_SOURCE_ID);
#else
	// It contains the SRE of legacy video capture source, the new video capture source and the capture card source.
	if (pluginID && strcmp(pluginID, common::OBS_DSHOW_SOURCE_ID) != 0 && strcmp(pluginID, common::OBS_MACOS_VIDEO_CAPTURE_SOURCE_ID) != 0 && strcmp(pluginID, common::OBS_MACOS_CAPTURE_CARD_SOURCE_ID) != 0) {
		return;
	}
#endif

	QString oldID = obs_data_get_string(previous, CAMERA_DEVICE_ID);
	QString newID = obs_data_get_string(current, CAMERA_DEVICE_ID);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && !newID.isEmpty()) {
		QString key = QString(CAMERA_DEVICE_ID) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		std::string temp = value.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, ACT_SRC_CAMERA, EVENT_TYPE_CONFIRMED, value));
		SendPropToNelo(pluginID, "cameraDeviceName", temp.c_str());
	}

#if defined(_WIN32)
	if (!newID.isEmpty()) {
		QString oldAudioID = obs_data_get_string(previous, CUSTOM_AUDIO_DEVICE_ID);
		QString newAudioID = obs_data_get_string(current, CUSTOM_AUDIO_DEVICE_ID);

		bool oldUseAudio = obs_data_get_bool(previous, USE_CUSTOM_AUDIO);
		bool newUseAudio = obs_data_get_bool(current, USE_CUSTOM_AUDIO);

		if (operationFlags & OPERATION_ADD_SOURCE || oldID != newID || oldAudioID != newAudioID || oldUseAudio != newUseAudio) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_string(settings, "method", "notify_user_confirm");
			pls_source_set_private_data(source, settings);
			obs_data_release(settings);
		}
	}
#endif
}

void CheckDeckLink(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::DECKLINK_INPUT_SOURCE_ID);

	QString oldDeviceHash = obs_data_get_string(previous, DECKLINK_DEVICE_HASH);
	QString newDeviceHash = obs_data_get_string(current, DECKLINK_DEVICE_HASH);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldDeviceHash != newDeviceHash) && !newDeviceHash.isEmpty()) {
		QString key = QString(DECKLINK_DEVICE_HASH) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		std::string temp = value.toStdString();
		SendPropToNelo(pluginID, "deviceName", temp.c_str());
	}
}

void CheckAudio(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags, QString src)
{
	if (strcmp(pluginID, common::AUDIO_INPUT_SOURCE_ID) != 0 && strcmp(pluginID, common::AUDIO_OUTPUT_SOURCE_ID) != 0 && strcmp(pluginID, common::AUDIO_OUTPUT_SOURCE_ID_V2) != 0)
		return;

	QString oldID = obs_data_get_string(previous, AUDIO_DEVICE_ID);
	QString newID = obs_data_get_string(current, AUDIO_DEVICE_ID);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldID != newID) && !newID.isEmpty()) {
		QString key = QString(AUDIO_DEVICE_ID) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		std::string temp = value.toStdString();
		SendActionToNelo(pluginID, ACTION_EDIT_EVENT, temp.c_str());
		SendActionLog(ActionInfo(EVENT_MAIN_EDIT, src, EVENT_TYPE_CONFIRMED, value));
		SendPropToNelo(pluginID, "audioDeviceName", temp.c_str());
	}
}

void CheckApplicationAudio(const char *pluginID, OBSData previous, OBSData current, unsigned operationFlags)
{
	CHECK_SOURCE_ID(common::OBS_APP_AUDIO_CAPTURE_ID);

	QString oldWindow = obs_data_get_string(previous, WINDOW_WIN);
	QString newWindow = obs_data_get_string(current, WINDOW_WIN);
	if ((operationFlags & OPERATION_ADD_SOURCE || oldWindow != newWindow) && !newWindow.isEmpty()) {
		QString key = QString(WINDOW_WIN) + PROPERTY_LIST_SELECTED_KEY;
		QString value = obs_data_get_string(current, key.toUtf8());
		std::string temp = value.toStdString();
		SendPropToNelo(pluginID, "window", temp.c_str());
	}
}

void CheckPropertyAction(OBSSource source, OBSData previous, OBSData current, unsigned operationFlags)
{
	if (!source || !previous || !current)
		return;

	const char *id = obs_source_get_unversioned_id(source);
	int iconType = obs_source_get_icon_type(obs_source_get_id(source));
	switch (iconType) {
	case OBS_ICON_TYPE_CAMERA:
	case PLS_ICON_TYPE_CAPTURE_CARD:
		CheckCamera(source, id, previous, current, operationFlags);
		CheckDeckLink(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_AUDIO_INPUT:
		CheckAudio(id, previous, current, operationFlags, ACT_SRC_AUDIO);
		break;

	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		CheckAudio(id, previous, current, operationFlags, ACT_SRC_OUTPUT_AUDIO);
		break;

	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		CheckApplicationAudio(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		CheckScreenCapture(id, previous, current, operationFlags);
		CheckMonitor(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_MEDIA:
		CheckMedia(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_IMAGE:
		CheckImage(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_SLIDESHOW:
		CheckSlideShow(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_COLOR:
		CheckColorPanel(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_BROWSER:
		CheckBrowser(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_TEXT:
		CheckText(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		CheckWindow(id, previous, current, operationFlags);
		break;

	case OBS_ICON_TYPE_GAME_CAPTURE:
		CheckGame(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_NDI:
		CheckNDI(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_TEXT_TEMPLATE:
		CheckTextTemp(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_CHAT:
		CheckChatSource(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_SPECTRALIZER:
		CheckAudioVirual(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_VIRTUAL_BACKGROUND:
		CheckBackgroundTemp(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_PRISM_TIMER:
		CheckTimer(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_PRISM_MOBILE:
		CheckMobileOutput(id, previous, current, operationFlags);
		break;

	case PLS_ICON_TYPE_VIEWER_COUNT:
		CheckViewerCount(id, previous, current, operationFlags);
		break;

	default:
		break;
	}
}

void OnGroupChildAdded(std::vector<QString> &childs)
{
	QString strList = "(";
	size_t count = childs.size();
	for (size_t i = 0; i < count; ++i) {
		if (i != 0)
			strList += ",";
		strList += childs[i];
	}
	strList += ")";

	action::SendActionLog(action::ActionInfo(EVENT_MAIN_STATUS, ACT_SRC_GROUP, EVENT_TYPE_GROUP_ADDSOURCE, strList));
}

//---------------------- name space end --------------------------
}
