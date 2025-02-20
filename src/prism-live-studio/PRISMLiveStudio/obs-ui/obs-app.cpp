/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <time.h>
#include <stdio.h>
#include <wchar.h>
#include <chrono>
#include <ratio>
#include <string>
#include <sstream>
#include <mutex>
#include <util/bmem.h>
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/cf-parser.h>
#include <obs-config.h>
#include <obs.hpp>

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QProcess>
#include <QAccessible>

#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "obs-proxy-style.hpp"
#include "log-viewer.hpp"
#include "volume-control.hpp"
#include "window-basic-main.hpp"
#include "liblog.h"
#include "PLSMotionFileManager.h"
#include "pls/pls-obs-api.h"

#ifdef __APPLE__
#include "window-permissions.hpp"
#endif
#include "window-basic-settings.hpp"
#include "platform.hpp"

#include <fstream>

#include <curl/curl.h>

#include "libipc.h"
#include "PLSIPCHandler.h"
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <util/windows/win-version.h>
#include <gdiplus.h>
#else
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(_WIN32) || defined(ENABLE_SPARKLE_UPDATER)
#include "update/models/branches.hpp"
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#include <qpa/qplatformnativeinterface.h>
#endif

#include <iostream>

#include "ui-config.h"
//#ifdef Q_OS_WIN
#include "PLSDumpAnalyzer.h"
//#else
//#endif

#include "log/log.h"
#include "PLSBasic.h"

#include <chrono>

#include <libutils-api.h>
#include <frontend-api.h>
#include "pls-shared-values.h"
#include "frontend-internal.hpp"
#include "PLSApp.h"
#include "PLSGuideTipsframe.h"
#include "PLSErrorHandler.h"

#ifdef _WIN32
#pragma comment(lib, "Gdiplus.lib")
#endif
#include <QSettings>

using namespace std;

namespace {
struct LocalGlobalVars {

	static log_handler_t def_log_handler;
	static string currentLogFile;
	static string lastLogFile;
	static string lastCrashLogFile;

	static bool unclean_shutdown;
	static bool disable_shutdown_check;
	static bool multi;

	static bool log_verbose;
	static bool unfiltered_log;

	static string opt_starting_collection;
	static string opt_starting_profile;
#if defined(Q_OS_WIN)
	static HANDLE handle;
#endif
};

log_handler_t LocalGlobalVars::def_log_handler;
string LocalGlobalVars::currentLogFile;
string LocalGlobalVars::lastLogFile;
string LocalGlobalVars::lastCrashLogFile;

bool LocalGlobalVars::unclean_shutdown = false;
bool LocalGlobalVars::disable_shutdown_check = false;
bool LocalGlobalVars::multi = false;

bool LocalGlobalVars::log_verbose = false;
bool LocalGlobalVars::unfiltered_log = false;
string LocalGlobalVars::opt_starting_collection;
string LocalGlobalVars::opt_starting_profile;
#if defined(Q_OS_WIN)
HANDLE LocalGlobalVars::handle = nullptr;
#endif
}
bool GlobalVars::isLogined = true;
std::chrono::steady_clock::time_point GlobalVars::startTime;

std::string GlobalVars::prismSession;
std::string GlobalVars::prismSubSession;
std::string GlobalVars::videoAdapter;
std::string GlobalVars::crashFileMutexUuid;

std::string GlobalVars::logUserID;
std::string GlobalVars::maskingLogUserID{};

bool GlobalVars::portable_mode = false;
bool GlobalVars::steam = false;
bool GlobalVars::safe_mode = false;
bool GlobalVars::disable_3p_plugins = false;
bool GlobalVars::restart_safe = false;
QStringList GlobalVars::arguments{};

bool GlobalVars::opt_start_streaming = false;
bool GlobalVars::opt_start_recording = false;
bool GlobalVars::opt_studio_mode = false;
bool GlobalVars::opt_start_replaybuffer = false;
bool GlobalVars::opt_start_virtualcam = false;
bool GlobalVars::opt_disable_updater = false;
bool GlobalVars::opt_disable_missing_files_check = false;

bool GlobalVars::opt_minimize_tray = false;
bool GlobalVars::opt_allow_opengl = false;
bool GlobalVars::opt_always_on_top = false;

bool GlobalVars::remuxAfterRecord = false;
string GlobalVars::opt_starting_scene;
string GlobalVars::remuxFilename;
string GlobalVars::gcc = "KR";
QStringList GlobalVars::gpuNames;
bool GlobalVars::restart = false;
QPointer<OBSLogViewer> GlobalVars::obsLogViewer = nullptr;
bool GlobalVars::isStartByDaemon = false;
bool GlobalVars::g_bUseAPIServer = true;
#ifdef ENABLE_TEST
bool GlobalVars::unitTest = false;
int GlobalVars::unitTestExitCode = 0;
#endif
#ifndef _WIN32
int OBSApp::sigintFd[2];
#endif
// GPU hint exports for AMD/NVIDIA laptops
#ifdef _MSC_VER
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

QObject *CreateShortcutFilter(QObject *parent)
{
	return new OBSEventFilter(parent, [](QObject *obj, QEvent *event) {
		auto mouse_event = [](QMouseEvent &event) {
			if (!App()->HotkeysEnabledInFocus() &&
			    event.button() != Qt::LeftButton)
				return true;

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event.type() == QEvent::MouseButtonPress;

			switch (event.button()) {
			case Qt::NoButton:
			case Qt::LeftButton:
			case Qt::RightButton:
			case Qt::AllButtons:
			case Qt::MouseButtonMask:
				return false;

			case Qt::MiddleButton:
				hotkey.key = OBS_KEY_MOUSE3;
				break;

#define MAP_BUTTON(i, j)                       \
	case Qt::ExtraButton##i:               \
		hotkey.key = OBS_KEY_MOUSE##j; \
		break;
				MAP_BUTTON(1, 4);
				MAP_BUTTON(2, 5);
				MAP_BUTTON(3, 6);
				MAP_BUTTON(4, 7);
				MAP_BUTTON(5, 8);
				MAP_BUTTON(6, 9);
				MAP_BUTTON(7, 10);
				MAP_BUTTON(8, 11);
				MAP_BUTTON(9, 12);
				MAP_BUTTON(10, 13);
				MAP_BUTTON(11, 14);
				MAP_BUTTON(12, 15);
				MAP_BUTTON(13, 16);
				MAP_BUTTON(14, 17);
				MAP_BUTTON(15, 18);
				MAP_BUTTON(16, 19);
				MAP_BUTTON(17, 20);
				MAP_BUTTON(18, 21);
				MAP_BUTTON(19, 22);
				MAP_BUTTON(20, 23);
				MAP_BUTTON(21, 24);
				MAP_BUTTON(22, 25);
				MAP_BUTTON(23, 26);
				MAP_BUTTON(24, 27);
#undef MAP_BUTTON
			}

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event.modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		auto key_event = [&](QKeyEvent *event) {
			int key = event->key();
			bool enabledInFocus = App()->HotkeysEnabledInFocus();

			if (key != Qt::Key_Enter && key != Qt::Key_Escape &&
			    key != Qt::Key_Return && !enabledInFocus)
				return true;

			QDialog *dialog = qobject_cast<QDialog *>(obj);

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event->type() == QEvent::KeyPress;

			switch (key) {
			case Qt::Key_Shift:
			case Qt::Key_Control:
			case Qt::Key_Alt:
			case Qt::Key_Meta:
				break;

#ifdef __APPLE__
			case Qt::Key_CapsLock:
				// kVK_CapsLock == 57
				hotkey.key = obs_key_from_virtual_key(57);
				pressed = true;
				break;
#endif

			case Qt::Key_Enter:
			case Qt::Key_Escape:
			case Qt::Key_Return:
				if (dialog && pressed)
					return false;
				if (!enabledInFocus)
					return true;
				/* Falls through. */
			default:
				hotkey.key = obs_key_from_virtual_key(
					event->nativeVirtualKey());
			}

			if (event->isAutoRepeat())
				return true;

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(
				event->modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
			return mouse_event(*static_cast<QMouseEvent *>(event));

		/*case QEvent::MouseButtonDblClick:
		case QEvent::Wheel:*/
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return key_event(static_cast<QKeyEvent *>(event));

		default:
			return false;
		}
	});
}

static void LogString(fstream &logFile, const char *timeString, char *str,
		      int log_level)
{
	static mutex logfile_mutex;
	string msg;
	msg += timeString;
	msg += str;

	logfile_mutex.lock();
	logFile << msg << endl;
	logfile_mutex.unlock();

	if (!!GlobalVars::obsLogViewer)
		QMetaObject::invokeMethod(GlobalVars::obsLogViewer.data(),
					  "AddLine", Qt::QueuedConnection,
					  Q_ARG(int, log_level),
					  Q_ARG(QString, QString(msg.c_str())));
}

static inline void LogStringChunk(fstream &logFile, char *str, int log_level)
{
	char *nextLine = str;
	string timeString = CurrentTimeString();
	timeString += ": ";

	while (*nextLine) {
		char *nextLine = strchr(str, '\n');
		if (!nextLine)
			break;

		if (nextLine != str && nextLine[-1] == '\r') {
			nextLine[-1] = 0;
		} else {
			nextLine[0] = 0;
		}

		LogString(logFile, timeString.c_str(), str, log_level);
		nextLine++;
		str = nextLine;
	}

	LogString(logFile, timeString.c_str(), str, log_level);
}

#define MAX_REPEATED_LINES 30
#define MAX_CHAR_VARIATION (255 * 3)

static inline int sum_chars(const char *str)
{
	int val = 0;
	for (; *str != 0; str++)
		val += *str;

	return val;
}

static inline bool too_many_repeated_entries(fstream &logFile, const char *msg,
					     const char *output_str)
{
	static mutex log_mutex;
	static const char *last_msg_ptr = nullptr;
	static int last_char_sum = 0;
	static int rep_count = 0;

	int new_sum = sum_chars(output_str);

	lock_guard<mutex> guard(log_mutex);

	if (LocalGlobalVars::unfiltered_log) {
		return false;
	}

	if (last_msg_ptr == msg) {
		int diff = std::abs(new_sum - last_char_sum);
		if (diff < MAX_CHAR_VARIATION) {
			return (rep_count++ >= MAX_REPEATED_LINES);
		}
	}

	if (rep_count > MAX_REPEATED_LINES) {
		logFile << CurrentTimeString()
			<< ": Last log entry repeated for "
			<< to_string(rep_count - MAX_REPEATED_LINES)
			<< " more lines" << endl;
	}

	last_msg_ptr = msg;
	last_char_sum = new_sum;
	rep_count = 0;

	return false;
}

static void do_log(int log_level, const char *msg, va_list args, void *param)
{
	fstream &logFile = *static_cast<fstream *>(param);
	char str[8192];

	va_list args2;
	va_copy(args2, args);

	vsnprintf(str, sizeof(str), msg, args);

#ifdef _WIN32
	if (IsDebuggerPresent()) {
		int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
		if (wNum > 1) {
			static wstring wide_buf;
			static mutex wide_mutex;

			lock_guard<mutex> lock(wide_mutex);
			wide_buf.reserve(wNum + 1);
			wide_buf.resize(wNum - 1);
			MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0],
					    wNum);
			wide_buf.push_back('\n');

			OutputDebugStringW(wide_buf.c_str());
		}
	}
#endif

	if (log_level <= LOG_INFO || LocalGlobalVars::log_verbose) {

		if (!too_many_repeated_entries(logFile, msg, str))
			LogStringChunk(logFile, str, log_level);
	}

#if defined(_WIN32) && defined(OBS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif

	va_end(args2);
}

