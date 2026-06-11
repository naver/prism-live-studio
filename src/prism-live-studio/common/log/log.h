#if !defined(_PRISM_COMMON_LIBLOG_LOG_LOG_H)
#define _PRISM_COMMON_LIBLOG_LOG_LOG_H

#include <liblog.h>

#if defined(_PRISM_PLUGIN)
/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_log(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_log(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_log(kr, log_level, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_logex(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, int arg_count, const char *format, ...);
template<typename... Args>
void pls_logex(bool kr, pls_log_level_t log_level, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, const char *format, Args &&...args)
{
	pls_logex(kr, log_level, file_name, file_line, fields, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print error log
  * param:
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_error(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_error(bool kr, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_error(kr, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print warn log
  * param:
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_warn(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_warn(bool kr, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_warn(kr, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print info log
  * param:
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_info(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_info(bool kr, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_info(kr, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print debug log
  * param:
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_debug(bool kr, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_debug(bool kr, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_debug(kr, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print ui step log
  * param:
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
void pls_ui_step(bool kr, const char *controls, const char *action, const char *file_name, int file_line);

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_LOG(log_level, format, ...) pls_log(false, log_level, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define PLS_PLUGIN_LOG_KR(log_level, format, ...) pls_log(true, log_level, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] field_count: custom nelo log field count
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_LOGEX(log_level, fields, format, ...) pls_logex(false, log_level, __FILE__, __LINE__, fields, format, ##__VA_ARGS__)
#define PLS_PLUGIN_LOGEX_KR(log_level, fields, format, ...) pls_logex(true, log_level, __FILE__, __LINE__, fields, format, ##__VA_ARGS__)

/**
  * print error log
  * param:
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_ERROR(format, ...) pls_error(false, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define PLS_PLUGIN_ERROR_KR(format, ...) pls_error(true, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_WARN(format, ...) pls_warn(false, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define PLS_PLUGIN_WARN_KR(format, ...) pls_warn(true, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_INFO(format, ...) pls_info(false, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define PLS_PLUGIN_INFO_KR(format, ...) pls_info(true, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
  * print debug log
  * param:
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_PLUGIN_DEBUG(format, ...) pls_debug(false, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define PLS_PLUGIN_DEBUG_KR(format, ...) pls_debug(true, __FILE__, __LINE__, format, ##__VA_ARGS__)

/**
  * print ui step log
  * param:
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_PLUGIN_UI_STEP(controls, action) pls_ui_step(false, controls, action, __FILE__, __LINE__)
#define PLS_PLUGIN_UI_STEP_KR(controls, action) pls_ui_step(true, controls, action, __FILE__, __LINE__)
#endif // _PRISM_PLUGIN

#endif // _PRISM_COMMON_LIBLOG_LOG_LOG_H
