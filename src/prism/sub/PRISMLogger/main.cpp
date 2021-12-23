#include <nelo2.h>
#include <liblog.h>
#include <Windows.h>
#include <process.h>
#include <vector>
#include <map>
#include "string-convert.h"
#include "send-dump.hpp"

#pragma comment(lib, "ws2_32.lib")

#define ConsolePrint(format, ...) printf(format, __VA_ARGS__)

#pragma pack(push, 1)
enum message_type_t {
	// types
	MT_SET_USER_ID = 1,
	MT_ADD_GLOBAL_FIELD,
	MT_REMOVE_GLOBAL_FIELD,
	MT_LOG,
	MT_EXIT,
	MT_CRASHED,
	MT_SUBPROCESS_DISAPPEAR,

	// tag
	MT_CN_TAG = 0x0000,
	MT_KR_TAG = 0x1000,

	// mask
	MT_KR_MASK = 0x1000,
	MT_TYPE_MASK = 0x0fff
};

struct message_t {
	int type;
	int total_length;
};

struct add_global_field_t {
	message_t message;
	int key_offset;
	int value_offset;
};

enum log_type_t {
	// types
	LT_GENERIC_LOG = 1,
	LT_ACTION_LOG,

	// tag
	LT_CN_TAG = 0x0000,
	LT_KR_TAG = 0x1000,

	// mask
	LT_KR_MASK = 0x1000,
	LT_TYPE_MASK = 0x0fff
};

struct list_t {
	list_t *prev;
	list_t *next;
};
struct log_info_t {
	union {
		message_t message;
		list_t listpos;
	};
	int type;
	int total_length;
};
struct generic_log_info_t {
	log_info_t log_info;
	pls_log_level_t log_level;
	int file_line;
	uint32_t tid;
	int field_count;
	pls_time_t time;
	int module_name_offset;
	int file_name_offset;
	int fields_offset;
	int message_offset;
	//char *module_name;
	//char *file_name;
	//char ***fields;
	//char *message;
};
struct generic_log_field_t {
	int key_offset;
	int value_offset;
	int next_offset;
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
struct kvpair_t {
	uint32_t key_length;
	uint32_t value_length;
	/*char key[key_length];
	* char value[value_length];
	*/
};
struct kvpairs_t {
	uint32_t kv_count;
	/* kvpair_t ky[kv_count] */
};
#pragma pack(pop)

enum ExitCode {
	EC_Ok,
	EC_Nelo2InitFailed,
	EC_CreateNamedPipeFailed,
	EC_ConnectNamedPipeFailed,
	EC_ParamsError,
};

DWORD block_timeout_s = 5; // in seconds
DWORD block_time_ms = 0;   // in miliseconds
std::string logFrom = "ExternalLog";
static std::string block_dump_path = "";
static std::string prism_user_id = "";
static std::string prism_version = "";
static std::string prism_session = "";
static std::string prism_project_name = "";
static std::string prism_cpu_name = "";
static std::string prism_video_adapter = "";

NELO2Log *nelo2_log = nullptr;

static void log_process(NELO2Log *nelo2_log, generic_log_info_t *log_info);
static void set_user_id(NELO2Log *nelo2_log, const char *user_id);
static void add_global_field(NELO2Log *nelo2_log, add_global_field_t *field);
static void remove_global_field(NELO2Log *nelo2_log, const char *key);
static bool read_n(HANDLE named_pipe, char *buffer, int count);
static message_t *read_message(HANDLE named_pipe, message_t *msg);
static bool is_file_exist(const char *path);
static void __cdecl nelo_send_failed_callback(const NELO_READONLY_PAIR *fields, size_t nFields, const char *errMsg, void *context);
static void parse_kvpairs(std::map<std::string, std::string> &pairs, message_t *msg);

typedef void (*found_dump_callback)(bool found);
extern bool wait_send_prism_dump(const std::string &prism_pid, const std::string &user_id, const std::string &prism_version, const std::string &prism_session, const std::string &project_name,
				 const std::string &cpu_name, const std::string &video_adapter_name);
extern void send_subprocess_dump(const std::string &subprocess_name, const std::string &subprocess_pid, const std::string &src, const std::string &user_id, const std::string &prism_version,
				 const std::string &prism_session, const std::string &project_name, const std::string &cpu_name, std::string &video_adapter_name, found_dump_callback callback);

int main(int argc, char **argv)
{
	/**
	  *     [in]1 projectName: project name
	  *     [in]2 projectVersion: project version
	  *     [in]3 logSource: log source
	  *     [in]4 clientPid: client process id
	  *     [in]5 clientCid: client connection id
	  */
	if (argc < 6) {
		return EC_ParamsError;
	}

	const char *projectName = argv[1];
	const char *projectVersion = argv[2];
	const char *logSource = argv[3];
	const char *clientPid = argv[4];
	const char *clientCid = argv[5];
	bool cn = !strcmp("cn", clientCid);

	int retval = EC_Ok;

	HANDLE named_pipe = nullptr;
	message_t message;

	ConsolePrint("projectName: %s\n", projectName);
	ConsolePrint("projectVersion: %s\n", projectVersion);
	ConsolePrint("logSource: %s\n", logSource);
	ConsolePrint("clientPid: %s\n", clientPid);
	ConsolePrint("clientCid: %s\n", clientCid);

	char loggerRunEventName[128];
	sprintf_s(loggerRunEventName, "PRISMLoggerRunEvent%s", clientPid);
	HANDLE logger_run_event = OpenEventA(EVENT_ALL_ACCESS, FALSE, loggerRunEventName);
	if (!logger_run_event) {
		ConsolePrint("open PRISMLoggerRunEvent failed\n");
		return -1;
	}

	auto bAbnormalExit = false;
	auto bExitByCrashed = false;

	nelo2_log = new NELO2Log();
	if (NELO2_OK != nelo2_log->initialize(projectName, projectVersion, logSource, "nelo2-app", "nelo2-col.navercorp.com", 80)) {
		ConsolePrint("NELO2 initialize failed\n");
		retval = EC_Nelo2InitFailed;
		goto EXIT_APP;
	} else if (!nelo2_log->openCrashCatcher(false, NELO_LANG_ENGLISH)) {
		ConsolePrint("NELO2 openCrashCatcher failed\n");
		retval = EC_Nelo2InitFailed;
		goto EXIT_APP;
	}
	//nelo2_log->disableHostField();
	nelo2_log->setSendFailedCallback(nelo_send_failed_callback);

	prism_version = projectVersion;

	char pipeName[256];
	sprintf_s(pipeName, "\\\\.\\pipe\\PRISMLoggerPipe_%s_%s", clientPid, clientCid);
	if ((named_pipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 10240, 10240, 1000, NULL)) == INVALID_HANDLE_VALUE) {
		ConsolePrint("CreateNamedPipe failed\n");
		retval = EC_CreateNamedPipeFailed;
		goto EXIT_APP;
	}