#define DEFAULT_LANG "en-US"

bool OBSApp::InitGlobalConfigDefaults()
{
	config_set_default_uint(globalConfig, "General", "MaxLogs", 10);
	config_set_default_int(globalConfig, "General", "InfoIncrement", -1);
	config_set_default_string(globalConfig, "General", "ProcessPriority",
				  "Normal");
	config_set_default_bool(globalConfig, "General", "EnableAutoUpdates",
				true);

	config_set_default_bool(globalConfig, "General", "ConfirmOnExit", true);
	config_set_default_bool(globalConfig, "General", "Watermark", true);

#if _WIN32
	config_set_default_string(globalConfig, "Video", "Renderer",
				  "Direct3D 11");
#else
	config_set_default_string(globalConfig, "Video", "Renderer", "OpenGL");
#endif

	config_set_default_bool(globalConfig, "BasicWindow", "PreviewEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"VerticalPreviewEnabled", false);
	config_set_default_bool(globalConfig, "BasicWindow",
				"PreviewProgramMode", true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"SceneDuplicationMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SnappingEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "ScreenSnapping",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "SourceSnapping",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "CenterSnapping",
				false);
	config_set_default_double(globalConfig, "BasicWindow", "SnapDistance",
				  10.0);
	config_set_default_bool(globalConfig, "BasicWindow",
				"SpacingHelpersEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"RecordWhenStreaming", false);
	config_set_default_bool(globalConfig, "BasicWindow",
				"KeepRecordingWhenStreamStops", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayEnabled",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"SysTrayWhenStarted", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SaveProjectors",
				false);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowTransitions",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowStatusBar",
				true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowSourceIcons",
				true);
	config_set_default_bool(globalConfig, "BasicWindow",
				"ShowContextToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow", "StudioModeLabels",
				true);

	config_set_default_string(globalConfig, "General", "HotkeyFocusType",
				  "NeverDisableHotkeys");

	config_set_default_bool(globalConfig, "BasicWindow",
				"VerticalVolControl", false);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewMouseSwitch", true);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewDrawNames", true);

	config_set_default_bool(globalConfig, "BasicWindow",
				"MultiviewDrawAreas", true);

#ifdef _WIN32
	config_set_default_bool(globalConfig, "Audio", "DisableAudioDucking",
				true);
	config_set_default_bool(globalConfig, "General", "BrowserHWAccel",
				true);
#endif

#ifdef __APPLE__
	config_set_default_bool(globalConfig, "General", "BrowserHWAccel",
				true);
	config_set_default_bool(globalConfig, "Video", "DisableOSXVSync", true);
	config_set_default_bool(globalConfig, "Video", "ResetOSXVSyncOnExit",
				true);
#endif

	config_set_default_bool(globalConfig, "BasicWindow",
				"MediaControlsCountdownTimer", true);

	return true;
}

static bool do_mkdir(const char *path)
{
	if (os_mkdirs(path) == MKDIR_ERROR) {
		PLS_ERROR(MAINFILTER_MODULE, "failed to create directory.");
		PLS_ERROR_KR(MAINFILTER_MODULE, "failed to create directory %s",
			     path);
		return false;
	}

	return true;
}

static bool MakeUserDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/logs") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
	//create file to resolve streamup plugin
	QDir dir(QString::fromUtf8(path));
	if (dir.isEmpty()) {
		pls_write_data(
			QString::fromUtf8(path) +
				QString("/%1.txt").arg(
					QDateTime::currentDateTime().toString(
						"yyyyMMddhhmmss")),
			"");
	}
	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/profiler_data") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/crashes") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
#endif

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/user") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
	if (!do_mkdir(path))
		return false;
	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/updates") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/Cache") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/naver_shopping") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/crashDump") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/laboratory") <=
	    0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static bool MakeUserProfileDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/basic/profiles") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes") <=
	    0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static string GetProfileDirFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/basic/profiles") <= 0)
		return outputPath;

	strcat(path, "/*");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (!ent.directory)
			continue;

		strcpy(path, ent.path);
		strcat(path, "/basic.ini");

		ConfigFile config;
		if (config.Open(path, CONFIG_OPEN_EXISTING) != 0)
			continue;

		const char *curName =
			config_get_string(config, "General", "Name");
		if (astrcmpi(curName, name) == 0) {
			outputPath = ent.path;
			break;
		}
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

static string GetSceneCollectionFileFromName(const char *name)
{
	string outputPath;
	os_glob_t *glob;
	char path[512];

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes") <=
	    0)
		return outputPath;

	strcat(path, "/*.json");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (ent.directory)
			continue;

		OBSDataAutoRelease data =
			obs_data_create_from_json_file_safe(ent.path, "bak");
		const char *curName = obs_data_get_string(data, "name");

		if (astrcmpi(name, curName) == 0) {
			outputPath = ent.path;
			obs_data_release(data);
			break;
		}

		obs_data_release(data);
	}

	os_globfree(glob);

	if (!outputPath.empty()) {
		outputPath.resize(outputPath.size() - 5);
		replace(outputPath.begin(), outputPath.end(), '\\', '/');
		const char *start = strrchr(outputPath.c_str(), '/');
		if (start)
			outputPath.erase(0, start - outputPath.c_str() + 1);
	}

	return outputPath;
}

bool OBSApp::UpdatePre22MultiviewLayout(const char *layout)
{
	if (!layout)
		return false;

	if (astrcmpi(layout, "horizontaltop") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::HORIZONTAL_TOP_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "horizontalbottom") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::HORIZONTAL_BOTTOM_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalleft") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::VERTICAL_LEFT_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalright") == 0) {
		config_set_int(
			globalConfig, "BasicWindow", "MultiviewLayout",
			static_cast<int>(
				MultiviewLayout::VERTICAL_RIGHT_8_SCENES));
		return true;
	}

	return false;
}

