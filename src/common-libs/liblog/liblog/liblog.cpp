#include "liblog.h"

#include <stdarg.h>
#include <stdio.h>

#include <array>
#include <map>
#include <string>

#include <qdir.h>
#include <quuid.h>
#include <qsystemsemaphore.h>
#include <qthread.h>

#include <libutils-api.h>
#include <libutils-api-log.h>

#if defined(Q_OS_WIN)
#include <malloc.h>
#else
#include <malloc/malloc.h>
#endif

constexpr auto LIBLOG_MODULE = "liblog";

#pragma pack(push, 1)
struct log_msg_t {
	// types
	static const int MT_EXIT = 1;
	static const int MT_SET_USER_ID = 2;
	static const int MT_ADD_GLOBAL_FIELD = 3;
	static const int MT_REMOVE_GLOBAL_FIELD = 4;
	static const int MT_SUBPROCESS_EXCEPTION = 5;
	static const int MT_RUNTIME_STATS = 6;
	static const int MT_GENERIC_LOG = 7;

	// tag
	static const int MT_CN_TAG = 0x0000;
	static const int MT_KR_TAG = 0x1000;

	// mask
	static const int MT_KR_MASK = 0x1000;
	static const int MT_TYPE_MASK = 0x0fff;

	// for thread queue
	pls_queue_item_t list_item;
	// message
	int type;         // message type, MT_*
	int total_length; // message total length
};
struct add_global_field_t {
	int key_offset;
	int value_offset;
};
struct log_info_t {
	pls_log_level_t log_level;
	int file_line;
	uint32_t tid;
	int field_count;
	pls_datetime_t log_time;
	int module_name_offset;
	int file_name_offset;
	int fields_offset;
	int message_offset;
#if 0
	char *module_name;
	char *file_name;
	char ***fields;
	char *header;
#endif
};
struct log_field_t {
	int key_offset;
	int value_offset;
	int next_offset;
};
struct kvpair_t {
	uint32_t key_length;
	uint32_t value_length;
#if 0
	char key[key_length];
	char value[value_length];
#endif
};
struct kvpairs_t {
	uint32_t kv_count;
	/* kvpair_t ky[kv_count] */
};
#pragma pack(pop)

static void def_log_handler(bool kr, pls_log_level_t log_level, const char *module_name, const pls_datetime_t &time, uint32_t tid, const char *file_name, int file_line, const char *header,
			    void *param)
{
	pls_unused(kr, log_level, module_name, time, tid, file_name, file_line, header, param);
}
static void logex(bool kr, int log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields, int arg_count,
		  const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logvaex(kr, static_cast<pls_log_level_t>(log_level), module_name, file_name, file_line, fields, arg_count, format, args);
	va_end(args);
}

namespace {
struct LocalGlobalVars {
	static bool is_initialized;
	static pls_process_t *logger_process;

	static pls_log_handler_t log_handler;
	static void *log_param;
	static std::string log_gcc;

	static pls_thread_t *log_proc_thread;
	static pls_thread_t *nelo_log_proc_thread;
	static pls_thread_t *check_logger_running_thread;

	static QFile *log_file;
};

bool LocalGlobalVars::is_initialized = false;
pls_process_t *LocalGlobalVars::logger_process = nullptr;

pls_log_handler_t LocalGlobalVars::log_handler = def_log_handler;
void *LocalGlobalVars::log_param = nullptr;
std::string LocalGlobalVars::log_gcc;

pls_thread_t *LocalGlobalVars::log_proc_thread = nullptr;
pls_thread_t *LocalGlobalVars::nelo_log_proc_thread = nullptr;
pls_thread_t *LocalGlobalVars::check_logger_running_thread = nullptr;

QFile *LocalGlobalVars::log_file = nullptr;
}

namespace {
class WaitLoggerStartedThread : public QThread {
	QSystemSemaphore m_sem;

public:
	WaitLoggerStartedThread(const QString &name) : m_sem(name) { start(); }

