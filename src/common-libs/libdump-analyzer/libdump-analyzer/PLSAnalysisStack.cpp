#pragma once
#if _WIN32
#include <WS2tcpip.h>
#include <Windows.h>
#include "windows/PLSParseDumpFile.h"
#include "windows/PLSSoftStatistic.h"
#else
#include "mac/PLSAnalysisStackInterface.h"
#endif

#include "PLSAnalysisStack.h"
#include "libutils-api.h"
#include "libhttp-client.h"
#include "PLSUtil.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QCryptographicHash>
#include <QVariantMap>
#include <vector>
#include <array>
#include <memory>
#include <QDir>

namespace pls {

const static QString RESOURCE_PATH = "\\data\\prism-studio";

const static QString INSTALL_AUDIO_CAPTURE_PATH = "\\prism-plugins\\laboratory-win-capture-audio.dll";
const static QString USER_AUDIO_CAPTURE_PATH = "PRISMLiveStudio\\laboratory\\win-capture-audio\\laboratory-win-capture-audio.dll";

const static QString PRISM_CRASH_CONFIG_PATH = "PRISMLiveStudio\\crashDump\\crash.json";
const static QString PROCESS_CRASH_FILE = "PRISMLiveStudio\\crashDump\\process.json";
const static QString MODULES_FILE = "\\PRISMLiveStudio\\crashDump\\modules.json";
const static QString GPOP_FILE = "\\user\\gpop.json";

const static QString EVENT_CRASH = "crash";
const static QString EVENT_CRASH_GENERAL = "general";
const static QString EVENT_CRASH_DEVICE = "device";
const static QString EVENT_CRASH_OBS_CAM_DEVICE = "obs_cam_device";
const static QString EVENT_CRASH_CAM_DEVICE = "cam_device";
const static QString EVENT_CRASH_AUDIO_DEVICE = "audio_interface";
const static QString EVENT_CRASH_AUDIO_CAPTURE = "lab_audio_capture";
const static QString EVENT_CRASH_GENERAL_TYPE = "other_type";
const static QString EVENT_CRASH_GRAPHIC_CARD = "graphic_card";
const static QString EVENT_CRASH_PLUGIN = "plugin";
const static QString EVENT_CRASH_EXTERNAL_PLUGIN = "external_plugin";
const static QString EVENT_CRASH_VST_PLUGIN = "vst";
const static QString EVENT_CRASH_PROGRAM = "program";
const static QString EVENT_CRASH_THIRD_PROGRAM = "third_program";
const static QString EVENT_CRASH_CUSTOM = "custom";

const static std::string PROCESS_CAM_SESSION = "cam-session.exe";
const static std::string PROCESS_PRISM = "PRISMLiveStudio.exe";

struct StaticValue {
	static uint64_t thread_id;
	static LogFunc do_log;
	static ProcessInfo info;
	static std::string prefix;
};

uint64_t StaticValue::thread_id = 0;
LogFunc StaticValue::do_log = nullptr;
ProcessInfo StaticValue::info{};
std::string StaticValue::prefix{};

struct ActionInfo {
	QString event1;
	QString event2;
	QString event3;
	QString target;
	QString resourceID;
};

static void log_prefix(ProcessInfo const &info)
{
	StaticValue::prefix.clear();
	if (info.dump_type == DumpType::DT_UI_BLOCK)
		StaticValue::prefix.append("[").append(info.process_name).append(" UI Block PID : ").append(info.pid).append("] ");
	else
		StaticValue::prefix.append("[").append(info.process_name).append(" PID : ").append(info.pid).append("] ");

	StaticValue::prefix.append("(").append(std::to_string(StaticValue::thread_id)).append(") ");
	return;
}

static std::map<std::string, std::string, std::less<>> log_ctx(const std::string &ctx)
{
	std::map<std::string, std::string, std::less<>> map{};
	std::string log{};
	log.append(StaticValue::prefix).append(ctx);
	map.emplace("ctx", log);
	return map;
}

static std::map<std::string, std::string, std::less<>> log_ctx(const std::string &ctx, std::map<std::string, std::string, std::less<>> fields)
{
	auto ret = log_ctx(ctx);
	ret.insert(fields.begin(), fields.end());
	return ret;
}

#if _WIN32

LONG WINAPI unhandled_exception_filter(struct _EXCEPTION_POINTERS *pExceptionPointers)
{
	SYSTEMTIME st;
	::GetLocalTime(&st);

	std::string strUserPath;
	if (StaticValue::info.dump_path.empty())
		strUserPath = get_app_data_dir("PRISMLiveStudio\\crashDump\\").toStdString();
	else
		strUserPath = StaticValue::info.dump_file;

	std::array<wchar_t, MAX_PATH> path;
	swprintf_s(path.data(), path.size(), L"%s%s.%d.%04d_%02d_%02d_%02d_%02d_%02d_%03d.dmp", pls_utf8_to_unicode(strUserPath.c_str()).c_str(),
		   pls_utf8_to_unicode(StaticValue::info.process_name.c_str()).c_str(),
		   GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	HANDLE lhDumpFile = CreateFileW(path.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (lhDumpFile && (lhDumpFile != INVALID_HANDLE_VALUE)) {
		MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
		loExceptionInfo.ExceptionPointers = pExceptionPointers;
		loExceptionInfo.ThreadId = GetCurrentThreadId();
		loExceptionInfo.ClientPointers = TRUE;

		int nDumpType = MiniDumpWithFullMemoryInfo;
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), lhDumpFile, (MINIDUMP_TYPE)nDumpType, &loExceptionInfo, nullptr, nullptr);
		CloseHandle(lhDumpFile);

		if (StaticValue::info.sync_send_dump) {
			StaticValue::info.dump_file = pls_unicode_to_utf8(path.data());
			StaticValue::info.pid = std::to_string(GetCurrentProcessId());
			analysis_stack_and_send_dump(StaticValue::info, StaticValue::info.analysis_stack);
		}
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

static void check_fasoo(const std::string &crash_type)
{
	std::vector<std::string> blacklist = PLSSoftStatistic::GetBalcklist();
	auto finder = [crash_type](const std::string &item) { return crash_type.find(item) != std::wstring::npos; };

	auto ret = std::find_if(blacklist.begin(), blacklist.end(), finder);
	if (ret == blacklist.end()) {
		if (StaticValue::do_log)
			StaticValue::do_log(log_ctx("No blacklist software crashed."));
		return;
	}

	std::vector<SoftInfo> vSoft = PLSSoftStatistic::GetInstalledSoftware();
	if (!vSoft.empty()) {
		for (const auto &item : vSoft) {
			std::string softDesc = item.softName + " v" + item.softVersion;
			std::map<std::string, std::string, std::less<>> filed{{"stageDesc", "ExceptionOccurred"},  {"blacklistKey", item.key.c_str()},
									      {"softName", item.softName.c_str()}, {"softVersion", item.softVersion.c_str()},
									      {"softDesc", softDesc.c_str()},      {"softPublisher", item.softPublisher.c_str()}};

			std::array<char, 1024> logctx;
			sprintf_s(logctx.data(), sizeof(logctx), "Installed blacklist software detected.  \nstageDesc : ExceptionOccurred \nblacklistKey : %s \nsoftDesc : %s \nsoftPublisher : %s ",
				  item.key.c_str(), softDesc.c_str(), item.softPublisher.c_str());

			if (StaticValue::do_log)
				StaticValue::do_log(log_ctx(logctx.data(), filed));
		}
	}
}

static QJsonArray get_modules_by_record(const std::string &prism_session)
{
	QJsonArray modules;
	QString moudles_file = pls_get_app_data_dir_pn("") ;
	if (moudles_file.contains("PRISMLogger")) {
		QDir dir(moudles_file);
		dir.cdUp();
		moudles_file = dir.path().append(MODULES_FILE);
	} else {
		moudles_file.append(MODULES_FILE);
	}
	auto data = pls_read_data(moudles_file);
	if (data.isEmpty())
		return modules;

	QJsonObject obj = QJsonDocument::fromJson(data).object();
	QString session = obj["prismSession"].toString();
	if (0 == session.compare(prism_session.c_str()))
		modules = obj["modules"].toArray();

	return modules;
}

static bool find_module_by_record(const QJsonArray modules, ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo)
{
	if (modules.empty())
		return false;
	bool ok = false;
	for (const auto &item : modules) {
		auto mod = item.toObject();
		quint64 base = mod["modBaseAddr"].toString().toULongLong(&ok, 16);
		qint64 end = mod["modEndAddr"].toString().toULongLong(&ok, 16);
		if (addr >= base && addr <= end) {
			moduleInfo.BaseOfImage = base;
			moduleInfo.SizeOfImage = (ULONG32)mod["SizeOfImage"].toInteger();
			moduleInfo.bInternal = mod["bInternal"].toBool();
			mod["ModuleName"].toString().toWCharArray(moduleInfo.ModuleName.data());

			offset = addr - moduleInfo.BaseOfImage;
			return true;
		}
	}

	return false;
}

static bool find_module(const std::vector<ModuleInfo> &modules, const QJsonArray modulesJson, ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo)
{
	bool find = false;
	for (const auto &item : modules) {
		if (addr >= item.BaseOfImage && addr <= item.BaseOfImage + item.SizeOfImage) {
			moduleInfo = item;
			offset = addr - item.BaseOfImage;
			find = true;
			break;
		}
	}

	if (!find) {
		find = find_module_by_record(modulesJson, addr, offset, moduleInfo);
	}

	return find;
}

static bool send_data(const std::string &post_bodfy)
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
		WSACleanup();
		return false;
	}

	RUN_WHEN_SECTION_END([&]() { closesocket(s); });

	addrinfo hints;
	addrinfo *result = nullptr;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	DWORD dwRetval = getaddrinfo("nelo2-col.navercorp.com", nullptr, &hints, &result);
	if (dwRetval != 0) {
		return false;
	}

	std::array<char, INET_ADDRSTRLEN> ip4;
	for (struct addrinfo *ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
		if (ptr->ai_family == AF_INET) {
			auto addr = (sockaddr_in const *)ptr->ai_addr;
			inet_ntop(AF_INET, &addr->sin_addr, ip4.data(), INET_ADDRSTRLEN);
			break;
		}
	}

	struct sockaddr_in sd = {0};
	inet_pton(AF_INET, ip4.data(), &sd.sin_addr.s_addr);
	sd.sin_port = htons(80);
	sd.sin_family = AF_INET;

	freeaddrinfo(result);

	ret = connect(s, (sockaddr *)&sd, sizeof(sd));
	if (ret == SOCKET_ERROR) {
		return false;
	}

	std::array<char, 20> content_length{0};
	snprintf(content_length.data(), content_length.size(), "%u", (unsigned)post_bodfy.length());

	std::string send_data = "POST /_store HTTP/1.1\n"
				"Connection: close\n"
				"Content-Type: application/x-www-form-urlencoded\n"
				"Host: nelo2-col.navercorp.com:80\n"
				"Content-Length:";

	send_data += content_length.data();
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

		std::array<char, 4096> buf{0};

		while (true) {
			int res = recv(s, buf.data(), buf.size(), 0);
			if (res > 0) {
				buf[res] = 0;
				response += buf.data();
			} else {
				break;
			}
		}

		send_ok = (response.find("\"Success\"") != std::string::npos);
		assert(send_ok);
	}
	return send_ok;
}
#endif

