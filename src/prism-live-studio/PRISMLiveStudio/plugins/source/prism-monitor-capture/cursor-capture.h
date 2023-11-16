#pragma once
#include <Windows.h>
#include <stdint.h>
#include "obs.h"

struct capture_cursor_info {
	uint8_t *bitmap = nullptr;
	HCURSOR current_cursor = nullptr;
	bool same_as_previous = false;
	bool visible = false;
	long hotspot_x = 0;
	long hotspot_y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	POINT pos = {0};
};

class PLSCursorCapture {
public:
	PLSCursorCapture() = default;
	~PLSCursorCapture();

	PLSCursorCapture(const PLSCursorCapture &) = delete;
	PLSCursorCapture &operator=(const PLSCursorCapture &) = delete;

	PLSCursorCapture(PLSCursorCapture &&) = delete;
	PLSCursorCapture &operator=(PLSCursorCapture &&) = delete;

	bool cursor_render(long x_offset, long y_offset, float x_scale, float y_scale, long width, long height);

private:
	bool cursor_capture_icon(HICON icon);
	uint8_t *cursor_capture_icon_bitmap(ICONINFO *ii, uint32_t &width, uint32_t &height);
	uint8_t *copy_from_color(ICONINFO *ii, uint32_t &width, uint32_t &height);
	uint8_t *copy_from_mask(ICONINFO *ii, uint32_t &width, uint32_t &height);
	uint8_t *get_bitmap_data(HBITMAP hbmp, BITMAP *bmp);
	uint8_t bit_to_alpha(const uint8_t *data, long pixel, bool invert) const;
	bool bitmap_has_alpha(const uint8_t *data, long num_pixels) const;
	void apply_mask(uint8_t *color, const uint8_t *mask, long num_pixels) const;

	bool update_texture();
	bool cursor_capture();
	void release_cursor_buffer();

	capture_cursor_info cursor_info;
	int texture_width = 0;
	int texture_height = 0;
	gs_texture *texture = nullptr;
};