	void waitFinished(int timeout)
	{
		wait(timeout);
		m_sem.release();
		wait();
	}

private:
	void run() override { m_sem.acquire(); }
};
class Initializer {
public:
	Initializer()
	{
		pls::set_log_levels(PLS_LOG_ERROR, PLS_LOG_WARN, PLS_LOG_INFO, PLS_LOG_DEBUG);
		pls::set_log_cbs(pls_error, pls_warn, pls_info, pls_debug, &logex);
	}
	~Initializer() { pls::set_log_cbs(nullptr, nullptr, nullptr, nullptr, nullptr); }

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }
};
}

static char *get_log_info_module_name(log_info_t *log_info)
{
	return ((char *)log_info) + log_info->module_name_offset;
}
static char *get_log_info_file_name(log_info_t *log_info)
{
	return log_info->file_name_offset > 0 ? (((char *)log_info) + log_info->file_name_offset) : nullptr;
}
static char *get_log_info_message(log_info_t *log_info)
{
	return ((char *)log_info) + log_info->message_offset;
}
static int gen_msg_type(int type, bool kr)
{
	return type | (kr ? log_msg_t::MT_KR_TAG : log_msg_t::MT_CN_TAG);
}
static bool is_kr(int type)
{
	return (type & log_msg_t::MT_KR_MASK) ? true : false;
}
static void init_log_msg(log_msg_t *log_msg, int type, bool kr, int total_length)
{
	pls_list_item_init(&log_msg->list_item);
	log_msg->type = gen_msg_type(type, kr);
	log_msg->total_length = total_length - sizeof(pls_list_item_t);
}
static log_msg_t *new_log_msg(int type, bool kr = false, int body_length = 0)
{
	int total_length = sizeof(log_msg_t) + body_length;
	if (auto log_msg = pls_malloc<log_msg_t>(total_length); log_msg) {
		init_log_msg(log_msg, type, kr, total_length);
		memset(log_msg + 1, 0, body_length);
		return log_msg;
	}
	return nullptr;
}
template<typename MsgBody> static log_msg_t *new_log_msg(MsgBody *&msg_body, int type, bool kr, int body_length)
{
	if (auto log_msg = new_log_msg(type, kr, body_length); log_msg) {
		msg_body = (MsgBody *)(log_msg + 1);
		return log_msg;
	}
	return nullptr;
}
static log_msg_t *new_log_msg(int type, bool kr, const char *body)
{
	char *msg_body = nullptr;
	if (auto log_msg = new_log_msg(msg_body, type, kr, (int)strlen(body) + 1); log_msg) {
		strcpy(msg_body, body);
		return log_msg;
	}
	return nullptr;
}
static log_msg_t *new_log_msg(bool kr, pls_log_level_t log_level, const char *module_name, const pls_datetime_t &log_time, uint32_t tid, const char *file_name, int file_line,
			      const std::vector<std::pair<const char *, const char *>> &fields, const char *format, va_list args)
{
	int body_length = sizeof(log_info_t);
	int module_name_length = 0;
	int file_name_length = 0;
	int fields_length = 0;
	std::array<std::array<int, 2>, 100> filed_lengths;
	int message_length = 0;

	module_name_length = int(strlen(module_name) + 1);
	body_length += module_name_length;

	if (file_name) {
		file_name_length = int(strlen(file_name) + 1);
		body_length += file_name_length;
	}

	auto field_count = int(fields.size());
	if (field_count > 0) {
		field_count = (field_count < 100) ? field_count : 100;

		fields_length = sizeof(log_field_t) * field_count;
		body_length += fields_length;

		for (int i = 0; i < field_count; ++i) {
			filed_lengths[i][0] = int(strlen(fields[i].first) + 1);
			body_length += filed_lengths[i][0];

			filed_lengths[i][1] = int(strlen(fields[i].second) + 1);
			body_length += filed_lengths[i][1];
		}
	}

	va_list _args;
	va_copy(_args, args);
	message_length = vsnprintf(nullptr, 0, format, _args) + 1;
	body_length += message_length;
	va_end(_args);

	log_info_t *log_info = nullptr;
	auto log_msg = new_log_msg(log_info, log_msg_t::MT_GENERIC_LOG, kr, body_length);
	if (!log_msg) {
		return nullptr;
	}

	log_info->log_level = log_level;
	log_info->file_line = file_line;
	log_info->tid = tid;
	log_info->field_count = field_count;
	log_info->log_time = log_time;

	auto buffer = (char *)(log_info + 1);

	log_info->module_name_offset = int(buffer - (char *)log_info);
	strcpy(buffer, module_name);
	buffer += module_name_length;

	if (file_name) {
		log_info->file_name_offset = int(buffer - (char *)log_info);
		strcpy(buffer, file_name);
		buffer += file_name_length;
	} else {
		log_info->file_name_offset = 0;
	}

	if (field_count > 0) {
		log_info->fields_offset = int(buffer - (char *)log_info);

		for (int i = 0; i < field_count; ++i) {
			auto field = (log_field_t *)buffer;

			buffer += sizeof(log_field_t);
			field->key_offset = int(buffer - (char *)log_info);
			strcpy(buffer, fields[i].first);

			buffer += filed_lengths[i][0];
			field->value_offset = int(buffer - (char *)log_info);
			strcpy(buffer, fields[i].second);

			buffer += filed_lengths[i][1];
			field->next_offset = int(buffer - (char *)log_info);
		}
	} else {
		log_info->fields_offset = 0;
	}

	log_info->message_offset = int(buffer - (char *)log_info);
	vsnprintf(buffer, message_length, format, args);
	return log_msg;
}

