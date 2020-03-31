#include "liblog.h"

#include <stdarg.h>
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <malloc.h>

enum class log_type_t { GENERIC_LOG, ACTION_LOG };

#pragma pack(push, 1)
struct list_t {
	list_t *prev;
	list_t *next;
};
struct log_info_t {
	list_t listpos;
	log_type_t type;
};
struct generic_log_info_t {
	log_info_t log_info;
	pls_log_level_t log_level;
	int file_line;
	uint32_t tid;
	int field_count;
	pls_time_t time;
	char *module_name;
	char *file_name;
	char ***fields;
	char *message;
};
struct action_log_info_t {
	log_info_t log_info;
	int file_line;
	uint32_t tid;
	pls_time_t time;
	char *module_name;
	char *controls;
	char *action;
	char *file_name;
};
#pragma pack(pop)

#define UNUSED_PARAM(p) (void)p

static void def_log_handler(pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *message, void *param)
{
	UNUSED_PARAM(log_level);
	UNUSED_PARAM(module_name);
	UNUSED_PARAM(time);
	UNUSED_PARAM(tid);
	UNUSED_PARAM(file_name);
	UNUSED_PARAM(file_line);
	UNUSED_PARAM(message);
	UNUSED_PARAM(param);
}
static void def_action_log_handler(const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line, void *param)
{
	UNUSED_PARAM(module_name);
	UNUSED_PARAM(time);
	UNUSED_PARAM(tid);
	UNUSED_PARAM(controls);
	UNUSED_PARAM(action);
	UNUSED_PARAM(file_name);
	UNUSED_PARAM(file_line);
	UNUSED_PARAM(param);
}

static bool is_initialized = false;

static pls_log_handler_t log_handler = def_log_handler;
static void *log_param = nullptr;
static pls_action_log_handler_t action_log_handler = def_action_log_handler;
static void *action_log_param = nullptr;

static bool async_thread_running = false;
static HANDLE async_thread = nullptr;
static HANDLE log_queue_sem = nullptr;
static HANDLE log_queue_mutex = nullptr;
static list_t log_queue = {&log_queue, &log_queue};

static const char *get_file_name(const char *path)
{
	for (const char *pos = path + strlen(path) - 1; pos >= path; --pos) {
		if ((*pos == '\\') || (*pos == '/')) {
			return pos + 1;
		}
	}
	return path;
}
static void get_time(pls_time_t &time)
{
	SYSTEMTIME st = {0};
	::GetLocalTime(&st);
	time.year = st.wYear;
	time.month = st.wMonth;
	time.day = st.wDay;
	time.hour = st.wHour;
	time.minute = st.wMinute;
	time.second = st.wSecond;
	time.milliseconds = st.wMilliseconds;

	TIME_ZONE_INFORMATION tzi = {0};
	::GetTimeZoneInformation(&tzi);
	time.timezone = tzi.Bias;
}

