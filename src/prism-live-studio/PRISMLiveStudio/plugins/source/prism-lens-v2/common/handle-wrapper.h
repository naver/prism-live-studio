#pragma once
#include <Windows.h>

/* clang-format off */

class CHandleWrapper {
public:
	static HANDLE GetAlreadyMap(const char *pName);
	static HANDLE GetMap(const char *pName, unsigned size, bool *pNewCreate = NULL);

	static HANDLE GetAlreadyEvent(const char *pName);
	static HANDLE GetEvent(const char *pName, bool bNeedManualReset = true, bool *pNewCreate = NULL);

	// WaitForSingleObject, ReleaseMutex
	static HANDLE GetMutex(const char *pName, bool *pNewCreate = NULL);
	static bool IsMutexAlready(const char *pName);

	static bool IsHandleValid(const HANDLE &hHandle);
	static bool IsHandleSigned(const HANDLE &hHandle, DWORD dwMilliSecond = 0);
	static void CloseHandleEx(HANDLE &hHandle);
	static void WaitThreadEnd(HANDLE &hThread, DWORD dwMilliSecond = INFINITE);
};

class CCriticalSection {
public:
	CCriticalSection() { InitializeCriticalSection(&m_Lock); }
	~CCriticalSection() { DeleteCriticalSection(&m_Lock); }

public:
	CRITICAL_SECTION m_Lock;
};

class CAutoLockCS {
public:
	explicit CAutoLockCS(CCriticalSection &cs) : m_cs(cs.m_Lock) { EnterCriticalSection(&m_cs); }
	~CAutoLockCS() { LeaveCriticalSection(&m_cs); }

private:
	CRITICAL_SECTION &m_cs;
};

class CAutoLockMutex {
public:
	explicit CAutoLockMutex(HANDLE &mt) : m_mutex(mt) { WaitForSingleObject(m_mutex, INFINITE); }
	virtual ~CAutoLockMutex() { ReleaseMutex(m_mutex); }

private:
	HANDLE &m_mutex;
};

/* clang-format on */
