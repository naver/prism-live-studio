#include "PLSBlockDump.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSPlatformApi/PLSPlatformApi.h"
#include "window-basic-status-bar.hpp"
#include <process.h>
#include <ShlObj_core.h>
#include <tchar.h>
#include "platform.hpp"
#include <Shlwapi.h>
#include <string>
#include "libutils-api.h"
#include "util/platform.h"
#include "PLSApp.h"

#pragma comment(lib, "WinMM.lib")
#pragma comment(lib, "Shlwapi.lib")

const auto HEARTBEAT_INTERVAL = 500;   // in milliseconds
const auto SAVE_DUMP_INTERVAL = 10000; // in milliseconds
const auto MAX_BLOCK_DUMP_COUNT = 3;

#define info(module, format, ...) PLS_INFO(module, "[PLSBlockDump] " format, ##__VA_ARGS__)
#define warn(module, format, ...) PLS_WARN(module, "[PLSBlockDump] " format, ##__VA_ARGS__)
#define plslogex(level, module, fields, count, format, ...) PLS_LOGEX(level, module, fields, count, "[PLSBlockDump] " format, ##__VA_ARGS__)

unsigned __stdcall PLSBlockDump::CheckThread(void *pParam)
{
	info(MAINFRAME_MODULE, "Thread for UI block entered.");
	auto self = static_cast<PLSBlockDump *>(pParam);
	self->CheckThreadInner();
	info(MAINFRAME_MODULE, "Thread for UI block to exit.");
	return 0;
}

PLSBlockDump *PLSBlockDump::Instance()
{
	static PLSBlockDump ins;
	return &ins;
}

PLSBlockDump::PLSBlockDump()
{

	threadExitEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	InitSavePath();
	//StartMonitor(); // Here should not invoke StartMonitor() because QT may not be ready
}

PLSBlockDump::~PLSBlockDump()
{
	StopMonitor();
	::CloseHandle(threadExitEvent);
}

void PLSBlockDump::StartMonitor()
{
	if (!heartbeatTimer) {
		heartbeatTimer = this->startTimer(HEARTBEAT_INTERVAL);
		assert(heartbeatTimer > 0);

		auto app = static_cast<PLSApp *>(QCoreApplication::instance());
		connect(app, &PLSApp::AppNotify, this, [this](void *obj, void *evt) {
			preEventTime = GetTickCount();
			preObject = (DWORD64)obj;
			preEvent = (DWORD64)evt;
		});
	}

	if (!checkBlockThread) {
		::ResetEvent(threadExitEvent);
		checkBlockThread = (HANDLE)_beginthreadex(nullptr, 0, CheckThread, this, 0, nullptr);
	}
}

void PLSBlockDump::StopMonitor()
{
	if (heartbeatTimer) {
		killTimer(heartbeatTimer);
		heartbeatTimer = 0;
	}

	if (checkBlockThread) {
		::SetEvent(threadExitEvent);

#ifdef DEBUG
		::WaitForSingleObject(checkBlockThread, INFINITE);
#else
		if (WAIT_OBJECT_0 != ::WaitForSingleObject(checkBlockThread, 5000)) {
			warn(MAINFRAME_MODULE, "Failed to wait block thread exit, terminate it");
			TerminateThread(checkBlockThread, 1);
		}
#endif

		::CloseHandle(checkBlockThread);
		checkBlockThread = nullptr;
	}
}

void PLSBlockDump::SignExitEvent()
{
	info(MAINFRAME_MODULE, "Notify to exit block thread");
	::SetEvent(threadExitEvent);
}

void PLSBlockDump::InitSavePath()
{
	QString temp = pls_get_user_path("PRISMLiveStudio/blockDump/");
	dumpDirectory = temp.toStdWString();

	int err = SHCreateDirectoryEx(nullptr, dumpDirectory.c_str(), nullptr);
	if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS) {
		warn(MAINFRAME_MODULE, "Failed to create directory for saving block dump. error:%d", err);
	}
}

void PLSBlockDump::CheckThreadInner()
{
	preEventTime = GetTickCount();

	bool isBlocked = false;
	int fileIndex = 1;
	int dumpCount = 0;
	DWORD preDumpTime = 0;

#ifdef _DEBUG
	bool debug_mode = true;
#else
	bool debug_mode = false;
#endif

	QString timeoutStr = QString::number(PLSGpopData::instance()->getUIBlockingTimeS());
	pls_add_global_field("blockTimeoutS", timeoutStr.toStdString().c_str(), PLS_SET_TAG_CN);

	long previous_state = 0;

	while (!IsHandleSigned(threadExitEvent, HEARTBEAT_INTERVAL)) {

		bool blocked = IsBlockState(preEventTime, GetTickCount());
		if (blocked != isBlocked) {
			isBlocked = blocked;
			if (blocked) {

				plslogex(PLS_LOG_WARN, MAINFRAME_MODULE, {{"UIBlock", GlobalVars::prismSession.c_str()}}, "%s UI thread is blocked", debug_mode ? "[Debug Mode]" : "");
			} else {
				pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
				plslogex(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "%s UI thread recovered", debug_mode ? "[Debug Mode]" : "");
			}
		}

		if (!blocked) {
			preDumpTime = 0;
			dumpCount = 0;
			continue;
		}

		if (dumpCount < MAX_BLOCK_DUMP_COUNT) {
			DWORD dumpInterval = dumpCount * SAVE_DUMP_INTERVAL;
			if (GetTickCount() - preDumpTime < dumpInterval) {
				continue;
			}

			std::string path = SaveDumpFile(fileIndex);

			if (IsHandleSigned(threadExitEvent, 0)) {
				info(MAINFRAME_MODULE, "Ignore the saved block dump because to exit thread");
				break;
			}

			if (!path.empty()) {
				pls_add_global_field("blockDumpPath", path.c_str(), PLS_SET_TAG_CN);
				info(MAINFRAME_MODULE, "blocked dump is sent to log process");
			}

			preDumpTime = GetTickCount();
			++fileIndex;
			++dumpCount;
		}
	}

	if (isBlocked) {
		pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
		plslogex(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "%s PRISM is exiting, so we think UI thread recovered", debug_mode ? "[Debug Mode]" : "");
	}
}

