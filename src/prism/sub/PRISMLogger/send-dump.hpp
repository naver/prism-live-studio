#pragma once
#include <Windows.h>
#include <WinSock.h>
#include <memory>
#include <versionhelpers.h>
#include "util/windows/win-version.h"
#include "pls/pls-util.hpp"
#include "string-convert.h"
#include "ParseDumpFile.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QString>
#include <QFile>
#include <QDataStream>
#include <vector>
#include <nelo2.h>

#include <shlobj.h>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#define RESOURCE_PATH "data\\prism-studio"
#define USER_APP_PATH "\\PRISMLiveStudio"

#define CHILD_PROCESS_CRASH_FILE "//crashDump//childProcess.json"
#define GPOP_FILE "//user//gpop.json"

extern DWORD block_timeout_s;
extern DWORD block_time_ms;
extern std::string logFrom;
extern NELO2Log *nelo2_log;

extern void send_log(NELO_LOG_LEVEL eLevel, const char *format, ...);

#define Log(level, format, ...) send_log(level, format, __VA_ARGS__)

enum class DumpType {
	DT_UI_BLOCK,
	DT_CRASH_SUB,
	DT_CRASH_MAIN,
};

static std::shared_ptr<char> read_file(const char *path, size_t &length)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		assert(false);
		return NULL;
	}

	RUN_WHEN_SECTION_END([&]() { file.close(); });

	qint64 len = file.size();
	if (!len) {
		return NULL;
	}

	char *ptr = new char[len];
	if (!ptr) {
		return NULL;
	}

	std::shared_ptr<char> ret(ptr);
	file.read(ptr, len);

	length = len;
	return ret;
}

static void write_file(char buf[], int bufferSize, const std::string &pathStr)
{
#ifdef _WIN32
	int length = MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, nullptr, 0);
	if (length > 0) {
		wchar_t *ptemp = new (std::nothrow) wchar_t[length];
		if (!ptemp) {
			return;
		}
		MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, ptemp, length);
		FILE *stream = NULL;
		errno_t err = _wfopen_s(&stream, ptemp, L"wb");
		if (err == 0 && stream != NULL) {
			fwrite(buf, 1, bufferSize, stream);
			fclose(stream);
		}

		delete[] ptemp;
	}
#else
	FILE *stream = fopen(pathStr.c_str(), "wb");
	if (stream) {
		fwrite(buf, 1, bufferSize, stream);
		fclose(stream);
	}
#endif
}

static bool send_data(std::string &post_bodfy)
{
	bool send_ok = false;

	WSADATA wd;
	int ret = ::WSAStartup(MAKEWORD(2, 2), &wd);
	if (ret != NO_ERROR) {
		return false;
	}

	RUN_WHEN_SECTION_END([&]() { WSACleanup(); });

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		return false;
	}

	RUN_WHEN_SECTION_END([&]() { closesocket(s); });

	hostent *phost = gethostbyname("nelo2-col.navercorp.com");
	std::string ip_str = phost ? inet_ntoa(*(in_addr *)phost->h_addr_list[0]) : "";

	struct sockaddr_in sd = {0};
	sd.sin_addr.s_addr = inet_addr(ip_str.c_str());
	sd.sin_port = htons(80);
	sd.sin_family = AF_INET;

	ret = connect(s, (sockaddr *)&sd, sizeof(sd));
	if (ret == SOCKET_ERROR) {
		return false;
	}

	char content_length[20] = {0};
	sprintf(content_length, "%u", (unsigned)post_bodfy.length());

	std::string send_data = "POST /_store HTTP/1.1\n"
				"Connection: close\n"
				"Content-Type: application/x-www-form-urlencoded\n"
				"Host: nelo2-col.navercorp.com:80\n"
				"Content-Length:";

	send_data += std::string(content_length);
	send_data += "\n\n";
	send_data += post_bodfy;

	int sended_len = 0;
	while (true) {
		if (sended_len >= send_data.length()) {
			break; // send completed
		}
		int res = send(s, &send_data.c_str()[sended_len], int(send_data.length() - sended_len), 0);
		if (res > 0) {
			sended_len += res;
		} else {
			break;
		}
	}

	if (sended_len > 0) {
		std::string response = "";

		char buf[4096] = {0};
		int rcv_len = sizeof(buf) - sizeof(buf[0]);

		while (true) {
			int res = recv(s, buf, rcv_len, 0);
			if (res > 0) {
				buf[res] = 0;
				response += buf;
			} else {
				break;
			}
		}

		send_ok = (response.find("\"Success\"") != std::string::npos);
		assert(send_ok);
	}

	return send_ok;
}