static void generic_logva(bool kr, pls_log_level_t log_level, const char *module_name, const pls_datetime_t &time, uint32_t tid, const char *file_name, int file_line,
			  const std::vector<std::pair<const char *, const char *>> &fields, const char *format, va_list args)
{
	if (!LocalGlobalVars::is_initialized) {
		return;
	}

	if (!LocalGlobalVars::log_gcc.empty()) {
		std::vector<std::pair<const char *, const char *>> tmp_fields = fields;
		tmp_fields.push_back({"gcc", LocalGlobalVars::log_gcc.c_str()});
		if (log_msg_t *log_msg = new_log_msg(kr, log_level, module_name, time, tid, file_name, file_line, tmp_fields, format, args); log_msg) {
			pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
		}
	} else if (log_msg_t *log_msg = new_log_msg(kr, log_level, module_name, time, tid, file_name, file_line, fields, format, args); log_msg) {
		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	}
}
static void generic_log(bool kr, pls_log_level_t log_level, const char *module_name, const pls_datetime_t &time, uint32_t tid, const char *file_name, int file_line,
			const std::vector<std::pair<const char *, const char *>> &fields, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	generic_logva(kr, log_level, module_name, time, tid, file_name, file_line, fields, format, args);
	va_end(args);
}

static void process_log(log_msg_t *log_msg, log_info_t *log_info)
{
	bool kr = (log_msg->type & log_msg_t::MT_KR_MASK) ? true : false;
	const char *module_name = get_log_info_module_name(log_info);
	const char *file_name = get_log_info_file_name(log_info);
	const char *message = get_log_info_message(log_info);

	LocalGlobalVars::log_handler(kr, log_info->log_level, module_name, log_info->log_time, log_info->tid, file_name, log_info->file_line, message, LocalGlobalVars::log_param);

	if (LocalGlobalVars::log_file && LocalGlobalVars::log_file->isOpen()) {
		QString log_line;
		log_line.append(pls_datetime_to_string(log_info->log_time));
		log_line.append(kr ? QStringLiteral(" [KR]") : QStringLiteral(" [CN]"));
		switch (log_info->log_level) {
		case PLS_LOG_ERROR:
			log_line.append(QStringLiteral(" [ERROR]"));
			break;
		case PLS_LOG_WARN:
			log_line.append(QStringLiteral(" [WARN]"));
			break;
		case PLS_LOG_INFO:
			log_line.append(QStringLiteral(" [INFO]"));
			break;
		case PLS_LOG_DEBUG:
			log_line.append(QStringLiteral(" [DEBUG]"));
			break;
		default:
			log_line.append(QStringLiteral(" []"));
			break;
		}
		log_line.append(QStringLiteral(" ["));
		log_line.append(QLatin1String(module_name));
		log_line.append(']');
		if (!pls_is_empty(file_name) && log_info->file_line > 0) {
			log_line.append(QStringLiteral(" ["));
			log_line.append(QLatin1String(file_name));
			log_line.append(':');
			log_line.append(QString::number(log_info->file_line));
			log_line.append(']');
		}
		log_line.append(QStringLiteral(" ("));
		log_line.append(QString::number(log_info->tid));
		log_line.append(QStringLiteral("): "));
		log_line.append(QString::fromUtf8(message));
		log_line.append('\n');
		LocalGlobalVars::log_file->write(log_line.toUtf8());
		LocalGlobalVars::log_file->flush();
	}

	if (pls_is_debugger_present()) {
#if defined(Q_OS_WIN)
		pls_printf(pls_datetime_to_string(log_info->log_time));
		pls_printf(L" ");
		pls_printf(pls_get_app_pn());
		pls_printf(L" ");
		switch (log_info->log_level) {
		case PLS_LOG_DEBUG:
			pls_printf(L"DEBUG ");
			break;
		case PLS_LOG_INFO:
			pls_printf(L"INFO ");
			break;
		case PLS_LOG_WARN:
			pls_printf(L"WARN ");
			break;
		case PLS_LOG_ERROR:
			pls_printf(L"ERROR ");
			break;
		default:
			break;
		}
		if (kr) {
			pls_printf(L"KR ");
		}
		pls_printf(pls_utf8_to_unicode(module_name).c_str());
		pls_printf(L" ");
		if (!pls_is_empty(file_name) && log_info->file_line > 0) {
			pls_printf(pls_utf8_to_unicode(file_name).c_str());
			pls_printf(L":");
			pls_printf(std::to_wstring(log_info->file_line).c_str());
			pls_printf(L" ");
		}
		pls_printf(pls_utf8_to_unicode(message).c_str());
		pls_printf(L"\n");
#else
		const char *logLevel = "";
		switch (log_info->log_level) {
		case PLS_LOG_DEBUG:
			logLevel = "DEBUG";
			break;
		case PLS_LOG_INFO:
			logLevel = "INFO";
			break;
		case PLS_LOG_WARN:
			logLevel = "WARN";
			break;
		case PLS_LOG_ERROR:
			logLevel = "ERROR";
			break;
		default:
			break;
		}

		if (!pls_is_empty(file_name) && log_info->file_line > 0)
			pls_mac_printf("%s %s %s %s%s %s:%i %s\n", pls_datetime_to_string(log_info->log_time).toUtf8().data(), pls_get_app_pn().toUtf8().data(), logLevel, kr ? "KR " : "", module_name,
				       file_name, log_info->file_line, message);
		else
			pls_mac_printf("%s %s %s %s%s %s\n", pls_datetime_to_string(log_info->log_time).toUtf8().data(), pls_get_app_pn().toUtf8().data(), logLevel, kr ? "KR " : "", module_name,
				       message);

#endif
	}
}
static void process_log(log_msg_t *log_msg)
{
	if (auto type = log_msg->type & log_msg_t::MT_TYPE_MASK; type == log_msg_t::MT_GENERIC_LOG) {
		process_log(log_msg, (log_info_t *)(log_msg + 1));
	}
}
static void log_proc_thread_entry(pls_thread_t *thread)
{
	while (pls_thread_running(thread)) {
		if (log_msg_t *log_msg = (log_msg_t *)pls_thread_queue_pop(thread); log_msg) {
			process_log(log_msg);
			if (LocalGlobalVars::logger_process && LocalGlobalVars::nelo_log_proc_thread) {
				pls_thread_queue_push(LocalGlobalVars::nelo_log_proc_thread, &log_msg->list_item);
			} else {
				pls_free(log_msg);
			}
		}
	}

	while (log_msg_t *log_msg = (log_msg_t *)pls_thread_queue_try_pop(thread)) {
		process_log(log_msg);
		if (LocalGlobalVars::logger_process && LocalGlobalVars::nelo_log_proc_thread) {
			pls_thread_queue_push(LocalGlobalVars::nelo_log_proc_thread, &log_msg->list_item);
		} else {
			pls_free(log_msg);
		}
	}
}
static void nelo_log_proc_thread_entry(pls_thread_t *thread)
{
	while (pls_thread_running(thread)) {
		auto log_msg = (log_msg_t *)pls_thread_queue_pop(thread);
		if (log_msg) {
			pls_free(log_msg);
		}
	}

	for (auto log_msg = (log_msg_t *)pls_thread_queue_pop(thread); log_msg; log_msg = (log_msg_t *)pls_thread_queue_pop(thread)) {
		pls_free(log_msg);
	}
}
static void check_logger_running_entry(pls_thread_t *thread)
{
	while (pls_thread_running(thread)) {
		if (pls_process_wait(LocalGlobalVars::logger_process, 2000) > 0) {
			pls_delete(LocalGlobalVars::logger_process, pls_process_destroy, nullptr);
			break;
		}
	}
}
static void init_log_file(const char *session)
{
	if (!LocalGlobalVars::log_file && pls_prism_is_dev()) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::log_file) {
#if defined(Q_OS_WIN)
			QDir dir(QString("%1/PRISMLiveStudio/log/%2").arg(qEnvironmentVariable("APPDATA"), session));
#elif defined(Q_OS_MACOS)
			QDir dir(QString("%1/PRISMLiveStudio/log/%2").arg(pls_get_mac_app_data_dir(), session));
#endif
			if (!dir.exists()) {
				dir.mkpath(dir.absolutePath());
			}

			QString processName = pls_get_app_pn();
			auto log_file = pls_new<QFile>(QString("%1/%2_%3.txt").arg(dir.absolutePath(), QDateTime::currentDateTime().toString("yyyyMMddhhmmss"), processName));
			if (log_file->open(QFile::WriteOnly)) {
				LocalGlobalVars::log_file = log_file;
			} else {
				PLS_WARN(LIBLOG_MODULE, "create local log file failed");
				pls_delete(log_file);
			}
		}
	}
}

