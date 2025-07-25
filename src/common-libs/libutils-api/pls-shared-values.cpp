#include "pls-shared-values.h"
#include "QVariant"

LIBUTILSAPI_API bool pls_is_init_exit_code(PLSCode::init_exception_code code)
{
	if (PLSCode::init_exception_code::init_code_min < code && code < PLSCode::init_exception_code::init_code_max)
		return true;
	else
		return false;
}

LIBUTILSAPI_API QString pls_get_init_exit_code_str(init_exception_code code)
{
	return QVariant::fromValue(code).toString();
}

LIBUTILSAPI_API bool pls_is_repull_exit_code(int exitCode)
{
	if (exitCode == 0) {
		return false;
	}

#if __APPLE__
	// if main process is killed by force (but not crashed), don't restart it.
	if (exitCode == SIGKILL) {
		return false;
	}
#endif

	// (init_code_min, running_exit_code_max) will not re-pull prismlivestudio
	if (static_cast<int>(init_exception_code::init_code_min) < exitCode && exitCode < static_cast<int>(init_exception_code::running_exit_code_max)) {
		return false;
	}

	return true;
}

namespace shared_values {

LIBUTILSAPI_API const QString errorContent = "context";
LIBUTILSAPI_API const QString errorTitle = "location";
LIBUTILSAPI_API const QString errorTime = "timestamp";

LIBUTILSAPI_API const QString k_launcher_command_type = "--from-type=";
LIBUTILSAPI_API const QString k_launcher_command_normal = "normal-start";
LIBUTILSAPI_API const QString k_launcher_command_restart = "restart";
LIBUTILSAPI_API const QString k_launcher_command_update = "update";
LIBUTILSAPI_API const QString k_launcher_command_launcher = "launcher";
LIBUTILSAPI_API const QString k_launcher_command_daemon = "daemon";

LIBUTILSAPI_API const QString k_launcher_command_update_file = "--update-file-url=";
LIBUTILSAPI_API const QString k_launcher_command_update_version = "--update--version=";
LIBUTILSAPI_API const QString k_launcher_command_update_gcc = "--gcc=";
LIBUTILSAPI_API const QString k_launcher_command_log_prism_session = "--log-prism-session=";
LIBUTILSAPI_API const QString k_launcher_command_running_path = "--running-path=";
LIBUTILSAPI_API const QString k_launcher_command_log_sub_prism_session = "--log-prism-sub-session=";
LIBUTILSAPI_API extern const QString prism_user_image_path = "user/prismThumbnail.png";
LIBUTILSAPI_API const int k_task_crash_return_code = 2;
LIBUTILSAPI_API const QString k_launcher_command_restarted_session = "--log-restarted-prism-session=";
LIBUTILSAPI_API const QString k_launcher_command_log_process_ids = "--log-process-ids=";
LIBUTILSAPI_API const QString k_launcher_command_prism_pid = "--prism-pid=";
LIBUTILSAPI_API const QString k_launcher_prism_version = "--prism-version=";
LIBUTILSAPI_API const QString k_daemon_limit_retry_count = "--prism-daemon-limit-retry-count=";
LIBUTILSAPI_API const QString k_daemon_parent_is_debugger = "--prism-daemon-parent-is-debugger";

LIBUTILSAPI_API const QString k_daemon_sm_key = "prism_daemon_exit_key";
}