static log_info_t *new_generic_log_info(pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *fields[][2],
					int field_count, const char *format, va_list args)
{
	int total_length = sizeof(generic_log_info_t);
	int module_name_length = 0;
	int file_name_length = 0;
	int fields_length = 0;
	int filed_lengths[100][2] = {0};
	int message_length = 0;

	module_name_length = int(strlen(module_name) + 1);
	total_length += module_name_length;

	if (file_name) {
		file_name_length = int(strlen(file_name) + 1);
		total_length += file_name_length;
	}

	if (field_count > 0) {
		field_count = (field_count < 100) ? field_count : 100;

		fields_length = sizeof(char **) * field_count + sizeof(char *) * 2 * field_count;
		total_length += fields_length;

		for (int i = 0; i < field_count; ++i) {
			for (int j = 0; j < 2; ++j) {
				filed_lengths[i][j] = int(strlen(fields[i][j]) + 1);
				total_length += filed_lengths[i][j];
			}
		}
	}

	message_length = vsnprintf(nullptr, 0, format, args) + 1;
	total_length += message_length;

	generic_log_info_t *generic_log_info = (generic_log_info_t *)malloc(total_length);
	if (!generic_log_info) {
		return nullptr;
	}

	memset(generic_log_info, 0, total_length);

	generic_log_info->log_info.type = log_type_t::GENERIC_LOG;

	generic_log_info->log_level = log_level;
	generic_log_info->file_line = file_line;
	generic_log_info->tid = tid;
	generic_log_info->field_count = field_count;
	generic_log_info->time = time;

	char *buffer = (char *)(generic_log_info + 1);

	strcpy(generic_log_info->module_name = buffer, module_name);
	buffer += module_name_length;

	if (file_name) {
		strcpy(generic_log_info->file_name = buffer, file_name);
		buffer += file_name_length;
	}

	if (field_count > 0) {
		generic_log_info->fields = (char ***)buffer;
		buffer += sizeof(char **) * field_count;

		for (int i = 0; i < field_count; ++i) {
			generic_log_info->fields[i] = (char **)buffer;
			buffer += sizeof(char *) * 2;
		}

		for (int i = 0; i < field_count; ++i) {
			for (int j = 0; j < 2; ++j) {
				strcpy(generic_log_info->fields[i][j] = buffer, fields[i][j]);
				buffer += filed_lengths[i][j];
			}
		}
	}

	vsnprintf(generic_log_info->message = buffer, message_length, format, args);

	return &generic_log_info->log_info;
}
static log_info_t *new_action_log_info(const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line)
{
	int total_length = sizeof(action_log_info_t);
	int module_name_length = 0;
	int controls_length = 0;
	int action_length = 0;
	int file_name_length = 0;

	module_name_length = int(strlen(module_name) + 1);
	total_length += module_name_length;

	controls_length = int(strlen(controls) + 1);
	total_length += controls_length;

	action_length = int(strlen(action) + 1);
	total_length += action_length;

	if (file_name) {
		file_name_length = int(strlen(file_name) + 1);
		total_length += file_name_length;
	}

	action_log_info_t *action_log_info = (action_log_info_t *)malloc(total_length);
	if (!action_log_info) {
		return nullptr;
	}

	memset(action_log_info, 0, total_length);

	action_log_info->log_info.type = log_type_t::ACTION_LOG;

	action_log_info->file_line = file_line;
	action_log_info->tid = tid;
	action_log_info->time = time;

	char *buffer = (char *)(action_log_info + 1);
	strcpy(action_log_info->module_name = buffer, module_name);
	buffer += module_name_length;

	strcpy(action_log_info->controls = buffer, controls);
	buffer += controls_length;

	strcpy(action_log_info->action = buffer, action);
	buffer += action_length;

	if (file_name) {
		strcpy(action_log_info->file_name = buffer, file_name);
		buffer += file_name_length;
	}

	return &action_log_info->log_info;
}

static bool list_empty(list_t *list)
{
	return list->next == list;
}
static void list_push(list_t *list, list_t *one)
{
	list->prev->next = one;
	one->prev = list->prev;
	one->next = list;
	list->prev = one;
}
static list_t *list_pop(list_t *list)
{
	if (list_empty(list)) {
		return nullptr;
	}

	list_t *one = list->next;
	one->next->prev = one->prev;
	one->prev->next = one->next;
	return one;
}

static void push_log_info(log_info_t *log_info)
{
	WaitForSingleObject(log_queue_mutex, INFINITE);
	list_push(&log_queue, &log_info->listpos);
	ReleaseMutex(log_queue_mutex);

	ReleaseSemaphore(log_queue_sem, 1, nullptr);
}
static log_info_t *pop_log_info(uint32_t timeout = INFINITE)
{
	if (WaitForSingleObject(log_queue_sem, timeout) == WAIT_OBJECT_0) {
		WaitForSingleObject(log_queue_mutex, INFINITE);
		log_info_t *one = (log_info_t *)list_pop(&log_queue);
		ReleaseMutex(log_queue_mutex);
		return one;
	}
	return nullptr;
}