static uint32_t get_windows_version()
{
	static uint32_t ver = 0;

	if (ver == 0) {
		struct win_version_info ver_info;

		get_win_ver(&ver_info);
		ver = (ver_info.major << 8) | ver_info.minor;
	}

	return ver;
}

static uint32_t get_windows_build_version()
{
	struct win_version_info ver_info;

	get_win_ver(&ver_info);
	return ver_info.build;
}

static QString get_windows_version_info()
{
	bool server = IsWindowsServer();
	uint32_t version_info = get_windows_version();

	QString ver_str("");
	if (version_info >= 0x0A00) {
		if (server) {
			ver_str = "Windows Server 2016 Technical Preview";
		} else {
			uint32_t build_version = get_windows_build_version();
			if (build_version >= 21664) {
				ver_str = "Windows 11";
			} else {
				ver_str = "Windows 10";
			}
		}
	} else if (version_info >= 0x0603) {
		if (server) {
			ver_str = "Windows Server 2012 r2";
		} else {
			ver_str = "Windows 8.1";
		}
	} else if (version_info >= 0x0602) {
		if (server) {
			ver_str = "Windows Server 2012";
		} else {
			ver_str = "Windows 8";
		}
	} else if (version_info >= 0x0601) {
		if (server) {
			ver_str = "Windows Server 2008 r2";
		} else {
			ver_str = "Windows 7";
		}
	} else if (version_info >= 0x0600) {
		if (server) {
			ver_str = "Windows Server 2008";
		} else {
			ver_str = "Windows Vista";
		}
	} else {
		ver_str = "Windows Before Vista";
	}

	QString bitStr = (sizeof(void *) == 8) ? "(x64)" : "(x32)";
	return (ver_str + bitStr);
}

static std::string get_resource_path()
{
	std::string exe_path;
	TCHAR app_path[MAX_PATH] = {0};
	GetModuleFileName(NULL, app_path, sizeof(app_path));
	PathRemoveFileSpec(app_path);
	exe_path = str::w2u(app_path);
	exe_path += "\\";
	return exe_path + RESOURCE_PATH;
}

static std::string get_userapp_path()
{
	TCHAR user_path[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, user_path);
	return str::w2u(user_path) + USER_APP_PATH;
}

static QJsonObject get_blacklist()
{
	QJsonObject blacklist{};
	size_t len = 0;
	std::string gpop = get_userapp_path() + GPOP_FILE;
	std::shared_ptr<char> gpop_data = read_file(gpop.c_str(), len);
	if (!gpop_data) {
		gpop = get_resource_path() + GPOP_FILE;
		len = 0;
		gpop_data = read_file(gpop.c_str(), len);
		if (!gpop_data) {
			Log(NELO_LL_WARN, "Read gpop file failed.");
			return blacklist;
		}
	}

	QByteArray ba;
	ba.append(gpop_data.get(), (int)len);
	QJsonObject obj = QJsonDocument::fromJson(ba).object();
	QJsonObject optional = obj["optional"].toObject();
	blacklist = optional["blacklist"].toObject();
	return blacklist;
}

static bool check_blacklist(QString value, QJsonObject blacklist, std::string &crashType)
{
	if (blacklist.isEmpty())
		return false;

	QJsonArray graphicsDrivers = blacklist["graphicsDrivers"].toArray();
	for (auto item : graphicsDrivers) {
		if (value == item.toString()) {
			crashType = "GraphicsDrivers BlackList Disappear";
			return true;
		}
	}
	QJsonArray deviceDrivers = blacklist["deviceDrivers"].toArray();
	for (auto item : deviceDrivers) {
		if (value == item.toString()) {
			crashType = "DeviceDrivers BlackList Disappear";
			return true;
		}
	}
	QJsonArray thirdPlugins = blacklist["3rdPlugins"].toArray();
	for (auto item : thirdPlugins) {
		if (value == item.toString()) {
			crashType = "3rd Plugins BlackList Disappear";
			return true;
		}
	}
	QJsonArray vstPlugins = blacklist.value("vstPlugins").toArray();
	for (auto item : vstPlugins) {
		if (value == item.toString()) {
			crashType = "VST Plugins BlackList Disappear";
			return true;
		}
	}
	QJsonArray thirdPrograms = blacklist.value("3rdPrograms").toArray();
	for (auto item : thirdPrograms) {
		if (value == item.toString()) {
			crashType = "3rd Programs BlackList Disappear";
			return true;
		}
	}

	return false;
}