static void log_cleanup()
{
	if (LocalGlobalVars::log_proc_thread) {
		log_msg_t *log_msg = new_log_msg(log_msg_t::MT_EXIT);
		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	}

	if (LocalGlobalVars::log_proc_thread) {
		pls_thread_quit(LocalGlobalVars::log_proc_thread);
		while (!pls_thread_queue_empty(LocalGlobalVars::log_proc_thread))
			pls_sleep_ms(10);
		pls_delete(LocalGlobalVars::log_proc_thread, pls_thread_destroy, nullptr);
	}

	if (LocalGlobalVars::nelo_log_proc_thread) {
		pls_thread_quit(LocalGlobalVars::nelo_log_proc_thread);
		while (!pls_thread_queue_empty(LocalGlobalVars::nelo_log_proc_thread))
			pls_sleep_ms(10);
		pls_delete(LocalGlobalVars::nelo_log_proc_thread, pls_thread_destroy, nullptr);
	}

	if (LocalGlobalVars::check_logger_running_thread) {
		pls_delete(LocalGlobalVars::check_logger_running_thread, pls_thread_destroy, nullptr);
	}

	if (LocalGlobalVars::logger_process) {
		pls_process_wait(LocalGlobalVars::logger_process, 1000);
		pls_delete(LocalGlobalVars::logger_process, pls_process_destroy, nullptr);
	}

	if (LocalGlobalVars::log_file) {
		LocalGlobalVars::log_file->flush();
		pls_delete(LocalGlobalVars::log_file, nullptr);
	}
}
LIBLOG_API bool pls_log_init(const char *project_name, const char *project_token, const char *project_name_kr, const char *project_token_kr, const char *project_version, const char *log_source,
			     const char *local_log_session)
{
	if (LocalGlobalVars::is_initialized) {
		return true;
	}

	if (!pls_is_empty(local_log_session)) {
		init_log_file(local_log_session);
	}

	LocalGlobalVars::log_proc_thread = pls_thread_create(log_proc_thread_entry, true);
	LocalGlobalVars::is_initialized = true;

	uint32_t pid = pls_current_process_id();
	QString uuid = pls_gen_uuid();

	// PRISMLogger.exe project_name project_token project_name_kr project_token_kr project_version log_source pid uuid
	QString program;
#if defined(Q_OS_WIN)
	program = pls_get_dll_dir("liblog") + QStringLiteral("\\PRISMLogger.exe");
#elif defined(Q_OS_MACOS)
	program = pls_get_app_dir() + QStringLiteral("/PRISMLogger");
#endif

	//pls_get_app_dir
	QStringList arguments{QString::fromUtf8(project_name),
			      QString::fromUtf8(project_token),
			      QString::fromUtf8(project_name_kr),
			      QString::fromUtf8(project_token_kr),
			      QString::fromUtf8(project_version),
			      QString::fromUtf8(log_source),
			      QString::number(pid),
			      uuid};
	if (LocalGlobalVars::logger_process = pls_process_create(program, arguments); !LocalGlobalVars::logger_process) {
		pls_thread_start(LocalGlobalVars::log_proc_thread);
		return false;
	}

	QString startedSemName = QStringLiteral("PRISMLoggerStartedSem_%1_%2").arg(pid).arg(uuid);
	WaitLoggerStartedThread waitThread(startedSemName);
	waitThread.waitFinished(10000);

	pls_thread_start(LocalGlobalVars::log_proc_thread);

	LocalGlobalVars::nelo_log_proc_thread = pls_thread_create(nelo_log_proc_thread_entry);
	pls_thread_start(LocalGlobalVars::log_proc_thread);

	LocalGlobalVars::check_logger_running_thread = pls_thread_create(check_logger_running_entry);
	pls_thread_start(LocalGlobalVars::check_logger_running_thread);

	std::string pn = pls_get_app_pn().toStdString();
	pls_add_global_field("processName", pn.c_str());

	return true;
}
LIBLOG_API bool pls_prism_log_init(const char *project_version, const char *log_source, const char *local_log_session)
{
	constexpr const char *PLS_PROJECT_NAME = "";
	constexpr const char *PLS_PROJECT_NAME_KR = "";
	constexpr const char *PLS_PROJECT_TOKEN = "";
	constexpr const char *PLS_PROJECT_TOKEN_KR = "";
	return pls_log_init(PLS_PROJECT_NAME, PLS_PROJECT_TOKEN, PLS_PROJECT_NAME_KR, PLS_PROJECT_TOKEN_KR, project_version, log_source, local_log_session);
}
LIBLOG_API void pls_log_cleanup()
{
	if (!LocalGlobalVars::is_initialized) {
		return;
	}

	LocalGlobalVars::is_initialized = false;
	log_cleanup();
}

