#pragma once
#include <Windows.h>

class PLSGdiData {
public:
	explicit PLSGdiData(HWND hwnd);
	~PLSGdiData();

	PLSGdiData(const PLSGdiData &) = delete;
	PLSGdiData &operator=(const PLSGdiData &) = delete;

	PLSGdiData(PLSGdiData &&) = delete;
	PLSGdiData &operator=(PLSGdiData &&) = delete;

	void lock();
	void unlock();

	CRITICAL_SECTION cs;
	HWND hwn_window;
	HDC source_dc = nullptr;
	HDC dest_dc = nullptr;
	BYTE *buffer = nullptr;
	HBITMAP bitmap = nullptr;
};
