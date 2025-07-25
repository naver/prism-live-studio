#pragma once
#include <functional>
#include <chrono>
#include <map>
#include "qglobal.h"

#ifdef Q_OS_WIN
#ifdef LIBDUMPANALUZER_LIB
#define LIBDUMPANALUZER_API __declspec(dllexport)
#else
#define LIBDUMPANALUZER_API __declspec(dllimport)
#endif

#else
#define LIBDUMPANALUZER_API
#endif

class CAutoRunWhenSecEnd {
public:
	CAutoRunWhenSecEnd(std::function<void()> f) : func(f) {}
	virtual ~CAutoRunWhenSecEnd() { func(); }

private:
	std::function<void()> func;
};

#define COMBINE2(a, b) a##b
#define COMBINE1(a, b) COMBINE2(a, b)

#define RUN_WHEN_SECTION_END(lambda) CAutoRunWhenSecEnd COMBINE1(autoRunVariable, __LINE__)(lambda)

using FoundDumpFunc = void (*)(const std::string &exit_type, const std::string &process_name);
using LogFunc = void (*)(const std::map<std::string, std::string, std::less<>> &all);
using ProcessFunc = void (*)(std::string const &location, std::string const &url);

enum class DumpType : unsigned int {
	DT_NONE = 0,
	DT_UI_BLOCK,
	DT_CRASH,
	DT_DISAPPEAR,
};

LIBDUMPANALUZER_API struct ProcessInfo {
	bool is_main = false; // true: PRISM master process
	bool test_module_crash = false;
	bool sync_send_dump = false;
	bool analysis_stack = true;
	bool pc_shutdown_happen = false;
	std::string dump_path; // Folder directory generated by dump file.  default path : USER_APP_PATH\\PRISMLiveStudio\\crashDump
	std::string dump_file; // Default file : The latest dump file in the 'dump_path' folder directory.
	std::string user_id;
	std::string prism_version;
	std::string prism_session;
	std::string project_name;
	std::string cpu_name;
	std::string video_adapter_name;
	std::string pid;          // process ID
	std::string process_name; // process name
	std::string src;          // cam-session.exe process cameara source ptr
	std::string logFrom;
	std::string setup_session;
	std::string prism_sub_session;
	std::string os_type;
	DumpType dump_type = DumpType::DT_NONE;
	unsigned long block_timeout_s = 0;
	std::chrono::steady_clock::time_point block_time_ms;

	FoundDumpFunc found_dumo_func = nullptr;
	LogFunc log_func = nullptr;
	ProcessFunc process_func = nullptr;

	int duplicate_crash_limit = 2;

	ProcessInfo() {};
	ProcessInfo(std::string process_name_, std::string pid_, std::string user_id_, std::string version, std::string session, std::string subsession, std::string _log_from, std::string _os_type,
		    std::string project_name_, std::string cpu_name_, std::string video_adapter_name_, FoundDumpFunc found_dumo_func_ = nullptr, LogFunc log_func_ = nullptr,
		    ProcessFunc process_func_ = nullptr, bool pc_shutdown = false)
		: process_name(process_name_),
		  pid(pid_),
		  user_id(user_id_),
		  prism_version(version),
		  prism_session(session),
		  prism_sub_session(subsession),
		  logFrom(_log_from),
		  os_type(_os_type),
		  project_name(project_name_),
		  cpu_name(cpu_name_),
		  video_adapter_name(video_adapter_name_),
		  found_dumo_func(found_dumo_func_),
		  log_func(log_func_),
		  process_func(process_func_),
		  pc_shutdown_happen(pc_shutdown) {};
};
