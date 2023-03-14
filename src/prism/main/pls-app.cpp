/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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
#include <sstream>
#include <mutex>
#include <optional>
#include <util/bmem.h>
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/profiler.hpp>
#include <util/cf-parser.h>
#include <obs-config.h>
#include <obs.hpp>

#include <QGuiApplication>
#include <QProxyStyle>
#include <QScreen>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QMessageBox>

#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "window-basic-settings.hpp"
#include "crash-report.hpp"
#include "platform.hpp"
#include "main-view.hpp"
#include "log/log.h"
#include "alert-view.hpp"
#include "notice-view.hpp"
#include "PLSChannelsEntrance.h"
#include <fstream>
#include <QThread>
#include <curl/curl.h>

#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tchar.h>
#include <regex>

#define UseFreeMusic

#ifdef _WIN32
#include <windows.h>
#include <versionhelpers.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#else
#include <signal.h>
#include <pthread.h>
#endif

#include <tlhelp32.h>
#include <iostream>

#include "ui-config.h"

#include "PLSPlatformApi/PLSPlatformApi.h"
#include "prism/PLSPlatformPrism.h"

#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include "PLSMenu.hpp"
#include "window-basic-status-bar.hpp"
#include "PLSChatHelper.h"
#include "PLSNetworkMonitor.h"
#include "obs.hpp"
#include "PLSMotionFileManager.h"
#include "PLSAction.h"
#include "pls/crash-writer.h"
#include < WinSock.h>

#include "PLSBlockDump.h"

#pragma comment(lib, "ws2_32.lib")

#pragma comment(lib, "dbghelp.lib")

#define IS_CHAT_IS_HIDDEN_FIRST_SETTED "isChatIsHiddenFirstSetted"
#define SIDE_BAR_WINDOW_INITIALLIZED "sideBarWindowInitialized"

#define RENDER_ENGINE_CHECK_PROCESS L"EnvCheck.exe"
#define MAX_BACKTRACE 1024

#define PLS_PROJECT_NAME "P8e4826_PRISMLiveStudio-ZT"
#define PLS_PROJECT_NAME_KR "P8e4826_PRISMLiveStudio-KR"

using namespace std;

static log_handler_t def_log_handler;
std::string logUserID;
std::string maskingLogUserID;

std::string prismSession;
std::string videoAdapter;
std::string prism_cpuName;
static std::string crashFileMutexUuid;
static string currentLogFile;
static string lastLogFile;
static string lastCrashLogFile;
static string logFrom = "ExternalLog";

bool portable_mode = false;
static bool multi = false;
static bool log_verbose = false;
static bool unfiltered_log = false;
bool opt_start_streaming = false;
bool opt_start_recording = false;
bool opt_studio_mode = false;
bool opt_start_replaybuffer = false;
bool opt_minimize_tray = false;
bool opt_allow_opengl = false;
bool opt_always_on_top = false;
string opt_starting_collection;
string opt_starting_profile;
string opt_starting_scene;

bool remuxAfterRecord = false;
string remuxFilename;

// GPU hint exports for AMD/NVIDIA laptops
#ifdef _MSC_VER
extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 1;
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

extern bool file_exists(const char *path);
int pls_auto_dump();
void killSplashScreen();

