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
#include "PLSBlockRecorder.hpp"

#pragma comment(lib, "WinMM.lib")
#pragma comment(lib, "Shlwapi.lib")

const auto SAVE_DUMP_INTERVAL = 10000; // in milliseconds
const auto MAX_BLOCK_DUMP_COUNT = 3;

#define info(module, format, ...) PLS_INFO(module, "[PLSBlockDump] " format, ##__VA_ARGS__)
#define warn(module, format, ...) PLS_WARN(module, "[PLSBlockDump] " format, ##__VA_ARGS__)

extern std::atomic<bool> exception_happened;

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

void PLSBlockDump::UpdateNotifyEvent(QObject *obj, QEvent *)
{
	preEventTime = GetTickCount64();

	if (!obj)
		return;

	std::lock_guard<std::recursive_mutex> lock(lockPreName);
	preObjectName = obj->objectName();
	preClassName = obj->metaObject()->className();
}

std::string PLSBlockDump::GetPreviousObject()
{
	QString preObject;
	QString preClass;

	{
		std::lock_guard<std::recursive_mutex> lock(lockPreName);
		preObject = preObjectName;
		preClass = preClassName;
	}

	if (!preObject.isEmpty() && !preClass.isEmpty()) {
		auto text = QString("%1::%2").arg(preClass).arg(preObject);
		return text.toStdString();
	} else if (!preObject.isEmpty())
		return preObject.toStdString();
	else if (!preClass.isEmpty())
		return preClass.toStdString();
	else
		return "unknown";
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

#define RESET_CHECK                                                                                                                                                                 \
	{                                                                                                                                                                           \
		preEventTime = GetTickCount64();                                                                                                                                    \
		preDumpTime = 0;                                                                                                                                                    \
		dumpCount = 0;                                                                                                                                                      \
		recorder.Reset();                                                                                                                                                   \
		if (isBlocked) {                                                                                                                                                    \
			isBlocked = false;                                                                                                                                          \
			pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);                                                                                                  \
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s UI thread recovered and PC may have slept", \
				  debug_mode ? "[Debug Mode]" : "");                                                                                                                \
		}                                                                                                                                                                   \
	}

void PLSBlockDump::CheckThreadInner()
{
	preEventTime = GetTickCount64();

	bool isBlocked = false;
	int fileIndex = 1;
	int dumpCount = 0;
	DWORD64 preDumpTime = 0;

#ifdef _DEBUG
	bool debug_mode = true;
#else
	bool debug_mode = false;
#endif

	auto timeoutSec = PLSGpopData::instance()->getUIBlockingTimeS();
	QString timeoutStr = QString::number(timeoutSec);
	pls_add_global_field("blockTimeoutS", timeoutStr.toUtf8().constData(), PLS_SET_TAG_CN);

	long previous_state = 0;

	PLSBlockRecorder recorder;
	DWORD64 preLoopTime = GetTickCount64();

	while (!exception_happened && !IsHandleSigned(threadExitEvent, HEARTBEAT_INTERVAL)) {
		if (pls_ignore_render_drop()) {
			RESET_CHECK;
			continue;
		}

		auto crtTime = GetTickCount64();
		if (crtTime - preLoopTime > (10 * 1000)) {
			RESET_CHECK;
		}

		preLoopTime = crtTime;

		bool sreBlocked = IsBlockState(preEventTime.load(std::memory_order_relaxed), crtTime, SRE_BLOCK_TIMEOUT);
		recorder.UpdateBlockState(sreBlocked, sreBlocked ? GetPreviousObject() : std::string());

		bool blocked = IsBlockState(preEventTime.load(std::memory_order_relaxed), crtTime, timeoutSec * 1000);
		if (blocked != isBlocked) {
			isBlocked = blocked;
			if (blocked) {
				std::string preObj = GetPreviousObject();
				PLS_LOGEX(PLS_LOG_WARN, MAINFRAME_MODULE,
					  {
						  {"UIBlock", GlobalVars::prismSession.c_str()},
						  {"Position", preObj.c_str()},
					  },
					  "[PLSBlockDump] %s UI thread is blocked at %s", debug_mode ? "[Debug Mode]" : "", preObj.c_str());
			} else {
				pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
				PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s UI thread recovered", debug_mode ? "[Debug Mode]" : "");
			}
		}

		if (!blocked) {
			preDumpTime = 0;
			dumpCount = 0;
			continue;
		}

		if (dumpCount < MAX_BLOCK_DUMP_COUNT) {
			DWORD dumpInterval = dumpCount * SAVE_DUMP_INTERVAL;
			if (crtTime - preDumpTime < dumpInterval) {
				continue;
			}

			std::string path = SaveDumpFile(fileIndex);

			if (pls_ignore_render_drop()) {
				RESET_CHECK;
				continue;
			}

			if (exception_happened || IsHandleSigned(threadExitEvent, 0)) {
				info(MAINFRAME_MODULE, "Ignore the saved block dump because to exit thread");
				break;
			}

			if (!path.empty()) {
				pls_add_global_field("blockDumpPath", path.c_str(), PLS_SET_TAG_CN);
				info(MAINFRAME_MODULE, "blocked dump is sent to log process");
			}

			preDumpTime = GetTickCount64(); // here need to get real current time
			++fileIndex;
			++dumpCount;
		}
	}

	if (isBlocked) {
		pls_add_global_field("blockDumpPath", "", PLS_SET_TAG_CN);
		PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"UIRecover", GlobalVars::prismSession.c_str()}}, "[PLSBlockDump] %s PRISM is exiting, so we think UI thread recovered",
			  debug_mode ? "[Debug Mode]" : "");
	}
}

bool PLSBlockDump::IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond) const
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

bool PLSBlockDump::IsBlockState(ULONGLONG preHeartbeat, ULONGLONG currentTime, int timeoutMs) const
{
	if (currentTime <= preHeartbeat) {
		return false; // normal state
	}

	ULONGLONG heartbeatSpace = (currentTime - preHeartbeat);
	if (heartbeatSpace < (ULONGLONG)timeoutMs) {
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

	info(MAINFRAME_MODULE, "Call MiniDumpWriteDump() to save block dump");
	DWORD tm = timeGetTime();
	bool bOK = RunCaptureProcess(processPath.c_str(), dumpPath.data());
	tm = timeGetTime() - tm;
	info(MAINFRAME_MODULE, "Finish MiniDumpWriteDump(). %ums is taken. success:%s dump: %s ignoreDrop:%d", tm, bOK ? "yes" : "no", bOK ? pls_get_path_file_name(full_path.c_str()) : "no dump",
	     pls_ignore_render_drop());

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