static void log_process(generic_log_info_t *log_info)
{
	log_handler(log_info->log_level, log_info->module_name, log_info->time, log_info->tid, log_info->file_name, log_info->file_line, log_info->message, log_param);
}
static void log_process(action_log_info_t *log_info)
{
	action_log_handler(log_info->module_name, log_info->time, log_info->tid, log_info->controls, log_info->action, log_info->file_name, log_info->file_line, log_param);
}

static void generic_logva(pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *fields[][2], int field_count,
			  const char *format, va_list args)
{
	if (is_initialized) {
		log_info_t *log_info = new_generic_log_info(log_level, module_name, time, tid, file_name, file_line, fields, field_count, format, args);
		if (log_info) {
			push_log_info(log_info);
		}
	}
}
static void generic_log(pls_log_level_t log_level, const char *module_name, const pls_time_t &time, uint32_t tid, const char *file_name, int file_line, const char *fields[][2], int field_count,
			const char *format, ...)
{
	va_list args;
	va_start(args, format);
	generic_logva(log_level, module_name, time, tid, file_name, file_line, fields, field_count, format, args);
	va_end(args);
}
static void action_log(const char *module_name, const pls_time_t &time, uint32_t tid, const char *controls, const char *action, const char *file_name, int file_line)
{
	if (is_initialized) {
		log_info_t *log_info = new_action_log_info(module_name, time, tid, controls, action, file_name, file_line);
		if (log_info) {
			push_log_info(log_info);
		}
	}
}

static unsigned __stdcall async_thread_proc(void *param)
{
	while (async_thread_running) {
		log_info_t *log_info = pop_log_info();
		if (!log_info) {
			continue;
		}

		switch (log_info->type) {
		case log_type_t::GENERIC_LOG:
			log_process((generic_log_info_t *)log_info);
			free(log_info);
			break;
		case log_type_t::ACTION_LOG:
			log_process((action_log_info_t *)log_info);
			free(log_info);
			break;
		}
	}

	while (log_info_t *log_info = pop_log_info(0)) {
		switch (log_info->type) {
		case log_type_t::GENERIC_LOG:
			log_process((generic_log_info_t *)log_info);
			free(log_info);
			break;
		case log_type_t::ACTION_LOG:
			log_process((action_log_info_t *)log_info);
			free(log_info);
			break;
		}
	}
	return 0;
}

static void log_cleanup()
{
	async_thread_running = false;

	if (async_thread) {
		ReleaseSemaphore(log_queue_sem, 1, nullptr);
		WaitForSingleObject(async_thread, INFINITE);
		CloseHandle(async_thread);
		async_thread = nullptr;
	}

	if (log_queue_sem) {
		CloseHandle(log_queue_sem);
		log_queue_sem = nullptr;
	}

	if (log_queue_mutex) {
		CloseHandle(log_queue_mutex);
		log_queue_mutex = nullptr;
	}
}

LIBLOG_API bool pls_log_init(const char *project_name, const char *project_version, const char *log_source, const char *log_type, const char *server_addr, int server_port, bool https)
{
	if (is_initialized) {
		return true;
	}

	if (!(log_queue_sem = CreateSemaphoreW(nullptr, 0, INT_MAX, nullptr)) || !(log_queue_mutex = CreateMutexW(nullptr, FALSE, nullptr))) {
		log_cleanup();
		return false;
	}

	async_thread_running = true;
	if (!(async_thread = (HANDLE)_beginthreadex(nullptr, 0, &async_thread_proc, nullptr, 0, nullptr))) {
		log_cleanup();
		return false;
	}

	is_initialized = true;
	return true;
}
LIBLOG_API void pls_log_cleanup()
{
	if (!is_initialized) {
		return;
	}

	is_initialized = false;
	log_cleanup();
}