bool OBSApp::InitGlobalConfig()
{
	char path[512];
	bool changed = false;

	int len =
		GetConfigPath(path, sizeof(path), "PRISMLiveStudio/global.ini");
	if (len <= 0) {
		return false;
	}

	int errorcode = globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		OBSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}

	if (!LocalGlobalVars::opt_starting_collection.empty()) {
		string path = GetSceneCollectionFileFromName(
			LocalGlobalVars::opt_starting_collection.c_str());
		if (!path.empty()) {
			config_set_string(
				globalConfig, "Basic", "SceneCollection",
				LocalGlobalVars::opt_starting_collection
					.c_str());
			config_set_string(globalConfig, "Basic",
					  "SceneCollectionFile", path.c_str());
			changed = true;
		}
	}

	if (!LocalGlobalVars::opt_starting_profile.empty()) {
		string path = GetProfileDirFromName(
			LocalGlobalVars::opt_starting_profile.c_str());
		if (!path.empty()) {
			config_set_string(
				globalConfig, "Basic", "Profile",
				LocalGlobalVars::opt_starting_profile.c_str());
			config_set_string(globalConfig, "Basic", "ProfileDir",
					  path.c_str());
			changed = true;
		}
	}

	uint32_t lastVersion =
		config_get_int(globalConfig, "General", "LastVersion");

	if (!config_has_user_value(globalConfig, "General", "Pre19Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(19, 0, 0);

		config_set_bool(globalConfig, "General", "Pre19Defaults",
				useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre21Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(21, 0, 0);

		config_set_bool(globalConfig, "General", "Pre21Defaults",
				useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre23Defaults")) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(23, 0, 0);

		config_set_bool(globalConfig, "General", "Pre23Defaults",
				useOldDefaults);
		changed = true;
	}

#define PRE_24_1_DEFS "Pre24.1Defaults"
	if (!config_has_user_value(globalConfig, "General", PRE_24_1_DEFS)) {
		bool useOldDefaults = lastVersion &&
				      lastVersion <
					      MAKE_SEMANTIC_VERSION(24, 1, 0);

		config_set_bool(globalConfig, "General", PRE_24_1_DEFS,
				useOldDefaults);
		changed = true;
	}
#undef PRE_24_1_DEFS

	if (config_has_user_value(globalConfig, "BasicWindow",
				  "MultiviewLayout")) {
		const char *layout = config_get_string(
			globalConfig, "BasicWindow", "MultiviewLayout");
		changed |= UpdatePre22MultiviewLayout(layout);
	}

	if (lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(24, 0, 0)) {
		bool disableHotkeysInFocus = config_get_bool(
			globalConfig, "General", "DisableHotkeysInFocus");
		if (disableHotkeysInFocus)
			config_set_string(globalConfig, "General",
					  "HotkeyFocusType",
					  "DisableHotkeysInFocus");
		changed = true;
	}

	if (changed)
		config_save_safe(globalConfig, "tmp", nullptr);

	return InitGlobalConfigDefaults();
}

// support locale by function
static bool loadLocaleFile(TextLookup &lookup, const char *filePath)
{
	if (lookup) {
		return text_lookup_add(lookup, filePath);
	} else if (lookup = text_lookup_create(filePath); lookup) {
		return true;
	}
	return false;
}
static void loadLocaleFile(TextLookup &lookup, const char *filePath,
			   const char *fileName)
{
	if (loadLocaleFile(lookup, filePath)) {
		PLS_INFO(MAINFRAME_MODULE, "Successful to load locale file %s.",
			 fileName);
	} else {
		PLS_ERROR(MAINFRAME_MODULE, "Failed to load locale file %s.",
			  fileName);
	}
}
static void loadLocaleDir(TextLookup &lookup, const QString &languageDir,
			  const QString &language,
			  const QString &relativeFileName = QString())
{
	QDir dir(languageDir);
	auto fis = dir.entryInfoList(QDir::Dirs | QDir::Files |
					     QDir::NoDotAndDotDot,
				     QDir::DirsLast | QDir::Name);
	for (const auto &fi : fis) {
		QString fileName;
		if (!relativeFileName.isEmpty()) {
			fileName = relativeFileName + '/' + fi.fileName();
		} else {
			fileName = fi.fileName();
		}

		if (fi.isDir()) {
			loadLocaleDir(lookup, fi.absoluteFilePath(), language,
				      fileName);
		} else if (fi.baseName() == language) {
			loadLocaleFile(
				lookup,
				fi.absoluteFilePath().toUtf8().constData(),
				fileName.toUtf8().constData());
		}
	}
}

bool OBSApp::InitLocale()
{
	ProfileScope("OBSApp::InitLocale");
	PLS_INIT_INFO(MAINFRAME_MODULE, "initialize app language");
	locale = pls_prism_get_locale().toStdString();

	// set basic default application locale
	if (!locale.empty())
		QLocale::setDefault(QLocale(
			QString::fromStdString(locale).replace('-', '_')));

	string localePath;
	if (!GetDataFilePath("locale", localePath)) {
		PLS_ERROR(MAINFRAME_MODULE, "Failed to find %s.",
			  localePath.c_str());
		return false;
	}

	loadLocaleDir(textLookup, QString::fromStdString(localePath),
		      QStringLiteral(DEFAULT_LANG));
	if (locale != DEFAULT_LANG) {
		loadLocaleDir(textLookup, QString::fromStdString(localePath),
			      QString::fromStdString(locale));
	}
	return true;
}

bool OBSApp::HotkeyEnable() const
{
	return hotkeyEnable;
}

#if defined(_WIN32) || defined(ENABLE_SPARKLE_UPDATER)
void ParseBranchesJson(const std::string &jsonString, vector<UpdateBranch> &out,
		       std::string &error)
{
	JsonBranches branches;

	try {
		nlohmann::json json = nlohmann::json::parse(jsonString);
		branches = json.get<JsonBranches>();
	} catch (nlohmann::json::exception &e) {
		error = e.what();
		return;
	}

	for (const JsonBranch &json_branch : branches) {
#ifdef _WIN32
		if (!json_branch.windows)
			continue;
#elif defined(__APPLE__)
		if (!json_branch.macos)
			continue;
#endif

		UpdateBranch branch = {
			QString::fromStdString(json_branch.name),
			QString::fromStdString(json_branch.display_name),
			QString::fromStdString(json_branch.description),
			json_branch.enabled,
			json_branch.visible,
		};
		out.push_back(branch);
	}
}

bool LoadBranchesFile(vector<UpdateBranch> &out)
{
	string error;
	string branchesText;

	BPtr<char> branchesFilePath =
		GetConfigPathPtr("PRISMLiveStudio/updates/branches.json");

	QFile branchesFile(branchesFilePath.Get());
	if (!branchesFile.open(QIODevice::ReadOnly)) {
		error = "Opening file failed.";
		goto fail;
	}

	branchesText = branchesFile.readAll();
	if (branchesText.empty()) {
		error = "File empty.";
		goto fail;
	}

	ParseBranchesJson(branchesText, out, error);
	if (error.empty())
		return !out.empty();

fail:
	blog(LOG_WARNING, "Loading branches from file failed: %s",
	     error.c_str());
	return false;
}
#endif

void OBSApp::SetBranchData(const string &data)
{
#if defined(_WIN32) || defined(ENABLE_SPARKLE_UPDATER)
	string error;
	vector<UpdateBranch> result;

	ParseBranchesJson(data, result, error);

	if (!error.empty()) {
		blog(LOG_WARNING, "Reading branches JSON response failed: %s",
		     error.c_str());
		return;
	}

	if (!result.empty())
		updateBranches = result;

	branches_loaded = true;
#else
	UNUSED_PARAMETER(data);
#endif
}

std::vector<UpdateBranch> OBSApp::GetBranches()
{
	vector<UpdateBranch> out;
	/* Always ensure the default branch exists */
	out.push_back(UpdateBranch{"stable", "", "", true, true});

#if defined(_WIN32) || defined(ENABLE_SPARKLE_UPDATER)
	if (!branches_loaded) {
		vector<UpdateBranch> result;
		if (LoadBranchesFile(result))
			updateBranches = result;

		branches_loaded = true;
	}
#endif

	/* Copy additional branches to result (if any) */
	if (!updateBranches.empty())
		out.insert(out.end(), updateBranches.begin(),
			   updateBranches.end());

	return out;
}

OBSApp::OBSApp(int &argc, char **argv, profiler_name_store_t *store)
	: PLSUiApp(argc, argv), profilerNameStore(store)
{
	pls_set_translate_cb(Str);

	/* fix float handling */
#if defined(Q_OS_UNIX)
	if (!setlocale(LC_NUMERIC, "C"))
		blog(LOG_WARNING, "Failed to set LC_NUMERIC to C locale");
#endif

#ifndef _WIN32
	/* Handle SIGINT properly */
	socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd);
	snInt = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, this);
	connect(snInt, &QSocketNotifier::activated, this,
		&OBSApp::ProcessSigInt);
#else
	connect(qApp, &QGuiApplication::commitDataRequest, this,
		&OBSApp::commitData);
#endif

	sleepInhibitor = os_inhibit_sleep_create("OBS Video/audio");

#ifndef __APPLE__
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));
#endif

	//setDesktopFileName("com.obsproject.Studio");
}

OBSApp::~OBSApp()
{
	if (globalConfig != nullptr) {
#ifdef _WIN32
		bool disableAudioDucking = config_get_bool(
			globalConfig, "Audio", "DisableAudioDucking");
		if (disableAudioDucking)
			DisableAudioDucking(false);
#else
		delete snInt;
		close(sigintFd[0]);
		close(sigintFd[1]);
#endif

#ifdef __APPLE__
		bool vsyncDisabled = config_get_bool(globalConfig, "Video",
						     "DisableOSXVSync");
		bool resetVSync = config_get_bool(globalConfig, "Video",
						  "ResetOSXVSyncOnExit");
		if (vsyncDisabled && resetVSync)
			EnableOSXVSync(true);
#endif
	}

	os_inhibit_sleep_set_active(sleepInhibitor, false);
	os_inhibit_sleep_destroy(sleepInhibitor);

	if (libobs_initialized) {
		PLS_INFO(MAINFRAME_MODULE, "Start invoking obs_shutdown");
#if __APPLE__
		if (!pls_is_main_window_closing()) {
			PLS_INFO(
				MAINFRAME_MODULE,
				"Closing PRISM from Dock menu, skip obs_shutdown");
			pls_log_cleanup();
			kill(getpid(), SIGKILL);
		}
#endif
		obs_shutdown();
		PLS_INFO(MAINFRAME_MODULE, "End invoking obs_shutdown");
	}
}