	if (logger_run_event) {
		SetEvent(logger_run_event);
		CloseHandle(logger_run_event);
		logger_run_event = NULL;
	}

	if (!ConnectNamedPipe(named_pipe, NULL) && (GetLastError() != ERROR_PIPE_CONNECTED)) {
		ConsolePrint("ConnectNamedPipe failed\n");
		retval = EC_ConnectNamedPipeFailed;
		goto EXIT_APP;
	}

	bAbnormalExit = true;

	while (true) {
		if (!read_n(named_pipe, (char *)&message, sizeof(message))) {
			ConsolePrint("read message failed\n");
			break;
		}

		if ((message.type & MT_TYPE_MASK) == MT_LOG) {
			message_t *msg = read_message(named_pipe, &message);
			if (msg) {
				log_process(nelo2_log, (generic_log_info_t *)msg);
				free(msg);
			} else {
				break;
			}
		} else if ((message.type & MT_TYPE_MASK) == MT_SET_USER_ID) {
			message_t *msg = read_message(named_pipe, &message);
			if (msg) {
				set_user_id(nelo2_log, (char *)(msg + 1));
				free(msg);
			} else {
				break;
			}
		} else if ((message.type & MT_TYPE_MASK) == MT_ADD_GLOBAL_FIELD) {
			message_t *msg = read_message(named_pipe, &message);
			if (msg) {
				add_global_field(nelo2_log, (add_global_field_t *)msg);
				free(msg);
			} else {
				break;
			}
		} else if ((message.type & MT_TYPE_MASK) == MT_REMOVE_GLOBAL_FIELD) {
			message_t *msg = read_message(named_pipe, &message);
			if (msg) {
				remove_global_field(nelo2_log, (char *)(msg + 1));
				free(msg);
			} else {
				break;
			}
		} else if ((message.type & MT_TYPE_MASK) == MT_EXIT) {
			ConsolePrint("read exit message\n");
			bAbnormalExit = false;
			break;
		} else if ((message.type & MT_TYPE_MASK) == MT_CRASHED) {
			bExitByCrashed = true;
		} else if ((message.type & MT_TYPE_MASK) == MT_SUBPROCESS_DISAPPEAR) {
			message_t *msg = read_message(named_pipe, &message);
			if (msg) {
				std::map<std::string, std::string> infos;
				parse_kvpairs(infos, msg);
				if (infos.size() == 3) {
					//{{"process", process}, {"pid", pid},{"src", src}}

					auto callback([](bool found) {
						NELO2Log::CustomField customField;
						const char *abnormalExitLog = nullptr;
						if (found) {
							customField.addField("SubExitType", "DisappearWithDump");
							abnormalExitLog = "SUB_PROCESS exited abnormally [DisappearWithDump]";
						} else {
							customField.addField("SubExitType", "DisappearWithoutDump");
							abnormalExitLog = "SUB_PROCESS exited abnormally [DisappearWithoutDump]";
						}
						nelo2_log->sendLog(NELO_LL_FATAL, abnormalExitLog, customField);
						return;
					});

					send_subprocess_dump(infos["process"], infos["pid"], infos["src"], prism_user_id, prism_version, prism_session, projectName, prism_cpu_name,
							     prism_video_adapter, callback);
				}
				free(msg);
			} else {
				break;
			}
		}
	}

EXIT_APP:
	ConsolePrint("app exit\n");

