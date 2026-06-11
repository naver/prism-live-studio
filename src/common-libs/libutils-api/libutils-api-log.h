#pragma once

#include "libutils-export.h"

#include <vector>
#include <utility>

namespace pls {

using log_cb_t = void (*)(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...);
using logex_cb_t = void (*)(bool kr, int log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, int arg_count,
			    const char *format, ...);

LIBUTILSAPI_API void set_log_levels(int error_lvl, int warn_lvl, int info_lvl, int debug_lvl);
LIBUTILSAPI_API int log_error_lvl();
LIBUTILSAPI_API int log_warn_lvl();
LIBUTILSAPI_API int log_info_lvl();
LIBUTILSAPI_API int log_debug_lvl();

LIBUTILSAPI_API void set_log_cbs(log_cb_t error_cb, log_cb_t warn_cb, log_cb_t info_cb, log_cb_t debug_cb, logex_cb_t logex_cb);
LIBUTILSAPI_API log_cb_t log_error_cb();
LIBUTILSAPI_API log_cb_t log_warn_cb();
LIBUTILSAPI_API log_cb_t log_info_cb();
LIBUTILSAPI_API log_cb_t log_debug_cb();
LIBUTILSAPI_API logex_cb_t logex_cb();

template<typename... Args> void log_error(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	if (auto cb = log_error_cb(); cb) {
		cb(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
	}
}
template<typename... Args> void log_warn(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	if (auto cb = log_warn_cb(); cb) {
		cb(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
	}
}
template<typename... Args> void log_info(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	if (auto cb = log_info_cb(); cb) {
		cb(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
	}
}
template<typename... Args> void log_debug(bool kr, const char *module_name, const char *file_name, int file_line, const char *format, Args &&...args)
{
	if (auto cb = log_debug_cb(); cb) {
		cb(kr, module_name, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
	}
}
template<typename... Args>
void logex(bool kr, int log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, const char *format, Args &&...args)
{
	if (auto cb = logex_cb(); cb) {
		cb(kr, log_level, module_name, file_name, file_line, fields, sizeof...(Args), format, std::forward<Args>(args)...);
	}
}

}

#ifdef LIBUTILSAPI_LIB

/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_ERROR(module_name, format, ...) pls::log_error(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_ERROR_KR(module_name, format, ...) pls::log_error(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_WARN(module_name, format, ...) pls::log_warn(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_WARN_KR(module_name, format, ...) pls::log_warn(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INFO(module_name, format, ...) pls::log_info(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INFO_KR(module_name, format, ...) pls::log_info(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_DEBUG(module_name, format, ...) pls::log_debug(false, module_name, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_DEBUG_KR(module_name, format, ...) pls::log_debug(true, module_name, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] fields: custom nelo log field, example: {{"key1", "value1"}, {"key2", "value2"}}
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LOGEX(log_level, module_name, fields, format, ...) pls::logex(false, log_level, module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)
#define PLS_LOGEX_KR(log_level, module_name, fields, format, ...) pls::logex(true, log_level, module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)

#define PLS_ERROR_LOGEX(module_name, fields, format, ...) pls::logex(false, pls::log_error_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)
#define PLS_ERROR_LOGEX_KR(module_name, fields, format, ...) pls::logex(true, pls::log_error_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)

#define PLS_WARN_LOGEX(module_name, fields, format, ...) pls::logex(false, pls::log_warn_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)
#define PLS_WARN_LOGEX_KR(module_name, fields, format, ...) pls::logex(true, pls::log_warn_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)

#define PLS_INFO_LOGEX(module_name, fields, format, ...) pls::logex(false, pls::log_info_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)
#define PLS_INFO_LOGEX_KR(module_name, fields, format, ...) pls::logex(true, pls::log_info_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)

#define PLS_DEBUG_LOGEX(module_name, fields, format, ...) pls::logex(false, pls::log_debug_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)
#define PLS_DEBUG_LOGEX_KR(module_name, fields, format, ...) pls::logex(true, pls::log_debug_lvl(), module_name, __FILE__, __LINE__, fields, format, __VA_ARGS__)

#endif
