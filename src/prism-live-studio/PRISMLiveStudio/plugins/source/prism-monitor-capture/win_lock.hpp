#pragma once
#include <Windows.h>

class CCSection {
	CRITICAL_SECTION m_cs;

public:
	CCSection() { InitializeCriticalSection(&m_cs); }
	~CCSection() { DeleteCriticalSection(&m_cs); }

	CCSection(const CCSection &) = delete;
	CCSection &operator=(const CCSection &) = delete;
	CCSection(CCSection &&) = delete;
	CCSection &operator=(CCSection &&) = delete;

	void Lock() { EnterCriticalSection(&m_cs); }
	void Unlock() { LeaveCriticalSection(&m_cs); }
};

class CAutoLockCS {
	CCSection &m_Lock;

public:
	explicit CAutoLockCS(CCSection &cs) : m_Lock(cs) { m_Lock.Lock(); }
	~CAutoLockCS() { m_Lock.Unlock(); }

	CAutoLockCS(const CAutoLockCS &) = delete;
	CAutoLockCS &operator=(const CAutoLockCS &) = delete;
	CAutoLockCS(CAutoLockCS &&) = delete;
	CAutoLockCS &operator=(CAutoLockCS &&) = delete;
};