static QJsonObject get_blacklist()
{
#if _WIN32
	QString syncName = "PRISMLiveStudio\\library\\library_Policy_PC\\BlackList.json";
	QString localName = "..\\..\\data\\prism-studio\\user\\BlackList.json";
#else
	QString syncName = "PRISMLiveStudio/library/library_Policy_PC/BlackList.json";
	QString localName = "data/prism-studio/user/BlackList.json";
#endif

	auto syncPath = get_app_data_dir(syncName);
	auto syncData = pls_read_data(syncPath);

	auto localPath = get_app_run_dir(localName);
	auto localData = pls_read_data(localPath);

	if (syncData.isEmpty() && localData.isEmpty() && StaticValue::do_log) {
		StaticValue::do_log(log_ctx("Read sync and local blacklist is empty."));
		return QJsonObject();
	}
	int syncVersion = 0;
	QJsonObject syncBlacklist{};
	if (!syncData.isEmpty()) {
		QJsonObject obj = QJsonDocument::fromJson(syncData).object();
		syncVersion = obj["version"].toInt();
		QJsonObject platform;
#if _WIN32
		platform = obj["win64"].toObject();
#else
		platform = obj["mac"].toObject();
#endif
		syncBlacklist = platform["blacklist"].toObject();
	}
	int localVersion = 0;
	QJsonObject localBlacklist{};
	if (!localData.isEmpty()) {
		QJsonObject obj = QJsonDocument::fromJson(localData).object();
		localVersion = obj["version"].toInt();
		QJsonObject platform;
#if _WIN32
		platform = obj["win64"].toObject();
#else
		platform = obj["mac"].toObject();
#endif
		localBlacklist = platform["blacklist"].toObject();
	}

	return syncVersion >= localVersion ? syncBlacklist : localBlacklist;
}