QObject *CreateShortcutFilter()
{
	return new PLSEventFilter([](QObject *obj, QEvent *event) {
		auto mouse_event = [obj](QMouseEvent &event) {
			if (PLSDialogView *dialogView = dynamic_cast<PLSDialogView *>(obj)) {
				obj->eventFilter(obj, &event);
			}

			switch (event.button()) {
			case Qt::NoButton:
			case Qt::LeftButton:
			case Qt::RightButton:
			case Qt::AllButtons:
			case Qt::MouseButtonMask:
				return false;
			}

			if (!App()->HotkeysEnabledInFocus())
				return true;

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event.type() == QEvent::MouseButtonPress;

			switch (event.button()) {
			case Qt::MidButton:
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

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(event.modifiers());

			obs_hotkey_inject_event(hotkey, pressed);
			return true;
		};

		auto key_event = [&](QKeyEvent *event) {
			if (!App()->HotkeysEnabledInFocus())
				return true;

			QDialog *dialog = qobject_cast<QDialog *>(obj);

			obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
			bool pressed = event->type() == QEvent::KeyPress;

			switch (event->key()) {
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
				/* Falls through. */
				__fallthrough;
			default:
				hotkey.key = obs_key_from_virtual_key(event->nativeVirtualKey());
				break;
			}

			hotkey.modifiers = TranslateQtKeyboardEventModifiers(event->modifiers());

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

string CurrentTimeString()
{
	return CurrentDateTimeString();
}

string CurrentDateTimeString()
{
	auto curTime = QDateTime::currentDateTime();
	return curTime.toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString();
}

static inline void LogString(fstream &logFile, const char *timeString, char *str)
{
	logFile << timeString << str << endl;
}

static inline void LogStringChunk(fstream &logFile, const char *log_level, const char *module_name, const char *tid, char *str)
{
	char *nextLine = str;
	string timeString = CurrentTimeString();
	timeString += module_name;
	timeString += log_level;
	timeString += tid;

	while (*nextLine) {
		char *nextLine = strchr(str, '\n');
		if (!nextLine)
			break;

		if (nextLine != str && nextLine[-1] == '\r') {
			nextLine[-1] = 0;
		} else {
			nextLine[0] = 0;
		}

		LogString(logFile, timeString.c_str(), str);
		nextLine++;
		str = nextLine;
	}

	LogString(logFile, timeString.c_str(), str);
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

static inline bool too_many_repeated_entries(fstream &logFile, const char *, const char *tid, const char *msg, const char *output_str)
{
	static mutex log_mutex;
	static const char *last_msg_ptr = nullptr;
	static int last_char_sum = 0;
	static std::vector<char> cmp_str;
	static int rep_count = 0;

	int new_sum = sum_chars(output_str);
	int size = _scprintf(msg, output_str) + 1;
	cmp_str.resize(size);

	lock_guard<mutex> guard(log_mutex);

	if (unfiltered_log) {
		return false;
	}

	if (last_msg_ptr == msg) {
		int diff = std::abs(new_sum - last_char_sum);
		if (diff < MAX_CHAR_VARIATION) {
			return (rep_count++ >= MAX_REPEATED_LINES);
		}
	}

	if (rep_count > MAX_REPEATED_LINES) {
		logFile << CurrentTimeString() << " [OBS][INFO]" << tid << "Last log entry repeated for " << to_string(rep_count - MAX_REPEATED_LINES) << " more lines" << endl;
	}

	last_msg_ptr = msg;
	strcpy(cmp_str.data(), output_str);
	last_char_sum = new_sum;
	rep_count = 0;

	return false;
}

static const char *obsLogLevelToStr(int log_level)
{
	switch (log_level) {
	case LOG_ERROR:
		return "[ERROR]";
	case LOG_WARNING:
		return "[WARNING]";
	case LOG_INFO:
		return "[INFO]";
	case LOG_DEBUG:
	default:
		return "[DEBUG]";
	}
}

static void do_log(int log_level, const char *format, va_list args, void *param)
{
	if (IsDebuggerPresent()) {
		struct log_info_s {
			bool kr;
			uint32_t tid;
			const char *module_name;
			const char *format;
		};

		log_info_s *log_info = (log_info_s *)(void *)format;
		const char *module_name = !strcmp(log_info->module_name, "obs") ? " [OBS]" : " [PLS]";
		const char *msg = log_info->format;
		uint32_t tid = log_info->tid;

		std::vector<char> str;
		int size = _vscprintf(msg, args) + 1;
		str.resize(size);
		vsnprintf(str.data(), size, msg, args);

		char tidstr[64];
		sprintf(tidstr, log_info->kr ? "[%u][KR] " : "[%u] ", tid);

		int wNum = MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, NULL, 0);
		if (wNum > 1) {
			static wstring wide_buf;
			static mutex wide_mutex;

			lock_guard<mutex> lock(wide_mutex);
			wide_buf.reserve(size_t(wNum) + 1);
			wide_buf.resize(size_t(wNum) - 1);
			MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, &wide_buf[0], wNum);
			wide_buf.push_back('\n');

			if (log_info->kr)
				OutputDebugStringW(L"KR: ");
			OutputDebugStringW(wide_buf.c_str());
		}
	}

#if defined(_WIN32) && defined(PLS_DEBUGBREAK_ON_ERROR)
	if (log_level <= LOG_ERROR && IsDebuggerPresent())
		__debugbreak();
#endif
}

#define DEFAULT_LANG "en-US"

bool PLSApp::InitGlobalConfigDefaults()
{
	config_set_default_string(globalConfig, "General", "Language", DEFAULT_LANG);
	config_set_default_uint(globalConfig, "General", "MaxLogs", 10);
	config_set_default_int(globalConfig, "General", "InfoIncrement", -1);
	config_set_default_string(globalConfig, "General", "ProcessPriority", "Normal");
	config_set_default_bool(globalConfig, "General", "EnableAutoUpdates", true);

#if _WIN32
	config_set_default_string(globalConfig, "Video", "Renderer", "Direct3D 11");
#else
	config_set_default_string(globalConfig, "Video", "Renderer", "OpenGL");
#endif

	config_set_default_bool(globalConfig, "BasicWindow", "PreviewEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "PreviewProgramMode", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SceneDuplicationMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SwapScenesMode", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SnappingEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ScreenSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SourceSnapping", true);
	config_set_default_bool(globalConfig, "BasicWindow", "CenterSnapping", false);
	config_set_default_double(globalConfig, "BasicWindow", "SnapDistance", 10.0);
	config_set_default_bool(globalConfig, "BasicWindow", "RecordWhenStreaming", false);
	config_set_default_bool(globalConfig, "BasicWindow", "KeepRecordingWhenStreamStops", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayEnabled", true);
	config_set_default_bool(globalConfig, "BasicWindow", "SysTrayWhenStarted", false);
	config_set_default_bool(globalConfig, "BasicWindow", "SaveProjectors", false);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowTransitions", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowListboxToolbars", true);
	config_set_default_bool(globalConfig, "BasicWindow", "ShowStatusBar", true);
	config_set_default_bool(globalConfig, "BasicWindow", "StudioModeLabels", true);

	if (!config_get_bool(globalConfig, "General", "Pre21Defaults")) {
		config_set_default_string(globalConfig, "General", "CurrentTheme", DEFAULT_THEME);
	}

	config_set_default_string(globalConfig, "General", "HotkeyFocusType", "NeverDisableHotkeys");

	config_set_default_bool(globalConfig, "BasicWindow", "VerticalVolControl", false);

	config_set_default_bool(globalConfig, "BasicWindow", "MultiviewMouseSwitch", true);

	config_set_default_bool(globalConfig, "BasicWindow", "MultiviewDrawNames", true);

	config_set_default_bool(globalConfig, "BasicWindow", "MultiviewDrawAreas", true);

#ifdef _WIN32
	uint32_t winver = GetWindowsVersion();

	config_set_default_bool(globalConfig, "Audio", "DisableAudioDucking", true);
	config_set_default_bool(globalConfig, "General", "BrowserHWAccel", winver > 0x601);
#endif

#ifdef __APPLE__
	config_set_default_bool(globalConfig, "Video", "DisableOSXVSync", true);
	config_set_default_bool(globalConfig, "Video", "ResetOSXVSyncOnExit", true);
#endif
	return true;
}

static bool do_mkdir(const char *path)
{
	if (os_mkdirs(path) == MKDIR_ERROR) {
		PLSErrorBox(NULL, "Failed to create directory");
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

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/profiler_data") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/user") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/textmotion") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

#ifdef _WIN32
	//if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/crashes") <= 0)
	//	return false;
	//if (!do_mkdir(path))
	//	return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/updates") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;
#endif

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/plugin_config") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/Cache") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/naver_shopping") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/crashDump") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	return true;
}

static bool MakeUserProfileDirs()
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/profiles") <= 0)
		return false;
	if (!do_mkdir(path))
		return false;

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes") <= 0)
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

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/profiles") <= 0)
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

		const char *curName = config_get_string(config, "General", "Name");
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

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/basic/scenes") <= 0)
		return outputPath;

	strcat(path, "/*.json");

	if (os_glob(path, 0, &glob) != 0)
		return outputPath;

	for (size_t i = 0; i < glob->gl_pathc; i++) {
		struct os_globent ent = glob->gl_pathv[i];
		if (ent.directory)
			continue;

		obs_data_t *data = obs_data_create_from_json_file_safe(ent.path, "bak");
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

static void removeChildProcessCrashFile()
{
	char path[512];
	int len = os_get_config_path(path, sizeof(path), "PRISMLiveStudio/crashDump/childProcess.json");
	if (len <= 0) {
		return;
	}

	if (os_is_file_exist(path)) {
		if (remove(path) == -1)
			PLS_WARN(MAIN_EXCEPTION, "Remove child process json file failed.");
	}
	return;
}

bool PLSApp::UpdatePre22MultiviewLayout(const char *layout)
{
	if (!layout)
		return false;

	if (astrcmpi(layout, "horizontaltop") == 0) {
		config_set_int(globalConfig, "BasicWindow", "MultiviewLayout", static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "horizontalbottom") == 0) {
		config_set_int(globalConfig, "BasicWindow", "MultiviewLayout", static_cast<int>(MultiviewLayout::HORIZONTAL_BOTTOM_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalleft") == 0) {
		config_set_int(globalConfig, "BasicWindow", "MultiviewLayout", static_cast<int>(MultiviewLayout::VERTICAL_LEFT_8_SCENES));
		return true;
	}

	if (astrcmpi(layout, "verticalright") == 0) {
		config_set_int(globalConfig, "BasicWindow", "MultiviewLayout", static_cast<int>(MultiviewLayout::VERTICAL_RIGHT_8_SCENES));
		return true;
	}

	return false;
}

bool PLSApp::InitGlobalConfig()
{
	char path[512];
	bool changed = false;

	PLS_INIT_INFO(MAINFRAME_MODULE, "initialize global configuration");

	int len = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/global.ini");
	if (len <= 0) {
		return false;
	}

	int errorcode = globalConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		PLSErrorBox(NULL, "Failed to open global.ini: %d", errorcode);
		return false;
	}

	//create the update global init
	len = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/updates/update.ini");
	if (len <= 0) {
		return false;
	}

	//open the update init file
	errorcode = updateConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		PLSErrorBox(NULL, "Failed to open update.ini: %d", errorcode);
		return false;
	}

	//create the navershopping global init
	len = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/naver_shopping/naver_shopping.ini");
	if (len <= 0) {
		return false;
	}

	//open the update init file
	errorcode = naverShoppingConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		PLSErrorBox(NULL, "Failed to open update.ini: %d", errorcode);
		return false;
	}

	//open the cookie init file
	len = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/Cache/cookies.ini");
	if (len <= 0) {
		return false;
	}

	//open the update init file
	errorcode = cookieConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		PLSErrorBox(NULL, "Failed to open cookies.ini: %d", errorcode);
		return false;
	}

	if (!opt_starting_collection.empty()) {
		string path = GetSceneCollectionFileFromName(opt_starting_collection.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "SceneCollection", opt_starting_collection.c_str());
			config_set_string(globalConfig, "Basic", "SceneCollectionFile", path.c_str());
			changed = true;
		}
	}

	if (!opt_starting_profile.empty()) {
		string path = GetProfileDirFromName(opt_starting_profile.c_str());
		if (!path.empty()) {
			config_set_string(globalConfig, "Basic", "Profile", opt_starting_profile.c_str());
			config_set_string(globalConfig, "Basic", "ProfileDir", path.c_str());
			changed = true;
		}
	}

	uint32_t lastVersion = config_get_int(globalConfig, "General", "LastVersion");

	if (!config_has_user_value(globalConfig, "General", "Pre19Defaults")) {
		bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(19, 0, 0);

		config_set_bool(globalConfig, "General", "Pre19Defaults", useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre21Defaults")) {
		bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(21, 0, 0);

		config_set_bool(globalConfig, "General", "Pre21Defaults", useOldDefaults);
		changed = true;
	}

	if (!config_has_user_value(globalConfig, "General", "Pre23Defaults")) {
		bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(23, 0, 0);

		config_set_bool(globalConfig, "General", "Pre23Defaults", useOldDefaults);
		changed = true;
	}

#define PRE_24_1_DEFS "Pre24.1Defaults"
	if (!config_has_user_value(globalConfig, "General", PRE_24_1_DEFS)) {
		bool useOldDefaults = lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(24, 1, 0);

		config_set_bool(globalConfig, "General", PRE_24_1_DEFS, useOldDefaults);
		changed = true;
	}
#undef PRE_24_1_DEFS

	if (config_has_user_value(globalConfig, "BasicWindow", "MultiviewLayout")) {
		const char *layout = config_get_string(globalConfig, "BasicWindow", "MultiviewLayout");
		changed |= UpdatePre22MultiviewLayout(layout);
	}

	if (lastVersion && lastVersion < MAKE_SEMANTIC_VERSION(24, 0, 0)) {
		bool disableHotkeysInFocus = config_get_bool(globalConfig, "General", "DisableHotkeysInFocus");
		if (disableHotkeysInFocus)
			config_set_string(globalConfig, "General", "HotkeyFocusType", "DisableHotkeysInFocus");
		changed = true;
	}

	if (changed)
		config_save_safe(globalConfig, "tmp", nullptr);

	InitCrashConfigDefaults();

	return InitGlobalConfigDefaults();
}

bool PLSApp::InitLocale()
{
	ProfileScope("PLSApp::InitLocale");

	PLS_INIT_INFO(MAINFRAME_MODULE, "initialize app language");

	const char *lang = config_get_string(globalConfig, "General", "Language");

	locale = lang;

	string englishPath;
	if (!GetDataFilePath("locale/" DEFAULT_LANG ".ini", englishPath)) {
		PLSErrorBox(NULL, "Failed to find locale/" DEFAULT_LANG ".ini");
		return false;
	}

	textLookup = text_lookup_create(englishPath.c_str());
	if (!textLookup) {
		PLSErrorBox(NULL, "Failed to create locale from file '%s'", englishPath.c_str());
		return false;
	}

	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PRISM Live Studio", QSettings::NativeFormat);
	int languageID = settings.value("InstallLanguage").toInt();
	std::string language = languageID2Locale(languageID);
	config_set_string(globalConfig, "General", "Language", language.c_str());
	locale = lang = language.c_str();
	if (!astrcmpi(lang, DEFAULT_LANG)) {
		return true;
	}

	stringstream file;
	file << "locale/" << lang << ".ini";

	string path;
	if (GetDataFilePath(file.str().c_str(), path)) {
		if (!text_lookup_add(textLookup, path.c_str()))
			PLS_ERROR(MAIN_EXCEPTION, "Failed to add locale file '%s'", GetFileName(path).c_str());
	} else {
		PLS_ERROR(MAIN_EXCEPTION, "Could not find locale file '%s'", GetFileName(file.str().c_str()).c_str());
	}

	return true;
}

void PLSApp::AddExtraThemeColor(QPalette &pal, int group, const char *name, uint32_t color)
{
	std::function<void(QPalette::ColorGroup)> func;

#define DEF_PALETTE_ASSIGN(name)                                                                                         \
	do {                                                                                                             \
		func = [&](QPalette::ColorGroup group) { pal.setColor(group, QPalette::name, QColor::fromRgb(color)); }; \
	} while (false)

	if (astrcmpi(name, "alternateBase") == 0) {
		DEF_PALETTE_ASSIGN(AlternateBase);
	} else if (astrcmpi(name, "base") == 0) {
		DEF_PALETTE_ASSIGN(Base);
	} else if (astrcmpi(name, "brightText") == 0) {
		DEF_PALETTE_ASSIGN(BrightText);
	} else if (astrcmpi(name, "button") == 0) {
		DEF_PALETTE_ASSIGN(Button);
	} else if (astrcmpi(name, "buttonText") == 0) {
		DEF_PALETTE_ASSIGN(ButtonText);
	} else if (astrcmpi(name, "brightText") == 0) {
		DEF_PALETTE_ASSIGN(BrightText);
	} else if (astrcmpi(name, "dark") == 0) {
		DEF_PALETTE_ASSIGN(Dark);
	} else if (astrcmpi(name, "highlight") == 0) {
		DEF_PALETTE_ASSIGN(Highlight);
	} else if (astrcmpi(name, "highlightedText") == 0) {
		DEF_PALETTE_ASSIGN(HighlightedText);
	} else if (astrcmpi(name, "light") == 0) {
		DEF_PALETTE_ASSIGN(Light);
	} else if (astrcmpi(name, "link") == 0) {
		DEF_PALETTE_ASSIGN(Link);
	} else if (astrcmpi(name, "linkVisited") == 0) {
		DEF_PALETTE_ASSIGN(LinkVisited);
	} else if (astrcmpi(name, "mid") == 0) {
		DEF_PALETTE_ASSIGN(Mid);
	} else if (astrcmpi(name, "midlight") == 0) {
		DEF_PALETTE_ASSIGN(Midlight);
	} else if (astrcmpi(name, "shadow") == 0) {
		DEF_PALETTE_ASSIGN(Shadow);
	} else if (astrcmpi(name, "text") == 0 || astrcmpi(name, "foreground") == 0) {
		DEF_PALETTE_ASSIGN(Text);
	} else if (astrcmpi(name, "toolTipBase") == 0) {
		DEF_PALETTE_ASSIGN(ToolTipBase);
	} else if (astrcmpi(name, "toolTipText") == 0) {
		DEF_PALETTE_ASSIGN(ToolTipText);
	} else if (astrcmpi(name, "windowText") == 0) {
		DEF_PALETTE_ASSIGN(WindowText);
	} else if (astrcmpi(name, "window") == 0 || astrcmpi(name, "background") == 0) {
		DEF_PALETTE_ASSIGN(Window);
	} else {
		return;
	}

#undef DEF_PALETTE_ASSIGN

	switch (group) {
	case QPalette::Disabled:
	case QPalette::Active:
	case QPalette::Inactive:
		func((QPalette::ColorGroup)group);
		break;
	default:
		func((QPalette::ColorGroup)QPalette::Disabled);
		func((QPalette::ColorGroup)QPalette::Active);
		func((QPalette::ColorGroup)QPalette::Inactive);
	}
}

void PLSApp::InitSideBarWindowVisible()
{
	if (!mainView)
		return;
	if (mainView->isVisible()) {
		PLSBasic::Get()->InitSideBarWindowVisible();
	} else {
		// app minimum to the system tray so here we show side bar window delay
		connect(
			mainView, &PLSMainView::isshowSignal, mainView,
			[=](bool isShow) {
				if (isShow && !mainView->property(SIDE_BAR_WINDOW_INITIALLIZED).toBool()) {
					mainView->setProperty(SIDE_BAR_WINDOW_INITIALLIZED, true);
					PLSBasic::Get()->InitSideBarWindowVisible();
				}
			},
			Qt::QueuedConnection);
	}
}

void PLSApp::InitCrashConfigDefaults()
{
	char path[512];
	int len = os_get_config_path(path, sizeof(path), PRISM_CRASH_CONFIG_PATH);
	if (len <= 0) {
		return;
	}

	obs_data *crashData = obs_data_create_from_json_file_safe(path, "bak");
	if (!crashData)
		crashData = obs_data_create();

	obs_data *currentObj = obs_data_create();
	obs_data_set_string(currentObj, PRISM_SESSION, prismSession.c_str());
	obs_data_set_array(currentObj, MODULES, nullptr);
	obs_data_set_obj(crashData, CURRENT_PRISM, currentObj);

	obs_data_save_json_safe(crashData, path, "tmp", "bak");

	obs_data_release(currentObj);
	obs_data_release(crashData);
}

struct CFParser {
	cf_parser cfp = {};
	inline ~CFParser() { cf_parser_free(&cfp); }
	inline operator cf_parser *() { return &cfp; }
	inline cf_parser *operator->() { return &cfp; }
};

void PLSApp::ParseExtraThemeData(const char *path)
{
	BPtr<char> data = os_quick_read_utf8_file(path);
	QPalette pal = palette();
	CFParser cfp;
	int ret;

	cf_parser_parse(cfp, data, path);

	while (cf_go_to_token(cfp, "PLSTheme", nullptr)) {
		if (!cf_next_token(cfp))
			return;

		int group = -1;

		if (cf_token_is(cfp, ":")) {
			ret = cf_next_token_should_be(cfp, ":", nullptr, nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp))
				return;

			if (cf_token_is(cfp, "disabled")) {
				group = QPalette::Disabled;
			} else if (cf_token_is(cfp, "active")) {
				group = QPalette::Active;
			} else if (cf_token_is(cfp, "inactive")) {
				group = QPalette::Inactive;
			} else {
				continue;
			}

			if (!cf_next_token(cfp))
				return;
		}

		if (!cf_token_is(cfp, "{"))
			continue;

		for (;;) {
			if (!cf_next_token(cfp))
				return;

			ret = cf_token_is_type(cfp, CFTOKEN_NAME, "name", nullptr);
			if (ret != PARSE_SUCCESS)
				break;

			DStr name;
			dstr_copy_strref(name, &cfp->cur_token->str);

			ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp))
				return;

			const char *array;
			uint32_t color = 0;

			if (cf_token_is(cfp, "#")) {
				array = cfp->cur_token->str.array;
				color = strtol(array + 1, nullptr, 16);

			} else if (cf_token_is(cfp, "rgb")) {
				ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10) << 16;

				ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10) << 8;

				ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
				if (ret != PARSE_SUCCESS)
					continue;
				if (!cf_next_token(cfp))
					return;

				array = cfp->cur_token->str.array;
				color |= strtol(array, nullptr, 10);

			} else if (cf_token_is(cfp, "white")) {
				color = 0xFFFFFF;

			} else if (cf_token_is(cfp, "black")) {
				color = 0;
			}

			if (!cf_go_to_token(cfp, ";", nullptr))
				return;

			AddExtraThemeColor(pal, group, name->array, color);
		}

		ret = cf_token_should_be(cfp, "}", "}", nullptr);
		if (ret != PARSE_SUCCESS)
			continue;
	}

	setPalette(pal);
}