	if (logger_run_event) {
		SetEvent(logger_run_event);
		CloseHandle(logger_run_event);
		logger_run_event = NULL;
	}

	if (named_pipe) {
		CloseHandle(named_pipe);
		named_pipe = nullptr;
	}

	bool dum_sended = false;
	if (cn && !block_dump_path.empty()) {
		dum_sended = send_to_nelo(block_dump_path.c_str(), prism_user_id.c_str(), prism_version.c_str(), prism_session.c_str(), projectName, prism_cpu_name.c_str(),
					  prism_video_adapter.c_str(), DumpType::DT_UI_BLOCK, clientPid);
	}

	if (nelo2_log) {
		if (cn && bAbnormalExit) {
			if (bExitByCrashed || block_dump_path.empty()) {
				nelo2_log->sendLog(NELO_LL_INFO, "PRISM is not blocked.");
			} else {
#ifdef _DEBUG
				bool debug_mode = true;
#else
				bool debug_mode = false;
#endif
				char log[4096];
				sprintf_s(log, sizeof(log) / sizeof(log[0]), "%sPRISM is blocked. Send result : %s", debug_mode ? "[Debug Mode] " : "", dum_sended ? "yes" : "error");

				nelo2_log->sendLog(NELO_LL_WARN, log);
			}

			NELO2Log::CustomField customField;
			const char *abnormalExitLog = nullptr;
			if (bExitByCrashed) {
				customField.addField("ExitType", "Crashed");
				abnormalExitLog = "PRISM exited abnormally [Crashed]";
			} else {
				// check system captured dump file
				// wait for dump write finish
				QString dump_file_path;
				if (wait_send_prism_dump(clientPid, prism_user_id, prism_version, prism_session, projectName, prism_cpu_name, prism_video_adapter)) {
					customField.addField("ExitType", "DisappearWithDump");
					abnormalExitLog = "PRISM exited abnormally [DisappearWithDump]";
				} else {
					customField.addField("ExitType", "DisappearWithoutDump");
					abnormalExitLog = "PRISM exited abnormally [DisappearWithoutDump]";
				}
			}
			nelo2_log->sendLog(NELO_LL_FATAL, abnormalExitLog, customField);
		}

		nelo2_log->closeCrashCatcher();
		nelo2_log->destory();
		delete nelo2_log;
		nelo2_log = nullptr;
	}

	return retval;
}