bool PLSBlockDump::IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond) const
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

bool PLSBlockDump::IsBlockState(DWORD preHeartbeat, DWORD currentTime) const
{
	if (currentTime <= preHeartbeat) {
		return false; // normal state
	}

	DWORD heartbeatSpaceS = (currentTime - preHeartbeat) / 1000; // in seconds
	if (heartbeatSpaceS < (DWORD)PLSGpopData::instance()->getUIBlockingTimeS()) {
		return false; // normal state
	}

	return true; // blocked
}

std::string PLSBlockDump::SaveDumpFile(int index)
{
	std::string full_path = "";

	std::array<wchar_t, MAX_PATH> dir;
	GetModuleFileNameW(nullptr, dir.data(), (DWORD)dir.size());
	PathRemoveFileSpecW(dir.data());

	std::wstring processPath = std::wstring(dir.data()) + std::wstring(L"\\PrismCommandLine.exe");
	if (-1 == _waccess(processPath.c_str(), 0)) {
		warn(MAINFRAME_MODULE, "command line exe is not found");
		return "";
	}

	SYSTEMTIME st;
	::GetLocalTime(&st);

	std::array<wchar_t, MAX_PATH> dumpPath;
	swprintf_s(dumpPath.data(), dumpPath.size(), _T("%sBLOCK_%04d%02d%02d-%02d%02d%02d_%u(%d).dmp"), dumpDirectory.c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
		   GetCurrentProcessId(), index);

	char *utf8_path = nullptr;
	os_wcs_to_utf8_ptr(dumpPath.data(), 0, &utf8_path);
	if (utf8_path) {
		full_path = utf8_path;
		bfree(utf8_path);
	}

	info(MAINFRAME_MODULE, "Call MiniDumpWriteDump() to save block dump. PreObject: %X PreEvent: %X", preObject.load(), preEvent.load());
	DWORD tm = timeGetTime();
	bool bOK = RunCaptureProcess(processPath.c_str(), dumpPath.data());
	tm = timeGetTime() - tm;
	info(MAINFRAME_MODULE, "Finish MiniDumpWriteDump(). %ums is taken. success:%s dump: %s", tm, bOK ? "yes" : "no", bOK ? pls_get_path_file_name(full_path.c_str()) : "no dump");

	if (bOK) {
		return full_path;
	} else {
		return "";
	}
}

bool PLSBlockDump::RunCaptureProcess(const wchar_t *exePath, const wchar_t *dumpPath)
{
	std::array<wchar_t, 2048> cmd;
	swprintf_s(cmd.data(), cmd.size(), L"\"%s\" \"CaptureDump\" \"%s\" \"%u\"", exePath, dumpPath, GetCurrentProcessId());

	PROCESS_INFORMATION pi = {};
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_FORCEOFFFEEDBACK;
	si.wShowWindow = SW_HIDE;

	BOOL bOK = CreateProcessW(exePath, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
	if (!bOK) {
		DWORD errorCode = GetLastError();
		warn(MAINFRAME_MODULE, "Fail to run command line exe and last error is %u", errorCode);
		assert(false);
		return false;
	}

	info(MAINFRAME_MODULE, "Success to run command line exe and PID is %u", pi.dwProcessId);

	std::array<HANDLE, 2> events = {threadExitEvent, pi.hProcess};
	WaitForMultipleObjects((DWORD)events.size(), events.data(), FALSE, 60 * 1000);

	if (IsHandleSigned(threadExitEvent, 0)) {
		info(MAINFRAME_MODULE, "Detected exit event and will ignore command line exe");
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return false;
	}

	if (IsHandleSigned(pi.hProcess, 0)) {
		DWORD exitCode = 0;
		GetExitCodeProcess(pi.hProcess, &exitCode);

		info(MAINFRAME_MODULE, "command line exe exited and code is %u", exitCode);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return (exitCode == 0);

	} else {
		// Warning: Here we can't call TerminateProcess()
		warn(MAINFRAME_MODULE, "Timeout to wait command line exe exit");
		assert(false);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return false;
	}
}
