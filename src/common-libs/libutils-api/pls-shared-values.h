#pragma once
#include <qstring.h>
#include <QObject>

#include "libutils-export.h"

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

class PLSCode {
	Q_GADGET
public:
	enum class init_exception_code {

	//start progress exit code
#if defined(_WIN32)
		init_code_min = 0x1000,
#else
		init_code_min = 100,
#endif

		common,
		engine_not_support,
		engine_param_error,
		engine_not_support_dx_version,
		failed_init_global_config,
		failed_create_profile_directory,
		failed_find_locale_file,
		failed_open_locale_file,
		failed_load_theme,
		failed_load_locale,
		failed_init_application_bunndle,
		failed_create_required_user_directory,
		failed_unknow_exception,
		failed_fasoo_reason,
		failed_startup_obs,
		prism_already_running,
	//add more value here

#if defined(_WIN32)
		init_code_max = 0x1100,
#else
		init_code_max = 150,
#endif

		running_exit_code_min = init_code_max,

		// running state exit code
		timeout_by_encoder,
		timeout_by_source,
		timeout_by_shutdown,
	//add more value here

#if defined(_WIN32)
		running_exit_code_max = 0x1200,
#else
		running_exit_code_max = 200,
#endif

		// fatal error, PRISM process has to exit automatically.
		// daemon procee need reopen PRISM
		obs_engine_error,
		crashed_exit_code
	};
	Q_ENUM(init_exception_code)
};

using init_exception_code = PLSCode::init_exception_code;
LIBUTILSAPI_API bool pls_is_init_exit_code(init_exception_code code);
LIBUTILSAPI_API QString pls_get_init_exit_code_str(init_exception_code code);
LIBUTILSAPI_API bool pls_is_repull_exit_code(int exitCode);

namespace shared_values {

LIBUTILSAPI_API extern const QString errorTitle;
LIBUTILSAPI_API extern const QString errorContent;
LIBUTILSAPI_API extern const QString errorTime;

LIBUTILSAPI_API extern const QString k_launcher_command_type;
LIBUTILSAPI_API extern const QString k_launcher_command_normal;
LIBUTILSAPI_API extern const QString k_launcher_command_restart;
LIBUTILSAPI_API extern const QString k_launcher_command_update;
LIBUTILSAPI_API extern const QString k_launcher_command_launcher;
LIBUTILSAPI_API extern const QString k_launcher_command_daemon;
LIBUTILSAPI_API extern const QString k_launcher_command_update_file;
LIBUTILSAPI_API extern const QString k_launcher_command_update_version;
LIBUTILSAPI_API extern const QString k_launcher_command_update_gcc;
LIBUTILSAPI_API extern const QString k_launcher_command_log_prism_session;
LIBUTILSAPI_API extern const QString k_launcher_command_running_path;
LIBUTILSAPI_API extern const QString k_launcher_command_log_sub_prism_session;
LIBUTILSAPI_API extern const QString prism_user_image_path;
LIBUTILSAPI_API extern const int k_task_crash_return_code;
LIBUTILSAPI_API extern const QString k_launcher_command_restarted_session;
LIBUTILSAPI_API extern const QString k_launcher_command_log_process_ids;
LIBUTILSAPI_API extern const QString k_launcher_command_prism_pid;
LIBUTILSAPI_API extern const QString k_launcher_prism_version;
LIBUTILSAPI_API extern const QString k_daemon_limit_retry_count;
LIBUTILSAPI_API extern const QString k_daemon_sm_key;
LIBUTILSAPI_API extern const QString k_daemon_parent_is_debugger;
}