static void log_process(NELO2Log *nelo2_log, generic_log_info_t *log_info)
{
	char *module_name = ((char *)log_info) + log_info->module_name_offset;
	char *file_name = log_info->file_name_offset > 0 ? (((char *)log_info) + log_info->file_name_offset) : nullptr;
	char *message = ((char *)log_info) + log_info->message_offset;

	int new_sum = log_info->log_info.total_length;
	std::vector<char> msg;
	msg.resize(new_sum + 128);

	if (file_name && (log_info->file_line > 0)) {
		snprintf(msg.data(), msg.size() - 1, "%04u-%02u-%02u_%02u:%02u:%02u.%03u%+03d:%02d [%s:%d] (%u) %s", log_info->time.year, log_info->time.month, log_info->time.day, log_info->time.hour,
			 log_info->time.minute, log_info->time.second, log_info->time.milliseconds, (0 - log_info->time.timezone / 60), (abs(log_info->time.timezone) % 60), file_name,
			 log_info->file_line, log_info->tid, message);
	} else {
		snprintf(msg.data(), msg.size() - 1, "%04u-%02u-%02u_%02u:%02u:%02u.%03u%+03d:%02d (%u) %s", log_info->time.year, log_info->time.month, log_info->time.day, log_info->time.hour,
			 log_info->time.minute, log_info->time.second, log_info->time.milliseconds, (0 - log_info->time.timezone / 60), (abs(log_info->time.timezone) % 60), log_info->tid, message);
	}

	ConsolePrint("%s\n", msg.data());

	NELO2Log::CustomField customField;
	customField.addField("module", module_name);
	if (log_info->field_count > 0) {
		char *fields = ((char *)log_info) + log_info->fields_offset;
		for (int i = 0; i < log_info->field_count; ++i) {
			generic_log_field_t *field = (generic_log_field_t *)fields;
			customField.addField(((char *)log_info) + field->key_offset, ((char *)log_info) + field->value_offset);
			fields = ((char *)log_info) + field->next_offset;
		}
	}

	switch (log_info->log_level) {
	case PLS_LOG_ERROR:
		nelo2_log->sendLog(NELO_LL_ERROR, msg.data(), customField);
		break;
	case PLS_LOG_WARN:
		nelo2_log->sendLog(NELO_LL_WARN, msg.data(), customField);
		break;
	case PLS_LOG_INFO:
		nelo2_log->sendLog(NELO_LL_INFO, msg.data(), customField);
		break;
	case PLS_LOG_DEBUG:
		nelo2_log->sendLog(NELO_LL_DEBUG, msg.data(), customField);
		break;
	}
}
static void set_user_id(NELO2Log *nelo2_log, const char *user_id)
{
	ConsolePrint("set user id: %s\n", user_id);
	nelo2_log->setUserId(user_id);
	prism_user_id = user_id;
}
static void add_global_field(NELO2Log *nelo2_log, add_global_field_t *field)
{
	char *key = ((char *)field) + field->key_offset;
	char *value = ((char *)field) + field->value_offset;
	ConsolePrint("add global field: key = %s, value = %s\n", key, value);

	if (0 == strcmp(key, "blockDumpPath")) {
		if (is_file_exist(value)) {
			if (block_dump_path.empty()) {
				block_time_ms = GetTickCount();
			}
			block_dump_path = value;
		} else {
			block_dump_path = "";
			block_time_ms = 0;
		}
		return;

	} else if (0 == strcmp(key, "blockTimeoutS")) {
		block_timeout_s = atoi(value);
		return;

	} else if (0 == strcmp(key, "prismSession")) {
		prism_session = value;
	} else if (0 == strcmp(key, "LogFrom")) {
		logFrom = value;
	} else if (0 == strcmp(key, "cpuName")) {
		prism_cpu_name = value;
	} else if (0 == strcmp(key, "videoAdapter")) {
		prism_video_adapter = value;
	}

	nelo2_log->addGlobalField(key, value);
}
static void remove_global_field(NELO2Log *nelo2_log, const char *key)
{
	ConsolePrint("remove global field: key = %s\n", key);
	nelo2_log->delGlobalField(key);
}
static bool read_n(HANDLE named_pipe, char *buffer, int count)
{
	DWORD read_bytes = 0;
	for (DWORD readed = 0; count > 0; count -= readed, read_bytes += readed) {
		if (!ReadFile(named_pipe, buffer + read_bytes, count, &readed, NULL)) {
			return false;
		}
	}
	return true;
}
static message_t *read_message(HANDLE named_pipe, message_t *message)
{
	if (message->total_length < sizeof(message_t)) {
		return nullptr;
	}

	char *buffer = (char *)malloc(message->total_length);
	if (!buffer) {
		ConsolePrint("malloc buffer failed\n");
		return nullptr;
	}

	if (read_n(named_pipe, buffer + sizeof(message_t), message->total_length - sizeof(message_t))) {
		memcpy(buffer, message, sizeof(message_t));
		return (message_t *)buffer;
	} else {
		ConsolePrint("read message failed\n");
		free(buffer);
		return nullptr;
	}
}
static bool is_file_exist(const char *path)
{
	WIN32_FIND_DATAW wfd;
	HANDLE hFind = FindFirstFileW(str::u2w(path).c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return true;
	} else {
		return false;
	}
}
static void __cdecl nelo_send_failed_callback(const NELO_READONLY_PAIR * /*fields*/, size_t /*nFields*/, const char *errMsg, void * /*context*/)
{
	ConsolePrint("nelo send failed, reason: %s\n", errMsg);
}
static void parse_kvpairs(std::map<std::string, std::string> &pairs, message_t *msg)
{
	kvpairs_t *kvpairs = (kvpairs_t *)(msg + 1);
	kvpair_t *kvpair = (kvpair_t *)(kvpairs + 1);
	for (int i = 0; i < kvpairs->kv_count; ++i) {
		char *key = (char *)(kvpair + 1);
		char *value = (char *)(key + kvpair->key_length);
		pairs.insert(std::map<std::string, std::string>::value_type(key, value));
		kvpair = (kvpair_t *)(value + kvpair->value_length);
	}
}

void send_log(NELO_LOG_LEVEL eLevel, const char *format, ...)
{
	char msg[4096];
	va_list args;
	va_start(args, format);
	vsnprintf(msg, sizeof(msg) - 1, format, args);
	va_end(args);
	if (nelo2_log)
		nelo2_log->sendLog(eLevel, msg);
}
