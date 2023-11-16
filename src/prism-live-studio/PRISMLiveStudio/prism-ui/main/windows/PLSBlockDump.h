#pragma once
#include <atomic>
#include <Windows.h>
#include <string>
#include <QObject>
#include <QTimerEvent>

// %APPDATA%\PRISMLiveStudio\blockDump
class PLSBlockDump : public QObject {
	Q_OBJECT

	PLSBlockDump();

public:
	static unsigned __stdcall CheckThread(void *pParam);

	static PLSBlockDump *Instance();

	~PLSBlockDump() override;

	PLSBlockDump(const PLSBlockDump &) = delete;
	PLSBlockDump &operator=(const PLSBlockDump &) = delete;
	PLSBlockDump(PLSBlockDump &&) = delete;
	PLSBlockDump &operator=(PLSBlockDump &&) = delete;

	void StartMonitor();
	void StopMonitor();
	void SignExitEvent();

protected:
	void InitSavePath();
	void CheckThreadInner();
	bool IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond) const;
	bool IsBlockState(DWORD preHeartbeat, DWORD currentTime) const;
	std::string SaveDumpFile(int index);
	bool RunCaptureProcess(const wchar_t *exePath, const wchar_t *dumpPath);

private:
	std::wstring dumpDirectory = L""; // include "\" at the tail

	int heartbeatTimer = 0;

	std::atomic<DWORD> preEventTime = GetTickCount();
	std::atomic<DWORD64> preObject = 0;
	std::atomic<DWORD64> preEvent = 0;

	HANDLE checkBlockThread = nullptr;
	HANDLE threadExitEvent = nullptr;
};