LIBLOG_API void pls_set_log_handler(pls_log_handler_t handler, void *param)
{
	LocalGlobalVars::log_handler = handler;
	LocalGlobalVars::log_param = param;
}
LIBLOG_API void pls_get_log_handler(pls_log_handler_t *handler, void **param)
{
	if (handler)
		*handler = LocalGlobalVars::log_handler;
	if (param)
		*param = LocalGlobalVars::log_param;
}
LIBLOG_API void pls_reset_log_handler()
{
	LocalGlobalVars::log_handler = def_log_handler;
	LocalGlobalVars::log_param = nullptr;
}

static void set_user_id(bool kr, const char *user_id)
{
	if (!user_id || !LocalGlobalVars::log_proc_thread) {
		return;
	}

	if (auto log_msg = new_log_msg(log_msg_t::MT_SET_USER_ID, kr, user_id); log_msg) {
		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	} else {
		PLS_ERROR(LIBLOG_MODULE, "%s nelo2 set user id failed, because malloc memory failed.", kr ? "kr" : "cn");
	}
}
LIBLOG_API void pls_set_user_id(const char *user_id, pls_set_tag_t set_tag)
{
	switch (set_tag) {
	case PLS_SET_TAG_ALL:
		set_user_id(false, user_id);
		set_user_id(true, user_id);
		break;
	case PLS_SET_TAG_CN:
		set_user_id(false, user_id);
		break;
	case PLS_SET_TAG_KR:
		set_user_id(true, user_id);
		break;
	default:
		break;
	}
}
static void add_global_field(bool kr, const char *key, const char *value)
{
	if (!key || !value || !LocalGlobalVars::log_proc_thread) {
		return;
	}

	auto key_length = int(strlen(key) + 1);
	auto value_length = int(strlen(value) + 1);
	auto body_length = int(sizeof(add_global_field_t) + key_length + value_length);

	add_global_field_t *add_global_field = nullptr;
	if (auto log_msg = new_log_msg(add_global_field, log_msg_t::MT_ADD_GLOBAL_FIELD, kr, body_length); log_msg) {
		auto buffer = (char *)(add_global_field + 1);

		add_global_field->key_offset = int(buffer - (char *)add_global_field);
		strcpy(buffer, key);
		buffer += key_length;

		add_global_field->value_offset = int(buffer - (char *)add_global_field);
		strcpy(buffer, value);

		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	} else {
		PLS_ERROR(LIBLOG_MODULE, "%s nelo2 add global field failed, because malloc memory failed.", kr ? "kr" : "cn");
	}
}
LIBLOG_API void pls_add_global_field(const char *key, const char *value, pls_set_tag_t set_tag)
{
	if (!qstricmp(key, "prismSession")) {
		init_log_file(value);
	}

	switch (set_tag) {
	case PLS_SET_TAG_ALL:
		add_global_field(false, key, value);
		add_global_field(true, key, value);
		break;
	case PLS_SET_TAG_CN:
		add_global_field(false, key, value);
		break;
	case PLS_SET_TAG_KR:
		add_global_field(true, key, value);
		break;
	default:
		break;
	}
}
static void remove_global_field(bool kr, const char *key)
{
	if (!key || !LocalGlobalVars::log_proc_thread) {
		return;
	}

	if (auto log_msg = new_log_msg(log_msg_t::MT_REMOVE_GLOBAL_FIELD, kr, key); log_msg) {
		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	} else {
		PLS_ERROR(LIBLOG_MODULE, "%s nelo2 remove global field failed, because malloc memory failed.", kr ? "kr" : "cn");
	}
}
LIBLOG_API void pls_remove_global_field(const char *key, pls_set_tag_t set_tag)
{
	switch (set_tag) {
	case PLS_SET_TAG_ALL:
		remove_global_field(false, key);
		remove_global_field(true, key);
		break;
	case PLS_SET_TAG_CN:
		remove_global_field(false, key);
		break;
	case PLS_SET_TAG_KR:
		remove_global_field(true, key);
		break;
	default:
		break;
	}
}