static void move_basic_to_profiles(void)
{
	char path[512];
	char new_path[512];
	os_glob_t *glob;

	/* if not first time use */
	if (GetConfigPath(path, 512, "PRISMLiveStudio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	/* if the profiles directory doesn't already exist */
	if (GetConfigPath(new_path, 512, "PRISMLiveStudio/basic/profiles") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/*.*");
	if (os_glob(path, 0, &glob) != 0)
		return;

	strcpy(path, new_path);

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		char *file;

		if (ent.directory)
			continue;

		file = strrchr(ent.path, '/');
		if (!file++)
			continue;

		if (astrcmpi(file, "scenes.json") == 0)
			continue;

		strcpy(new_path, path);
		strcat(new_path, "/");
		strcat(new_path, file);
		os_rename(ent.path, new_path);
	}

	os_globfree(glob);
}

static void move_basic_to_scene_collections(void)
{
	char path[512];
	char new_path[512];

	if (GetConfigPath(path, 512, "PRISMLiveStudio/basic") <= 0)
		return;
	if (!os_file_exists(path))
		return;

	if (GetConfigPath(new_path, 512, "PRISMLiveStudio/basic/scenes") <= 0)
		return;
	if (os_file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/scenes.json");
	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	strcat(new_path, ".json");

	os_rename(path, new_path);
}

void OBSApp::AppInit()
{
	ProfileScope("OBSApp::AppInit");

	if (!MakeUserDirs())
		throw init_exception_code::failed_create_required_user_directory;

	if (!InitGlobalConfig())
		throw init_exception_code::failed_init_global_config;
	if (!InitLocale())
		throw init_exception_code::failed_load_locale;

	config_set_default_string(globalConfig, "Basic", "Profile",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "ProfileDir",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollection",
				  Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollectionFile",
				  Str("Untitled"));
	config_set_default_bool(globalConfig, "Basic", "ConfigOnNewProfile",
				true);

	if (!config_has_user_value(globalConfig, "Basic", "Profile")) {
		config_set_string(globalConfig, "Basic", "Profile",
				  Str("Untitled"));
		config_set_string(globalConfig, "Basic", "ProfileDir",
				  Str("Untitled"));
	}

	if (!config_has_user_value(globalConfig, "Basic", "SceneCollection")) {
		config_set_string(globalConfig, "Basic", "SceneCollection",
				  Str("Untitled"));
		config_set_string(globalConfig, "Basic", "SceneCollectionFile",
				  Str("Untitled"));
	}

#ifdef _WIN32
	bool disableAudioDucking =
		config_get_bool(globalConfig, "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(true);
#endif

#ifdef __APPLE__
	if (config_get_bool(globalConfig, "Video", "DisableOSXVSync"))
		EnableOSXVSync(false);
#endif

	UpdateHotkeyFocusSetting(false);

	move_basic_to_profiles();
	move_basic_to_scene_collections();

	if (!MakeUserProfileDirs())
		throw init_exception_code::failed_create_profile_directory;
}

const char *OBSApp::GetRenderModule() const
{
	const char *renderer =
		config_get_string(globalConfig, "Video", "Renderer");

	return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
}

static bool StartupOBS(const char *locale, profiler_name_store_t *store)
{
	char path[512];

	if (GetConfigPath(path, sizeof(path),
			  "PRISMLiveStudio/plugin_config") <= 0)
		return false;

	return obs_startup(locale, path, store);
}

inline void OBSApp::ResetHotkeyState(bool inFocus)
{
	obs_hotkey_enable_background_press(
		(inFocus && enableHotkeysInFocus) ||
		(!inFocus && enableHotkeysOutOfFocus));
}

void OBSApp::UpdateHotkeyFocusSetting(bool resetState)
{
	if (globalConfig == nullptr) {
		return;
	}
	hotkeyEnable = true;
	enableHotkeysInFocus = true;
	enableHotkeysOutOfFocus = true;
	emit HotKeyEnabled(hotkeyEnable);
	const char *hotkeyFocusType =
		config_get_string(globalConfig, "General", "HotkeyFocusType");

	if (astrcmpi(hotkeyFocusType, "DisableHotkeysInFocus") == 0) {
		enableHotkeysInFocus = false;
	} else if (astrcmpi(hotkeyFocusType, "DisableHotkeysOutOfFocus") == 0) {
		enableHotkeysOutOfFocus = false;
	}

	if (resetState)
		ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

void OBSApp::DisableHotkeys()
{
	hotkeyEnable = false;
	enableHotkeysInFocus = false;
	enableHotkeysOutOfFocus = false;
	emit HotKeyEnabled(hotkeyEnable);
	ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

Q_DECLARE_METATYPE(VoidFunc)

void OBSApp::Exec(VoidFunc func)
{
	func();
}

static void ui_task_handler(obs_task_t task, void *param, bool wait)
{
	auto doTask = [=]() {
		/* to get clang-format to behave */
		task(param);
	};
	QMetaObject::invokeMethod(App(), "Exec",
				  wait ? WaitConnection() : Qt::AutoConnection,
				  Q_ARG(VoidFunc, doTask));
}

static PLSBasic *newMainView(QPointer<PLSMainView> &mainView,
			     QPointer<OBSMainWindow> &mainWindow)
{
	mainView = pls_new<PLSMainView>(nullptr);
	PLSBasic *basic = pls_new<PLSBasic>(mainView);
	mainWindow = basic;
	return basic;
}

bool OBSApp::OBSInit()
{
	ProfileScope("OBSApp::OBSInit");

	qRegisterMetaType<VoidFunc>("VoidFunc");

#if !defined(_WIN32) && !defined(__APPLE__)
	if (QApplication::platformName() == "xcb") {
		obs_set_nix_platform(OBS_NIX_PLATFORM_X11_EGL);
		blog(LOG_INFO, "Using EGL/X11");
	}

#ifdef ENABLE_WAYLAND
	if (QApplication::platformName().contains("wayland")) {
		obs_set_nix_platform(OBS_NIX_PLATFORM_WAYLAND);
		setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
		blog(LOG_INFO, "Platform: Wayland");
	}
#endif

	QPlatformNativeInterface *native =
		QGuiApplication::platformNativeInterface();
	obs_set_nix_platform_display(
		native->nativeResourceForIntegration("display"));
#endif

#ifdef __APPLE__
	setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

	if (!StartupOBS(locale.c_str(), GetProfilerNameStore())) {
		PLS_ERROR(MAINFRAME_MODULE, "start obs config failed");
		PLSApp::uploadAnalogInfo(
			RUNAPP_API_PATH,
			{{SUCCESSFAIL, false},
			 {FAILREASON, "start obs config failed"}},
			true);
		throw init_exception_code::failed_startup_obs;
	}

	libobs_initialized = true;

	obs_set_ui_task_handler(ui_task_handler);

	//PRISM/Xiewei/20230213/for sources which contain sub-sources
	auto signal_handler = obs_get_signal_handler();
	signal_handler_add(signal_handler,
			   "void source_create_finished(int source_address)");
	signal_handler_add(
		signal_handler,
		"void source_notify(ptr source, int message, int sub_code)");
	signal_handler_add(
		signal_handler,
		"void source_message(ptr source, int event_type, ptr msg_data)");

#if defined(_WIN32) || defined(__APPLE__)
	bool browserHWAccel =
		config_get_bool(globalConfig, "General", "BrowserHWAccel");

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "BrowserHWAccel", browserHWAccel);
	obs_apply_private_data(settings);

	blog(LOG_INFO, "Current Date/Time: %s",
	     CurrentDateTimeString().c_str());

	blog(LOG_INFO, "Browser Hardware Acceleration: %s",
	     browserHWAccel ? "true" : "false");
#endif
#ifdef _WIN32
	bool hideFromCapture = config_get_bool(globalConfig, "BasicWindow",
					       "HideOBSWindowsFromCapture");
	blog(LOG_INFO, "Hide OBS windows from screen capture: %s",
	     hideFromCapture ? "true" : "false");
#endif

	blog(LOG_INFO, "Qt Version: %s (runtime), %s (compiled)", qVersion(),
	     QT_VERSION_STR);
	blog(LOG_INFO, "Portable mode: %s",
	     GlobalVars::portable_mode ? "true" : "false");

	if (GlobalVars::safe_mode) {
		blog(LOG_WARNING, "Safe Mode enabled.");
	} else if (GlobalVars::disable_3p_plugins) {
		blog(LOG_WARNING, "Third-party plugins disabled.");
	}

	setQuitOnLastWindowClosed(false);

	auto basic = newMainView(mainView, mainWindow);

	PLS_INIT_INFO(MAINFRAME_MODULE, "main window create success.");
	mainView->setCloseEventCallback(std::bind(
		&OBSBasic::mainViewClose, basic, std::placeholders::_1));

#if defined(Q_OS_WIN)
	basic->menuBar()->hide();
#endif

	connect(mainView, &PLSMainView::sideBarButtonClicked, basic,
		&PLSBasic::OnSideBarButtonClicked);

	QHBoxLayout *hl = pls_new<QHBoxLayout>(mainView->content());
	hl->setContentsMargins(5, 0, 5, 5);
	hl->setSpacing(0);
	hl->addWidget(mainWindow, 1);

	mainView->setAttribute(Qt::WA_DeleteOnClose, true);
#ifdef ENABLE_TEST
	connect(
		mainView, &QObject::destroyed, this,
		[]() {
			exit(GlobalVars::unitTest ? GlobalVars::unitTestExitCode
						  : 0);
		},
		Qt::QueuedConnection);
#else
	connect(mainView, SIGNAL(destroyed()), this, SLOT(quit()),
		Qt::QueuedConnection);
#endif
	bool initialized = mainWindow->OBSInit();
	if (initialized) {
		connect(this, &QGuiApplication::applicationStateChanged,
			[this](Qt::ApplicationState state) {
				ResetHotkeyState(state ==
						 Qt::ApplicationActive);
			});
		ResetHotkeyState(applicationState() == Qt::ApplicationActive);
	}
	return initialized;
}

string OBSApp::GetVersionString(bool platform) const
{
	stringstream ver;

#ifdef HAVE_OBSCONFIG_H
	ver << obs_get_version_string();
#else
	ver << LIBOBS_API_MAJOR_VER << "." << LIBOBS_API_MINOR_VER << "."
	    << LIBOBS_API_PATCH_VER;

#endif

	if (platform) {
		ver << " (";
#ifdef _WIN32
		if (sizeof(void *) == 8)
			ver << "64-bit, ";
		else
			ver << "32-bit, ";

		ver << "windows)";
#elif __APPLE__
		ver << "mac)";
#elif __OpenBSD__
		ver << "openbsd)";
#elif __FreeBSD__
		ver << "freebsd)";
#else /* assume linux for the time being */
		ver << "linux)";
#endif
	}

	return ver.str();
}

bool OBSApp::IsPortableMode()
{
	return GlobalVars::portable_mode;
}

bool OBSApp::IsUpdaterDisabled()
{
	return GlobalVars::opt_disable_updater;
}

bool OBSApp::IsMissingFilesCheckDisabled()
{
	return GlobalVars::opt_disable_missing_files_check;
}

#ifdef __APPLE__
#define INPUT_AUDIO_SOURCE "coreaudio_input_capture"
#define OUTPUT_AUDIO_SOURCE "coreaudio_output_capture"
#elif _WIN32
#define INPUT_AUDIO_SOURCE "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#else
#define INPUT_AUDIO_SOURCE "pulse_input_capture"
#define OUTPUT_AUDIO_SOURCE "pulse_output_capture"
#endif

const char *OBSApp::InputAudioSource() const
{
	return INPUT_AUDIO_SOURCE;
}

const char *OBSApp::OutputAudioSource() const
{
	return OUTPUT_AUDIO_SOURCE;
}

const char *OBSApp::GetLastLog() const
{
	return LocalGlobalVars::lastLogFile.c_str();
}

const char *OBSApp::GetCurrentLog() const
{
	return LocalGlobalVars::currentLogFile.c_str();
}

const char *OBSApp::GetLastCrashLog() const
{
	return LocalGlobalVars::lastCrashLogFile.c_str();
}

bool OBSApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}
	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

// Global handler to receive all QEvent::Show events so we can apply
// display affinity on any newly created windows and dialogs without
// caring where they are coming from (e.g. plugins).
bool OBSApp::notify(QObject *receiver, QEvent *e)
{
	QWidget *w;
	QWindow *window;
	int windowType;

	if (!receiver->isWidgetType())
		goto skip;

	if (e->type() != QEvent::Show)
		goto skip;

	w = qobject_cast<QWidget *>(receiver);

	if (!w->isWindow())
		goto skip;

	window = w->windowHandle();
	if (!window)
		goto skip;

	windowType = window->flags() & Qt::WindowType::WindowType_Mask;

	if (windowType == Qt::WindowType::Dialog ||
	    windowType == Qt::WindowType::Window ||
	    windowType == Qt::WindowType::Tool) {
		OBSBasic *main = reinterpret_cast<OBSBasic *>(GetMainWindow());
		if (main)
			main->SetDisplayAffinity(window);
	}

skip:
	return QApplication::notify(receiver, e);
}

QString OBSTranslator::translate(const char *context, const char *sourceText,
				 const char *disambiguation, int n) const
{

	QString str(sourceText);
	str.replace(" ", "");
	QString tranStr;
	if (PLSErrorHandler::instance()->getTranslateString(str, tranStr)) {
		return tranStr;
	}

	const char *out = nullptr;
	if (!App()->TranslateString(QT_TO_UTF8(str), &out))
		return QString(sourceText);

	auto trans = QT_UTF8(out);
	if (str == QStringLiteral("Dialog.About.Button.Forums")) {
		trans.replace("PRISM", "OBS");
	}

	UNUSED_PARAMETER(context);
	UNUSED_PARAMETER(disambiguation);
	UNUSED_PARAMETER(n);
	return trans;
}

static bool get_token(lexer *lex, string &str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	str.assign(token.text.array, token.text.len);
	return true;
}

static bool expect_token(lexer *lex, const char *str, base_token_type type)
{
	base_token token;
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != type)
		return false;

	return strref_cmp(&token.text, str) == 0;
}

static uint64_t convert_log_name(bool has_prefix, const char *name)
{
	BaseLexer lex;
	string year, month, day, hour, minute, second;

	lexer_start(lex, name);

	if (has_prefix) {
		string temp;
		if (!get_token(lex, temp, BASETOKEN_ALPHA))
			return 0;
	}

	if (!get_token(lex, year, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, month, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, day, BASETOKEN_DIGIT))
		return 0;
	if (!get_token(lex, hour, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, minute, BASETOKEN_DIGIT))
		return 0;
	if (!expect_token(lex, "-", BASETOKEN_OTHER))
		return 0;
	if (!get_token(lex, second, BASETOKEN_DIGIT))
		return 0;

	stringstream timestring;
	timestring << year << month << day << hour << minute << second;
	return std::stoull(timestring.str());
}

