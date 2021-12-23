#include "log.h"

#include <qglobal.h>

#include "ui-config.h"

const int LOG_DISAPPEAR = -100;

static void def_prism_handler(int log_level, const char *format, va_list args, void *param)
{
	Q_UNUSED(log_level)
	Q_UNUSED(format)
	Q_UNUSED(args)
	Q_UNUSED(param)
}

static log_handler_t log_handler = def_prism_handler;
static void *log_param = nullptr;

static int to_obs_level(pls_log_level_t log_level)
{
	switch (log_level) {
	case PLS_LOG_ERROR:
		return LOG_ERROR;
	case PLS_LOG_WARN:
		return LOG_WARNING;
	case PLS_LOG_INFO:
		return LOG_INFO;
	case PLS_LOG_DEBUG:
	default:
		return LOG_DEBUG;
	}
}
static void call_obs_handler(bool kr, pls_log_level_t log_level, const char *module_name, uint32_t tid, const char *format, ...)
{
	struct log_info_s {
		bool kr;
		uint32_t tid;
		const char *module_name;
		const char *format;
	};

	va_list args;
	va_start(args, format);
	log_info_s log_info = {kr, tid, module_name, format};
	log_handler(to_obs_level(log_level), (const char *)(void *)&log_info, args, log_param);
	va_end(args);
}

static void def_obs_log_handler(bool kr, int log_level, const char *format, va_list args, const char *fields[][2], int field_count, void *)
{
	int arg_count = -1;
	switch (log_level) {
	case LOG_ERROR:
		pls_logvaex(kr, PLS_LOG_ERROR, "obs", nullptr, 0, fields, field_count, arg_count, format, args);
		break;
	case LOG_WARNING:
		pls_logvaex(kr, PLS_LOG_WARN, "obs", nullptr, 0, fields, field_count, arg_count, format, args);
		break;
	case LOG_INFO:
		pls_logvaex(kr, PLS_LOG_INFO, "obs", nullptr, 0, fields, field_count, arg_count, format, args);
		break;
	case LOG_DEBUG:
		pls_logvaex(kr, PLS_LOG_DEBUG, "obs", nullptr, 0, fields, field_count, arg_count, format, args);
		break;
	case LOG_DISAPPEAR:
		if (field_count == 3) {
			const char *process = nullptr;
			const char *pid = nullptr;
			const char *src = nullptr;
			for (int i = 0; i < field_count; ++i) {
				if (!strcmp(fields[i][0], "process")) {
					process = fields[i][1];
				} else if (!strcmp(fields[i][0], "pid")) {
					pid = fields[i][1];
				} else if (!strcmp(fields[i][0], "src")) {
					src = fields[i][1];
				}
			}

			if (process && pid && src) {
				pls_subprocess_disappear(process, pid, src);
			}
		}
		break;
	}
}
static void def_obs_crash_handler(const char *, va_list, void *) {}

static void def_pls_log_handler(bool kr, pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *message,
				void *param)
{
	Q_UNUSED(kr)
	Q_UNUSED(module_name)
	Q_UNUSED(time)
	Q_UNUSED(tid)
	Q_UNUSED(file_name)
	Q_UNUSED(file_line)
	Q_UNUSED(param)

	call_obs_handler(kr, log_level, module_name, tid, "%s", message);
}
static void def_pls_action_log_handler(bool kr, const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line,
				       void *param)
{
	Q_UNUSED(kr)
	Q_UNUSED(module_name)
	Q_UNUSED(time)
	Q_UNUSED(tid)
	Q_UNUSED(controls)
	Q_UNUSED(action)
	Q_UNUSED(file_name)
	Q_UNUSED(file_line)
	Q_UNUSED(param)
}

bool log_init(const char *session_id, const char *project_name, const char *project_name_kr)
{
	//base_get_log_handler(&log_handler, &log_param);
	base_set_log_handler_ex(def_obs_log_handler, nullptr);

	base_set_crash_handler(def_obs_crash_handler, nullptr);

	pls_log_init(project_name, project_name_kr, PLS_VERSION, "prism-log");
	pls_add_global_field("prismSession", session_id);

	pls_set_log_handler(def_pls_log_handler, nullptr);
	pls_set_action_log_handler(def_pls_action_log_handler, nullptr);
	return true;
}
void log_cleanup()
{
	pls_log_cleanup();
	pls_reset_log_handler();
	pls_reset_action_log_handler();
}

void set_log_handler(log_handler_t handler, void *param)
{
	log_handler = handler;
	log_param = param;
}

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
void pls_flow_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	const char *fields[][2] = {{"flow", flow}};
	pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, 1, arg_count, format, args);
	va_end(args);
}

/**
  * print log
  * param:
  *     [in] log_level: log level
  *     [in] module_name: module name
  *     [in] flow: flow
  *	[in] video_seq: video sequence
  *     [in] file_name: file name
  *     [in] file_line: file line
  *     [in] format: format string
  *     [in] ...: variadic params
  */
void pls_flow2_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	auto isEmpty = nullptr == video_seq || video_seq[0] == '\0';
	if (isEmpty) {
		const char *fields[][2] = {{"flow", flow}};
		pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, 1, arg_count, format, args);
	} else {
		const char *fields[][2] = {{"flow", flow}, {"videoSeq", video_seq}};
		pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, 2, arg_count, format, args);
	}
	va_end(args);
}
/**
  * print ui step log
  * param:
  *     [in] module_name: module name
  *     [in] flow: flow
  *     [in] controls: trigger controls
  *     [in] action: trigger action
  *     [in] file_name: file name
  *     [in] file_line: file line
  */
void pls_flow3_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *flow, const char *video_seq, const char *otherFlow, const char *file_name, int file_line, int arg_count,
		   const char *format, ...)
{
	va_list args;
	va_start(args, format);
	auto isEmpty = nullptr == video_seq || video_seq[0] == '\0';
	if (isEmpty) {
		const char *fields[][2] = {{"flow", flow}, {"liveAbort", otherFlow}};
		pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, 2, arg_count, format, args);
	} else {
		const char *fields[][2] = {{"flow", flow}, {"videoSeq", video_seq}, {"liveAbort", otherFlow}};
		pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, 3, arg_count, format, args);
	}
	va_end(args);
}

void pls_flow_ui(bool kr, const char *module_name, const char *flow, const char *controls, const char *action, const char *file_name, int file_line)
{
	const char *fields[][2] = {{"flow", flow}};
	pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, fields, 1);
}

void pls_flow2_ui(bool kr, const char *module_name, const char *flow, const char *video_seq, const char *controls, const char *action, const char *file_name, int file_line)
{
	auto isEmpty = nullptr == video_seq || video_seq[0] == '\0';
	if (isEmpty) {
		const char *fields[][2] = {{"flow", flow}};
		pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, fields, 1);
	} else {
		const char *fields[][2] = {{"flow", flow}, {"videoSeq", video_seq}};
		pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, fields, 2);
	}
}
