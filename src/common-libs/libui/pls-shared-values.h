#pragma once
#include <qstring.h>

#include "libui-globals.h"

enum class Progress {
	launchPrism = 0,
	loadLocalized = 5,
	initTheme = 10,
	loadUserDir = 12,
	loadUserData = 15,
	loadMainData = 20,
	initCore = 30,
	loadMainView = 38,
	initAudio = 45,
	initVideo = 50,
	loadPlugin = 70,
	loadSourceStart = 70,
	loadSourceEnd = 98,
	loadComplete = 100,
	prismUpAndRunning = 200
};

/**
 * if want to translate code to string must add string in pls_get_init_exit_code_str method.
 * 0x1001 - 0x1100: is init failed code.  the launcher will not restart prism but show init failed alert.
 * 0x1100 - 0x1200: this is prism or launcher closed self or force kill by self. the launcher will not restart prism and not show failed alert. 
 */
enum class init_exception_code {
	common = 0x1001,
	engine_not_support = 0x1002,
	engine_param_error = 0x1003,
	engine_not_support_dx_version = 0x1004,
	failed_init_global_config = 0x1005,
	failed_create_profile_directory = 0x1006,
	failed_find_locale_file = 0x1007,
	failed_open_locale_file = 0x1008,
	failed_load_theme = 0x1009,
	failed_load_locale = 0x1010,
	failed_init_application_bunndle = 0x1011,
	failed_create_required_user_directory = 0x1012,
	failed_unknow_exception = 0x1013,
	failed_fasoo_reason = 0x1014,
	//add more value here
	need_init_alert_max = 0x1100,

	file_not_found = 0x1101,
	permission_denied = 0x1102,
	prism_already_running = 0x1103,
	launcher_already_running = 0x1104,
	prism_not_start_by_launcher = 0x1105,
	failed_startup_obs = 0x1106,
	timeout_by_startup_30s = 0x1107,
	timeout_by_encoder = 0x1108,
	timeout_by_source = 0x1109,
	launcher_kill_old_prism = 0x1110,
	prism_kill_old_launcher = 0x1111,
	//add more value here
	force_kill_max = 0x1200,
	// since obs engine error, PRISM process has to exit automatically.
	obs_engine_error = 0x2000,

};

namespace shared_values {

LIBUI_API extern const QString errorTitle;
LIBUI_API extern const QString errorContent;
LIBUI_API extern const QString errorTime;

LIBUI_API extern const QString k_launcher_command_type;
LIBUI_API extern const QString k_launcher_command_normal;
LIBUI_API extern const QString k_launcher_command_restart;
LIBUI_API extern const QString k_launcher_command_update;
LIBUI_API extern const QString k_launcher_command_launcher;
LIBUI_API extern const QString k_launcher_command_update_file;
LIBUI_API extern const QString k_launcher_command_update_version;
LIBUI_API extern const QString k_launcher_command_update_gcc;
LIBUI_API extern const QString k_launcher_command_log_prism_session;
LIBUI_API extern const QString k_launcher_command_running_path;
LIBUI_API extern const QString k_launcher_command_log_sub_prism_session;
LIBUI_API extern const QString prism_user_image_path;
LIBUI_API extern const int k_task_crash_return_code;
LIBUI_API extern const QString k_launcher_command_restarted_session;
LIBUI_API extern const QString k_launcher_command_log_process_ids;
LIBUI_API extern const QString k_launcher_command_prism_pid;
}