LIBLOG_API void pls_set_log_handler(pls_log_handler_t handler, void *param)
{
	log_handler = handler;
	log_param = param;
}
LIBLOG_API void pls_get_log_handler(pls_log_handler_t *handler, void **param)
{
	if (handler) {
		*handler = log_handler;
	}
	if (param) {
		*param = log_param;
	}
}
LIBLOG_API void pls_reset_log_handler()
{
	log_handler = def_log_handler;
	log_param = nullptr;
}

LIBLOG_API void pls_set_action_log_handler(pls_action_log_handler_t handler, void *param)
{
	action_log_handler = handler;
	action_log_param = param;
}
LIBLOG_API void pls_get_action_log_handler(pls_action_log_handler_t *handler, void **param)
{
	if (handler) {
		*handler = action_log_handler;
	}
	if (param) {
		*param = action_log_param;
	}
}
LIBLOG_API void pls_reset_action_log_handler()
{
	action_log_handler = def_action_log_handler;
	action_log_param = nullptr;
}

LIBLOG_API const char *pls_get_user_id()
{
	return "";
}
LIBLOG_API void pls_set_user_id(const char *user_id)
{
}

LIBLOG_API bool pls_add_global_field(const char *key, const char *value)
{
	return false;
}
LIBLOG_API void pls_remove_global_field(const char *key)
{
}

LIBLOG_API pls_log_level_t pls_get_report_log_level()
{
	return PLS_LOG_DEBUG;
}
LIBLOG_API void pls_set_report_log_level(pls_log_level_t log_level)
{
}

LIBLOG_API void pls_logva(pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *format, va_list args)
{
	pls_logvaex(log_level, module_name, file_name, file_line, nullptr, 0, format, args);
}
LIBLOG_API void pls_logvaex(pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *fields[][2], int field_count, const char *format, va_list args)
{
	pls_time_t time;
	get_time(time);

	uint32_t tid = GetCurrentThreadId();

	file_name = file_name ? get_file_name(file_name) : file_name;

	generic_logva(log_level, module_name, time, tid, file_name, file_line, fields, field_count, format, args);
}
LIBLOG_API void pls_log(pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(log_level, module_name, file_name, file_line, format, args);
	va_end(args);
}
LIBLOG_API void pls_logex(pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const char *fields[][2], int field_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logvaex(log_level, module_name, file_name, file_line, fields, field_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_error(const char *module_name, const char *file_name, int file_line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(PLS_LOG_ERROR, module_name, file_name, file_line, format, args);
	va_end(args);
}
LIBLOG_API void pls_warn(const char *module_name, const char *file_name, int file_line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(PLS_LOG_WARN, module_name, file_name, file_line, format, args);
	va_end(args);
}
LIBLOG_API void pls_info(const char *module_name, const char *file_name, int file_line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(PLS_LOG_INFO, module_name, file_name, file_line, format, args);
	va_end(args);
}
LIBLOG_API void pls_debug(const char *module_name, const char *file_name, int file_line, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(PLS_LOG_DEBUG, module_name, file_name, file_line, format, args);
	va_end(args);
}
LIBLOG_API void pls_action_log(const char *module_name, const char *controls, const char *action, const char *file_name, int file_line)
{
	pls_time_t time;
	get_time(time);

	uint32_t tid = GetCurrentThreadId();

	file_name = file_name ? get_file_name(file_name) : file_name;

	action_log(module_name, time, tid, controls, action, file_name, file_line);
}
LIBLOG_API void pls_ui_step(const char *module_name, const char *controls, const char *action, const char *file_name, int file_line)
{
	pls_time_t time;
	get_time(time);

	uint32_t tid = GetCurrentThreadId();

	file_name = file_name ? get_file_name(file_name) : file_name;

	const char *fields[][2] = {{"ui-step", "all"}};
	generic_log(PLS_LOG_INFO, module_name, time, tid, file_name, file_line, fields, 1, "UI: [UI STEP] %s %s", controls, action);
}