static std::string type_to_crash_key(DumpType dump_type, bool is_main)
{
	if (dump_type == DumpType::DT_CRASH)
		return is_main ? "MainThirdPartyCrash" : "SubThirdPartyCrash";
	else if (dump_type == DumpType::DT_DISAPPEAR)
		return is_main ? "MainThirdPartyDisappear" : "SubThirdPartyDisappear";
	return std::string();
}

static std::string type_to_crash_type(DumpType dump_type)
{
	if (DumpType::DT_CRASH == dump_type)
		return "Crash";
	else if (DumpType::DT_DISAPPEAR == dump_type)
		return "Disappear";
	else if (DumpType::DT_UI_BLOCK == dump_type)
		return "UI Block";
	return std::string();
}

static bool check_blacklist(QString value, QJsonObject blacklist, ProcessInfo const &info, ActionInfo &actionInfo, std::string &crash_type, std::string &url, QString local)
{
	if (blacklist.isEmpty())
		return false;

	auto finder = [local, url](const QMap<QString, QVariant> &map) {
		if (!map.isEmpty()) {
			auto localUrl = map.value(local);
			if (localUrl.isValid()) {
				return localUrl.toString().toStdString();
			}
		}
		return url;
	};

	QMap<QString, QVariant> graphicsDrivers = blacklist["graphicsDrivers"].toVariant().toMap();
	auto it = graphicsDrivers.find(value);
	if (it != graphicsDrivers.end()) {
		auto map = it.value().toMap();
		url = finder(map);
		actionInfo.event2 = EVENT_CRASH_DEVICE;
		actionInfo.event3 = EVENT_CRASH_GRAPHIC_CARD;
		actionInfo.target = info.video_adapter_name.c_str();
		crash_type = "GraphicsDrivers BlackList " + type_to_crash_type(info.dump_type);
		return true;
	}

	QMap<QString, QVariant> deviceDrivers = blacklist["deviceDrivers"].toVariant().toMap();
	it = deviceDrivers.find(value);
	if (it != deviceDrivers.end()) {
		auto map = it.value().toMap();
		url = finder(map);
		actionInfo.event2 = EVENT_CRASH_DEVICE;
		actionInfo.target = value.toUtf8().constData();
		crash_type = "DeviceDrivers BlackList " + type_to_crash_type(info.dump_type);
		return true;
	}

	QMap<QString, QVariant> thirdPlugins = blacklist["3rdPlugins"].toVariant().toMap();
	it = thirdPlugins.find(value);
	if (it != thirdPlugins.end()) {
		auto map = it.value().toMap();
		url = finder(map);
		actionInfo.event2 = EVENT_CRASH_PLUGIN;
		actionInfo.event3 = EVENT_CRASH_EXTERNAL_PLUGIN;
		actionInfo.target = value.toUtf8().constData();
		crash_type = "3rd Plugins BlackList " + type_to_crash_type(info.dump_type);
		return true;
	}
	QMap<QString, QVariant> vstPlugins = blacklist["vstPlugins"].toVariant().toMap();
	it = vstPlugins.find(value);
	if (it != vstPlugins.end()) {
		auto map = it.value().toMap();
		url = finder(map);
		actionInfo.event2 = EVENT_CRASH_PLUGIN;
		actionInfo.event3 = EVENT_CRASH_VST_PLUGIN;
		actionInfo.target = value.toUtf8().constData();
		crash_type = "VST Plugins BlackList " + type_to_crash_type(info.dump_type);
		return true;
	}
	QMap<QString, QVariant> thirdPrograms = blacklist["3rdPrograms"].toVariant().toMap();
	it = thirdPrograms.find(value);
	if (it != thirdPrograms.end()) {
		auto map = it.value().toMap();
		url = finder(map);
		actionInfo.event2 = EVENT_CRASH_PROGRAM;
		actionInfo.event3 = EVENT_CRASH_THIRD_PROGRAM;
		actionInfo.target = value.toUtf8().constData();
		crash_type = "3rd Programs BlackList " + type_to_crash_type(info.dump_type);
		return true;
	}
	auto custommap = blacklist.value("customType").toVariant().toMap();
	for (auto key : custommap.keys()) {
		auto item = custommap.find(key);
		if (item != custommap.end()) {
			auto itemMap = item.value().toMap();
			auto it_ = itemMap.find(value);
			if (it_ != itemMap.end()) {
				auto map = it_.value().toMap();
				url = finder(map);
				actionInfo.event2 = EVENT_CRASH_CUSTOM;
				actionInfo.event3 = key;
				actionInfo.target = value.toUtf8().constData();
				crash_type = key.toStdString().append(type_to_crash_type(info.dump_type));
				return true;
			}
		}
	}

	return false;
}