bool PLSApp::SetTheme(std::string name, std::string path)
{
	theme = name;

	QString themePath = QApplication::applicationDirPath() + "/themes/" + "dark-theme.dll";
	themePlugin.setFileName(themePath);

	if (!themePlugin.load()) {
		return false;
	}

	/* Check user dir first, then preinstalled themes. */
	if (path == "") {
		char userDir[512];
		name = "themes/" + name + ".qss";
		string temp = "PRISMLiveStudio/" + name;
		int ret = GetConfigPath(userDir, sizeof(userDir), temp.c_str());

		if (ret > 0 && QFile::exists(userDir)) {
			path = string(userDir);
		} else if (!GetDataFilePath(name.c_str(), path)) {
			PLSErrorBox(NULL, "Failed to find %s.", name.c_str());
			return false;
		}
	}

	QString mpath = QString("file:///") + path.c_str();
	setPalette(defaultPalette);
	setStyleSheet(mpath);
	ParseExtraThemeData(path.c_str());

	emit StyleChanged();
	return true;
}

bool PLSApp::InitTheme()
{
	PLS_INIT_INFO(MAINFRAME_MODULE, "initialize app theme");
	defaultPalette = palette();
	return SetTheme("Dark");
}

bool PLSApp::InitWatermark()
{
	bool isWatermark = config_has_user_value(globalConfig, "General", "Watermark");
	if (!isWatermark) {
		config_set_bool(globalConfig, "General", "Watermark", true);
		obs_watermark_set_enabled(true);
	} else {
		obs_watermark_set_enabled(config_get_bool(globalConfig, "General", "Watermark"));
	}
	return true;
}

bool PLSApp::HotkeyEnable()
{
	return hotkeyEnable;
}

bool PLSApp::GetCurrentThemePath(std::string &themePath)
{
	std::string name = "themes/";
	if (!GetDataFilePath((name + theme).c_str(), themePath)) {
		PLSErrorBox(NULL, "Failed to find themes/.");
		return false;
	}
	return true;
}

const char *PLSApp::getProjectName()
{
	return PLS_PROJECT_NAME;
}

const char *PLSApp::getProjectName_kr()
{
	return PLS_PROJECT_NAME_KR;
}

void QtMessageCallback(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
	QByteArray localMsg = msg.toLocal8Bit();

	// Check log which we care about.
	if (type == QtWarningMsg) {
		if (msg.contains("QMetaObject::invokeMethod: No such method") || msg.contains("QMetaMethod::invoke: Unable to handle unregistered datatype")) {
			PLS_WARN(MAINFRAME_MODULE, "%s", localMsg.constData());
			assert(false && "invokeMethod exception");
			return;
		}
	}

	switch (type) {
	case QtDebugMsg:
		PLS_DEBUG(MAINFRAME_MODULE, "[QT::Debug] %s", localMsg.constData());
		break;
	case QtInfoMsg:
		PLS_DEBUG(MAINFRAME_MODULE, "[QT::Info] %s", localMsg.constData());
		break;
	case QtWarningMsg:
		PLS_DEBUG(MAINFRAME_MODULE, "[QT::Warning] %s", localMsg.constData());
		break;
	case QtCriticalMsg:
		PLS_DEBUG(MAINFRAME_MODULE, "[QT::Critical] %s", localMsg.constData());
		break;
	case QtFatalMsg:
		PLS_DEBUG(MAINFRAME_MODULE, "[QT::Fatal] %s", localMsg.constData());
		break;
	}
}

void PLSApp::sessionExpiredhandler()
{
	PLSAlertView::warning(getMainView(), tr("Alert.Title"), tr("main.message.prism.login.session.expired"));
	pls_prism_change_over_login_view();
}

PLSApp::PLSApp(int &argc, char **argv, profiler_name_store_t *store) : QApplication(argc, argv), profilerNameStore(store)
{
	// force modify current work directory
	QDir::setCurrent(QApplication::applicationDirPath());

	std::ignore = CoInitialize(nullptr);

	qInstallMessageHandler(QtMessageCallback);

#ifdef _WIN32
	PLSNetworkMonitor::Instance()->StartListen();
#endif

	sleepInhibitor = os_inhibit_sleep_create("PLS Video/audio");

	setWindowIcon(QIcon::fromTheme("obs", QIcon(":/res/images/PRISMLiveStudio.ico")));

	if (pls_is_dev_server()) {
		pls_load_dev_server();
		enable_popup_messagebox(true);
	}
}

PLSApp::~PLSApp()
{
	PLS_INFO(MAINSCENE_MODULE, __FUNCTION__);

#ifdef _WIN32
	PLSNetworkMonitor::Instance()->StopListen();

	bool disableAudioDucking = globalConfig && config_get_bool(globalConfig, "Audio", "DisableAudioDucking");
	if (disableAudioDucking)
		DisableAudioDucking(false);
#endif

#ifdef __APPLE__
	bool vsyncDiabled = config_get_bool(globalConfig, "Video", "DisableOSXVSync");
	bool resetVSync = config_get_bool(globalConfig, "Video", "ResetOSXVSyncOnExit");
	if (vsyncDiabled && resetVSync)
		EnableOSXVSync(true);
#endif

	os_inhibit_sleep_set_active(sleepInhibitor, false);
	os_inhibit_sleep_destroy(sleepInhibitor);

	CoUninitialize();

	removeChildProcessCrashFile();
	os_mutex_handle_close(crashFileMutexUuid.c_str());
}

static void move_basic_to_profiles(void)
{
	char path[512];
	char new_path[512];
	os_glob_t *glob;

	/* if not first time use */
	if (GetConfigPath(path, 512, "PRISMLiveStudio/basic") <= 0)
		return;
	if (!file_exists(path))
		return;

	/* if the profiles directory doesn't already exist */
	if (GetConfigPath(new_path, 512, "PRISMLiveStudio/basic/profiles") <= 0)
		return;
	if (file_exists(new_path))
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
	if (!file_exists(path))
		return;

	if (GetConfigPath(new_path, 512, "PRISMLiveStudio/basic/scenes") <= 0)
		return;
	if (file_exists(new_path))
		return;

	if (os_mkdir(new_path) == MKDIR_ERROR)
		return;

	strcat(path, "/scenes.json");
	strcat(new_path, "/");
	strcat(new_path, Str("Untitled"));
	strcat(new_path, ".json");

	os_rename(path, new_path);
}

