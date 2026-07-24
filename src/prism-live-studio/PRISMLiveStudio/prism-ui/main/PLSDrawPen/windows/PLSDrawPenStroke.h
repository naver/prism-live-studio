#pragma once
#include <vector>
#include <string>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <util/windows/ComPtr.hpp>
#include "../PLSDrawPenDefine.h"

struct Stroke {
	bool show = true;
	int lineWidth;
	long long index = -1;
	uint32_t rgba;
	std::string id;
	ShapeType shapeType;
	DrawType drawType;

	ComPtr<ID2D1Geometry> geometry{};
	std::vector<PointF> points;
	std::vector<Stroke> batchStrokes{};
};

template<class Interface> inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != nullptr) {
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = nullptr;
	}
}

class CCSection {
	CRITICAL_SECTION m_cs;

public:
	CCSection() { InitializeCriticalSection(&m_cs); }
	~CCSection() { DeleteCriticalSection(&m_cs); }

	void Lock() { EnterCriticalSection(&m_cs); }
	void Unlock() { LeaveCriticalSection(&m_cs); }
};

class CAutoLockCS {
	CCSection &m_Lock;

public:
	explicit CAutoLockCS(CCSection &cs) : m_Lock(cs) { m_Lock.Lock(); }

	~CAutoLockCS() { m_Lock.Unlock(); }
};