static bool check_device_thread(QJsonObject device, QString &target)
{
	QJsonArray threads = device.value("threads").toArray();
	for (auto thread : threads) {
		auto threadObj = thread.toObject();
		auto threadId = threadObj["threadId"].toInt();
		if (threadId == StaticValue::thread_id) {
			if (threadObj.value("enumerating").toBool())
				target = "DeviceEnumerating";
			else
				target = device.value("moduleName").toString();

			return true;
		}
	}
	return false;
}

static void check_delete_audio_capture_plugin(QString event3)
{
	if (event3 == EVENT_CRASH_AUDIO_CAPTURE) {
		if (StaticValue::do_log) {
			StaticValue::do_log(log_ctx("The lab audio capture plugin crashed and will delete it."));
		}

		auto audioCapturePath = get_app_run_dir(INSTALL_AUDIO_CAPTURE_PATH);
		if (!audioCapturePath.isEmpty()) {
			bool ret = pls_remove_file(audioCapturePath);
			if (StaticValue::do_log) {
				std::string log = ret ? "Successfully deleted the laboratory-win-capture-audio.dll in the installation dir success."
						      : "Failed to delete the laboratory-win-capture-audio.dll in the installation dir.";
				StaticValue::do_log(log_ctx(log));
			}
		}

		audioCapturePath = get_app_data_dir(USER_AUDIO_CAPTURE_PATH);
		if (!audioCapturePath.isEmpty()) {
			bool ret = pls_remove_file(audioCapturePath);
			if (StaticValue::do_log) {
				std::string log = ret ? "Successfully deleted the laboratory-win-capture-audio.dll in user dir." : "Failed to delete the laboratory-win-capture-audio.dll in user dir.";
				StaticValue::do_log(log_ctx(log));
			}
		}
	}
}

