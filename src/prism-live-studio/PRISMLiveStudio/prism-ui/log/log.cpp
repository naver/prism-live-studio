#include "log.h"

#include <qglobal.h>
#include <qdatetime.h>
#include <pls/pls-base.h>

#include "ui-config.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "prism-version.h"
#include "PLSPlatformPrism.h"
#include "PLSApp.h"
#include "liblog.h"
#include "pls-shared-values.h"
#include <stdlib.h>

const int LOG_SUBPROCESS_EXCEPTION = -100;

static void def_prism_handler(int log_level, const char *format, va_list args, void *param)
{
	Q_UNUSED(log_level)
	Q_UNUSED(format)
	Q_UNUSED(args)
	Q_UNUSED(param)
}

static log_handler_t log_handler = def_prism_handler;
static void *log_param = nullptr;
static std::string log_session_id;

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

static void def_obs_log_handler(int log_level, const char *format, va_list args, void *)
{
	switch (log_level) {
	case LOG_ERROR:
		pls_logvaex(false, PLS_LOG_ERROR, "obs", nullptr, 0, {}, -1, format, args);
		break;
	case LOG_WARNING:
		pls_logvaex(false, PLS_LOG_WARN, "obs", nullptr, 0, {}, -1, format, args);
		break;
	case LOG_INFO:
		pls_logvaex(false, PLS_LOG_INFO, "obs", nullptr, 0, {}, -1, format, args);
		break;
	case LOG_DEBUG:
		pls_logvaex(false, PLS_LOG_DEBUG, "obs", nullptr, 0, {}, -1, format, args);
		break;
	default:
		break;
	}
}