static int get_format_param_count(const char *format)
{
	if (!format || !format[0]) {
		return 0;
	}

	int count = 0;
	while (format[0]) {
		if (format[0] == '%') {
			if (format[1] == '%') {
				++format;
			} else {
				++count;
			}
		}
		++format;
	}
	return count;
}

LIBLOG_API void pls_logva(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, va_list args)
{
	pls_logvaex(kr, log_level, module_name, file_name, file_line, {}, arg_count, format, args);
}
LIBLOG_API void pls_logvaex(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields,
			    int arg_count, const char *format, va_list args)
{
	if ((!format || !format[0]) || ((arg_count >= 0) && (get_format_param_count(format) != arg_count))) {
		PLS_WARN("liblog", "log format param error, format: %s, argcount: %d, file: %s + %d", format ? format : "nullptr", arg_count, file_name ? file_name : "null", file_line);
		assert(false && "You are passing wrong params!");
		return;
	}

	pls_datetime_t time;
	pls_get_current_datetime(time);

	uint32_t tid = pls_current_thread_id();

	file_name = file_name ? pls_get_path_file_name(file_name) : file_name;
	generic_logva(kr, log_level, module_name, time, tid, file_name, file_line, fields, format, args);
}
LIBLOG_API void pls_log(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, log_level, module_name, file_name, file_line, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_logex(bool kr, pls_log_level_t log_level, const char *module_name, const char *file_name, int file_line, const std::vector<std::pair<const char *, const char *>> &fields,
			  int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logvaex(kr, log_level, module_name, file_name, file_line, fields, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_error(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_ERROR, module_name, file_name, file_line, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_warn(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_WARN, module_name, file_name, file_line, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_info(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_INFO, module_name, file_name, file_line, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_debug(bool kr, const char *module_name, const char *file_name, int file_line, int arg_count, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	pls_logva(kr, PLS_LOG_DEBUG, module_name, file_name, file_line, arg_count, format, args);
	va_end(args);
}
LIBLOG_API void pls_ui_step(bool kr, const char *module_name, const char *controls, const char *action, const char *file_name, int file_line)
{
	pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, {});
}
LIBLOG_API void pls_ui_stepex(bool kr, const char *module_name, const char *controls, const char *action, const char *file_name, int file_line,
			      const std::vector<std::pair<const char *, const char *>> &fields)
{
	pls_datetime_t time;
	pls_get_current_datetime(time);

	uint32_t tid = pls_current_thread_id();

	file_name = file_name ? pls_get_path_file_name(file_name) : file_name;

	auto tmp_fields = fields;
	tmp_fields.push_back({"ui-step", "all"});
	generic_log(kr, PLS_LOG_INFO, module_name, time, tid, file_name, file_line, tmp_fields, "UI: [UI STEP] %s %s", controls, action);
}

static void send_kvpairs_to_cn(const pls::map<std::string, std::string> &pairs, int messate_type)
{
	if (!LocalGlobalVars::log_proc_thread)
		return;

	int body_length = sizeof(kvpairs_t);
	for (const auto &pair : pairs) {
		body_length += int(sizeof(kvpair_t) + pair.first.length() + 1 + pair.second.length() + 1);
	}

	kvpairs_t *kvpairs = nullptr;
	if (auto log_msg = new_log_msg(kvpairs, messate_type, false, body_length); log_msg) {
		kvpairs->kv_count = (std::uint32_t)pairs.size();
		auto kvpair = (kvpair_t *)(kvpairs + 1);
		for (const auto &pair : pairs) {
			kvpair->key_length = (std::uint32_t)(pair.first.length() + 1);
			kvpair->value_length = (std::uint32_t)(pair.second.length() + 1);

			auto key = (char *)(kvpair + 1);
			memcpy(key, pair.first.c_str(), kvpair->key_length);
			auto value = key + kvpair->key_length;
			memcpy(value, pair.second.c_str(), kvpair->value_length);
			kvpair = (kvpair_t *)(value + kvpair->value_length);
		}

		pls_thread_queue_push(LocalGlobalVars::log_proc_thread, &log_msg->list_item);
	} else {
		PLS_ERROR(LIBLOG_MODULE, "cn %d send kvpairs failed, because malloc memory failed.", messate_type);
	}
}
LIBLOG_API void pls_subprocess_exception(const char *process, const char *pid, const char *src)
{
	if (process && process[0] && pid && pid[0] && src && src[0]) {
		send_kvpairs_to_cn({{"process", process}, {"pid", pid}, {"src", src}}, log_msg_t::MT_SUBPROCESS_EXCEPTION | log_msg_t::MT_CN_TAG);
	}
}
LIBLOG_API void pls_runtime_stats(pls_runtime_stats_type_t runtime_stats_type, const std::chrono::steady_clock::time_point &time, const map_t<std::string, std::string> &values)
{
	pls::map<std::string, std::string> kvpairs = values;
	kvpairs["runtime_stats_type"] = std::to_string(runtime_stats_type);
	kvpairs["time"] = std::to_string(std::chrono::time_point_cast<std::chrono::milliseconds>(time).time_since_epoch().count());
	send_kvpairs_to_cn(kvpairs, log_msg_t::MT_RUNTIME_STATS | log_msg_t::MT_CN_TAG);
}

LIBLOG_API void pls_set_gcc(const char *gcc)
{
	LocalGlobalVars::log_gcc = gcc;
}

LIBLOG_API std::vector<uint32_t> pls_get_logger_pids()
{
	std::vector<uint32_t> pids;
	if (auto pid = pls_process_id(LocalGlobalVars::logger_process); pid > 0) {
		pids.push_back(pid);
	}
	return pids;
}