static void check_crash_json(ProcessInfo const &info, QJsonObject obj, const std::string &crash_type, ActionInfo &actionInfo, const std::string &location, bool find_blacklist)
{
	std::string type = "DeviceDrivers BlackList " + type_to_crash_type(info.dump_type);
	if (!find_blacklist || 0 == crash_type.compare(type)) {
		QJsonObject crashDevice{};
		QString event3{};
		QString target{};
		QJsonObject prismObj = obj["currentPrism"].toObject();
		QJsonArray devices_modules = prismObj["modules"].toArray();

		for (auto item : devices_modules) {
			auto device = item.toObject();
			if (device.value("sourceId").toString() == "dshow_input")
				event3 = EVENT_CRASH_CAM_DEVICE;
			else if (device.value("sourceId").toString() == "dshow_input_obs")
				event3 = EVENT_CRASH_OBS_CAM_DEVICE;
			else if (device.value("sourceId").toString() == "audio_capture")
				event3 = EVENT_CRASH_AUDIO_CAPTURE;
			else
				event3 = EVENT_CRASH_AUDIO_DEVICE;

			if (check_device_thread(device, target)) {
				crashDevice = device;
				break;
			}
		}

		if (!crashDevice.isEmpty()) {
			actionInfo.event2 = EVENT_CRASH_DEVICE;
			actionInfo.event3 = event3;
			actionInfo.target = target.isEmpty() ? location.c_str() : target.toUtf8().constData();

			if (StaticValue::do_log) {
				QJsonDocument jsonDoc;
				jsonDoc.setObject(crashDevice);
				std::string log = "DeviceDrivers crashed by check crash.json.\n Device crash data : ";
				log.append(jsonDoc.toJson().toStdString());
				StaticValue::do_log(log_ctx(log));
			}

			check_delete_audio_capture_plugin(event3);

		} else {
			actionInfo.event2 = EVENT_CRASH_GENERAL;
			actionInfo.event3 = EVENT_CRASH_GENERAL_TYPE;
			actionInfo.target = location.c_str();
		}
	}
}

static void check_mainprocess_info(ProcessInfo const &info, const std::string &crash_type, ActionInfo &actionInfo, const std::string &location, bool find_blacklist)
{
	if (!info.is_main || info.dump_type == DumpType::DT_UI_BLOCK)
		return;

	auto main_file = get_app_data_dir(PRISM_CRASH_CONFIG_PATH);
	auto data = pls_read_data(main_file);

	QJsonObject obj = QJsonDocument::fromJson(data).object();
	//check camera device/audio device/audio plugin crash
	//check_crash_json(info, obj, crash_type, actionInfo, location, find_blacklist);

	QJsonObject curProc;
	curProc.insert("crashed", true);
	curProc.insert("prismSession", info.prism_session.c_str());
	curProc.insert("event1", actionInfo.event1);
	curProc.insert("event2", actionInfo.event2);
	curProc.insert("event3", actionInfo.event3);
	curProc.insert("target", actionInfo.target);
	curProc.insert("resourceID", actionInfo.resourceID);
	obj.insert("actionEvent", curProc);

	QJsonDocument doc;
	doc.setObject(obj);
	QByteArray doc_data = doc.toJson(QJsonDocument::Compact);
	pls_write_data(main_file, doc_data);

	std::string log = "Writed crash json file data : ";
	log.append(doc_data.constData());
	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));
	return;
}

