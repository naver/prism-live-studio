#include "pls-shared-values.h"

namespace shared_values {

LIBUI_API const QString errorContent = "context";
LIBUI_API const QString errorTitle = "location";
LIBUI_API const QString errorTime = "timestamp";

LIBUI_API const QString k_launcher_command_type = "--from-type=";
LIBUI_API const QString k_launcher_command_normal = "normal-start";
LIBUI_API const QString k_launcher_command_restart = "restart";
LIBUI_API const QString k_launcher_command_update = "update";
LIBUI_API const QString k_launcher_command_launcher = "launcher";

LIBUI_API const QString k_launcher_command_update_file = "--update-file-url=";
LIBUI_API const QString k_launcher_command_update_version = "--update--version=";
LIBUI_API const QString k_launcher_command_update_gcc = "--gcc=";
LIBUI_API const QString k_launcher_command_log_prism_session = "--log-prism-session=";
LIBUI_API const QString k_launcher_command_running_path = "--running-path=";
LIBUI_API const QString k_launcher_command_log_sub_prism_session = "--log-prism-sub-session=";
LIBUI_API extern const QString prism_user_image_path = "user/prismThumbnail.png";
LIBUI_API const int k_task_crash_return_code = 2;
LIBUI_API const QString k_launcher_command_restarted_session = "--log-restarted-prism-session=";
LIBUI_API const QString k_launcher_command_log_process_ids = "--log-process-ids=";
LIBUI_API const QString k_launcher_command_prism_pid = "--prism-pid=";
}
