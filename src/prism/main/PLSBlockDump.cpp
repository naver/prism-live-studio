#include "PLSBlockDump.h"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSPlatformApi/PLSPlatformApi.h"
#include "window-basic-status-bar.hpp"
#include <process.h>
#include <shlobj_core.h>
#include <dbghelp.h>
#include <tchar.h>
#include "platform.hpp"

#pragma comment(lib, "dbghelp.lib")

#define HEARTBEAT_INTERVAL 500  // in milliseconds
#define SAVE_DUMP_INTERVAL 5000 // in milliseconds
#define MAX_BLOCK_DUMP_COUNT 5

unsigned __stdcall PLSBlockDump::CheckThread(void *pParam)
{
	PLS_INFO(MAINFRAME_MODULE, "Thread for UI block entered.");
	PLSBlockDump *self = reinterpret_cast<PLSBlockDump *>(pParam);
	self->CheckThreadInner();
	PLS_INFO(MAINFRAME_MODULE, "Thread for UI block to exit.");
	return 0;
}

PLSBlockDump *PLSBlockDump::Instance()
{
	static PLSBlockDump ins;
	return &ins;
}

PLSBlockDump::PLSBlockDump()
{
	threadExitEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
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
		preHeartbeatTime = GetTickCount();
		heartbeatTimer = this->startTimer(HEARTBEAT_INTERVAL);
		assert(heartbeatTimer > 0);
	}

	if (!checkBlockThread) {
		::ResetEvent(threadExitEvent);
		checkBlockThread = (HANDLE)_beginthreadex(0, 0, CheckThread, this, 0, 0);
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
			PLS_WARN(MAINFRAME_MODULE, "Failed to wait block thread exit, terminate it");
			TerminateThread(checkBlockThread, 1);
		}
#endif

		::CloseHandle(checkBlockThread);
		checkBlockThread = 0;
	}
}

void PLSBlockDump::SignExitEvent()
{
	PLS_INFO(MAINFRAME_MODULE, "Notify to exit block thread");
	::SetEvent(threadExitEvent);
}

void PLSBlockDump::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == heartbeatTimer) {
		preHeartbeatTime = GetTickCount();
	}
}

void PLSBlockDump::InitSavePath()
{
	QString temp = pls_get_user_path("PRISMLiveStudio/blockDump/");
	dumpDirectory = temp.toStdWString();

	int err = SHCreateDirectoryEx(0, dumpDirectory.c_str(), 0);
	if (err != ERROR_SUCCESS && err != ERROR_ALREADY_EXISTS) {
		PLS_WARN(MAINFRAME_MODULE, "Failed to create directory for saving block dump. error:%d", err);
	}
}

extern std::pair<double, double> getCurrentCPUUsage();
void PLSBlockDump::CheckThreadInner()
{
	preHeartbeatTime = GetTickCount();

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
	pls_add_global_field("blockTimeoutS", timeoutStr.toStdString().c_str());

	while (!IsHandleSigned(threadExitEvent, HEARTBEAT_INTERVAL)) {
		bool blocked = IsBlockState(preHeartbeatTime, GetTickCount());
		if (blocked != isBlocked) {
			isBlocked = blocked;
			if (blocked) {
				std::pair<double, double> cpuUsage = getCurrentCPUUsage();
				int cpuUsageTotal = (int)cpuUsage.first;
				int cpuUsagePrism = (int)cpuUsage.second;

				const char *fields[][2] = {{"UIBlock", prismSession.c_str()}};
				PLS_LOGEX(PLS_LOG_WARN, MAINFRAME_MODULE, fields, 1, "[PLSBlockDump]%s UI thread is blocked (CPU percent: %d/%d)", debug_mode ? "[Debug Mode]" : "", cpuUsagePrism,
					  cpuUsageTotal);
			} else {
				pls_add_global_field("blockDumpPath", "");
				const char *fields[][2] = {{"UIRecover", prismSession.c_str()}};
				PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, fields, 1, "[PLSBlockDump]%s UI thread recovered", debug_mode ? "[Debug Mode]" : "");
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

			std::wstring path = SaveDumpFile(fileIndex);

			if (IsHandleSigned(threadExitEvent, 0)) {
				PLS_INFO(MAINFRAME_MODULE, "Ignore the saved block dump because to exit thread");
				break;
			}

			if (!path.empty()) {
				char *utf8_path = NULL;
				os_wcs_to_utf8_ptr(path.c_str(), 0, &utf8_path);
				if (utf8_path) {
					PLS_INFO(MAINFRAME_MODULE, "Saved blocked dump : [%s]", GetFileName(utf8_path).c_str());
					pls_add_global_field("blockDumpPath", utf8_path);
					bfree(utf8_path);
					PLS_INFO(MAINFRAME_MODULE, "blocked dump is sent to log process");
				}
			}

			preDumpTime = GetTickCount();
			++fileIndex;
			++dumpCount;
		}
	}

	if (isBlocked) {
		pls_add_global_field("blockDumpPath", "");
		const char *fields[][2] = {{"UIRecover", prismSession.c_str()}};
		PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, fields, 1, "[PLSBlockDump]%s PRISM is exiting, so we think UI thread recovered", debug_mode ? "[Debug Mode]" : "");
	}
}

bool PLSBlockDump::IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond)
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

bool PLSBlockDump::IsBlockState(DWORD preHeartbeat, DWORD currentTime)
{
	if (currentTime <= preHeartbeat) {
		return false; // normal state
	}

	DWORD heartbeatSpaceS = (currentTime - preHeartbeat) / 1000; // in seconds
	if (heartbeatSpaceS < PLSGpopData::instance()->getUIBlockingTimeS()) {
		return false; // normal state
	}

	return true; // blocked
}

std::wstring PLSBlockDump::SaveDumpFile(int index)
{
	SYSTEMTIME st;
	::GetLocalTime(&st);

	wchar_t path[MAX_PATH] = {0};
	swprintf_s(path, _T("%sBLOCK_%04d%02d%02d-%02d%02d%02d_%u(%d).dmp"), dumpDirectory.c_str(), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId(), index);

	HANDLE lhDumpFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (lhDumpFile && lhDumpFile != INVALID_HANDLE_VALUE) {
		PLS_INFO(MAINFRAME_MODULE, "Call MiniDumpWriteDump() to save block dump");
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), lhDumpFile, (MINIDUMP_TYPE)MiniDumpWithFullMemoryInfo, NULL, NULL, NULL);
		CloseHandle(lhDumpFile);
		return path;
	} else {
		PLS_WARN(MAINFRAME_MODULE, "Failed to save blocked dump. LastErr:%u", GetLastError());
		return L"";
	}
}