static void delete_oldest_file(bool has_prefix, const char *location)
{
	BPtr<char> logDir(GetConfigPathPtr(location));
	string oldestLog;
	uint64_t oldest_ts = (uint64_t)-1;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(
		App()->GlobalConfig(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir);
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts =
				convert_log_name(has_prefix, entry->d_name);

			if (ts) {
				if (ts < oldest_ts) {
					oldestLog = entry->d_name;
					oldest_ts = ts;
				}

				count++;
			}
		}

		os_closedir(dir);

		if (count > maxLogs) {
			stringstream delPath;

			delPath << logDir << "/" << oldestLog;
			os_unlink(delPath.str().c_str());
		}
	}
}
void OBSApp::deleteOldestFile(bool has_prefix, const char *location)
{
	delete_oldest_file(has_prefix, location);
}
static void get_last_log(bool has_prefix, const char *subdir_to_use,
			 std::string &last)
{
	BPtr<char> logDir(GetConfigPathPtr(subdir_to_use));
	struct os_dirent *entry;
	os_dir_t *dir = os_opendir(logDir);
	uint64_t highest_ts = 0;

	if (dir) {
		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts =
				convert_log_name(has_prefix, entry->d_name);

			if (ts > highest_ts) {
				last = entry->d_name;
				highest_ts = ts;
			}
		}

		os_closedir(dir);
	}
}

string GenerateTimeDateFilename(const char *extension, bool noSpace)
{
	time_t now = time(0);
	char file[256] = {};
	struct tm *cur_time;

	cur_time = localtime(&now);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d.%s",
		 cur_time->tm_year + 1900, cur_time->tm_mon + 1,
		 cur_time->tm_mday, noSpace ? '_' : ' ', cur_time->tm_hour,
		 cur_time->tm_min, cur_time->tm_sec, extension);

	return string(file);
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace,
				 const char *format)
{
	BPtr<char> filename =
		os_generate_formatted_filename(extension, !noSpace, format);
	return string(filename);
}

static void FindBestFilename(string &strPath, bool noSpace)
{
	int num = 2;

	if (!os_file_exists(strPath.c_str()))
		return;

	const char *ext = strrchr(strPath.c_str(), '.');
	if (!ext)
		return;

	int extStart = int(ext - strPath.c_str());
	for (;;) {
		string testPath = strPath;
		string numStr;

		numStr = noSpace ? "_" : " (";
		numStr += to_string(num++);
		if (!noSpace)
			numStr += ")";

		testPath.insert(extStart, numStr);

		if (!os_file_exists(testPath.c_str())) {
			strPath = testPath;
			break;
		}
	}
}

static void ensure_directory_exists(string &path)
{
	replace(path.begin(), path.end(), '\\', '/');

	size_t last = path.rfind('/');
	if (last == string::npos)
		return;

	string directory = path.substr(0, last);
	os_mkdirs(directory.c_str());
}

static void remove_reserved_file_characters(string &s)
{
	replace(s.begin(), s.end(), '\\', '/');
	replace(s.begin(), s.end(), '*', '_');
	replace(s.begin(), s.end(), '?', '_');
	replace(s.begin(), s.end(), '"', '_');
	replace(s.begin(), s.end(), '|', '_');
	replace(s.begin(), s.end(), ':', '_');
	replace(s.begin(), s.end(), '>', '_');
	replace(s.begin(), s.end(), '<', '_');
}

string GetFormatString(const char *format, const char *prefix,
		       const char *suffix)
{
	string f;

	f = format;

	if (prefix && *prefix) {
		string str_prefix = prefix;

		if (str_prefix.back() != ' ')
			str_prefix += " ";

		size_t insert_pos = 0;
		size_t tmp;

		tmp = f.find_last_of('/');
		if (tmp != string::npos && tmp > insert_pos)
			insert_pos = tmp + 1;

		tmp = f.find_last_of('\\');
		if (tmp != string::npos && tmp > insert_pos)
			insert_pos = tmp + 1;

		f.insert(insert_pos, str_prefix);
	}

	if (suffix && *suffix) {
		if (*suffix != ' ')
			f += " ";
		f += suffix;
	}

	remove_reserved_file_characters(f);

	return f;
}

string GetFormatExt(const char *container)
{
	string ext = container;
	if (ext == "fragmented_mp4")
		ext = "mp4";
	else if (ext == "hybrid_mp4")
		ext = "mp4";
	else if (ext == "fragmented_mov")
		ext = "mov";
	else if (ext == "hls")
		ext = "m3u8";
	else if (ext == "mpegts")
		ext = "ts";

	return ext;
}

string GetOutputFilename(const char *path, const char *container, bool noSpace,
			 bool overwrite, const char *format)
{
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());

	os_dir_t *dir = path && path[0] ? os_opendir(path) : nullptr;

	if (!dir) {
		if (main->isVisible())
			OBSMessageBox::warning(main,
					       QTStr("Output.BadPath.Title"),
					       QTStr("Output.BadPath.Text"));
		else
			main->SysTrayNotify(QTStr("Output.BadPath.Text"),
					    QSystemTrayIcon::Warning);
		return "";
	}

	os_closedir(dir);

	string strPath;
	strPath += path;

	char lastChar = strPath.back();
	if (lastChar != '/' && lastChar != '\\')
		strPath += "/";

	string ext = GetFormatExt(container);
	strPath += GenerateSpecifiedFilename(ext.c_str(), noSpace, format);
	ensure_directory_exists(strPath);
	if (!overwrite)
		FindBestFilename(strPath, noSpace);

	return strPath;
}

vector<pair<string, string>> GetLocaleNames()
{
	string path;
	vector<pair<string, string>> names;

#if defined(Q_OS_WIN)
	path = pls_get_app_dir().toStdString() +
	       "/../../data/prism-studio/locale.ini";
#elif defined(Q_OS_MACOS)
	path = pls_get_app_resource_dir().toStdString() +
	       "/data/prism-studio/locale.ini";
#endif
	ConfigFile ini;
	if (ini.Open(path.c_str(), CONFIG_OPEN_EXISTING) != 0) {
		PLS_ERROR("MainWindowView", "Could not open locale.ini");
		return names;
	}
	size_t sections = config_num_sections(ini);

	names.reserve(sections);
	for (size_t i = 0; i < sections; i++) {
		const char *tag = config_get_section(ini, i);
		const char *name = config_get_string(ini, tag, "Name");
		names.emplace_back(tag, name);
	}

	return names;
}

static void create_log_file(fstream &logFile)
{
	stringstream dst;

	get_last_log(false, "PRISMLiveStudio/logs",
		     LocalGlobalVars::lastLogFile);
#ifdef _WIN32
	get_last_log(true, "PRISMLiveStudio/crashes",
		     LocalGlobalVars::lastCrashLogFile);
#endif

	LocalGlobalVars::currentLogFile = GenerateTimeDateFilename("txt");
	dst << "PRISMLiveStudio/logs/"
	    << LocalGlobalVars::currentLogFile.c_str();

	BPtr<char> path(GetConfigPathPtr(dst.str().c_str()));

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	logFile.open(wpath, ios_base::in | ios_base::out | ios_base::trunc);
#else
	logFile.open(path, ios_base::in | ios_base::out | ios_base::trunc);
#endif

	if (logFile.is_open()) {
		delete_oldest_file(false, "PRISMLiveStudio/logs");
		//base_set_log_handler(do_log, &logFile);
	} else {
		blog(LOG_ERROR, "Failed to open log file");
	}
}

static auto ProfilerNameStoreRelease = [](profiler_name_store_t *store) {
	profiler_name_store_free(store);
};

using ProfilerNameStore = std::unique_ptr<profiler_name_store_t,
					  decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
	return ProfilerNameStore{profiler_name_store_create(),
				 ProfilerNameStoreRelease};
}

static auto SnapshotRelease = [](profiler_snapshot_t *snap) {
	profile_snapshot_free(snap);
};

using ProfilerSnapshot =
	std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

ProfilerSnapshot GetSnapshot()
{
	return ProfilerSnapshot{profile_snapshot_create(), SnapshotRelease};
}

static void SaveProfilerData(const ProfilerSnapshot &snap)
{
	if (LocalGlobalVars::currentLogFile.empty())
		return;

	auto pos = LocalGlobalVars::currentLogFile.rfind('.');
	if (pos == LocalGlobalVars::currentLogFile.npos)
		return;

#define LITERAL_SIZE(x) x, (sizeof(x) - 1)
	ostringstream dst;
	dst.write(LITERAL_SIZE("PRISMLiveStudio/profiler_data/"));
	dst.write(LocalGlobalVars::currentLogFile.c_str(), pos);
	dst.write(LITERAL_SIZE(".csv.gz"));
#undef LITERAL_SIZE

	BPtr<char> path = GetConfigPathPtr(dst.str().c_str());
	if (!profiler_snapshot_dump_csv_gz(snap.get(), path))
		blog(LOG_WARNING, "Could not save profiler data to '%s'",
		     static_cast<const char *>(path));
}

static auto ProfilerFree = [](void *) {
	profiler_stop();

	auto snap = GetSnapshot();

	profiler_print(snap.get());
	profiler_print_time_between_calls(snap.get());

	SaveProfilerData(snap);

	profiler_free();
};

QAccessibleInterface *accessibleFactory(const QString &classname,
					QObject *object)
{
	if (classname == QLatin1String("VolumeSlider") && object &&
	    object->isWidgetType())
		return new VolumeAccessibleInterface(
			static_cast<QWidget *>(object));

	return nullptr;
}

void luncherMsgReceived(IPCTypeD, pls::ipc::Event, const QVariantHash &params)
{
	if (params.value("type").toInt() == (int)MessageType::MainViewReload) {
		QString running_path = params.value("msg").toString();
		PLS_INFO("MainWindowView",
			 "Received running path from luncher.");
	}
}