void PLSApp::AppInit()
{
	ProfileScope("PLSApp::AppInit");

	if (!InitGlobalConfig())
		throw "Failed to initialize global config";
	if (!InitLocale())
		throw "Failed to load locale";

	config_set_default_string(globalConfig, "Basic", "Profile", Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "ProfileDir", Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollection", Str("Untitled"));
	config_set_default_string(globalConfig, "Basic", "SceneCollectionFile", Str("Untitled"));

	if (!config_has_user_value(globalConfig, "Basic", "Profile")) {
		config_set_string(globalConfig, "Basic", "Profile", Str("Untitled"));
		config_set_string(globalConfig, "Basic", "ProfileDir", Str("Untitled"));
	}

	if (!config_has_user_value(globalConfig, "Basic", "SceneCollection")) {
		config_set_string(globalConfig, "Basic", "SceneCollection", Str("Untitled"));
		config_set_string(globalConfig, "Basic", "SceneCollectionFile", Str("Untitled"));
	}

#ifdef _WIN32
	bool disableAudioDucking = config_get_bool(globalConfig, "Audio", "DisableAudioDucking");
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
		throw "Failed to create profile directories";

	PLS_PLATFORM_API;
	PLS_PLATFORM_PRSIM; // zhangdewen singleton init
	PLS_HTTP_HELPER;
}

const char *PLSApp::GetRenderModule() const
{
	const char *renderer = config_get_string(globalConfig, "Video", "Renderer");

	return (astrcmpi(renderer, "Direct3D 11") == 0) ? DL_D3D11 : DL_OPENGL;
}

static bool StartupPLS(const char *locale, profiler_name_store_t *store)
{
	char path[512];

	if (GetConfigPath(path, sizeof(path), "PRISMLiveStudio/plugin_config") <= 0)
		return false;

	return obs_startup(locale, path, store);
}

inline void PLSApp::ResetHotkeyState(bool inFocus)
{
	obs_hotkey_enable_background_press((inFocus && enableHotkeysInFocus) || (!inFocus && enableHotkeysOutOfFocus));
}

void PLSApp::UpdateHotkeyFocusSetting(bool resetState)
{
	if (globalConfig == nullptr) {
		return;
	}
	hotkeyEnable = true;
	enableHotkeysInFocus = true;
	enableHotkeysOutOfFocus = true;
	emit HotKeyEnabled(hotkeyEnable);
	const char *hotkeyFocusType = config_get_string(globalConfig, "General", "HotkeyFocusType");

	if (astrcmpi(hotkeyFocusType, "DisableHotkeysInFocus") == 0) {
		enableHotkeysInFocus = false;
	} else if (astrcmpi(hotkeyFocusType, "DisableHotkeysOutOfFocus") == 0) {
		enableHotkeysOutOfFocus = false;
	}

	if (resetState)
		ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

void PLSApp::DisableHotkeys()
{
	hotkeyEnable = false;
	enableHotkeysInFocus = false;
	enableHotkeysOutOfFocus = false;
	emit HotKeyEnabled(hotkeyEnable);
	ResetHotkeyState(applicationState() == Qt::ApplicationActive);
}

Q_DECLARE_METATYPE(VoidFunc)

void PLSApp::Exec(VoidFunc func)
{
	func();
}

static void ui_task_handler(obs_task_t task, void *param, bool wait)
{
	auto doTask = [=]() {
		/* to get clang-format to behave */
		task(param);
	};
	QMetaObject::invokeMethod(App(), "Exec", wait ? WaitConnection() : Qt::AutoConnection, Q_ARG(VoidFunc, doTask));
}

static PLSBasic *newMainView(QPointer<PLSMainView> &mainView, QPointer<PLSMainWindow> &mainWindow)
{
	PLSDpiHelper dpiHelper;
	mainView = new PLSMainView(nullptr, dpiHelper);
	PLSBasic *basic = new PLSBasic(mainView, dpiHelper);
	mainWindow = basic;
	return basic;
}

static void check_local_host()
{
	char szHost[256];
	::gethostname(szHost, 256);
	hostent *pHost = ::gethostbyname(szHost);
	if (!pHost) {
		return;
	}
	in_addr addr;
	int i = 0;
	if (pHost->h_addrtype == AF_INET) {
		while (pHost->h_addr_list[i] != 0) {
			addr.s_addr = *(u_long *)pHost->h_addr_list[i++];
			char *slzp = ::inet_ntoa(addr);
			std::regex regExp("10.*");
			logFrom = std::regex_match(slzp, regExp) ? "InternalLog" : "ExternalLog";
		}
	}
}

QString get_windows_version_info()
{
	bool server = IsWindowsServer();
	bool x64bit = true;
	if (sizeof(void *) != 8) {
		x64bit = false;
	}
	QString bitStr = x64bit ? "(x64)" : "(x32)";

	uint32_t version_info = GetWindowsVersion();
	uint32_t build_version = GetWindowsBuildVersion();

	QString ver_str("");
	if (version_info >= 0x0A00) {
		if (server) {
			ver_str = "Windows Server 2016 Technical Preview";
		} else {
			if (build_version >= 21664) {
				ver_str = "Windows 11";
			} else {
				ver_str = "Windows 10";
			}
		}
	} else if (version_info >= 0x0603) {
		if (server) {
			ver_str = "Windows Server 2012 r2";
		} else {
			ver_str = "Windows 8.1";
		}
	} else if (version_info >= 0x0602) {
		if (server) {
			ver_str = "Windows Server 2012";
		} else {
			ver_str = "Windows 8";
		}
	} else if (version_info >= 0x0601) {
		if (server) {
			ver_str = "Windows Server 2008 r2";
		} else {
			ver_str = "Windows 7";
		}
	} else if (version_info >= 0x0600) {
		if (server) {
			ver_str = "Windows Server 2008";
		} else {
			ver_str = "Windows Vista";
		}
	} else {
		ver_str = "Windows Before Vista";
	}

	QString output_ver_info = ver_str + bitStr;
	return output_ver_info;
}

bool PLSApp::PLSInit()
{
	ProfileScope("PLSApp::PLSInit");

	setAttribute(Qt::AA_UseHighDpiPixmaps);

	qRegisterMetaType<VoidFunc>();

	if (!StartupPLS(locale.c_str(), GetProfilerNameStore()))
		return false;

	obs_set_ui_task_handler(ui_task_handler);

#ifdef _WIN32
	bool browserHWAccel = config_get_bool(globalConfig, "General", "BrowserHWAccel");

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "BrowserHWAccel", browserHWAccel);
	obs_apply_private_data(settings);
	obs_data_release(settings);

	PLS_INFO(MAINFRAME_MODULE, "Current Date/Time: %s", CurrentDateTimeString().c_str());

	PLS_INFO(MAINFRAME_MODULE, "Browser Hardware Acceleration: %s", browserHWAccel ? "true" : "false");
#endif

	PLS_INFO(MAINFRAME_MODULE, "Portable mode: %s", portable_mode ? "true" : "false");

	setQuitOnLastWindowClosed(false);

	PLSMotionFileManager::instance()->loadMotionFlagSvg();

	PLSBasic *basic = newMainView(mainView, mainWindow);
	PLS_INIT_INFO(MAINFRAME_MODULE, "main window create success.");

	basic->menuBar()->hide();
	mainView->setCloseEventCallback(std::bind(&PLSBasic::mainViewClose, basic, std::placeholders::_1));

	connect(mainView, &PLSMainView::popupSettingView, basic, &PLSBasic::onPopupSettingView);
	connect(mainView, &PLSMainView::sideBarButtonClicked, basic, &PLSBasic::OnSideBarButtonClicked);

	connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::sessionExpired, this, &PLSApp::sessionExpiredhandler);
	QHBoxLayout *hl = new QHBoxLayout(mainView->content());
	hl->setContentsMargins(5, 0, 5, 5);
	hl->setSpacing(0);
	hl->addWidget(mainWindow, 1);

	mainView->setAttribute(Qt::WA_DeleteOnClose, true);
	connect(mainView, SIGNAL(destroyed()), this, SLOT(quit()));
	connect(this, &QCoreApplication::aboutToQuit, this, [=] {
		PLS_INFO(MAINFRAME_MODULE, "QCoreApplication::aboutToQuit");

		if (mainView) {
			PLS_WARN(MAINFRAME_MODULE, "mainView is still valid");
			mainView->close();
		}
	});

	bool initialized = mainWindow->PLSInit();

	mainView->installEventFilter(mainView->statusBar());
	PLS_PLATFORM_API->initialize();

	PLS_INIT_INFO(MODULE_PlatformService, "Platform push flow interface instance initialization completed.");

	connect(this, &QGuiApplication::applicationStateChanged, [this](Qt::ApplicationState state) { ResetHotkeyState(state == Qt::ApplicationActive); });
	ResetHotkeyState(applicationState() == Qt::ApplicationActive);

	killSplashScreen();

	if (initialized) {
		basic->mainMenu = new PLSPopupMenu(mainView);
		basic->mainMenu->setObjectName("mainMenu");
		basic->mainMenu->addActions(mainWindow->menuBar()->actions());
		basic->mainMenu->asButtonPopupMenu(mainView->menuButton(), QPoint(0, 1));
		basic->initMainMenu();
		pls_flush_style(basic->mainMenu);

		// Unified interface to init side bar window visible
		InitSideBarWindowVisible();

		//token vaild
		PLS_INIT_INFO(MAINFRAME_MODULE, "check prism token vaild or not.");
		PLSBasic ::Get()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN);
		if (!mainView || mainView->isClosing()) {
			return false;
		}

		bool updateNow = false;

		// if update view never shown
		if (basic->getUpdateResult() == MainCheckUpdateResult::HasUpdate && basic->needShowUpdateView()) {
			PLS_INIT_INFO(UPDATE_MODULE, "show update window.");
			if (basic->ShowUpdateView(basic->getMainView())) {
				QMetaObject::invokeMethod(basic, &PLSBasic::ShowLoginView, Qt::QueuedConnection);
				updateNow = true;
			}
		}

		//PRISM/WangChuanjing/20200825/#3423/for main view load delay
		obs_set_system_initialized(true);
		PLS_INIT_INFO(MAINFRAME_MODULE, "System has been initialized.");

		if (!updateNow) {
			//notice view
			PLS_INIT_INFO(MAINFRAME_MODULE, "start request prism notice info.");
			QVariantMap noticeInfo = pls_get_new_notice_Info();
			if (noticeInfo.size()) {
				PLSNoticeView view(noticeInfo.value(NOTICE_CONTENE).toString(), noticeInfo.value(NOTICE_TITLE).toString(), noticeInfo.value(NOTICE_DETAIL_LINK).toString(), mainView);
				view.exec();
			}
		}
	} else {
		PLS_INIT_WARN(MAINFRAME_MODULE, "mainview initialization failed, try to close mainview.");
		mainView->close();
	}
	return mainView ? true : false;
}

string PLSApp::GetVersionString() const
{
	stringstream ver;

#ifdef HAVE_PLSCONFIG_H
	ver << PLS_VERSION;
#else
	ver << LIBOBS_API_MAJOR_VER << "." << LIBOBS_API_MINOR_VER << "." << LIBOBS_API_PATCH_VER;

#endif
	ver << " (";

#ifdef _WIN32
	if (sizeof(void *) == 8)
		ver << "64-bit, ";
	else
		ver << "32-bit, ";

	ver << "windows)";
#elif __APPLE__
	ver << "mac)";
#elif __FreeBSD__
	ver << "freebsd)";
#else /* assume linux for the time being */
	ver << "linux)";
#endif

	return ver.str();
}