static void write_subprocess_info(const char *pid, const char *location, const char *prism_session, const char *source_ptr, const char *stack_hash)
{
	QByteArray ba;
	QJsonObject obj;
	size_t len = 0;
	std::string sub_file = get_userapp_path() + CHILD_PROCESS_CRASH_FILE;
	std::shared_ptr<char> data = read_file(sub_file.c_str(), len);
	if (data) {
		ba.append(data.get(), (int)len);
		obj = QJsonDocument::fromJson(ba).object();
	}

	QJsonArray processes = obj["childProcesses"].toArray();

	for (auto it = processes.begin(); it != processes.end(); it++) {
		QJsonObject curProcess = it->toObject();
		QString prevSource = curProcess["sourcePtr"].toString();
		QString prevSession = curProcess["prismSession"].toString();
		if (0 == prevSource.compare(source_ptr) && (0 == prevSession.compare(prism_session))) {
			processes.erase(it);
			break;
		}
	}

	QJsonObject curProc;
	curProc.insert("prismSession", prism_session);
	curProc.insert("location", location);
	curProc.insert("sourcePtr", source_ptr);
	curProc.insert("stackHash", stack_hash);
	processes.push_back(curProc);

	obj.insert("childProcesses", processes);

	QJsonDocument doc;
	doc.setObject(obj);
	QByteArray doc_data = doc.toJson(QJsonDocument::Compact);

	write_file((char *)doc_data.constData(), doc_data.size(), sub_file.c_str());

	Log(NELO_LL_INFO, "[PID : %s] Writed child process data : %s.", pid, doc_data.constData());
}

