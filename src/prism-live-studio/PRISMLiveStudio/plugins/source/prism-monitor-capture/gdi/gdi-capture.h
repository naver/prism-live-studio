#pragma once
#include <Windows.h>
#include "gdi-data.h"
#include "obs.h"
#include <memory>

class PLSGdiCapture {
public:
	PLSGdiCapture() = default;
	~PLSGdiCapture();

	PLSGdiCapture(const PLSGdiCapture &) = delete;
	PLSGdiCapture &operator=(const PLSGdiCapture &) = delete;

	PLSGdiCapture(PLSGdiCapture &&) = delete;
	PLSGdiCapture &operator=(PLSGdiCapture &&) = delete;

	bool check_init(int cx, int cy, HWND hWnd = nullptr);
	void capture(int bkOffsetX, int bkOffsetY, bool bDrawCursor, int curOffsetX, int curOffsetY, float fZoom);
	void uninit();
	gs_texture *get_texture() { return texture; };
	void get_width_height(int &capture_width, int &capture_height) const;

protected:
	void draw_window(int offset_x, int offset_y) const;
	void draw_window_cursor(HDC &mem_dc, LONG offset_x, LONG offset_y, float zoom) const;

private:
	HWND hwnd_window = nullptr;
	int width = 0;
	int height = 0;
	std::unique_ptr<PLSGdiData> gdi_data_handle;
	gs_texture *texture = nullptr;
	int texture_width = 0;
	int texture_height = 0;
};