bool PLSApp::IsPortableMode()
{
	return portable_mode;
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

const char *PLSApp::InputAudioSource() const
{
	return INPUT_AUDIO_SOURCE;
}

const char *PLSApp::OutputAudioSource() const
{
	return OUTPUT_AUDIO_SOURCE;
}

const char *PLSApp::GetLastLog() const
{
	return lastLogFile.c_str();
}

const char *PLSApp::GetCurrentLog() const
{
	return currentLogFile.c_str();
}

const char *PLSApp::GetLastCrashLog() const
{
	return lastCrashLogFile.c_str();
}

bool PLSApp::TranslateString(const char *lookupVal, const char **out) const
{
	for (obs_frontend_translate_ui_cb cb : translatorHooks) {
		if (cb(lookupVal, out))
			return true;
	}

	return text_lookup_getstr(App()->GetTextLookup(), lookupVal, out);
}

static inline bool isValidChar(int ch)
{
	return (ch >= -1) && (ch <= 255);
}

QString PLSTranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const
{
	int sourceTextLen = (int)strlen(sourceText) + 1;
	char *sourceTextCopy = (char *)_malloca(sourceTextLen);
	if (sourceTextCopy == NULL) {
		PLS_ERROR(MAINFRAME_MODULE, "translate _malloca buffer failed");
		return "";
	}

	for (int i = 0, j = 0; i < sourceTextLen; ++i) {
		if (isValidChar(sourceText[i]) && sourceText[i] != '&' && !::isspace(sourceText[i])) {
			sourceTextCopy[j++] = sourceText[i];
		}
	}

	const char *out = nullptr;
	if (!App()->TranslateString(sourceTextCopy, &out)) {
		_freea(sourceTextCopy);
		return QString(sourceText);
	}

	_freea(sourceTextCopy);
	UNUSED_PARAMETER(context);
	UNUSED_PARAMETER(disambiguation);
	UNUSED_PARAMETER(n);
	return QT_UTF8(out);
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

/*static void delete_oldest_file(bool has_prefix, const char *location)
{
	BPtr<char> logDir(GetConfigPathPtr(location));
	string oldestLog;
	uint64_t oldest_ts = (uint64_t)-1;
	struct os_dirent *entry;

	unsigned int maxLogs = (unsigned int)config_get_uint(App()->GlobalConfig(), "General", "MaxLogs");

	os_dir_t *dir = os_opendir(logDir);
	if (dir) {
		unsigned int count = 0;

		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts = convert_log_name(has_prefix, entry->d_name);

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
}*/

static void get_last_log(bool has_prefix, const char *subdir_to_use, std::string &last)
{
	BPtr<char> logDir(GetConfigPathPtr(subdir_to_use));
	struct os_dirent *entry;
	os_dir_t *dir = os_opendir(logDir);
	uint64_t highest_ts = 0;

	if (dir) {
		while ((entry = os_readdir(dir)) != NULL) {
			if (entry->directory || *entry->d_name == '.')
				continue;

			uint64_t ts = convert_log_name(has_prefix, entry->d_name);

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
	char file[256] = {};
	SYSTEMTIME cur_time;
	GetLocalTime(&cur_time);
	snprintf(file, sizeof(file), "%d-%02d-%02d%c%02d-%02d-%02d-%03d.%s", cur_time.wYear, cur_time.wMonth, cur_time.wDay, noSpace ? '_' : ' ', cur_time.wHour, cur_time.wMinute, cur_time.wSecond,
		 cur_time.wMilliseconds, extension);

	return string(file);
}

string GenerateSpecifiedFilename(const char *extension, bool noSpace, const char *format)
{
	// Zhangdewen remove auto remux to mp4
	bool autoRemux = false; // config_get_bool(main->Config(), "Video", "AutoRemux");

	if ((strcmp(extension, "mp4") == 0) && autoRemux)
		extension = "mkv";

	BPtr<char> filename = os_generate_formatted_filename(extension, !noSpace, format);

	remuxFilename = string(filename);
	remuxAfterRecord = autoRemux;

	return string(filename);
}

static vector<pair<string, pair<int, string>>> getLocaleNames()
{
	string path;
	if (!GetDataFilePath("locale.ini", path))
		throw "Could not find locale.ini path";

	ConfigFile ini;
	if (ini.Open(path.c_str(), CONFIG_OPEN_EXISTING) != 0)
		throw "Could not open locale.ini";

	size_t sections = config_num_sections(ini);

	vector<pair<string, pair<int, string>>> names;
	names.reserve(sections);
	for (size_t i = 0; i < sections; i++) {
		const char *tag = config_get_section(ini, i);
		int id = config_get_int(ini, tag, "LID");
		const char *name = config_get_string(ini, tag, "Name");
		if (name != NULL && tag != NULL) {
			names.emplace_back(tag, pair<int, string>{id, name});
		}
	}
	return names;
}

vector<pair<string, pair<int, string>>> &GetLocaleNames()
{
	static vector<pair<string, pair<int, string>>> localeNames = getLocaleNames();
	return localeNames;
}

std::string languageID2Locale(int languageID, const std::string &defaultLanguage)
{
	for (const auto &localeName : GetLocaleNames()) {
		if (localeName.second.first == languageID) {
			return localeName.first;
		}
	}
	return defaultLanguage;
}

int locale2languageID(const std::string &locale, int defaultLanguageID)
{
	for (const auto &localeName : GetLocaleNames()) {
		if (localeName.first == locale) {
			return localeName.second.first;
		}
	}
	return defaultLanguageID;
}

static void init_local_log()
{
	char logDir[512];
	if (GetConfigPath(logDir, sizeof(logDir), "PRISMLiveStudio/logs/") > 0) {
		QDir dir(QString::fromUtf8(logDir));
		dir.removeRecursively();
	}

	set_log_handler(do_log, nullptr);
}

static auto ProfilerNameStoreRelease = [](profiler_name_store_t *store) { profiler_name_store_free(store); };

using ProfilerNameStore = std::unique_ptr<profiler_name_store_t, decltype(ProfilerNameStoreRelease)>;

ProfilerNameStore CreateNameStore()
{
	return ProfilerNameStore{profiler_name_store_create(), ProfilerNameStoreRelease};
}

static auto SnapshotRelease = [](profiler_snapshot_t *snap) { profile_snapshot_free(snap); };

using ProfilerSnapshot = std::unique_ptr<profiler_snapshot_t, decltype(SnapshotRelease)>;

ProfilerSnapshot GetSnapshot()
{
	return ProfilerSnapshot{profile_snapshot_create(), SnapshotRelease};
}

static void SaveProfilerData(const ProfilerSnapshot &snap)
{
	if (currentLogFile.empty())
		return;

	auto pos = currentLogFile.rfind('.');
	if (pos == currentLogFile.npos)
		return;

#define LITERAL_SIZE(x) x, (sizeof(x) - 1)
	ostringstream dst;
	dst.write(LITERAL_SIZE("PRISMLiveStudio/profiler_data/"));
	dst.write(currentLogFile.c_str(), pos);
	dst.write(LITERAL_SIZE(".csv.gz"));
#undef LITERAL_SIZE

	BPtr<char> path = GetConfigPathPtr(dst.str().c_str());
	if (!profiler_snapshot_dump_csv_gz(snap.get(), path))
		PLS_WARN(MAINFRAME_MODULE, "Could not save profiler data");
}

static auto ProfilerFree = [](void *) {
	profiler_stop();

	auto snap = GetSnapshot();

	profiler_print(snap.get());
	profiler_print_time_between_calls(snap.get());

	SaveProfilerData(snap);

	profiler_free();
};

class WakeupThread : public QThread {
	HANDLE hEvent;

public:
	WakeupThread(HANDLE hEvent_) : hEvent(hEvent_) {}
	virtual ~WakeupThread() {}

public:
	void run()
	{
		while (!isInterruptionRequested()) {
			if ((WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0) && !isInterruptionRequested()) {
				ResetEvent(hEvent);
				QMetaObject::invokeMethod(PLSBasic::Get(), "singletonWakeup");
			}
		}
	}
};

static void removeConfig(const char *config)
{
	char path[512];
	if (GetConfigPath(path, sizeof(path), config) <= 0) {
		PLS_INFO(MAINFRAME_MODULE, "remove config[%s] failed, because get config path failed.", config);
		return;
	}

	if (QFileInfo(path).isDir()) {
		PLS_DEBUG(MAINFRAME_MODULE, "clear config directory.");
		QDir dir(path);
		QFileInfoList fis = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
		for (QFileInfo &fi : fis) {
			QString fp = fi.absoluteFilePath();
			if (fi.isDir()) {
				PLS_DEBUG(MAINFRAME_MODULE, "remove config directory.");
				QDir subdir(fp);
				subdir.removeRecursively();
			} else {
				PLS_DEBUG(MAINFRAME_MODULE, "remove config file %s.", GetFileName(fp.toUtf8().constData()).c_str());
				QFile::remove(fp);
			}
		}
	} else {
		PLS_DEBUG(MAINFRAME_MODULE, "remove config file %s.", GetFileName(path).c_str());
		QFile::remove(path);
	}

	PLS_INFO(MAINFRAME_MODULE, "remove config[%s] successed.", config);
}

static QString getExceptionAlertString(init_exception_code code)
{
	QString alertString = "";
	switch (code) {
	case init_exception_code_common:
		alertString = QTStr(PRISM_ALERT_INIT_FAILED);
		break;
	case init_exception_code_engine_not_support:
		alertString = QTStr(ENGINE_ALERT_INIT_FAILED);
		break;
	case init_exception_code_engine_param_error:
		alertString = QTStr(ENGINE_ALERT_INIT_PARAM_ERROR);
		break;
	case init_exception_code_engine_not_support_dx_version:
		alertString = QTStr(ENGINE_ALERT_INIT_DX_VERSION);
		break;
	default:
		break;
	}
	return alertString;
}

std::wstring get_process_path()
{
	WCHAR szFilePath[MAX_PATH] = {};
	GetModuleFileNameW(NULL, szFilePath, MAX_PATH);

	int nLen = (int)wcslen(szFilePath);
	for (int i = nLen - 1; i >= 0; --i) {
		if (szFilePath[i] == '\\') {
			szFilePath[i + 1] = 0;
			break;
		}
	}

	return std::wstring(szFilePath) + std::wstring(RENDER_ENGINE_CHECK_PROCESS);
}

//return true unless exit_code != 0
static const char *render_engine_check_name = "render_engine_check";
enum render_engine_check_status {
	render_engine_check_status_normal = 0,
	render_engine_check_status_crash,
	render_engine_check_status_timeout,
};
static void render_engine_check(render_engine_check_status &status)
{
	const int process_wait_time = 5000;
	std::wstring process_path = get_process_path();
	if (process_path.empty()) {
		PLS_INFO(MAINFRAME_MODULE, "Render engine check process path empty");
		status = render_engine_check_status_normal;
		return;
	}

	if (!os_is_file_exist_ex(process_path.c_str())) {
		PLS_INFO(MAINFRAME_MODULE, "Render engine check exe not exist");
		status = render_engine_check_status_normal;
		return;
	}

	profile_start(render_engine_check_name);
	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK;
	si.wShowWindow = SW_HIDE;

	wchar_t cmd[2048];
	swprintf_s(cmd, ARRAY_COUNT(cmd), L"\"%s\"", process_path.c_str());

	BOOL bOK = CreateProcessW((LPWSTR)process_path.c_str(), cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
	if (!bOK) {
		PLS_INFO(MAINFRAME_MODULE, "Fail to run render engine check process! error:%u", GetLastError());
		status = render_engine_check_status_normal;
		return;
	}
	DWORD wait_res = WaitForSingleObject(pi.hProcess, process_wait_time);
	if (wait_res == WAIT_OBJECT_0) {
		DWORD exit_code;
		bool res = GetExitCodeProcess(pi.hProcess, &exit_code);
		if (exit_code == 0) {
			status = render_engine_check_status_normal;
		} else {
			status = render_engine_check_status_crash;
		}
	} else if (wait_res == WAIT_TIMEOUT) {
		status = render_engine_check_status_timeout;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	profile_end(render_engine_check_name);
}

static QJsonObject getObjByJsonFile(QString _path, bool configPath)
{
	QJsonObject obj;
	char *path = nullptr;
	if (configPath)
		path = GetConfigPathPtr(_path.toStdString().c_str());
	else
		path = os_get_executable_path_ptr(_path.toStdString().c_str());

	if (!path) {
		PLS_WARN(MAINFRAME_MODULE, "Get json path failed.");
		return obj;
	}

	if (!os_is_file_exist(path))
		PLS_WARN(MAINFRAME_MODULE, "Not found %s file.", GetFileName(path).c_str());

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		PLS_WARN(MAINFRAME_MODULE, "Open %s file failed.", GetFileName(path).c_str());
		return obj;
	}
	QByteArray data = file.readAll();
	file.close();

	obj = QJsonDocument::fromJson(data).object();

	return obj;
}

static QJsonObject getCrashFileData()
{
	os_mutex_handle_lock(crashFileMutexUuid.c_str());
	QJsonObject data = getObjByJsonFile(PRISM_CRASH_CONFIG_PATH, true);
	os_mutex_handle_unlock(crashFileMutexUuid.c_str());
	return data;
}

static void parseActionEvent()
{
	QJsonObject obj = getCrashFileData();
	QJsonObject actionObj = obj["actionEvent"].toObject();
	if (!actionObj.isEmpty() && actionObj.value(IS_CRASHED).toBool()) {
		PLS_INFO(MAINFRAME_MODULE, "Prism crashed last time.");
	}
}

static const char *run_program_init = "run_program_init";
static int run_program(int argc, char *argv[])
{
	pls_auto_dump();

	check_local_host();

	pls_set_config_path(&GetConfigPath);

	auto profilerNameStore = CreateNameStore();

	std::unique_ptr<void, decltype(ProfilerFree)> prof_release(static_cast<void *>(&ProfilerFree), ProfilerFree);

	profiler_start();
	profile_register_root(run_program_init, 0);

	ScopeProfiler prof{run_program_init};

	QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	QCoreApplication::addLibraryPath(".");
	QCoreApplication::setApplicationName("PRISMLiveStudio");
	QCoreApplication::setApplicationVersion(PLS_VERSION);
	QCoreApplication::setOrganizationName("Naver");
	QCoreApplication::setOrganizationDomain(APP_ORGANIZATION);

	PLS_INIT_INFO(MAINFRAME_MODULE, "Start PRISMLiveStudio");

	PLSApp program(argc, argv, profilerNameStore.get());
	try {
		if (!program.InitTheme())
			throw "Failed to load theme";
		if (!InitApplicationBundle())
			throw "Failed to initialize application bundle";
		if (!MakeUserDirs())
			throw "Failed to create required user directories";

		QString userID = PLSLoginUserInfo::getInstance()->getUserCode();
		QString maskingUserID = PLSLoginUserInfo::getInstance()->getUserCodeWithEncode();
		logUserID = userID.toUtf8();
		maskingLogUserID = maskingUserID.toUtf8();
		pls_set_user_id(logUserID.c_str(), PLS_SET_TAG_KR);
		pls_set_user_id(maskingUserID.toUtf8().constData(), PLS_SET_TAG_CN);

		std::string currentEnvironment = pls_is_dev_server() ? "Dev" : "Rel";
		PLS_INIT_INFO(MAINFRAME_MODULE, "version: " PLS_VERSION ", processId:%ld, neloSession: %s, currentRunEnvironment:%s;", GetCurrentProcessId(), prismSession.c_str(),
			      currentEnvironment.c_str());

		pls_add_global_field("LogFrom", logFrom.c_str());

		if (!PLSLoginUserInfo::getInstance()->isPrismLogined()) {
			PLS_INIT_INFO(MAINFRAME_MODULE, "user info expired, clear user configs");
			removeConfig("PRISMLiveStudio/basic/profiles");
			removeConfig("PRISMLiveStudio/basic/scenes");
			removeConfig("PRISMLiveStudio/Cache");
			removeConfig("PRISMLiveStudio/plugin_config");
			removeConfig("PRISMLiveStudio/user/gcc.json");
			removeConfig("PRISMLiveStudio/user/config.ini");
			removeConfig("PRISMLiveStudio/global.ini");
			removeConfig("PRISMLiveStudio/naver_shopping");
			PLSMotionFileManager::instance()->logoutClear();
		}
		removeConfig(QString("PRISMLiveStudio/user/%1.png").arg(PLSLoginUserInfo::getInstance()->getAuthType()).toUtf8().constData());

		program.AppInit();
		// delete_oldest_file(false, "PRISMLiveStudio/profiler_data");
		PLS_INIT_INFO(MAINFRAME_MODULE, "app configuration information initialization completed.");

		PLSTranslator translator;
		program.installTranslator(&translator);
		PLS_INIT_INFO(MAINFRAME_MODULE, "install language translator");

		const wchar_t *eventName = L"PRISMLiveStudio";

		HANDLE hEvent = OpenEventW(EVENT_ALL_ACCESS, false, eventName);
		bool already_running = !!hEvent;

		if (!already_running) {
			hEvent = CreateEventW(NULL, TRUE, FALSE, eventName);
			if (GetLastError() == ERROR_ALREADY_EXISTS) {
				already_running = true;
			}
		}

		if (already_running) {
			PLS_INFO("app/singleton", "app is already running");
			SetEvent(hEvent);
			CloseHandle(hEvent);
			return 0;
		}

		PLS_INIT_INFO(MAINFRAME_MODULE, "check render engine");
		render_engine_check_status engine_status = render_engine_check_status_normal;
		render_engine_check(engine_status);
		if (engine_status == render_engine_check_status_crash) {
			PLS_WARN(MAINFRAME_MODULE, "Render engine check error");
			PLSMessageBox::warning(NULL, QTStr("Alert.Title"), QTStr("prism.engine.alert.initcrash"));
			return 0;
		} else if (engine_status == render_engine_check_status_timeout) {
			PLS_WARN(MAINFRAME_MODULE, "Render engine check timeout");
			PLSAlertView::Button button = PLSMessageBox::question(NULL, QTStr("Alert.Title"), QTStr("prism.engine.alert.check.timeout"));
			if (button != PLSAlertView::Button::Yes) {
				PLS_INFO(MAINFRAME_MODULE, "Exit button clicked");
				return 0;
			}
		}

		PLS_INIT_INFO(MAINFRAME_MODULE, "init program");
		if (!program.PLSInit()) {
			CloseHandle(hEvent);
			QMetaObject::invokeMethod(
				&program, [&program]() { program.quit(); }, Qt::QueuedConnection);
			return program.exec();
		}

		parseActionEvent();

		prof.Stop();

		PLS_INIT_INFO(MAINFRAME_MODULE, "start wakeup thread");
		WakeupThread wakeupThread(hEvent);
		wakeupThread.start();

		QObject::connect(&program, &QApplication::aboutToQuit, [&]() {
			wakeupThread.requestInterruption();
			SetEvent(hEvent);
			wakeupThread.wait();
			CloseHandle(hEvent);
		});

		program.setAppRunning(true);

		return program.exec();

	} catch (const char *error) {
		PLS_ERROR(MAINFRAME_MODULE, "%s", error);
		PLSErrorBox(nullptr, "%s", error);
		exit(-1);
	} catch (init_exception_code code) {
		QString codeStr = getExceptionAlertString(code);
		if (!codeStr.isEmpty()) {
			PLSErrorBox(nullptr, "%s", codeStr.toStdString().c_str());
		}
		exit(-1);
	} catch (...) {
		PLS_ERROR(MAINFRAME_MODULE, "unknown exception catched");
		PLSErrorBox(nullptr, "unknown exception catched");
		exit(-1);
	}
}

void PLSApp::clearNaverShoppingConfig()
{
	removeConfig("PRISMLiveStudio/naver_shopping/naver_shopping.ini");
	//create the navershopping global init
	char path[512];
	int len = GetConfigPath(path, sizeof(path), "PRISMLiveStudio/naver_shopping/naver_shopping.ini");
	if (len <= 0) {
		return;
	}
	//open the update init file
	int errorcode = naverShoppingConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		return;
	}
}

#define MAX_CRASH_REPORT_SIZE (150 * 1024)

#ifdef _WIN32

#define CRASH_MESSAGE                                                      \
	"Woops, PLS has crashed!\n\nWould you like to copy the crash log " \
	"to the clipboard?  (Crash logs will still be saved to the "       \
	"%appdata%\\PRISMLiveStudio\\crashes directory)"

static void main_crash_handler(const char *format, va_list args, void *param)
{
	char *text = new char[MAX_CRASH_REPORT_SIZE];

	vsnprintf(text, MAX_CRASH_REPORT_SIZE, format, args);
	text[MAX_CRASH_REPORT_SIZE - 1] = 0;

	// delete_oldest_file(true, "PRISMLiveStudio/crashes");

	string name = "PRISMLiveStudio/crashes/Crash ";
	name += GenerateTimeDateFilename("txt");

	BPtr<char> path(GetConfigPathPtr(name.c_str()));

	fstream file;

#ifdef _WIN32
	BPtr<wchar_t> wpath;
	os_utf8_to_wcs_ptr(path, 0, &wpath);
	file.open(wpath, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
#else
	file.open(path, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
#endif
	file << text;
	file.close();

	int ret = MessageBoxA(NULL, CRASH_MESSAGE, "PLS has crashed!", MB_YESNO | MB_ICONERROR | MB_TASKMODAL);

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

	UNUSED_PARAMETER(param);
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

		AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL);
	}

	if (!!LookupPrivilegeValue(NULL, SE_INC_BASE_PRIORITY_NAME, &val)) {
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = val;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (!AdjustTokenPrivileges(token, false, &tp, sizeof(tp), NULL, NULL)) {
			PLS_INFO(MAINFRAME_MODULE, "Could not set privilege to "
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

#ifndef PLS_UNIX_STRUCTURE
#define PLS_UNIX_STRUCTURE 0
#endif

int GetConfigPath(char *path, size_t size, const char *name)
{
	if (!PLS_UNIX_STRUCTURE && portable_mode) {
		if (name && *name) {
			return snprintf(path, size, CONFIG_PATH "/%s", name);
		} else {
			return snprintf(path, size, CONFIG_PATH);
		}
	} else {
		return os_get_config_path(path, size, name);
	}
}

char *GetConfigPathPtr(const char *name)
{
	if (!PLS_UNIX_STRUCTURE && portable_mode) {
		char path[512];

		if (snprintf(path, sizeof(path), CONFIG_PATH "/%s", name) > 0) {
			return bstrdup(path);
		} else {
			return NULL;
		}
	} else {
		return os_get_config_path_ptr(name);
	}
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
	path.resize(strlen(path.c_str()));

	size_t len = path.size();

	auto is_slash = [](char ch) { return ch == '/' || ch == '\\'; };
	if (!is_slash(path[len - 1])) {
		if (extension) {
			path += ".";
			path += extension;
		}

		if (!file_exists(path.c_str())) {
			return true;
		}
	}

	int index = 1;

	do {
		path.resize(len);
		path += std::to_string(++index);
		if (extension) {
			path += ".";
			path += extension;
		}
	} while (file_exists(path.c_str()));

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

static inline bool arg_is(const char *arg, const char *long_form, const char *short_form)
{
	return (long_form && strcmp(arg, long_form) == 0) || (short_form && strcmp(arg, short_form) == 0);
}

#if !defined(_WIN32) && !defined(__APPLE__)
#define IS_UNIX 1
#endif

/* if using XDG and was previously using an older build of PLS, move config
 * files to XDG directory */
#if defined(USE_XDG) && defined(IS_UNIX)
static void move_to_xdg(void)
{
	char old_path[512];
	char new_path[512];
	char *home = getenv("HOME");
	if (!home)
		return;

	if (snprintf(old_path, 512, "%s/.PRISMLiveStudio", home) <= 0)
		return;

	/* make base xdg path if it doesn't already exist */
	if (GetConfigPath(new_path, 512, "") <= 0)
		return;
	if (os_mkdirs(new_path) == MKDIR_ERROR)
		return;

	if (GetConfigPath(new_path, 512, "PRISMLiveStudio") <= 0)
		return;

	if (file_exists(old_path) && !file_exists(new_path)) {
		rename(old_path, new_path);
	}
}
#endif

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

	const char *section = (strcmp(mode, "Advanced") == 0) ? "AdvOut" : "SimpleOutput";

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

static void convert_x264_settings(obs_data_t *data)
{
	bool use_bufsize = obs_data_get_bool(data, "use_bufsize");

	if (use_bufsize) {
		int buffer_size = (int)obs_data_get_int(data, "buffer_size");
		if (buffer_size == 0)
			obs_data_set_string(data, "rate_control", "CRF");
	}
}

static void convert_14_2_encoder_setting(const char *encoder, const char *file)
{
	obs_data_t *data = obs_data_create_from_json_file_safe(file, "bak");
	obs_data_item_t *cbr_item = obs_data_item_byname(data, "cbr");
	obs_data_item_t *rc_item = obs_data_item_byname(data, "rate_control");
	bool modified = false;
	bool cbr = true;

	if (cbr_item) {
		cbr = obs_data_item_get_bool(cbr_item);
		obs_data_item_unset_user_value(cbr_item);

		obs_data_set_string(data, "rate_control", cbr ? "CBR" : "VBR");

		modified = true;
	}

	if (!rc_item && astrcmpi(encoder, "obs_x264") == 0) {
		if (!cbr_item)
			obs_data_set_string(data, "rate_control", "CBR");
		else if (!cbr)
			convert_x264_settings(data);

		modified = true;
	}

	if (modified)
		obs_data_save_json_safe(data, file, "tmp", "bak");

	obs_data_item_release(&rc_item);
	obs_data_item_release(&cbr_item);
	obs_data_release(data);
}

static void upgrade_settings(void)
{
	char path[512];
	int pathlen = GetConfigPath(path, 512, "PRISMLiveStudio/basic/profiles");

	if (pathlen <= 0)
		return;
	if (!file_exists(path))
		return;

	os_dir_t *dir = os_opendir(path);
	if (!dir)
		return;

	struct os_dirent *ent = os_readdir(dir);

	while (ent) {
		if (ent->directory && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
			strcat(path, "/");
			strcat(path, ent->d_name);
			strcat(path, "/basic.ini");

			ConfigFile config;
			int ret;

			ret = config.Open(path, CONFIG_OPEN_EXISTING);
			if (ret == CONFIG_SUCCESS) {
				if (update_ffmpeg_output(config) || update_reconnect(config)) {
					config_save_safe(config, "tmp", nullptr);
				}
			}

			if (config) {
				const char *sEnc = config_get_string(config, "AdvOut", "Encoder");
				const char *rEnc = config_get_string(config, "AdvOut", "RecEncoder");

				/* replace "cbr" option with "rate_control" for
				 * each profile's encoder data */
				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/recordEncoder.json");
				convert_14_2_encoder_setting(rEnc, path);

				path[pathlen] = 0;
				strcat(path, "/");
				strcat(path, ent->d_name);
				strcat(path, "/streamEncoder.json");
				convert_14_2_encoder_setting(sEnc, path);
			}

			path[pathlen] = 0;
		}

		ent = os_readdir(dir);
	}

	os_closedir(dir);
}

void ctrlc_handler(int s)
{
	UNUSED_PARAMETER(s);

	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	main->close();
}

const char *generate_guid()
{
	static char buf[64] = {0};
	GUID guid;
	if (S_OK == ::CoCreateGuid(&guid)) {
		_snprintf(buf, sizeof(buf), "{%08X-%04X-%04x-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			  guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	}
	return (const char *)buf;
}

static std::chrono::steady_clock::time_point startTime;
void printTotalStartTime()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::string seconds = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count());
	const char *fields[][2] = {{"duration", seconds.c_str()}};
	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, fields, 1, "PRISMLiveStudio launch duration");
}

static PROCESS_INFORMATION splashScreenProcess = {NULL, NULL, 0, 0};
bool startSplashScreen()
{
	uint32_t pid = GetCurrentProcessId();

	char splashScreenApp[512];
	GetModuleFileNameA(NULL, splashScreenApp, 512);
	PathRemoveFileSpecA(splashScreenApp);
	strcat_s(splashScreenApp, "\\PRISMSplashScreen.exe");

	char commandLine[1024];
	sprintf_s(commandLine, "\"%s\" \"%u\"", splashScreenApp, pid);

	STARTUPINFOA si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.lpTitle = "PRISMSplashScreen";
	if (!CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &splashScreenProcess)) {
		return false;
	}
	return false;
}
void killSplashScreen()
{
	if (splashScreenProcess.hThread) {
		CloseHandle(splashScreenProcess.hThread);
		splashScreenProcess.hThread = nullptr;
	}

	if (splashScreenProcess.hProcess) {
		printTotalStartTime();
		TerminateProcess(splashScreenProcess.hProcess, -1);
		CloseHandle(splashScreenProcess.hProcess);
		splashScreenProcess.hProcess = nullptr;
	}
}

int main(int argc, char *argv[])
{
	startTime = std::chrono::steady_clock::now();

	startSplashScreen();

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);

	struct sigaction sig_handler;

	sig_handler.sa_handler = ctrlc_handler;
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

#ifdef _DEBUG
	auto cw = _control87(0, 0) & MCW_EM;
	cw &= ~(_EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW);
	_control87(cw, MCW_EM);
#endif

#ifdef _WIN32
	//obs_init_win32_crash_handler();
	SetErrorMode(SEM_FAILCRITICALERRORS);
	load_debug_privilege();
	//base_set_crash_handler(main_crash_handler, nullptr);
#endif

	base_get_log_handler(&def_log_handler, nullptr);
	prismSession = generate_guid();
	crashFileMutexUuid = generate_guid();
	if (os_mutex_handle_create(crashFileMutexUuid.c_str())) {
		cw_set_file_mutex_uuid(crashFileMutexUuid.c_str());
	}

	QString strVersion = get_windows_version_info();
	log_init(prismSession.c_str(), PLS_PROJECT_NAME, PLS_PROJECT_NAME_KR);

	init_local_log();

#if defined(USE_XDG) && defined(IS_UNIX)
	move_to_xdg();
#endif

	obs_set_cmdline_args(argc, argv);

	for (int i = 1; i < argc; i++) {
		if (arg_is(argv[i], "--portable", "-p")) {
			portable_mode = true;

		} else if (arg_is(argv[i], "--multi", "-m")) {
			multi = true;

		} else if (arg_is(argv[i], "--verbose", nullptr)) {
			log_verbose = true;

		} else if (arg_is(argv[i], "--always-on-top", nullptr)) {
			opt_always_on_top = true;

		} else if (arg_is(argv[i], "--unfiltered_log", nullptr)) {
			unfiltered_log = true;

		} else if (arg_is(argv[i], "--startstreaming", nullptr)) {
			opt_start_streaming = true;

		} else if (arg_is(argv[i], "--startrecording", nullptr)) {
			opt_start_recording = true;

		} else if (arg_is(argv[i], "--startreplaybuffer", nullptr)) {
			opt_start_replaybuffer = true;

		} else if (arg_is(argv[i], "--collection", nullptr)) {
			if (++i < argc)
				opt_starting_collection = argv[i];

		} else if (arg_is(argv[i], "--profile", nullptr)) {
			if (++i < argc)
				opt_starting_profile = argv[i];

		} else if (arg_is(argv[i], "--scene", nullptr)) {
			if (++i < argc)
				opt_starting_scene = argv[i];

		} else if (arg_is(argv[i], "--minimize-to-tray", nullptr)) {
			opt_minimize_tray = true;

		} else if (arg_is(argv[i], "--studio-mode", nullptr)) {
			opt_studio_mode = true;

		} else if (arg_is(argv[i], "--allow-opengl", nullptr)) {
			opt_allow_opengl = true;
		} else if (arg_is(argv[i], "--help", "-h")) {
			std::cout << "--help, -h: Get list of available commands.\n\n"
				  << "--startstreaming: Automatically start streaming.\n"
				  << "--startrecording: Automatically start recording.\n"
				  << "--startreplaybuffer: Start replay buffer.\n\n"
				  << "--collection <string>: Use specific scene collection."
				  << "\n"
				  << "--profile <string>: Use specific profile.\n"
				  << "--scene <string>: Start with specific scene.\n\n"
				  << "--studio-mode: Enable studio mode.\n"
				  << "--minimize-to-tray: Minimize to system tray.\n"
				  << "--portable, -p: Use portable mode.\n"
				  << "--multi, -m: Don't warn when launching multiple instances.\n\n"
				  << "--verbose: Make log more verbose.\n"
				  << "--always-on-top: Start in 'always on top' mode.\n\n"
				  << "--unfiltered_log: Make log unfiltered.\n\n"
				  << "--allow-opengl: Allow OpenGL on Windows.\n\n"
				  << "--version, -V: Get current version.\n";

			exit(0);

		} else if (arg_is(argv[i], "--version", "-V")) {
			std::cout << "PRISM Live Studio - " << App()->GetVersionString() << "\n";
			exit(0);
		}
	}

#if !PLS_UNIX_STRUCTURE
	if (!portable_mode) {
		portable_mode = file_exists(BASE_PATH "/portable_mode") || file_exists(BASE_PATH "/obs_portable_mode") || file_exists(BASE_PATH "/portable_mode.txt") ||
				file_exists(BASE_PATH "/obs_portable_mode.txt");
	}
#endif

	upgrade_settings();

	curl_global_init(CURL_GLOBAL_ALL);
	int ret = run_program(argc, argv);

	PLS_INFO(MAINFRAME_MODULE, "Number of memory leaks: %ld", bnum_allocs());
	log_cleanup();
	os_sym_cleanup();
	return ret;
}

void showCrashExceptionNotice(QString location)
{
	QSystemTrayIcon *trayIcon = new QSystemTrayIcon(QIcon::fromTheme("obs-tray", QIcon(":/res/images/PRISMLiveStudio.ico")), qApp);
	if (trayIcon) {
		trayIcon->setToolTip(QString("PRISMLiveStudio"));
		trayIcon->setIcon(QIcon(":/PRISMLiveStudio.ico"));
		trayIcon->show();
		location = location.isEmpty() ? "PRISM Live Studio" : location;
		if (QSystemTrayIcon::supportsMessages()) {
			QString notice = QTStr(BLACKLIST_CRASHED_NOTICE);
			PLS_INFO(MAIN_EXCEPTION, "Crash exception notice is %s Crash location is %s.", notice.toUtf8().constData(), location.toUtf8().constData());
			trayIcon->showMessage(location, notice, QSystemTrayIcon::Warning, 10000);
		} else {
			PLS_WARN(MAIN_EXCEPTION, "Crash exception notice not show. supportsMessages failed.");
		}

	} else {
		PLS_WARN(MAIN_EXCEPTION, "Crash exception notice not show. create QSystemTrayIcon failed.");
	}
}

QJsonObject getBlackList()
{
	QJsonObject obj = getObjByJsonFile("PRISMLiveStudio\\user\\gpop.json", true);
	if (obj.isEmpty()) {
		PLS_INFO(MAINFRAME_MODULE, "Try to read the gpop file for the second time.");
		obj = getObjByJsonFile("data\\prism-studio\\user\\gpop.json", false);
	}
	QJsonObject optional = obj["optional"].toObject();
	QJsonObject blacklist = optional["blacklist"].toObject();
	return blacklist;
}

void captureStackBackTrace(struct _EXCEPTION_POINTERS *pExceptionPointers, std::vector<DWORD64> &stackFrams)
{
	bool bFind = false;
	void *pBackTrace[MAX_BACKTRACE] = {0};
	auto captured = CaptureStackBackTrace(1, MAX_BACKTRACE, pBackTrace, nullptr);
	for (int i = 0; i < captured; ++i) {
		if (!bFind) {
			bFind = reinterpret_cast<DWORD64>(pBackTrace[i]) == pExceptionPointers->ContextRecord->Rip;
		}
		if (bFind) {
			stackFrams.push_back(reinterpret_cast<DWORD64>(pBackTrace[i]));
		}
	}
}

void stackWalk(struct _EXCEPTION_POINTERS *pExceptionPointers, std::vector<DWORD64> &stackFrams)
{
	STACKFRAME frame = {};
	frame.AddrPC.Offset = pExceptionPointers->ContextRecord->Rip;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Offset = pExceptionPointers->ContextRecord->Rbp;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Offset = pExceptionPointers->ContextRecord->Rsp;
	frame.AddrStack.Mode = AddrModeFlat;

	HANDLE process = GetCurrentProcess();
	stackFrams.clear();

	while (frame.AddrFrame.Offset) {
		bool ret = StackWalk(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, pExceptionPointers->ContextRecord, NULL, SymFunctionTableAccess, SymGetModuleBase, NULL);
		if (!ret || frame.AddrFrame.Offset == 0)
			break;
		stackFrams.push_back(frame.AddrPC.Offset);
	}
	return;
}

void getModuleInfo(DWORD64 addr, DWORD64 &offset, std::string &moduleName_)
{
	HMODULE hModule;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)(addr), &hModule);
	char moduleName[128];
	if (hModule != NULL) {
		char modulePath[256], ext[16];
		GetModuleFileNameA(hModule, modulePath, 256);
		_splitpath(modulePath, NULL, NULL, moduleName, ext);
		strcat(moduleName, ext);
		moduleName_ = std::string(moduleName);
		offset = addr - reinterpret_cast<DWORD64>(hModule);
	} else {
		moduleName_ = std::to_string(addr);
		offset = addr;
	}
	return;
}

void analyzeException(struct _EXCEPTION_POINTERS *pExceptionPointers, std::string &crashKey, std::string &crashValue, std::string &crashType)
{
	std::string stackFramsStr{};
	std::vector<DWORD64> stackFrams{}, tempBackTrace{}, tempStackFrams{};
	DWORD64 exceptionRip = pExceptionPointers->ContextRecord->Rip;

	captureStackBackTrace(pExceptionPointers, tempBackTrace);
	stackWalk(pExceptionPointers, tempStackFrams);

	QJsonObject blacklist = getBlackList();

	crashKey = "MainGeneralCrash";
	crashType = "GeneralCrash";

	bool findBlacklist = false, bFind = false;
	std::string topLocation{};
	if (tempBackTrace.size() > tempStackFrams.size()) {
		stackFrams.swap(tempBackTrace);
		PLS_INFO(MAIN_EXCEPTION, "Main Selected Stack API : CaptureStackBackTrace");
	} else {
		stackFrams.swap(tempStackFrams);
		PLS_INFO(MAIN_EXCEPTION, "Main Selected Stack API : StackWalk");
	}

	for (int i = 0; i < stackFrams.size(); ++i) {
		if (!bFind) {
			bFind = stackFrams[i] == exceptionRip;
		}
		if (bFind) {
			DWORD64 offset;
			std::string moduleName;
			getModuleInfo(stackFrams[i], offset, moduleName);

			char frame[128];
			sprintf_s(frame, "\n %d %s + 0x%x", i, moduleName.c_str(), offset);
			stackFramsStr.append(frame);

			if (topLocation.empty())
				topLocation = moduleName;

			if (findBlacklist)
				continue;

			crashValue = moduleName;
		}
	}

	showCrashExceptionNotice(QString::fromStdString(crashValue));

	//print back stack
	if (stackFramsStr.empty())
		PLS_INFO(MAIN_EXCEPTION, "Not found module. Crash rip : 0x%x", exceptionRip);
	else
		PLS_INFO(MAIN_EXCEPTION, "Main BackTrace : %s", stackFramsStr.c_str());

	return;
}

bool send_to_nelo(char *data, size_t len, std::string crashKey, std::string crashValue, std::string crashType)
{
	QByteArray ba;
	ba.append(data, (int)len);
	QString base64Dump = ba.toBase64();
	//std::string neloUrl = "http://nelo2-col.navercorp.com:80/_store";
	QString win_version = get_windows_version_info();
	QJsonObject neloInfo;
	neloInfo.insert("projectName", PLS_PROJECT_NAME);
	neloInfo.insert("projectVersion", PLS_VERSION);
	neloInfo.insert("prismSession", prismSession.c_str());
	neloInfo.insert("body", "Crash Dump File");

	neloInfo.insert("logLevel", "FATAL");
	neloInfo.insert("logSource", "CrashDump");
	neloInfo.insert("Platform", win_version);
	neloInfo.insert("logType", "nelo2-app");
	neloInfo.insert("UserID", maskingLogUserID.c_str());
	neloInfo.insert("DmpData", base64Dump);
	neloInfo.insert("DmpFormat", "bin");
	neloInfo.insert("DmpSymbol", "Required");
	neloInfo.insert(crashKey.c_str(), crashValue.c_str());
	neloInfo.insert("CrashType", crashType.c_str());
	neloInfo.insert("LogFrom", logFrom.c_str());

	neloInfo.insert("videoAdapter", videoAdapter.c_str());
	neloInfo.insert("cpuName", prism_cpuName.c_str());

	QJsonDocument doc;
	doc.setObject(neloInfo);
	QByteArray postData = doc.toJson(QJsonDocument::Compact);

	bool res = false;
	try {
		const int PORT_NUM = 80;
		WSADATA wData;
		std::ignore = ::WSAStartup(MAKEWORD(2, 2), &wData);

		hostent *phost = gethostbyname("nelo2-col.navercorp.com");
		std::string ip_str = phost ? inet_ntoa(*(in_addr *)phost->h_addr_list[0]) : "";

		SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in ServerAddr = {0};
		ServerAddr.sin_addr.s_addr = inet_addr(ip_str.c_str());
		ServerAddr.sin_port = htons(PORT_NUM);
		ServerAddr.sin_family = AF_INET;
		int errNo = connect(clientSocket, (sockaddr *)&ServerAddr, sizeof(ServerAddr));
		if (errNo == 0) {
			char len[20] = {0};
			sprintf(len, "%zd", postData.toStdString().length());
			std::string strLen = len;

			std::string strSend = "POST /_store HTTP/1.1\n"
					      "Connection: close\n"
					      "Content-Type: application/x-www-form-urlencoded\n"
					      "Host: nelo2-col.navercorp.com:80\n"
					      "Content-Length:";
			strSend = strSend + strLen + "\n\n" + postData.toStdString();

			int iSended = 0;
			for (;;) {
				if (iSended >= strSend.length()) {
					break;
				}
				int iLen = send(clientSocket, &strSend.c_str()[iSended], int(strSend.length() - iSended), 0);
				PLS_INFO(MAIN_EXCEPTION, "send length:%d", iLen);
				if (iLen > 0) {
					iSended += iLen;
				} else {
					break;
				}
			}

			if (iSended > 0) {
				char buf[4096] = {0};
				int iRecved = 0;
				for (;;) {
					if (iRecved >= sizeof(buf) - 1) {
						break;
					}
					int recv_res = recv(clientSocket, buf + iRecved, sizeof(buf) - iRecved - 1, 0);
					if (recv_res == SOCKET_ERROR || recv_res == 0) {
						break;
					} else {
						iRecved += recv_res;
					}
				}
				iRecved = min(iRecved, 4096 - 1);
				buf[iRecved] = 0;
				char *sub_success = strstr(buf, "Success");
				if (!sub_success) {
					PLS_WARN(MAIN_EXCEPTION, "Send dump file to nelo failed");
				} else {
					res = true;
				}
			} else {
				PLS_WARN(MAIN_EXCEPTION, "Send dump file to nelo failed");
			}
		} else {
			errNo = WSAGetLastError();
			PLS_INFO(MAIN_EXCEPTION, "Connect error, error code:%d", errNo);
		}
		closesocket(clientSocket);
		::WSACleanup();
		return res;
	} catch (...) {
		PLS_INFO(MAIN_EXCEPTION, "Exception happned when send dump to nelo");
		return false;
	}
	return false;
}

bool PostDumpToServer(QString strDump, std::string crashKey, std::string crashValue, std::string crashType)
{
	QFile file(strDump);
	if (!file.open(QIODevice::ReadOnly)) {
		PLS_WARN(MAIN_EXCEPTION, "Open dump file failed");
		return false;
	}
	QDataStream in(&file);
	QString strDumpData;
	qint64 len = file.size();
	if (len == 0) {
		PLS_WARN(MAIN_EXCEPTION, "Size of dump file is 0");
		file.close();
		if (!pls_is_dev_server()) {
			file.remove();
		}
		return false;
	}
	char *data = new char[len];
	file.read(data, len);
	if (send_to_nelo(data, len, crashKey, crashValue, crashType)) {
		file.close();
		if (!pls_is_dev_server()) {
			file.remove();
			PLS_INFO(MAIN_EXCEPTION, "Remove dump file : %s", GetFileName(strDump.toStdString()).c_str());
		} else {
			PLS_INFO(MAIN_EXCEPTION, "Do not remove dump file : %s", GetFileName(strDump.toStdString()).c_str());
		}
	} else {
		file.close();
		PLS_INFO(MAIN_EXCEPTION, "Dump file stay in: %s", GetFileName(strDump.toStdString()).c_str());
	}
	delete[] data;
	data = nullptr;
	return true;
}

LONG WINAPI PrismUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers)
{
	PLS_INFO(MAIN_EXCEPTION, "Crash happened");

	pls_crash_flag();
	PLSBlockDump::Instance()->SignExitEvent();

	unsigned length = mem_failed_length();
	if (length > 0) {
		uint64_t memoryUsingMB = os_get_proc_resident_size() / (1024 * 1024);
		char *retryTest = (char *)malloc(length);
		PLS_INFO(MAIN_EXCEPTION, "Crash happenned after failing to request memory for %u bytes while %lluMB memory is being using. Retry result : %s", length, memoryUsingMB,
			 retryTest ? "successed" : "failed");
		if (retryTest) {
			free(retryTest);
			retryTest = NULL;
		}
	}

	SYSTEMTIME st;
	::GetLocalTime(&st);
	QString strUserPath = pls_get_user_path("PRISMLiveStudio\\crashDump\\");
	wchar_t path[MAX_PATH] = {0};
	swprintf_s(path, _T("%s%04d_%02d_%02d_%02d_%02d_%02d_%03d.dmp"), strUserPath.toStdWString().c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	PLS_INFO(MAIN_EXCEPTION, "Crash Dump file will write.");

	bool writedDump = false;
	HANDLE lhDumpFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (lhDumpFile && (lhDumpFile != INVALID_HANDLE_VALUE)) {
		MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
		loExceptionInfo.ExceptionPointers = pExceptionPointers;
		loExceptionInfo.ThreadId = GetCurrentThreadId();
		loExceptionInfo.ClientPointers = TRUE;

		int nDumpType = MiniDumpWithFullMemoryInfo;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), lhDumpFile, (MINIDUMP_TYPE)nDumpType, &loExceptionInfo, NULL, NULL);
		CloseHandle(lhDumpFile);

		writedDump = true;
	} else {
		PLS_INFO(MAIN_EXCEPTION, "Write Crash Dump file failed:(%d)", GetLastError());
	}

	std::string crashKey = "MainGeneralCrash";
	std::string crashValue = "Unknown";
	std::string crashType = "GeneralCrash";

	os_sym_initialize(NULL);
	analyzeException(pExceptionPointers, crashKey, crashValue, crashType);

	if (writedDump) {
		QString dumpPath = QString::fromWCharArray(path);
		PostDumpToServer(dumpPath, crashKey, crashValue, crashType);
	}

	removeChildProcessCrashFile();
	os_sym_cleanup();
	os_mutex_handle_close(crashFileMutexUuid.c_str());
	return EXCEPTION_EXECUTE_HANDLER;
}

int pls_auto_dump()
{
	SetUnhandledExceptionFilter(PrismUnhandledExceptionFilter);
	return 0;
}
