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

#pragma once

#include <QApplication>
#include <QTranslator>
#include <QPointer>
#include <QFileSystemWatcher>
#ifndef _WIN32
#include <QSocketNotifier>
#else
#include <QSessionManager>
#endif
#include <obs.hpp>
#include <util/lexer.h>
#include <util/profiler.h>
#include <util/profiler.hpp>
#include <util/util.hpp>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <deque>
#include <filesystem>

#include <PLSUIApp.h>

#include "window-main.hpp"
#include "PLSMainView.hpp"

static const char *run_program_init = "run_program_init";

std::string CurrentTimeString();
std::string CurrentDateTimeString();
std::string GenerateTimeDateFilename(const char *extension, bool noSpace = false);
std::string GenerateSpecifiedFilename(const char *extension, bool noSpace, const char *format);
std::string GetFormatString(const char *format, const char *prefix, const char *suffix);
std::string GetFormatExt(const char *container);
std::string GetOutputFilename(const char *path, const char *container, bool noSpace, bool overwrite,
			      const char *format);
QObject *CreateShortcutFilter(QObject *parent);

struct BaseLexer {
	lexer lex;

public:
	inline BaseLexer() { lexer_init(&lex); }
	inline ~BaseLexer() { lexer_free(&lex); }
	operator lexer *() { return &lex; }
};

class OBSTranslator : public QTranslator {
	Q_OBJECT

public:
	virtual bool isEmpty() const override { return false; }

	virtual QString translate(const char *context, const char *sourceText, const char *disambiguation,
				  int n) const override;
};

typedef std::function<void()> VoidFunc;

struct UpdateBranch {
	QString name;
	QString display_name;
	QString description;
	bool is_enabled;
	bool is_visible;
};
class OBSLogViewer;
struct OBSStudioAPI;
class PLSLoginMainView;

class OBSApp : public PLSUiApp {
	Q_OBJECT

private:
	std::string locale;

	ConfigFile appConfig;
	TextLookup textLookup;
	QPointer<PLSMainView> mainView;
	QPointer<OBSMainWindow> mainWindow;
	profiler_name_store_t *profilerNameStore = nullptr;
	std::vector<UpdateBranch> updateBranches;
	bool branches_loaded = false;

	bool libobs_initialized = false;

	os_inhibit_t *sleepInhibitor = nullptr;
	int sleepInhibitRefs = 0;

	bool enableHotkeysInFocus = true;
	bool enableHotkeysOutOfFocus = true;
	QString appRunningPath;

	std::deque<obs_frontend_translate_ui_cb> translatorHooks;
	bool hotkeyEnable = true;

	friend struct OBSStudioAPI;

	bool UpdatePre22MultiviewLayout(const char *layout);

	bool InitGlobalConfig();
	bool InitGlobalConfigDefaults();
	bool InitGlobalLocationDefaults();

	bool MigrateGlobalSettings();
	void MigrateLegacySettings(uint32_t lastVersion);

	void InitUserConfigDefaults();

	bool InitLocale();
	bool HotkeyEnable() const;
	inline void ResetHotkeyState(bool inFocus);

	QPalette defaultPalette;

protected:
	bool notify(QObject *receiver, QEvent *e) override;

#ifndef _WIN32
	static int sigintFd[2];
	QSocketNotifier *snInt = nullptr;
#else
private slots:
	void commitData(QSessionManager &manager);
#endif

public:
	OBSApp(int &argc, char **argv, profiler_name_store_t *store);
	~OBSApp();

	void AppInit();
	bool OBSInit();
	Q_INVOKABLE void UpdateHotkeyFocusSetting(bool reset = true);
	Q_INVOKABLE void DisableHotkeys();

	inline bool HotkeysEnabledInFocus() const { return enableHotkeysInFocus; }

	inline PLSMainView *getMainView() const { return mainView.data(); }
	inline QMainWindow *GetMainWindow() const { return mainWindow.data(); }
	inline config_t *GlobalConfig() const { return appConfig; }
	inline config_t *GetAppConfig() const { return appConfig; }
	inline config_t *GetUserConfig() const { return appConfig; }
	std::filesystem::path userConfigLocation;
	std::filesystem::path userScenesLocation;
	std::filesystem::path userProfilesLocation;

	inline const char *GetLocale() const { return locale.c_str(); }

	bool IsThemeDark() const { return false; }

