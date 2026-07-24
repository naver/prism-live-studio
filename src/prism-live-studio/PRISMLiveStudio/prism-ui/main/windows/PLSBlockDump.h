#pragma once
#include <atomic>
#include <mutex>
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

	void UpdateNotifyEvent(QObject *, QEvent *);
	std::string GetPreviousObject();

protected:
	void InitSavePath();
	void CheckThreadInner();
	bool IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond) const;
	bool IsBlockState(ULONGLONG preHeartbeat, ULONGLONG currentTime, int timeoutMs) const;
	std::string SaveDumpFile(int index);
	bool RunCaptureProcess(const wchar_t *exePath, const wchar_t *dumpPath);

private:
	std::wstring dumpDirectory = L""; // include "\" at the tail

	int heartbeatTimer = 0;

	std::atomic<DWORD64> preEventTime = GetTickCount64();
	QString preObjectName;
	QString preClassName;
	std::recursive_mutex lockPreName;

	HANDLE checkBlockThread = nullptr;
	HANDLE threadExitEvent = nullptr;
};
