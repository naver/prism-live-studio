#if !defined(_PRISM_COMMON_LIBLOG_LIBLOG_LIBLOG_H)
#define _PRISM_COMMON_LIBLOG_LIBLOG_LIBLOG_H

#include <stdarg.h>
#include <stdint.h>
#include <utility>

#ifdef LIBLOG_LIB
#define LIBLOG_API extern "C" __declspec(dllexport)
#else
#define LIBLOG_API extern "C" __declspec(dllimport)
#endif

enum pls_log_level_t {
	PLS_LOG_ERROR, // error
	PLS_LOG_WARN,  // warning
	PLS_LOG_INFO,  // information
	PLS_LOG_DEBUG  // debug
};

struct pls_time_t {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int milliseconds;
	int timezone;
};

enum pls_set_tag_t {
	PLS_SET_TAG_ALL, // all
	PLS_SET_TAG_CN,  // cn log
	PLS_SET_TAG_KR   // kr log
};

/**
  * log handler
  * param:
  *     [in] kr: kr
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] time: log time
  *     [in] tid: thread id
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] message: message string
  *     [in] param:  transfer pls_set_log_handler second param
  */
typedef void (*pls_log_handler_t)(bool kr, pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *message,
				  void *param);

/**
  * action log handler
  * param:
  *     [in] kr: kr
  *     [in] module_name: module name
  *     [in] time: log time
  *     [in] tid: thread id
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] param:  transfer pls_set_action_log_handler second param
  */
typedef void (*pls_action_log_handler_t)(bool kr, const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line,
					 void *param);
/**
  * ui step log handler
  * param:
  *     [in] module_name: module name
  *     [in] time: log time
  *     [in] tid: thread id
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] param:  transfer pls_set_ui_step_log_handler second param
  */
typedef void (*pls_ui_step_log_handler_t)(const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line, void *param);

/**
  * log initialize, params for nelo log initialization
  * param:
  *     [in] project_name: project name
  *     [in] project_name_kr: kr project name
  *     [in] project_version: project version
  *     [in] log_source: log source
  * return:
  *     true for success, false for failed
  */
LIBLOG_API void pls_log_init(const char *project_name, const char *project_name_kr, const char *project_version, const char *log_source);
/**
  * log cleanup
  */
LIBLOG_API void pls_log_cleanup();

/**
  * set custom log handler
  * param:
  *     [in] handler: log handler
  *     [in] param: extern param
  */
LIBLOG_API void pls_set_log_handler(pls_log_handler_t handler, void *param);
/**
  * get log handler
  * param:
  *     [out] handler: log handler
  *     [out] param: extern param
  */
LIBLOG_API void pls_get_log_handler(pls_log_handler_t *handler, void **param);
/**
  * reset log handler
  */
LIBLOG_API void pls_reset_log_handler();

/**
  * set custom action log handler
  * param:
  *     [in] handler: action log handler
  *     [in] param: extern param
  */
LIBLOG_API void pls_set_action_log_handler(pls_action_log_handler_t handler, void *param);
/**
  * get action log handler
  * param:
  *     [out] handler: action log handler
  *     [out] param: extern param
  */
LIBLOG_API void pls_get_action_log_handler(pls_action_log_handler_t *handler, void **param);
/**
  * reset action log handler
  */
LIBLOG_API void pls_reset_action_log_handler();

/**
  * set nelo log user id
  * param:
  *     [in] user_id: log user id
  *     [in] set_tag: set tag
  */
LIBLOG_API void pls_set_user_id(const char *user_id, pls_set_tag_t set_tag = PLS_SET_TAG_ALL);

/**
  * add nelo log global field
  * param:
  *     [in] key: field name
  *     [in] value: field value
  *     [in] set_tag: set tag
  * return:
  *     true for success, false for failed
  */
LIBLOG_API void pls_add_global_field(const char *key, const char *value, pls_set_tag_t set_tag = PLS_SET_TAG_ALL);

/**
  * remove nelo log global field
  * param:
  *     [in] key: field name
  *     [in] set_tag: set tag
  */
LIBLOG_API void pls_remove_global_field(const char *key, pls_set_tag_t set_tag = PLS_SET_TAG_ALL);
/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] args: variadic params
  */
LIBLOG_API void pls_logva(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, va_list args);

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] field_count: custom nelo log field count
  *     [in] format: format string
  *     [in] args: variadic params
  */
LIBLOG_API void pls_logvaex(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *fields[][2], int field_count, int arg_count,
			    const char *format, va_list args);

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_log(kr, log_level, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] field_count: custom nelo log field count
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_logex(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *fields[][2], int field_count, int arg_count,
			  const char *format, ...);
template<typename... Args>
void pls_logex(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *fields[][2], int field_count, const char *format, Args &&...args)
{
	pls_logex(kr, log_level, module_name, file_name, file_line, fields, field_count, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_error(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_error(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_error(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_warn(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_warn(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_warn(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_info(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_info(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_info(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
LIBLOG_API void pls_debug(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_debug(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_debug(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print action log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
LIBLOG_API void pls_action_log(bool kr, const char *module_name, const char *controls, const char *action, const char *file_name, int file_line);
/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
LIBLOG_API void pls_ui_step(bool kr, const char *module_name, const char *controls, const char *action, const char *file_name, int file_line);
/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] field_count: custom nelo log field count
  */
LIBLOG_API void pls_ui_stepex(bool kr, const char *module_name, const char *controls, const char *action, const char *file_name, int file_line, const char *fields[][2], int field_count);

/**
  * send a flag to determine crash or disappear
  * param:
  */
LIBLOG_API void pls_crash_flag();

LIBLOG_API void pls_subprocess_disappear(const char *process, const char *pid, const char *src);

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LOG(log_level, module_name, format, ...) pls_log(false, log_level, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LOG_KR(log_level, module_name, format, ...) pls_log(true, log_level, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] field_count: custom nelo log field count
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LOGEX(log_level, module_name, fields, field_count, format, ...) pls_logex(false, log_level, module_name, __FILE__, __LINE__, fields, field_count, format, __VA_ARGS__)
#define PLS_LOGEX_KR(log_level, module_name, fields, field_count, format, ...) pls_logex(true, log_level, module_name, __FILE__, __LINE__, fields, field_count, format, __VA_ARGS__)

/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_ERROR(module_name, format, ...) pls_error(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_ERROR_KR(module_name, format, ...) pls_error(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_WARN(module_name, format, ...) pls_warn(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_WARN_KR(module_name, format, ...) pls_warn(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INFO(module_name, format, ...) pls_info(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INFO_KR(module_name, format, ...) pls_info(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_DEBUG(module_name, format, ...) pls_debug(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_DEBUG_KR(module_name, format, ...) pls_debug(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print action log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_ACTION_LOG(module_name, controls, action) pls_action_log(false, module_name, controls, action, __FILE__, __LINE__)
#define PLS_ACTION_LOG_KR(module_name, controls, action) pls_action_log(true, module_name, controls, action, __FILE__, __LINE__)

/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_UI_STEP(module_name, controls, action) pls_ui_step(false, module_name, controls, action, __FILE__, __LINE__)
#define PLS_UI_STEP_KR(module_name, controls, action) pls_ui_step(true, module_name, controls, action, __FILE__, __LINE__)

#endif // _PRISM_COMMON_LIBLOG_LIBLOG_LIBLOG_H