int run_program(int argc, char *argv[])
{
	int ret = -1;
	pls_set_config_path(&GetConfigPath);

	auto profilerNameStore = CreateNameStore();

	std::unique_ptr<void, decltype(ProfilerFree)> prof_release(
		static_cast<void *>(&ProfilerFree), ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{run_program_init};

#ifdef _WIN32
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
		Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

	QCoreApplication::addLibraryPath(".");

#if __APPLE__
	InstallNSThreadLocks();

	if (!isInBundle()) {
		blog(LOG_ERROR,
		     "OBS cannot be run as a standalone binary on macOS. Run the Application bundle instead.");
		return ret;
	}

	//not show system fullscreen menu, must be set before qapp init.
	QSettings("prismlive", "prismlivestudio")
		.setValue("NSFullScreenMenuItemEverywhere", false);
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
	/* NOTE: Users blindly set this, but this theme is incompatble with Qt6 and
	 * crashes loading saved geometry. Just turn off this theme and let users complain OBS
	 * looks ugly instead of crashing. */
	const char *platform_theme = getenv("QT_QPA_PLATFORMTHEME");
	if (platform_theme && strcmp(platform_theme, "qt5ct") == 0)
		unsetenv("QT_QPA_PLATFORMTHEME");

#if defined(ENABLE_WAYLAND) && defined(USE_XDG) && \
	QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
	/* NOTE: Qt doesn't use the Wayland platform on GNOME, so we have to
	 * force it using the QT_QPA_PLATFORM env var. It's still possible to
	 * use other QPA platforms using this env var, or the -platform command
	 * line option. Remove after Qt 6.3 is everywhere. */

	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	const char *session_type = getenv("XDG_SESSION_TYPE");
	if (session_type && desktop && strstr(desktop, "GNOME") != nullptr &&
	    strcmp(session_type, "wayland") == 0)
		setenv("QT_QPA_PLATFORM", "wayland", false);
#endif
#endif
	/* NOTE: This disables an optimisation in Qt that attempts to determine if
	 * any "siblings" intersect with a widget when determining the approximate
	 * visible/unobscured area. However, by Qt's own admission this is slow
	 * and in the case of OBS it significantly slows down lists with many
	 * elements (e.g. Hotkeys) and it is actually faster to disable it. */
	qputenv("QT_NO_SUBTRACTOPAQUESIBLINGS", "1");
	PLSApp program(argc, argv, profilerNameStore.get());

	PLSErrorHandler::instance(); //must  before with OBSTranslator
	PLS_INIT_INFO(MAINFRAME_MODULE, "install language translator");
	OBSTranslator translator;
	PLSApp::installTranslator(&translator);

	QString daemonPrismSession;
	QString limitRetryCount = "";
	auto args = pls_cmdline_args();
	for (int i = 0; i < args.size(); ++i) {
		QString str = args[i];
		if (str.startsWith("--running-path=") ||
		    str.endsWith(".psc", Qt::CaseInsensitive)) {
			QString pscPath = str.mid(str.lastIndexOf("=") + 1);
			pscPath.replace("\\", "/");
			PLS_INIT_INFO(MAINFRAME_MODULE, "set psc path: %s",
				      pls_get_path_file_name(pscPath)
					      .toStdString()
					      .c_str());
			program.setAppRunningPath(pscPath);
		} else if (str ==
			   QString(shared_values::k_launcher_command_type +
				   shared_values::k_launcher_command_daemon)) {
			GlobalVars::isStartByDaemon = true;
		} else if (str.startsWith(
				   shared_values::
					   k_launcher_command_log_prism_session)) {
			daemonPrismSession = str.remove(
				0,
				shared_values::k_launcher_command_log_prism_session
					.size());
		} else if (str.startsWith(
				   shared_values::k_daemon_limit_retry_count)) {
			limitRetryCount = str.remove(
				0, shared_values::k_daemon_limit_retry_count
					   .size());
		}
	}
	PLS_INIT_INFO(
		MAINFRAME_MODULE,
		"prism is started by daemon process:%d, the daemon prism session is: %s, limit retry count:%s",
		GlobalVars::isStartByDaemon,
		daemonPrismSession.toUtf8().constData(),
		limitRetryCount.toUtf8().constData());

	QAccessible::installFactory(accessibleFactory);
	QFontDatabase::addApplicationFont(":/fonts/OpenSans-Regular.ttf");
	QFontDatabase::addApplicationFont(":/fonts/OpenSans-Bold.ttf");
	QFontDatabase::addApplicationFont(":/fonts/OpenSans-Italic.ttf");
	/* xiewei: for outro */
#if defined(__APPLE__)
	QFontDatabase::addApplicationFont(":/outro/Pretendard-Regular.ttf");
#endif

	bool created_log = false;

	auto palette = PLSApp::palette();
	palette.setColor(QPalette::ColorGroup::Normal,
			 QPalette::ColorRole::Link, QColor(37, 142, 221));
	palette.setColor(QPalette::ColorGroup::Normal,
			 QPalette::ColorRole::LinkVisited,
			 QColor(37, 142, 221));
	PLSApp::setPalette(palette);

	ret = PLSApp::runProgram(program, argc, argv, prof);
#if 0
	if (GlobalVars::restart || GlobalVars::restart_safe) {
		GlobalVars::arguments = qApp->arguments();

		if (GlobalVars::restart_safe) {
			GlobalVars::arguments.append("--safe-mode");
		} else {
			GlobalVars::arguments.removeAll("--safe-mode");
		}
	}
#endif
	return ret;
}

#define MAX_CRASH_REPORT_SIZE (150 * 1024)

#ifdef _WIN32

#define CRASH_MESSAGE                                                      \
	"Woops, OBS has crashed!\n\nWould you like to copy the crash log " \
	"to the clipboard? The crash log will still be saved to:\n\n%s"

static void main_crash_handler(const char *format, va_list args,
			       void * /* param */)
{
	char *text = new char[MAX_CRASH_REPORT_SIZE];

	vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
	text[MAX_CRASH_REPORT_SIZE - 1] = 0;

	string crashFilePath = "PRISMLiveStudio/crashes";

	delete_oldest_file(true, crashFilePath.c_str());

	string name = crashFilePath + "/";
	name += "Crash " + GenerateTimeDateFilename("txt");

	BPtr<char> path(GetConfigPathPtr(name.c_str()));

	fstream file;

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	file.open(wpath, ios_base::in | ios_base::out | ios_base::trunc |
				 ios_base::binary);
#else
	file.open(path, ios_base::in | ios_base::out | ios_base::trunc |
				ios_base::binary);
#endif
	file << text;
	file.close();

	string pathString(path.Get());

#ifdef _WIN32
	std::replace(pathString.begin(), pathString.end(), '/', '\\');
#endif

	string absolutePath =
		canonical(filesystem::path(pathString)).u8string();

	size_t size = snprintf(nullptr, 0, CRASH_MESSAGE, absolutePath.c_str());

	unique_ptr<char[]> message_buffer(new char[size + 1]);

	snprintf(message_buffer.get(), size + 1, CRASH_MESSAGE,
		 absolutePath.c_str());

	string finalMessage =
		string(message_buffer.get(), message_buffer.get() + size);

	int ret = MessageBoxA(NULL, finalMessage.c_str(), "OBS has crashed!",
			      MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

	if (ret == IDYES) {
		size_t len = strlen(text);

		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len);
		memcpy(GlobalLock(mem), text, len);
		GlobalUnlock(mem);

		OpenClipboard(0);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
	}

	exit(-1);
}

static void load_debug_privilege(void)
{
	const DWORD flags = TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY;
	TOKEN_PRIVILEGES tp;
	HANDLE token;
	LUID val;

	if (!OpenProcessToken(GetCurrentProcess(), flags, &token)) {
		return;
	}

	if (!!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL,
				      NULL);
	}

	if (!!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL,
					   NULL)) {
			blog(LOG_INFO, "Could not set privilege to "
				       "increase GPU priority");
		}
	}

	CloseHandle(token);
}

#endif

#ifdef __APPLE__
#define BASE_PATH ".."
#else
#define BASE_PATH "../.."
#endif

#define CONFIG_PATH BASE_PATH "/config"

#if defined(LINUX_PORTABLE) || defined(_WIN32)
#define ALLOW_PORTABLE_MODE 1
#else
#define ALLOW_PORTABLE_MODE 0
#endif

int GetConfigPath(char *path, size_t size, const char *name)
{
#if ALLOW_PORTABLE_MODE
	if (GlobalVars::portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		} else {
			return snprintf(path, size, CONFIG_PATH);
		}
	} else {
		return os_get_config_path(path, size, name);
	}
#else
	return os_get_config_path(path, size, name);
#endif
}

char *GetConfigPathPtr(const char *name)
{
#if ALLOW_PORTABLE_MODE
	if (GlobalVars::portable_mode) {
		char path[512];

		if (snprintf(path, sizeof(path), CONFIG_PATH "/%s", name) > 0) {
			return bstrdup(path);
		} else {
			return NULL;
		}
	} else {
		return os_get_config_path_ptr(name);
	}
#else
	return os_get_config_path_ptr(name);
#endif
}

int GetProgramDataPath(char *path, size_t size, const char *name)
{
	return os_get_program_data_path(path, size, name);
}

char *GetProgramDataPathPtr(const char *name)
{
	return os_get_program_data_path_ptr(name);
}

bool GetFileSafeName(const char *name, std::string &file)
{
	size_t base_len = strlen(name);
	size_t len = os_utf8_to_wcs(name, base_len, nullptr, 0);
	std::wstring wfile;

	if (!len)
		return false;

	wfile.resize(len);
	os_utf8_to_wcs(name, base_len, &wfile[0], len + 1);

	for (size_t i = wfile.size(); i > 0; i--) {
		size_t im1 = i - 1;

		if (iswspace(wfile[im1])) {
			wfile[im1] = '_';
		} else if (wfile[im1] != '_' && !iswalnum(wfile[im1])) {
			wfile.erase(im1, 1);
		}
	}

	if (wfile.size() == 0)
		wfile = L"characters_only";

	len = os_wcs_to_utf8(wfile.c_str(), wfile.size(), nullptr, 0);
	if (!len)
		return false;

	file.resize(len);
	os_wcs_to_utf8(wfile.c_str(), wfile.size(), &file[0], len + 1);
	return true;
}

bool GetClosestUnusedFileName(std::string &path, const char *extension)
{
	size_t len = path.size();
	if (extension) {
		path += ".";
		path += extension;
	}

	if (!os_file_exists(path.c_str()))
		return true;

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (os_file_exists(path.c_str()));

	return true;
}

bool GetUnusedSceneCollectionFile(std::string &name, std::string &file)
{
	char path[512];
	int ret;

	if (!GetFileSafeName(name.c_str(), file)) {
		blog(LOG_WARNING, "Failed to create safe file name for '%s'",
		     name.c_str());
		return false;
	}

	ret = GetConfigPath(path, sizeof(path),
			    "PRISMLiveStudio/basic/scenes/");
	if (ret <= 0) {
		blog(LOG_WARNING, "Failed to get scene collection config path");
		return false;
	}

	file.insert(0, path);

	if (!GetClosestUnusedFileName(file, "json")) {
		blog(LOG_WARNING, "Failed to get closest file name for %s",
		     file.c_str());
		return false;
	}

	file = file.substr(file.find_last_of('/') + 1);
	std::string suffix = file.substr(file.find_last_of('.'));
	file.erase(file.size() - strlen(suffix.c_str()),
		   strlen(suffix.c_str()));
	return true;
}

bool WindowPositionValid(QRect rect)
{
	for (QScreen *screen : QGuiApplication::screens()) {
		if (screen->availableGeometry().intersects(rect))
			return true;
	}
	return false;
}

static inline bool arg_is(const char *arg, const char *long_form,
			  const char *short_form)
{
	return (long_form && strcmp(arg, long_form) == 0) ||
	       (short_form && strcmp(arg, short_form) == 0);
}