static void def_obs_log_handler(bool kr, int log_level, const char *format, va_list args, const char *fields[][2], int field_count, void *)
{
	std::vector<std::pair<const char *, const char *>> tmp_fields;
	for (int i = 0; i < field_count; ++i) {
		tmp_fields.push_back({fields[i][0], fields[i][1]});
	}

	int arg_count = -1;
	switch (log_level) {
	case LOG_ERROR:
		pls_logvaex(kr, PLS_LOG_ERROR, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_WARNING:
		pls_logvaex(kr, PLS_LOG_WARN, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_INFO:
		pls_logvaex(kr, PLS_LOG_INFO, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_DEBUG:
		pls_logvaex(kr, PLS_LOG_DEBUG, "obs", nullptr, 0, tmp_fields, arg_count, format, args);
		break;
	case LOG_SUBPROCESS_EXCEPTION:
		if (field_count == 2) {
			const char *process = nullptr;
			const char *pid = nullptr;
			for (int i = 0; i < field_count; ++i) {
				if (!strcmp(fields[i][0], "process")) {
					process = fields[i][1];
				} else if (!strcmp(fields[i][0], "pid")) {
					pid = fields[i][1];
				}
			}

			if (process && pid) {
				pls_subprocess_exception(process, pid);
			}
		}
		break;
	}
}

static std::string parse_bcrash_reason(const char *desc)
{
	std::string reason = desc;

	std::vector<std::string> predefine = {
		"Out of memory while trying to allocate", "obs-hotkey: Could not find hotkey id",
		// add more string which need to be handled
	};

	for (const auto &item : predefine) {
		if (reason.find(item) != std::string::npos) {
			reason = item;
			break;
		}
	}

	return reason;
}

static void def_obs_crash_handler(const char *fmt, va_list args, void *param)
{
	std::array<char, 1024> desc;
	vsnprintf(desc.data(), desc.size(), fmt, args);

	auto reason = parse_bcrash_reason(desc.data());
	PLS_LOGEX(PLS_LOG_ERROR, NOTICE_MODULE, {{PTS_LOG_TYPE, PTS_TYPE_EVENT}, {"EngineCrash", reason.c_str()}}, "bcrash is called from libobs, reason: %s", desc.data());

	auto title = QTStr("obs.engine.error.title");
	auto content = QTStr("obs.engine.error.content").arg(desc.data());
	pls_sync_call(qApp, [=]() { PLSAlertView::information(nullptr, title, content); });

#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(), (UINT)init_exception_code::obs_engine_error);
#else
	exit((int)init_exception_code::obs_engine_error);
#endif

	UNUSED_PARAMETER(param);
}

static void def_pls_log_handler(bool kr, pls_log_level_t log_level, const char *module_name, const pls_datetime_t &time, uint32_t tid, const char *file_name, int file_line, const char *message,
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

static void def_pls_add_global_field_cn(const char *key, const char *value)
{
	pls_add_global_field(key, value, PLS_SET_TAG_CN);
}

bool log_init(const char *session_id, const std::chrono::steady_clock::time_point &startTime, const char *sub_session_id)
{
	log_session_id = session_id;
	base_set_log_handler(def_obs_log_handler, nullptr);
	base_set_log_handler_ex(def_obs_log_handler, nullptr);
	base_set_log_global_field_handler_cn(def_pls_add_global_field_cn);
	base_set_crash_handler(def_obs_crash_handler, nullptr);

	pls_prism_log_init(PLS_VERSION, "prism-log", log_session_id.c_str());
	pls_add_global_field("prismSession", session_id);
#if defined(Q_OS_WIN)
	pls_add_global_field("OSType", "Windows");
#elif defined(Q_OS_MACOS)
	pls_add_global_field("OSType", "MAC");
#endif
	if (!pls_is_empty(sub_session_id)) {
		pls_add_global_field("prismSubSession", sub_session_id);
	}
	auto hashMac = QString("%1").arg(std::hash<std::string>()(pls_get_local_mac().toStdString()), 16, 16, QChar('0'));
	pls_add_global_field("hashMac", hashMac.toUtf8().constData());
	pls_runtime_stats(PLS_RUNTIME_STATS_TYPE_APP_START, startTime);

	pls_set_log_handler(def_pls_log_handler, nullptr);
	return true;
}
void log_cleanup()
{
	pls_log_cleanup();
	pls_reset_log_handler();
}

void set_log_handler(log_handler_t handler, void *param)
{
	log_handler = handler;
	log_param = param;
}

void runtime_stats(pls_runtime_stats_type_t runtime_stats_type, const std::chrono::steady_clock::time_point &time)
{
}

static QString toString(const QJsonArray &array)
{
	QStringList strs;
	for (int i = 0, count = array.count(); i < count; ++i) {
		strs.append(array[i].toString());
	}
	return strs.join(',');
}

void runtime_stats(pls_runtime_stats_type_t runtime_stats_type, const std::chrono::steady_clock::time_point &time, const QNetworkRequest &req)
{
	pls::map<std::string, std::string> values;

	values["url"] = req.url().toString(QUrl::FullyEncoded).toStdString();

	for (const QByteArray &name : req.rawHeaderList()) {
		values[("header-" + name).toStdString()] = req.rawHeader(name).toStdString();
	}

	QJsonObject baseInfo;
	PLSApp::setAnalogBaseInfo(baseInfo);
	for (const QString &key : baseInfo.keys()) {
		QJsonValue value = baseInfo[key];
		if (value.isString()) {
			values[("body-string-" + key).toStdString()] = value.toString().toStdString();
		} else if (value.isArray()) {
			values[("body-array-" + key).toStdString()] = toString(value.toArray()).toStdString();
		}
	}

	values["body-string-sessionId"] = log_session_id;
	values["body-int-unixTime"] = std::to_string(QDateTime::currentMSecsSinceEpoch());

	pls_runtime_stats(runtime_stats_type, time, values);
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
	pls_logvaex(kr, log_level, module_name, file_name, file_line, {{"flow", flow}}, arg_count, format, args);
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
		pls_logvaex(kr, log_level, module_name, file_name, file_line, {{"flow", flow}}, arg_count, format, args);
	} else {
		pls_logvaex(kr, log_level, module_name, file_name, file_line, {{"flow", flow}, {"videoSeq", video_seq}}, arg_count, format, args);
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
		pls_logvaex(kr, log_level, module_name, file_name, file_line, {{"flow", flow}, {"liveAbort", otherFlow}}, arg_count, format, args);
	} else {
		pls_logvaex(kr, log_level, module_name, file_name, file_line, {{"flow", flow}, {"videoSeq", video_seq}, {"liveAbort", otherFlow}}, arg_count, format, args);
	}
	va_end(args);
}

void pls_flow_ui(bool kr, const char *module_name, const char *flow, const char *controls, const char *action, const char *file_name, int file_line)
{
	pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, {{"flow", flow}});
}

void pls_flow2_ui(bool kr, const char *module_name, const char *flow, const char *video_seq, const char *controls, const char *action, const char *file_name, int file_line)
{
	auto isEmpty = nullptr == video_seq || video_seq[0] == '\0';
	if (isEmpty) {
		pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, {{"flow", flow}});
	} else {
		pls_ui_stepex(kr, module_name, controls, action, file_name, file_line, {{"flow", flow}, {"videoSeq", video_seq}});
	}
}