static bool check_repeat_crash(ProcessInfo const &info, const std::string &location, const std::string &stack_hash)
{
	bool repeat = false;
	if (info.dump_type == DumpType::DT_UI_BLOCK)
		return repeat;

	QByteArray byteArray(stack_hash.c_str(), (int)stack_hash.length());
	QByteArray stack_md5 = QCryptographicHash::hash(byteArray, QCryptographicHash::Md5).toHex();

	auto file = get_app_data_dir(PROCESS_CRASH_FILE);
	auto data = pls_read_data(file);
	if (data.isEmpty()) {
		if (StaticValue::do_log)
			StaticValue::do_log(log_ctx("Read process file failed.(Maybe this is the first crash)."));
	}

	QJsonObject obj = QJsonDocument::fromJson(data).object();
	QJsonArray processes = obj["Processes"].toArray();

	for (auto it = processes.begin(); it != processes.end(); it++) {
		QJsonObject curProcess = it->toObject();
		QString prevSession = curProcess["prismSession"].toString();
		QString prevStackHash = curProcess["stackHash"].toString();
		if (0 == prevSession.compare(info.prism_session.c_str()) && 0 == prevStackHash.compare(stack_md5.constData())) {
			processes.erase(it);
			repeat = true;
			break;
		}
	}

	std::string log{};
	repeat ? log.append("Same as the last crash, this is repeated crash.") : log.append("Not repeated crash.");
	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));

	QJsonObject curProc;
	curProc.insert("prismSession", info.prism_session.c_str());
	curProc.insert("location", location.c_str());
	curProc.insert("stackHash", stack_md5.constData());
	if (!info.src.empty())
		curProc.insert("sourcePtr", info.src.c_str());
	processes.push_back(curProc);

	obj.insert("Processes", processes);

	QJsonDocument doc;
	doc.setObject(obj);
	QByteArray doc_data = doc.toJson(QJsonDocument::Compact);
	pls_write_data(file, doc_data);

	log.clear();
	log.append("Writed process json file data : ").append(doc_data.constData());
	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));

	return repeat;
}

static void init_values(ProcessInfo const &info, std::string &crash_value, std::string &crash_type, std::string &crash_key, std::string &body)
{
	crash_value = "Unknown";

	switch (info.dump_type) {
	case DumpType::DT_UI_BLOCK:
		crash_type = "UI block";
#ifdef _DEBUG
		body = "UI blocked dump (Debug Mode)";
#else
		body = "UI blocked dump";
#endif
		break;
	case DumpType::DT_CRASH:
		crash_type = "GeneralCrash";
		crash_key = info.is_main ? "MainGeneralCrash" : "SubGeneralCrash";
		body = "Crash Dump File ";
		body.append(StaticValue::prefix);
		break;
	case DumpType::DT_DISAPPEAR:
		crash_type = "GeneralDisappear";
		crash_key = info.is_main ? "MainGeneralDisappear" : "SubGeneralDisappear";
		body = "Disappear Dump File ";
		body.append(StaticValue::prefix);
		break;
	default:
		break;
	}
}

static bool test_module()
{
#if _WIN32
	HKEY hOpen;
	LPCWSTR key = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	LPCWSTR test_mode_name = L"TestMode";
	LPCWSTR cam_crash_name = L"CamCrashTest";
	std::array<TCHAR, 64> buf;
	auto size = DWORD(buf.size() * sizeof(TCHAR));
	DWORD type = 0;
	DWORD type_size = sizeof(DWORD);
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &hOpen)) {
		if (ERROR_SUCCESS == RegQueryValueEx(hOpen, test_mode_name, nullptr, nullptr, (LPBYTE)buf.data(), &size)) {
			if (std::wcscmp(buf.data(), L"true") != 0) {
				RegCloseKey(hOpen);
				return false;
			}
		} else {
			RegCloseKey(hOpen);
			return false;
		}

		if (ERROR_SUCCESS == RegQueryValueEx(hOpen, cam_crash_name, nullptr, nullptr, (BYTE *)&type, &type_size)) {
			if ((int)type == 1) {
				RegCloseKey(hOpen);
				return true;
			}
			if ((int)type == 2) {
				RegCloseKey(hOpen);
				return true;
			}
		} else {
			RegCloseKey(hOpen);
			return false;
		}
		RegCloseKey(hOpen);
		return false;
	}
#endif
	return false;
}

static void remove_dump_file(ProcessInfo const &info)
{
	std::string log{};
    bool dump_removed = false;

#if _WIN32
    dump_removed = pls_remove_file(info.dump_file.c_str());
#elif __APPLE__
    dump_removed = mac_remove_crash_logs(info);
#endif
    
    log = dump_removed ? "Remove dump file success." : "Remove dump file failed.";

	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));
}