static bool update_ffmpeg_output(ConfigFile &config)
{
	if (config_has_user_value(config, "AdvOut", "FFOutputToFile"))
		return false;

	const char *url = config_get_string(config, "AdvOut", "FFURL");
	if (!url)
		return false;

	bool isActualURL = strstr(url, "://") != nullptr;
	if (isActualURL)
		return false;

	string urlStr = url;
	string extension;

	for (size_t i = urlStr.length(); i > 0; i--) {
		size_t idx = i - 1;

		if (urlStr[idx] == '.') {
			extension = &urlStr[i];
		}

		if (urlStr[idx] == '\\' || urlStr[idx] == '/') {
			urlStr[idx] = 0;
			break;
		}
	}

	if (urlStr.empty() || extension.empty())
		return false;

	config_remove_value(config, "AdvOut", "FFURL");
	config_set_string(config, "AdvOut", "FFFilePath", urlStr.c_str());
	config_set_string(config, "AdvOut", "FFExtension", extension.c_str());
	config_set_bool(config, "AdvOut", "FFOutputToFile", true);
	return true;
}

static bool move_reconnect_settings(ConfigFile &config, const char *sec)
{
	bool changed = false;

	if (config_has_user_value(config, sec, "Reconnect")) {
		bool reconnect = config_get_bool(config, sec, "Reconnect");
		config_set_bool(config, "Output", "Reconnect", reconnect);
		changed = true;
	}
	if (config_has_user_value(config, sec, "RetryDelay")) {
		int delay = (int)config_get_uint(config, sec, "RetryDelay");
		config_set_uint(config, "Output", "RetryDelay", delay);
		changed = true;
	}
	if (config_has_user_value(config, sec, "MaxRetries")) {
		int retries = (int)config_get_uint(config, sec, "MaxRetries");
		config_set_uint(config, "Output", "MaxRetries", retries);
		changed = true;
	}

	return changed;
}

static bool update_reconnect(ConfigFile &config)
{
	if (!config_has_user_value(config, "Output", "Mode"))
		return false;

	const char *mode = config_get_string(config, "Output", "Mode");
	if (!mode)
		return false;

	const char *section = (strcmp(mode, "Advanced") == 0) ? "AdvOut"
							      : "SimpleOutput";

	if (move_reconnect_settings(config, section)) {
		config_remove_value(config, "SimpleOutput", "Reconnect");
		config_remove_value(config, "SimpleOutput", "RetryDelay");
		config_remove_value(config, "SimpleOutput", "MaxRetries");
		config_remove_value(config, "AdvOut", "Reconnect");
		config_remove_value(config, "AdvOut", "RetryDelay");
		config_remove_value(config, "AdvOut", "MaxRetries");
		return true;
	}

	return false;
}

static void convert_nvenc_h264_presets(obs_data_t *data)
{
	const char *preset = obs_data_get_string(data, "preset");
	const char *rc = obs_data_get_string(data, "rate_control");

	// If already using SDK10+ preset, return early.
	if (astrcmpi_n(preset, "p", 1) == 0) {
		obs_data_set_string(data, "preset2", preset);
		return;
	}

	if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "mq")) {
		obs_data_set_string(data, "preset2", "p3");
		obs_data_set_string(data, "tune", "lossless");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "hp")) {
		obs_data_set_string(data, "preset2", "p2");
		obs_data_set_string(data, "tune", "lossless");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "mq") == 0) {
		obs_data_set_string(data, "preset2", "p5");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "qres");

	} else if (astrcmpi(preset, "hq") == 0) {
		obs_data_set_string(data, "preset2", "p5");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "default") == 0) {
		obs_data_set_string(data, "preset2", "p3");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "hp") == 0) {
		obs_data_set_string(data, "preset2", "p1");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "ll") == 0) {
		obs_data_set_string(data, "preset2", "p3");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "llhq") == 0) {
		obs_data_set_string(data, "preset2", "p4");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "llhp") == 0) {
		obs_data_set_string(data, "preset2", "p2");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");
	}
}

static void convert_nvenc_hevc_presets(obs_data_t *data)
{
	const char *preset = obs_data_get_string(data, "preset");
	const char *rc = obs_data_get_string(data, "rate_control");

	// If already using SDK10+ preset, return early.
	if (astrcmpi_n(preset, "p", 1) == 0) {
		obs_data_set_string(data, "preset2", preset);
		return;
	}

	if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "mq")) {
		obs_data_set_string(data, "preset2", "p5");
		obs_data_set_string(data, "tune", "lossless");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(rc, "lossless") == 0 && astrcmpi(preset, "hp")) {
		obs_data_set_string(data, "preset2", "p3");
		obs_data_set_string(data, "tune", "lossless");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "mq") == 0) {
		obs_data_set_string(data, "preset2", "p6");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "qres");

	} else if (astrcmpi(preset, "hq") == 0) {
		obs_data_set_string(data, "preset2", "p6");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "default") == 0) {
		obs_data_set_string(data, "preset2", "p5");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "hp") == 0) {
		obs_data_set_string(data, "preset2", "p1");
		obs_data_set_string(data, "tune", "hq");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "ll") == 0) {
		obs_data_set_string(data, "preset2", "p3");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "llhq") == 0) {
		obs_data_set_string(data, "preset2", "p4");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");

	} else if (astrcmpi(preset, "llhp") == 0) {
		obs_data_set_string(data, "preset2", "p2");
		obs_data_set_string(data, "tune", "ll");
		obs_data_set_string(data, "multipass", "disabled");
	}
}

static void convert_28_1_encoder_setting(const char *encoder, const char *file)
{
	OBSDataAutoRelease data =
		obs_data_create_from_json_file_safe(file, "bak");
	bool modified = false;

	if (astrcmpi(encoder, "jim_nvenc") == 0 ||
	    astrcmpi(encoder, "ffmpeg_nvenc") == 0) {

		if (obs_data_has_user_value(data, "preset") &&
		    !obs_data_has_user_value(data, "preset2")) {
			convert_nvenc_h264_presets(data);

			modified = true;
		}
	} else if (astrcmpi(encoder, "jim_hevc_nvenc") == 0 ||
		   astrcmpi(encoder, "ffmpeg_hevc_nvenc") == 0) {

		if (obs_data_has_user_value(data, "preset") &&
		    !obs_data_has_user_value(data, "preset2")) {
			convert_nvenc_hevc_presets(data);

			modified = true;
		}
	}

	if (modified)
		obs_data_save_json_safe(data, file, "tmp", "bak");
}

bool update_nvenc_presets(ConfigFile &config)
{
	if (config_has_user_value(config, "SimpleOutput", "NVENCPreset2") ||
	    !config_has_user_value(config, "SimpleOutput", "NVENCPreset"))
		return false;

	const char *streamEncoder =
		config_get_string(config, "SimpleOutput", "StreamEncoder");
	const char *nvencPreset =
		config_get_string(config, "SimpleOutput", "NVENCPreset");

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "preset", nvencPreset);

	if (astrcmpi(streamEncoder, "nvenc_hevc") == 0) {
		convert_nvenc_hevc_presets(data);
	} else {
		convert_nvenc_h264_presets(data);
	}

	config_set_string(config, "SimpleOutput", "NVENCPreset2",
			  obs_data_get_string(data, "preset2"));

	return true;
}

