#pragma once

#include <liblog.h>
#include <action.h>
#include <util/base.h>
#include <chrono>

#include "module_names.h"

bool log_init(const char *session_id, const std::chrono::steady_clock::time_point &startTime, const char *subsession_id = nullptr);
void log_cleanup();

void set_log_handler(log_handler_t handler, void *param);

class QNetworkRequest;
void runtime_stats(pls_runtime_stats_type_t runtime_stats_type, const std::chrono::steady_clock::time_point &time);
void runtime_stats(pls_runtime_stats_type_t runtime_stats_type, const std::chrono::steady_clock::time_point &time, const QNetworkRequest &req);

/**
  * print log
  * param:
  *     [in] kr: kr
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] flow: flow
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_flow_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args> void pls_flow_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_flow_log(kr, log_level, module_name, flow, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print log
  * param:
  *     [in] kr: kr
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] flow: flow
  *	[in] video_seq: video sequence
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_flow2_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *file_name, int file_line, int arg_count, const char *format, ...);
template<typename... Args>
void pls_flow2_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *file_name, int file_line, const char *format, Args &&...args)
{
	pls_flow2_log(kr, log_level, module_name, flow, video_seq, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print log
  * param:
  *     [in] kr: kr
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] flow: flow
  *	[in] video_seq: video sequence
  * *	[in] other: other flow
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_flow3_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *otherFlow, const char *file_name, int file_line, int arg_count,
		   const char *format, ...);
template<typename... Args>
void pls_flow3_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *otherFlow, const char *file_name, int file_line,
		   const char *format, Args &&...args)
{
	pls_flow3_log(kr, log_level, module_name, flow, video_seq, otherFlow, file_name, file_line, sizeof...(Args), format, std::forward<Args>(args)...);
}

/**
  * print ui step log
  * param:
  *     [in] kr: kr
  *     [in] module_name: module name
  *     [in] flow: flow
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
void pls_flow_ui(bool kr, const char *module_name, const char *flow, const char *controls, const char *action, const char *file_name, int file_line);

/**
  * print ui step log
  * param:
  *     [in] kr: kr
  *     [in] module_name: module name
  *     [in] flow: flow
  *	[in] video_seq: video sequence
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
void pls_flow2_ui(bool kr, const char *module_name, const char *flow, const char *video_seq, const char *controls, const char *action, const char *file_name, int file_line);

#if defined(Q_OS_WIN)
/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_ERROR(module_name, format, ...) pls_flow_log(false, PLS_LOG_ERROR, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INIT_ERROR_KR(module_name, format, ...) pls_flow_log(true, PLS_LOG_ERROR, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_WARN(module_name, format, ...) pls_flow_log(false, PLS_LOG_WARN, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INIT_WARN_KR(module_name, format, ...) pls_flow_log(true, PLS_LOG_WARN, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_INFO(module_name, format, ...) pls_flow_log(false, PLS_LOG_INFO, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INIT_INFO_KR(module_name, format, ...) pls_flow_log(true, PLS_LOG_INFO, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_DEBUG(module_name, format, ...) pls_flow_log(false, PLS_LOG_DEBUG, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_INIT_DEBUG_KR(module_name, format, ...) pls_flow_log(true, PLS_LOG_DEBUG, module_name, "AppInit", __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_INIT_UI_STEP(module_name, controls, action) pls_flow_ui(false, module_name, "AppInit", controls, action, __FILE__, __LINE__)
#define PLS_INIT_UI_STEP_KR(module_name, controls, action) pls_flow_ui(true, module_name, "AppInit", controls, action, __FILE__, __LINE__)

/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_ERROR(module_name, field, format, ...) pls_flow_log(false, PLS_LOG_ERROR, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_CHAT_ERROR_KR(module_name, field, format, ...) pls_flow_log(true, PLS_LOG_ERROR, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_WARN(module_name, field, format, ...) pls_flow_log(false, PLS_LOG_WARN, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_CHAT_WARN_KR(module_name, field, format, ...) pls_flow_log(true, PLS_LOG_WARN, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_INFO(module_name, field, format, ...) pls_flow_log(false, PLS_LOG_INFO, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_CHAT_INFO_KR(module_name, field, format, ...) pls_flow_log(true, PLS_LOG_INFO, module_name, field, __FILE__, __LINE__, format, __VA_ARGS__)

#define VideoSequence PLS_PLATFORM_PRSIM->getCharVideoSeq().c_str()
#define PLS_LIVE_ERROR(module_name, format, ...) pls_flow2_log(false, PLS_LOG_ERROR, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LIVE_ERROR_KR(module_name, format, ...) pls_flow2_log(true, PLS_LOG_ERROR, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_WARN(module_name, format, ...) pls_flow2_log(false, PLS_LOG_WARN, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LIVE_WARN_KR(module_name, format, ...) pls_flow2_log(true, PLS_LOG_WARN, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_INFO(module_name, format, ...) pls_flow2_log(false, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LIVE_ABORT_INFO(module_name, reason, format, ...) pls_flow3_log(false, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, reason, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LIVE_INFO_KR(module_name, format, ...) pls_flow2_log(true, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_DEBUG(module_name, format, ...) pls_flow2_log(false, PLS_LOG_DEBUG, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)
#define PLS_LIVE_DEBUG_KR(module_name, format, ...) pls_flow2_log(true, PLS_LOG_DEBUG, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, __VA_ARGS__)

/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_LIVE_UI_STEP(module_name, controls, action) pls_flow2_ui(false, module_name, "LiveStatus", VideoSequence, controls, action, __FILE__, __LINE__)
#define PLS_LIVE_UI_STEP_KR(module_name, controls, action) pls_flow2_ui(true, module_name, "LiveStatus", VideoSequence, controls, action, __FILE__, __LINE__)
#elif defined(Q_OS_MACOS)
/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_ERROR(module_name, format, args...) pls_flow_log(false, PLS_LOG_ERROR, module_name, "AppInit", __FILE__, __LINE__, format, ##args)
#define PLS_INIT_ERROR_KR(module_name, format, args...) pls_flow_log(true, PLS_LOG_ERROR, module_name, "AppInit", __FILE__, __LINE__, format, ##args)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_WARN(module_name, format, args...) pls_flow_log(false, PLS_LOG_WARN, module_name, "AppInit", __FILE__, __LINE__, format, ##args)
#define PLS_INIT_WARN_KR(module_name, format, args...) pls_flow_log(true, PLS_LOG_WARN, module_name, "AppInit", __FILE__, __LINE__, format, ##args)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_INFO(module_name, format, args...) pls_flow_log(false, PLS_LOG_INFO, module_name, "AppInit", __FILE__, __LINE__, format, ##args)
#define PLS_INIT_INFO_KR(module_name, format, args...) pls_flow_log(true, PLS_LOG_INFO, module_name, "AppInit", __FILE__, __LINE__, format, ##args)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_INIT_DEBUG(module_name, format, args...) pls_flow_log(false, PLS_LOG_DEBUG, module_name, "AppInit", __FILE__, __LINE__, format, ##args)
#define PLS_INIT_DEBUG_KR(module_name, format, args...) pls_flow_log(true, PLS_LOG_DEBUG, module_name, "AppInit", __FILE__, __LINE__, format, ##args)

/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_INIT_UI_STEP(module_name, controls, action) pls_flow_ui(false, module_name, "AppInit", controls, action, __FILE__, __LINE__)
#define PLS_INIT_UI_STEP_KR(module_name, controls, action) pls_flow_ui(true, module_name, "AppInit", controls, action, __FILE__, __LINE__)

/**
  * print error log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_ERROR(module_name, field, format, args...) pls_flow_log(false, PLS_LOG_ERROR, module_name, field, __FILE__, __LINE__, format, ##args)
#define PLS_CHAT_ERROR_KR(module_name, field, format, args...) pls_flow_log(true, PLS_LOG_ERROR, module_name, field, __FILE__, __LINE__, format, ##args)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_WARN(module_name, field, format, args...) pls_flow_log(false, PLS_LOG_WARN, module_name, field, __FILE__, __LINE__, format, ##args)
#define PLS_CHAT_WARN_KR(module_name, field, format, args...) pls_flow_log(true, PLS_LOG_WARN, module_name, field, __FILE__, __LINE__, format, ##args)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_CHAT_INFO(module_name, field, format, args...) pls_flow_log(false, PLS_LOG_INFO, module_name, field, __FILE__, __LINE__, format, ##args)
#define PLS_CHAT_INFO_KR(module_name, field, format, args...) pls_flow_log(true, PLS_LOG_INFO, module_name, field, __FILE__, __LINE__, format, ##args)

#define VideoSequence PLS_PLATFORM_PRSIM->getCharVideoSeq().c_str()
#define PLS_LIVE_ERROR(module_name, format, args...) pls_flow2_log(false, PLS_LOG_ERROR, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)
#define PLS_LIVE_ERROR_KR(module_name, format, args...) pls_flow2_log(true, PLS_LOG_ERROR, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)

/**
  * print warn log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_WARN(module_name, format, args...) pls_flow2_log(false, PLS_LOG_WARN, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)
#define PLS_LIVE_WARN_KR(module_name, format, args...) pls_flow2_log(true, PLS_LOG_WARN, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)

/**
  * print info log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_INFO(module_name, format, args...) pls_flow2_log(false, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)
#define PLS_LIVE_ABORT_INFO(module_name, reason, format, args...) pls_flow3_log(false, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, reason, __FILE__, __LINE__, format, ##args)
#define PLS_LIVE_INFO_KR(module_name, format, args...) pls_flow2_log(true, PLS_LOG_INFO, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)

/**
  * print debug log
  * param:
  *     [in] module_name: module name
  *     [in] format: format string
  *     [in] ...: variadic params
  */
#define PLS_LIVE_DEBUG(module_name, format, args...) pls_flow2_log(false, PLS_LOG_DEBUG, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)
#define PLS_LIVE_DEBUG_KR(module_name, format, args...) pls_flow2_log(true, PLS_LOG_DEBUG, module_name, "LiveStatus", VideoSequence, __FILE__, __LINE__, format, ##args)

/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  */
#define PLS_LIVE_UI_STEP(module_name, controls, action) pls_flow2_ui(false, module_name, "LiveStatus", VideoSequence, controls, action, __FILE__, __LINE__)
#define PLS_LIVE_UI_STEP_KR(module_name, controls, action) pls_flow2_ui(true, module_name, "LiveStatus", VideoSequence, controls, action, __FILE__, __LINE__)
#endif