static bool check_repeat_crash(const char *pid, const char *location, const char *prism_session, const char *source_ptr, const char *stack_hash)
{
	bool repeat = false;
	std::string sub_file = get_userapp_path() + CHILD_PROCESS_CRASH_FILE;
	if (!PathFileExists(str::a2w(sub_file.c_str()).c_str())) {
		HANDLE lhDumpFile = CreateFile(str::a2w(sub_file.c_str()).c_str(), GENERIC_ALL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		CloseHandle(lhDumpFile);
	}

	size_t len = 0;
	std::shared_ptr<char> data = read_file(sub_file.c_str(), len);
	if (!data) {
		Log(NELO_LL_INFO, "[PID : %s] Read child process file failed.(Maybe this is the first crash).", pid);
	}

	QByteArray ba;
	ba.append(data.get(), (int)len);
	QJsonObject obj = QJsonDocument::fromJson(ba).object();
	QJsonArray processes = obj["childProcesses"].toArray();

	for (auto it = processes.begin(); it != processes.end(); it++) {
		QJsonObject curProcess = it->toObject();
		QString prevSource = curProcess["sourcePtr"].toString();
		QString prevSession = curProcess["prismSession"].toString();
		QString prevStackHash = curProcess["stackHash"].toString();
		if (0 == prevSource.compare(source_ptr) && (0 == prevSession.compare(prism_session))) {
			if (0 == prevStackHash.compare(stack_hash)) {
				repeat = true;
				break;
			}
		}
	}

	repeat ? Log(NELO_LL_INFO, "[PID : %s] Same as the last crash, this is repeated crash.", pid) : Log(NELO_LL_INFO, "[PID : %s] Not repeated crash, Previous content : %s", pid, ba.constData());

	write_subprocess_info(pid, location, prism_session, source_ptr, stack_hash);

	return repeat;
}

static void init_values(std::string &crash_value, std::string &crash_type, std::string &crash_key, std::string &body, DumpType dump_type, std::string pid = "", std::string src = "")
{
	crash_value = "Unknown";

	switch (dump_type) {
	case DumpType::DT_UI_BLOCK:
		crash_type = "UI block";
#ifdef _DEBUG
		body = "UI blocked dump (Debug Mode)";
#else
		body = "UI blocked dump";
#endif
		break;
	case DumpType::DT_CRASH_SUB:
		crash_type = "GeneralDisappear";
		crash_key = "SubGeneralDisappear";
		body = "Disappear Dump File [SUB_PROCESS SRC:"; //eg. "Crash Dump File [SUB_PROCESS SRC:00000199BEFEDA00 PID:15296]"
		body.append(src).append(" PID:").append(pid).append("]");
		break;
	case DumpType::DT_CRASH_MAIN:
		crash_type = "GeneralDisappear";
		crash_key = "MainGeneralDisappear";
		body = "Disappear Dump File";
	default:
		break;
	}
}

static bool parse_stacktrace_and_check_repeat(std::string dump_file, std::string prism_session, std::string &crash_value, std::string &crash_type, std::string &crash_key, std::string &body,
					      std::string &location, DumpType dump_type, std::string pid = "", std::string src = "")
{
	ULONG64 stack_hash = 0;
	std::string stack{};
	bool find_blacklist = false, repeat = false, find_internal = false;
	bool main_process = dump_type == DumpType::DT_CRASH_SUB ? false : true;

	init_values(crash_value, crash_type, crash_key, body, dump_type, pid, src);

	QJsonObject blacklist = get_blacklist();
	std::vector<ULONG64> stack_frams = GetStackTrace(dump_file.c_str());
	std::vector<ModuleInfo> modules = GetModuleInfo(dump_file.c_str());

	auto findModule = [&modules](ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo) {
		for (int i = 0; i < modules.size(); ++i) {
			if (addr >= modules[i].BaseOfImage && addr <= modules[i].BaseOfImage + modules[i].SizeOfImage) {
				moduleInfo = modules[i];
				offset = addr - modules[i].BaseOfImage;
				return true;
			}
		}

		return false;
	};

	for (int i = 0; i < stack_frams.size(); ++i) {

		ULONG64 offset = 0;
		ModuleInfo module_info{};
		bool success = findModule(stack_frams[i], offset, module_info);
		if (success)
			stack_hash += offset;

		char offset_s[64];
		sprintf_s(offset_s, "0x%llx", offset);

		std::string module_name = str::T2u(module_info.ModuleName);
		std::string prefix = "\n " + std::to_string(i) + " ";
		std::string frame = module_name + " + " + offset_s;
		stack.append(prefix).append(frame);

		if (location.empty())
			location = frame;

		if (!find_internal && module_info.bInternal) {
			location = frame;
			find_internal = true;
		}

		if (find_blacklist)
			continue;

		crash_value = module_name;
		if (check_blacklist(QString::fromStdString(module_name), blacklist, crash_type)) {
			find_blacklist = true;
			crash_key = main_process ? "MainThirdPartyDisappear" : "SubThirdPartyDisappear";
		}
	}

	std::string s_type = dump_type == DumpType::DT_UI_BLOCK ? "UI Block" : dump_type == DumpType::DT_CRASH_MAIN ? "Main" : "Sub";
	Log(NELO_LL_INFO, "[PID : %s] %s BackTrace : %s", pid.c_str(), s_type.c_str(), stack.c_str());

	if (dump_type == DumpType::DT_CRASH_SUB)
		repeat = check_repeat_crash(pid.c_str(), location.c_str(), prism_session.c_str(), src.c_str(), std::to_string(stack_hash).c_str());

	return repeat;
}

static bool send_to_nelo(const char *dump_file, const char *user_id, const char *prism_version, const char *prism_session, const char *project_name, const char *cpu_name,
			 const char *video_adapter_name, DumpType dump_type, const char *pid = "", const char *src = "")
{
	std::string crash_value, crash_type, crash_key, body, location;
	bool repeat = parse_stacktrace_and_check_repeat(dump_file, prism_session, crash_value, crash_type, crash_key, body, location, dump_type, pid, src);
	if (repeat)
		return false;

	size_t dump_len = 0;
	std::shared_ptr<char> dump_data = read_file(dump_file, dump_len);
	if (!dump_data) {
		return false;
	}

	QByteArray ba;
	ba.append(dump_data.get(), (int)dump_len);
	QString base64Dump = ba.toBase64();

	QString win_version = get_windows_version_info();

	DWORD blocked_seconds = block_timeout_s + (GetTickCount() - block_time_ms) / 1000;
	QString blocked_str = QString("Blocked duration is %1 seconds").arg(blocked_seconds);

	QJsonObject nelo_info;
	nelo_info.insert("projectName", project_name);
	nelo_info.insert("projectVersion", prism_version);
	nelo_info.insert("prismSession", prism_session);
	nelo_info.insert("body", body.c_str());

	nelo_info.insert("logLevel", "FATAL");
	nelo_info.insert("logSource", "CrashDump");
	nelo_info.insert("Platform", win_version);

	nelo_info.insert("logType", "nelo2-app");
	nelo_info.insert("UserID", user_id);
	nelo_info.insert("DmpData", base64Dump);
	nelo_info.insert("DmpFormat", "bin");
	nelo_info.insert("DmpSymbol", "Required");

	if (dump_type == DumpType::DT_UI_BLOCK)
		nelo_info.insert("UIBlockExit", blocked_str);

	if (dump_type != DumpType::DT_UI_BLOCK)
		nelo_info.insert(crash_key.c_str(), crash_value.c_str());

	nelo_info.insert("CrashType", crash_type.c_str());
	nelo_info.insert("LogFrom", logFrom.c_str());
	nelo_info.insert("videoAdapter", video_adapter_name);
	nelo_info.insert("cpuName", cpu_name);
	nelo_info.insert("DumpLocation", location.c_str());

	QJsonDocument doc;
	doc.setObject(nelo_info);

	QByteArray post_data = doc.toJson(QJsonDocument::Compact);
	std::string post_bodfy = post_data.toStdString();

	return send_data(post_bodfy);
}