	void SetBranchData(const std::string &data);
	std::vector<UpdateBranch> GetBranches();

	inline lookup_t *GetTextLookup() const { return textLookup; }

	inline const char *GetString(const char *lookupVal) const { return textLookup.GetString(lookupVal); }

	bool TranslateString(const char *lookupVal, const char **out) const;

	profiler_name_store_t *GetProfilerNameStore() const { return profilerNameStore; }

	const char *GetLastLog() const;
	const char *GetCurrentLog() const;

	const char *GetLastCrashLog() const;

	std::string GetVersionString(bool platform = true) const;
	bool IsPortableMode();
	bool IsUpdaterDisabled();
	bool IsMissingFilesCheckDisabled();

	const char *InputAudioSource() const;
	const char *OutputAudioSource() const;

	const char *GetRenderModule() const;

	inline QString getAppRunningPath() const { return appRunningPath; }
	inline void setAppRunningPath(const QString &appRunningPath_) { appRunningPath = appRunningPath_; }

	inline void IncrementSleepInhibition()
	{
		if (!sleepInhibitor)
			return;
		if (sleepInhibitRefs++ == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, true);
	}

	inline void DecrementSleepInhibition()
	{
		if (!sleepInhibitor)
			return;
		if (sleepInhibitRefs == 0)
			return;
		if (--sleepInhibitRefs == 0)
			os_inhibit_sleep_set_active(sleepInhibitor, false);
	}

	inline void PushUITranslation(obs_frontend_translate_ui_cb cb) { translatorHooks.emplace_front(cb); }

	inline void PopUITranslation() { translatorHooks.pop_front(); }
#ifndef _WIN32
	static void SigIntSignalHandler(int);
#endif
	static void deleteOldestFile(bool has_prefix, const char *location);

public slots:
	void Exec(VoidFunc func);
	void ProcessSigInt();

signals:
	void HotKeyEnabled(bool);
};

int GetAppConfigPath(char *path, size_t size, const char *name);
char *GetAppConfigPathPtr(const char *name);

int GetProgramDataPath(char *path, size_t size, const char *name);
char *GetProgramDataPathPtr(const char *name);

inline OBSApp *App()
{
	return static_cast<OBSApp *>(qApp);
}

std::vector<std::pair<std::string, std::string>> GetLocaleNames();
inline const char *Str(const char *lookup)
{
	return App()->GetString(lookup);
}
inline QString QTStr(const char *lookupVal)
{
	return QString::fromUtf8(Str(lookupVal));
}

bool GetFileSafeName(const char *name, std::string &file);
bool GetClosestUnusedFileName(std::string &path, const char *extension);
bool GetUnusedSceneCollectionFile(std::string &name, std::string &file);

bool WindowPositionValid(QRect rect);

static inline int GetProfilePath(char *path, size_t size, const char *file)
{
	OBSMainWindow *window = reinterpret_cast<OBSMainWindow *>(App()->GetMainWindow());
	return window->GetProfilePath(path, size, file);
}

struct GlobalVars {
	static bool isLogined;
	static std::chrono::steady_clock::time_point startTime;
	static std::string prismSession;
	static std::string prismSubSession;
	static std::string videoAdapter;
	static std::string crashFileMutexUuid;

	static bool portable_mode;
	static bool steam;
	static bool safe_mode;
	static bool disable_3p_plugins;
	static bool restart;
	static bool restart_safe;
	static QStringList arguments;

	static bool remuxAfterRecord;
	static std::string remuxFilename;
	static std::string logUserID;
	static std::string maskingLogUserID;
	static bool opt_start_streaming;
	static bool opt_start_recording;
	static bool opt_start_replaybuffer;
	static bool opt_start_virtualcam;
	static bool opt_disable_updater;
	static bool opt_disable_missing_files_check;
	static bool opt_minimize_tray;
	static bool opt_studio_mode;
	static bool opt_allow_opengl;
	static bool opt_always_on_top;
	static std::string opt_starting_scene;
	static std::string gcc;
	static QStringList gpuNames;
	static QPointer<OBSLogViewer> obsLogViewer;

	static bool isStartByDaemon;
	static bool g_bUseAPIServer;

#ifdef ENABLE_TEST
	static bool unitTest;
	static int unitTestExitCode;
#endif
};

#ifdef _WIN32
extern "C" void install_dll_blocklist_hook(void);
extern "C" void log_blocked_dlls(void);
#endif