static bool send_dump(ProcessInfo const &info, std::string &crash_value, std::string &crash_type, std::string const &crash_key, std::string const &body, std::string const &location)
{
	bool res = false;
	size_t dump_len = 0;
#if _WIN32
	auto dump_data = pls_read_data(info.dump_file.c_str());
#elif __APPLE__
    std::string data;
    std::string dump_location;
    mac_get_latest_dump_data_location(info, data, dump_location);
    QByteArray dump_data = QString::fromStdString(data).toUtf8();
#endif
	if (dump_data.isEmpty()) {
		if (StaticValue::do_log)
			StaticValue::do_log(log_ctx("Size of dump file is 0"));
		remove_dump_file(info);
		return res;
	}

	QString base64Dump = dump_data.toBase64();
	std::string platform = get_os_version();

	QJsonObject nelo_info;
	nelo_info.insert("projectName", info.project_name.c_str());
	nelo_info.insert("projectVersion", info.prism_version.c_str());
	nelo_info.insert("abnormalProcess", info.process_name.c_str());
	nelo_info.insert("processName", info.process_name.c_str());
	nelo_info.insert("processId", info.pid.c_str());
	nelo_info.insert("body", body.c_str());
	nelo_info.insert("OSType", info.os_type.c_str());

	nelo_info.insert("logLevel", "FATAL");

	if (info.dump_type == DumpType::DT_CRASH)
		nelo_info.insert("DumpType", "Crash");
	else if (info.dump_type == DumpType::DT_DISAPPEAR)
		nelo_info.insert("DumpType", "Disappear");
	else if (info.dump_type == DumpType::DT_UI_BLOCK)
		nelo_info.insert("DumpType", "UIBlock");

	nelo_info.insert("logSource", "CrashDump");
	nelo_info.insert("Platform", platform.c_str());
	nelo_info.insert("logType", "nelo2-app");
	nelo_info.insert("DmpData", base64Dump);
	nelo_info.insert("DmpFormat", "bin");
	nelo_info.insert("DmpSymbol", "Required");
    
#if __APPLE__
    nelo_info.insert("DeviceName", get_device_name().c_str());
    nelo_info.insert("DumpLocation", dump_location.c_str());
#endif

	if (!StaticValue::info.setup_session.empty())
		nelo_info.insert("SetupSession", StaticValue::info.setup_session.c_str());
	if (!info.prism_session.empty())
		nelo_info.insert("prismSession", info.prism_session.c_str());
	if (!StaticValue::info.prism_sub_session.empty())
		nelo_info.insert("prismSubSession", StaticValue::info.prism_sub_session.c_str());

	if (info.dump_type == DumpType::DT_UI_BLOCK) {
		auto blocked_seconds = info.block_timeout_s + (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - info.block_time_ms).count());
		QString blocked_str = QString("Blocked duration is %1 seconds").arg(blocked_seconds);
		nelo_info.insert("UIBlockExit", blocked_str);
	} else {
		crash_value = !info.analysis_stack ? "Not analyzed" : crash_value;
		nelo_info.insert(crash_key.c_str(), crash_value.c_str());
	}

	crash_type = (info.test_module_crash || test_module()) ? "Test" + type_to_crash_type(info.dump_type) : crash_type;
	nelo_info.insert("CrashType", crash_type.c_str());
	nelo_info.insert("LogFrom", info.logFrom.c_str());

	if (!info.video_adapter_name.empty())
		nelo_info.insert("videoAdapter", info.video_adapter_name.c_str());
	if (!info.cpu_name.empty())
		nelo_info.insert("cpuName", info.cpu_name.c_str());
	if (!info.user_id.empty())
		nelo_info.insert("UserID", info.user_id.c_str());
	if (!location.empty())
		nelo_info.insert("DumpLocation", location.c_str());

	QJsonDocument doc;
	doc.setObject(nelo_info);

	QByteArray post_data = doc.toJson(QJsonDocument::Compact);
	
#if _WIN32
	res = send_data(post_data.data());
#else
    res = mac_send_data(post_data.data());
#endif

	std::string log = res ? "Send dump to nelo success." : "Send dump to nelo failed.";
	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));

    if (res) {
        remove_dump_file(info);
    }

#if _WIN32
	check_fasoo(crash_type);
#else
    // TODO: - mac next >>> maybe not neccessary
#endif
	return res;
}

