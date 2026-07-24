#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <memory>
#include <obs.hpp>
#include "pls/pls-lens-info.h"

struct lens_logo {
	int width = 0;
	int height = 0;
	gs_texture_t *tex = nullptr;

	~lens_logo()
	{
		if (tex) {
			obs_enter_graphics();
			gs_texture_destroy(tex);
			obs_leave_graphics();
		}
	}
};

namespace LensLogo {

void init_lens_logo();
void clear_lens_logo();

const std::shared_ptr<lens_logo> get_lens_logo(uint32_t lens_index);
const std::shared_ptr<lens_logo> get_lens_logo(int width, int height);

}; // namespace LensLogo