static void upgrade_settings(void)
{
	char path[512];
	int pathlen =
		GetConfigPath(path, 512, "PRISMLiveStudio/basic/profiles");

	if (pathlen <= 0)
		return;
	if (!os_file_exists(path))
		return;

	os_dir_t *dir = os_opendir(path);
	if (!dir)
		return;

	struct os_dirent *ent = os_readdir(dir);

	while (ent) {
		if (ent->directory && strcmp(ent->d_name, ".") != 0 &&
		    strcmp(ent->d_name, "..") != 0) {
			strcat(path, "/");
			strcat(path, ent->d_name);
			strcat(path, "/basic.ini");

			ConfigFile config;
			int ret;

			ret = config.Open(path, CONFIG_OPEN_EXISTING);
			if (ret == CONFIG_SUCCESS) {
				if (update_ffmpeg_output(config) ||
				    update_reconnect(config)) {
					config_save_safe(config, "tmp",
							 nullptr);
				}
			}

			if (config) {
				const char *sEnc = config_get_string(
					config, "AdvOut", "Encoder");
				const char *rEnc = config_get_string(
					config, "AdvOut", "RecEncoder");

				/* replace "cbr" option with "rate_control" for
				 * each profile's encoder data */
				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/recordEncoder.json");
				convert_28_1_encoder_setting(rEnc, path);

				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/streamEncoder.json");
				convert_28_1_encoder_setting(sEnc, path);
			}

			path[pathlen] = 0;
		}

		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

static void check_safe_mode_sentinel(void)
{
#ifndef NDEBUG
	/* Safe Mode detection is disabled in Debug builds to keep developers
	 * somewhat sane. */
	return;
#else
	if (LocalGlobalVars::disable_shutdown_check)
		return;

	BPtr sentinelPath = GetConfigPathPtr("obs-studio/safe_mode");
	if (os_file_exists(sentinelPath)) {
		LocalGlobalVars::unclean_shutdown = true;
		return;
	}

	os_quick_write_utf8_file(sentinelPath, nullptr, 0, false);
#endif
}

static void delete_safe_mode_sentinel(void)
{
	BPtr sentinelPath = GetConfigPathPtr("obs-studio/safe_mode");
	os_unlink(sentinelPath);
}

#ifndef _WIN32
void OBSApp::SigIntSignalHandler(int s)
{
	/* Handles SIGINT and writes to a socket. Qt will read
	 * from the socket in the main thread event loop and trigger
	 * a call to the ProcessSigInt slot, where we can safely run
	 * shutdown code without signal safety issues. */
	UNUSED_PARAMETER(s);

	char a = 1;
	send(sigintFd[0], &a, sizeof(a), 0);
}
#endif

void OBSApp::ProcessSigInt(void)
{
	/* This looks weird, but we can't ifdef a Qt slot function so
	 * the SIGINT handler simply does nothing on Windows. */
#ifndef _WIN32
	char tmp;
	recv(sigintFd[1], &tmp, sizeof(tmp), 0);

	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	if (main)
		main->close();
#endif
}

#ifdef _WIN32
void OBSApp::commitData(QSessionManager &manager)
{
	if (auto main = App()->GetMainWindow()) {
		QMetaObject::invokeMethod(main, "close", Qt::QueuedConnection);
		manager.cancel();
	}
}
#endif

#ifdef _WIN32
static constexpr char vcRunErrorTitle[] = "Outdated Visual C++ Runtime";
static constexpr char vcRunErrorMsg[] =
	"PRISM Live Studio requires a newer version of the Microsoft Visual C++ "
	"Redistributables.\n\nYou will now be directed to the download page.";
static constexpr char vcRunInstallerUrl[] =
	"https://obsproject.com/visual-studio-2022-runtimes";

static bool vc_runtime_outdated()
{
	win_version_info ver;
	if (!get_dll_ver(L"msvcp140.dll", &ver))
		return true;
	/* Major is always 14 (hence 140.dll), so we only care about minor. */
	if (ver.minor >= 40)
		return false;

	int choice = MessageBoxA(NULL, vcRunErrorMsg, vcRunErrorTitle,
				 MB_OKCANCEL | MB_ICONERROR | MB_TASKMODAL);
	if (choice == IDOK) {
		/* Open the URL in the default browser. */
		ShellExecuteA(NULL, "open", vcRunInstallerUrl, NULL, NULL,
			      SW_SHOWNORMAL);
	}

	return true;
}
#endif

int main(int argc, char *argv[])
{
	pls_set_cmdline_args(argc, argv);
	GlobalVars::startTime = std::chrono::steady_clock::now();

#if __APPLE__
	InstallNSApplicationSubclass();
#endif
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);

	struct sigaction sig_handler;

	sig_handler.sa_handler = OBSApp::SigIntSignalHandler;
	sigemptyset(&sig_handler.sa_mask);
	sig_handler.sa_flags = 0;

	sigaction(SIGINT, &sig_handler, NULL);

	/* Block SIGPIPE in all threads, this can happen if a thread calls write on
	a closed pipe. */
	sigset_t sigpipe_mask;
	sigemptyset(&sigpipe_mask);
	sigaddset(&sigpipe_mask, SIGPIPE);
	sigset_t saved_mask;
	if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
		perror("pthread_sigmask");
		exit(1);
	}
#endif

#ifdef _WIN32
	// Abort as early as possible if MSVC runtime is outdated
	if (vc_runtime_outdated())
		return 1;
	// Try to keep this as early as possible
	install_dll_blocklist_hook();

	pls_catch_unhandled_exceptions("PRISMLiveStudio.exe");

	/*obs_init_win32_crash_handler();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();
	base_set_crash_handler(main_crash_handler, nullptr);*/

	const HMODULE hRtwq = LoadLibrary(L"RTWorkQ.dll");
	if (hRtwq) {
		typedef HRESULT(STDAPICALLTYPE * PFN_RtwqStartup)();
		PFN_RtwqStartup func =
			(PFN_RtwqStartup)GetProcAddress(hRtwq, "RtwqStartup");
		func();
	}
#endif

	PLSApp::generatePrismSessionAndSubSession(argc, argv);

	GlobalVars::crashFileMutexUuid = pls_gen_uuid().toStdString();

	//LocalGlobalVars::handle =
	//	os_mutex_handle_create(GlobalVars::crashFileMutexUuid.c_str());
	//if (LocalGlobalVars::handle) {
	//	cw_set_file_mutex_uuid(GlobalVars::crashFileMutexUuid.c_str());
	//}

	log_init(GlobalVars::prismSession.c_str(), GlobalVars::startTime,
		 GlobalVars::prismSubSession.c_str());
	PLS_INFO(
		MAINFRAME_MODULE,
		"============================ PRISM APP Startup ============================");
	PLS_INFO(MAINFRAME_MODULE, "prism session = %s,\tprism subSession = %s",
		 GlobalVars::prismSession.c_str(),
		 GlobalVars::prismSubSession.c_str());
	pls_set_cmdline_args(argc, argv);
	obs_set_cmdline_args(argc, argv);

#if __APPLE__
	std::string pn = pls_get_app_pn().toStdString();
	pls_set_prism_sub_session(GlobalVars::prismSubSession);
	pls_set_prism_pid(std::to_string(getpid()));
	pls_catch_unhandled_exceptions(pn.c_str());
#endif

	for (int i = 1; i < argc; i++) {
		if (arg_is(argv[i], "--multi", "-m")) {
			LocalGlobalVars::multi = true;
			LocalGlobalVars::disable_shutdown_check = true;

#if ALLOW_PORTABLE_MODE
		} else if (arg_is(argv[i], "--portable", "-p")) {
			GlobalVars::portable_mode = true;

#endif
		} else if (arg_is(argv[i], "--verbose", nullptr)) {
			LocalGlobalVars::log_verbose = true;

		} else if (arg_is(argv[i], "--safe-mode", nullptr)) {
			GlobalVars::safe_mode = false;

		} else if (arg_is(argv[i], "--only-bundled-plugins", nullptr)) {
			GlobalVars::disable_3p_plugins = false;

		} else if (arg_is(argv[i], "--disable-shutdown-check",
				  nullptr)) {
			/* This exists mostly to bypass the dialog during development. */
			LocalGlobalVars::disable_shutdown_check = true;

		} else if (arg_is(argv[i], "--always-on-top", nullptr)) {
			GlobalVars::opt_always_on_top = true;

		} else if (arg_is(argv[i], "--unfiltered_log", nullptr)) {
			LocalGlobalVars::unfiltered_log = true;

		} else if (arg_is(argv[i], "--startstreaming", nullptr)) {
			GlobalVars::opt_start_streaming = true;

		} else if (arg_is(argv[i], "--startrecording", nullptr)) {
			GlobalVars::opt_start_recording = true;

		} else if (arg_is(argv[i], "--startreplaybuffer", nullptr)) {
			GlobalVars::opt_start_replaybuffer = true;

		} else if (arg_is(argv[i], "--startvirtualcam", nullptr)) {
			GlobalVars::opt_start_virtualcam = true;

		} else if (arg_is(argv[i], "--collection", nullptr)) {
			if (++i < argc)
				LocalGlobalVars::opt_starting_collection =
					argv[i];

		} else if (arg_is(argv[i], "--profile", nullptr)) {
			if (++i < argc)
				LocalGlobalVars::opt_starting_profile = argv[i];

		} else if (arg_is(argv[i], "--scene", nullptr)) {
			if (++i < argc)
				GlobalVars::opt_starting_scene = argv[i];

		} else if (arg_is(argv[i], "--minimize-to-tray", nullptr)) {
			GlobalVars::opt_minimize_tray = true;

		} else if (arg_is(argv[i], "--studio-mode", nullptr)) {
			GlobalVars::opt_studio_mode = true;

		} else if (arg_is(argv[i], "--allow-opengl", nullptr)) {
			GlobalVars::opt_allow_opengl = true;

		} else if (arg_is(argv[i], "--disable-updater", nullptr)) {
			GlobalVars::opt_disable_updater = true;

		} else if (arg_is(argv[i], "--disable-missing-files-check",
				  nullptr)) {
			GlobalVars::opt_disable_missing_files_check = true;

		} else if (arg_is(argv[i], "--steam", nullptr)) {
			GlobalVars::steam = true;

		}
#ifdef ENABLE_TEST
		else if (arg_is(argv[i], "--unit-test", nullptr)) {
			GlobalVars::unitTest = true;
		}
#endif
		else if (arg_is(argv[i], "--help", "-h")) {
			std::string help =
				"--help, -h: Get list of available commands.\n\n"
				"--startstreaming: Automatically start streaming.\n"
				"--startrecording: Automatically start recording.\n"
				"--startreplaybuffer: Start replay buffer.\n"
				"--startvirtualcam: Start virtual camera (if available).\n\n"
				"--collection <string>: Use specific scene collection."
				"\n"
				"--profile <string>: Use specific profile.\n"
				"--scene <string>: Start with specific scene.\n\n"
				"--studio-mode: Enable studio mode.\n"
				"--minimize-to-tray: Minimize to system tray.\n"
#if ALLOW_PORTABLE_MODE
				"--portable, -p: Use portable mode.\n"
#endif
				"--multi, -m: Don't warn when launching multiple instances.\n\n"
				"--safe-mode: Run in Safe Mode (disables third-party plugins, scripting, and WebSockets).\n"
				"--only-bundled-plugins: Only load included (first-party) plugins\n"
				"--disable-shutdown-check: Disable unclean shutdown detection.\n"
				"--verbose: Make log more verbose.\n"
				"--always-on-top: Start in 'always on top' mode.\n\n"
				"--unfiltered_log: Make log unfiltered.\n\n"
				"--disable-updater: Disable built-in updater (Windows/Mac only)\n\n"
				"--disable-missing-files-check: Disable the missing files dialog which can appear on startup.\n\n";

#ifdef _WIN32
			MessageBoxA(NULL, help.c_str(), "Help",
				    MB_OK | MB_ICONASTERISK);
#else
			std::cout << help
				  << "--version, -V: Get current version.\n";
#endif
			exit(0);

		} else if (arg_is(argv[i], "--version", "-V")) {
			std::cout << "OBS Studio - "
				  << App()->GetVersionString(false) << "\n";
			exit(0);
		}
	}

#if ALLOW_PORTABLE_MODE
	if (!GlobalVars::portable_mode) {
		GlobalVars::portable_mode =
			os_file_exists(BASE_PATH "/portable_mode") ||
			os_file_exists(BASE_PATH "/obs_portable_mode") ||
			os_file_exists(BASE_PATH "/portable_mode.txt") ||
			os_file_exists(BASE_PATH "/obs_portable_mode.txt");
	}

	if (!GlobalVars::opt_disable_updater) {
		GlobalVars::opt_disable_updater =
			os_file_exists(BASE_PATH "/disable_updater") ||
			os_file_exists(BASE_PATH "/disable_updater.txt");
	}

	if (!GlobalVars::opt_disable_missing_files_check) {
		GlobalVars::opt_disable_missing_files_check =
			os_file_exists(BASE_PATH
				       "/disable_missing_files_check") ||
			os_file_exists(BASE_PATH
				       "/disable_missing_files_check.txt");
	}
#endif

	check_safe_mode_sentinel();
#if defined(Q_OS_WIN)
	BOOL success = FALSE;
	SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
	PSID admin_group;
	success = AllocateAndInitializeSid(&auth, 2,
					   SECURITY_BUILTIN_DOMAIN_RID,
					   DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0,
					   0, 0, &admin_group);
	if (success) {
		if (!CheckTokenMembership(nullptr, admin_group, &success))
			success = false;
		FreeSid(admin_group);
	}
	PLS_INIT_INFO(MAINFRAME_MODULE,
		      "Start PRISMLiveStudio, Running as administrator: %s",
		      success ? "true" : "false");

	ULONG_PTR gdip_token;
	const Gdiplus::GdiplusStartupInput gdip_input;
	Gdiplus::GdiplusStartup(&gdip_token, &gdip_input, nullptr);
	pls_init_font_collection();
#endif

	upgrade_settings();

	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(argc, argv);

#ifdef _WIN32
	if (hRtwq) {
		typedef HRESULT(STDAPICALLTYPE * PFN_RtwqShutdown)();
		PFN_RtwqShutdown func =
			(PFN_RtwqShutdown)GetProcAddress(hRtwq, "RtwqShutdown");
		func();
		FreeLibrary(hRtwq);
	}

	log_blocked_dlls();
	pls_enter_font_collection();
	pls_destory_font_collection();
	pls_leave_font_collection();
	Gdiplus::GdiplusShutdown(gdip_token);
#endif

	delete_safe_mode_sentinel();
	blog(LOG_INFO, "Number of memory leaks: %ld", bnum_allocs());
	log_cleanup();

	return ret;
}