static bool parse_stacktrace_and_check_repeat(ProcessInfo &info, std::string &crash_value, std::string &crash_type, std::string &crash_key, std::string &location)
{
	std::string stack_hash = {};
	std::string stack{};
	std::string url{};
	bool find_blacklist = false;
	bool repeat = false;
	bool find_internal = false;

	ActionInfo actionInfo;
	actionInfo.event1 = EVENT_CRASH;
	actionInfo.event2 = EVENT_CRASH_GENERAL;
	actionInfo.resourceID = info.user_id.c_str();

	QJsonObject blacklist = get_blacklist();
	if (blacklist.empty() && StaticValue::do_log)
		StaticValue::do_log(log_ctx("Blacklist is null."));

	QString local = (pls_prism_get_locale() == "ko-KR") ? "ko-KR" : "en-US";
	if (!blacklist.empty()) {
		auto defaultUrls = blacklist["DefaultUrl"].toVariant().toMap();
		if (!defaultUrls.isEmpty()) {
			auto localUrl = defaultUrls.value(local);
			if (localUrl.isValid())
				url = localUrl.toString().toStdString();
		}
	}
#if _WIN32
	std::vector<ULONG64> stack_frams = GetStackTrace(info.dump_file.c_str());
	std::vector<ModuleInfo> modules = GetModuleInfo(info.dump_file.c_str());

	QJsonArray modulesJson = get_modules_by_record(info.prism_session);

	for (int i = 0; i < stack_frams.size(); ++i) {

		std::array<char, 64> offset_s{};
		ULONG64 offset = 0;
		ModuleInfo module_info{};

		bool success = find_module(modules, modulesJson, stack_frams[i], offset, module_info);
		sprintf_s(offset_s.data(), offset_s.size(), "0x%llx", offset);
		stack_hash.append(offset_s.data());

		std::string module_name = success ? pls_unicode_to_utf8(module_info.ModuleName.data()) : "unkown";
		std::string prefix = "\n " + std::to_string(i) + " ";
		std::string frame = module_name + " + " + offset_s.data();
		stack.append(prefix).append(frame);

		if (location.empty())
			location = frame;

		if (!find_internal && module_info.bInternal) {
			location = frame;
			find_internal = true;
		}

		if (info.dump_type == DumpType::DT_UI_BLOCK || find_blacklist)
			continue;

		crash_value = module_name;
		if (check_blacklist(QString::fromStdString(module_name), blacklist, info, actionInfo, crash_type, url, local)) {
			location = frame;
			find_blacklist = true;
			crash_key = type_to_crash_key(info.dump_type, info.is_main);
			break;
		}
	}
#elif __APPLE__
    // parse dump data, find module names and
    std::string dump_data;
    std::set<std::string> module_names;
    mac_get_latest_dump_data_module_names(info, dump_data, module_names);
    if (!dump_data.empty()) {
        stack.append(dump_data);
    }
    
    // check backlist
    if (module_names.size() > 0) {
        for (std::string module_name : module_names) {
            if (check_blacklist(QString::fromStdString(module_name), blacklist, info, actionInfo, crash_type, url, local)) {
                find_blacklist = true;
                crash_key = type_to_crash_key(info.dump_type, info.is_main);
                break;
            }
        }
    }
    
    if (info.dump_type == DumpType::DT_UI_BLOCK) {
        repeat = true;
    }

#endif

	if (!find_blacklist)
		crash_value = QString::fromStdString(location).split(" + ").front().toStdString();

	std::string log{};
	log.append(" BackTrace : ").append(stack);
	if (StaticValue::do_log)
		StaticValue::do_log(log_ctx(log));

	if (info.process_func)
		info.process_func(location, url);

#if _WIN32
    repeat = check_repeat_crash(info, location, stack_hash);
    check_mainprocess_info(info, crash_type, actionInfo, location, find_blacklist);
#endif

	return repeat;
}


bool analysis_stack_and_send_dump(ProcessInfo info, bool analysis_stack)
{
	if (!info.process_name.empty())
#if _WIN32
		info.is_main = std::strcmp(info.process_name.c_str(), PROCESS_PRISM.c_str()) == 0;
#elif __APPLE__
        info.is_main = std::strcmp(info.process_name.c_str(), pls_get_app_pn().toStdString().c_str());
#endif

#if _WIN32
	StaticValue::thread_id = GetThreadId(info.dump_file.c_str());
#endif
	StaticValue::do_log = info.log_func;
	log_prefix(info);
	if (StaticValue::do_log) {
		StaticValue::do_log(log_ctx(type_to_crash_type(info.dump_type) + " happened "));
	}

	std::string crash_value;
	std::string crash_type;
	std::string crash_key;
	std::string body;
	std::string location;

	init_values(info, crash_value, crash_type, crash_key, body);

    if (analysis_stack) {
        bool repeat = parse_stacktrace_and_check_repeat(info, crash_value, crash_type, crash_key, location);
        if (repeat) {
            remove_dump_file(info);
            return false;
        }
    }
#ifdef PRISM_BUILD_TYPE_DEBUG
    remove_dump_file(info);
    return true;
#else
    return send_dump(info, crash_value, crash_type, crash_key, body, location);
#endif
}

void catch_unhandled_exceptions(const std::string &process_name, const std::string &dump_path)
{
	StaticValue::info.sync_send_dump = false;
	StaticValue::info.dump_path = dump_path;
	StaticValue::info.process_name = process_name;
#if _WIN32
	auto ret = SetUnhandledExceptionFilter(unhandled_exception_filter);
#elif __APPLE__
    mac_install_crash_reporter(process_name);
#endif
}

void catch_unhandled_exceptions_and_send_dump(const ProcessInfo &info_)
{
	StaticValue::info = info_;
	StaticValue::info.sync_send_dump = true;
#if _WIN32
	SetUnhandledExceptionFilter(unhandled_exception_filter);
#endif
}

void set_prism_user_id(const std::string &user_id)
{
	StaticValue::info.user_id = user_id;
}
void set_prism_session(const std::string &prism_session)
{
	StaticValue::info.prism_session = prism_session;
}
void set_prism_video_adapter(const std::string &video_adapter)
{
	StaticValue::info.video_adapter_name = video_adapter;
}
void set_setup_session(const std::string &session)
{
	StaticValue::info.setup_session = session;
}
void set_prism_sub_session(const std::string &session)
{
	StaticValue::info.prism_sub_session = session;
}

}
