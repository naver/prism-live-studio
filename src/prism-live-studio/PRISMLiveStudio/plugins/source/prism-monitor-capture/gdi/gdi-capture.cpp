#include "gdi-capture.h"
#include <stdio.h>

PLSGdiCapture::~PLSGdiCapture()
{
	uninit();
}

bool PLSGdiCapture::check_init(int cx, int cy, HWND hwnd /*= 0*/)
{
	if (cx == 0 || cy == 0) {
		return false;
	}

	if (cx == width && cy == height && hwnd == hwnd_window && gdi_data_handle != nullptr && gdi_data_handle->buffer != nullptr) {
		return true;
	}

	uninit();

	hwnd_window = hwnd;
	width = cx;
	height = cy;
	gdi_data_handle = std::make_unique<PLSGdiData>(hwnd);

	if (hwnd)
		gdi_data_handle->source_dc = ::GetDC(hwnd);
	else
		gdi_data_handle->source_dc = CreateDC(TEXT("DISPLAY"), nullptr, nullptr, nullptr);

	gdi_data_handle->dest_dc = ::CreateCompatibleDC(gdi_data_handle->source_dc);

	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biWidth = width;
	bi.bmiHeader.biHeight = height;
	bi.bmiHeader.biPlanes = 1;

	gdi_data_handle->bitmap = CreateDIBSection(gdi_data_handle->dest_dc, &bi, DIB_RGB_COLORS, (void **)&gdi_data_handle->buffer, nullptr, 0);
	if (gdi_data_handle->bitmap)
		::SelectObject(gdi_data_handle->dest_dc, gdi_data_handle->bitmap);

	return !!gdi_data_handle->buffer;
}

void PLSGdiCapture::capture(int bkOffsetX, int bkOffsetY, bool draw_cursor, int cur_offset_x, int cur_offset_y, float zoom)
{
	if (!gdi_data_handle)
		return;

	gdi_data_handle->lock();
	draw_window(bkOffsetX, bkOffsetY);

	if (draw_cursor)
		draw_window_cursor(gdi_data_handle->dest_dc, cur_offset_x, cur_offset_y, zoom);

	if (!texture || texture_width != width || texture_height != height) {
		gs_texture_destroy(texture);
		texture = gs_texture_create(width, height, GS_BGRA, 1, nullptr, GS_DYNAMIC);
		texture_width = width;
		texture_height = height;
	}

	uint32_t line_size = width * 4;
	gs_texture_set_image(texture, gdi_data_handle->buffer, line_size, true);

	gdi_data_handle->unlock();
}

void PLSGdiCapture::uninit()
{
	hwnd_window = nullptr;
	width = 0;
	height = 0;
	gdi_data_handle.reset();
	if (texture) {
		obs_enter_graphics();
		gs_texture_destroy(texture);
		obs_leave_graphics();
		texture = nullptr;
	}
}

void PLSGdiCapture::draw_window(int offset_x, int offset_y) const
{
	if (!gdi_data_handle)
		return;

	memset(gdi_data_handle->buffer, 0, width * 4 * height);
	::BitBlt(gdi_data_handle->dest_dc, 0, 0, width, height, gdi_data_handle->source_dc, offset_x, offset_y, SRCCOPY);
}

void PLSGdiCapture::draw_window_cursor(HDC &mem_dc, LONG offset_x, LONG offset_y, float zoom) const
{
	CURSORINFO ci = {};
	ci.cbSize = sizeof(ci);

	HICON hIcon = nullptr;

	do {
		bool res = false;
		if (::GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING)) {
			hIcon = ::CopyIcon(ci.hCursor);
			if (hIcon) {
				res = true;
			}
		}

		if (!res)
			break;

		ICONINFO hIconInfo;
		if (::GetIconInfo(hIcon, &hIconInfo)) {
			POINT pos;
			pos.x = ci.ptScreenPos.x - (int)hIconInfo.xHotspot - (int)offset_x;
			pos.y = ci.ptScreenPos.y - (int)hIconInfo.yHotspot - (int)offset_y;

			if (zoom > 1.f) {
				pos.x = static_cast<LONG>((float)pos.x * zoom);
				pos.y = static_cast<LONG>((float)pos.y * zoom);
			}

			DrawIcon(mem_dc, pos.x, pos.y, hIcon);

			::DeleteObject(hIconInfo.hbmColor);
			::DeleteObject(hIconInfo.hbmMask);
		}
	} while (false);

	if (hIcon)
		::DestroyIcon(hIcon);

	if (ci.hCursor)
		::DeleteObject(ci.hCursor);
}

void PLSGdiCapture::get_width_height(int &capture_width, int &capture_height) const
{
	capture_width = texture_width;
	capture_height = texture_height;
}
