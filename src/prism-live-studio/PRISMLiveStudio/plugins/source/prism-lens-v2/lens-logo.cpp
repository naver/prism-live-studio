#include "lens-logo.h"
#include "prism-lens-source.h"
#include <pls/pls-base.h>
#include "pls/pls-obs-api.h"
#include <shlobj_core.h>
#include <shobjidl.h>
#include <strsafe.h>
#include <mutex>
#include <array>
#include <thread>

#pragma comment(lib, "Gdiplus.lib")

#define PLACE_HOLDER_LOGO_FRAME L"lens_logo.png"

std::recursive_mutex lock_logo;

// we cached all logo textures for kinds of resolution, all sources use the same texture
std::vector<std::shared_ptr<lens_logo>> lens_logos;

std::atomic_bool logo_inited = false;
std::thread *logo_thread = nullptr;

namespace LensLogo {
//------------------------------------------- lens logo image ----------------------------------------------
// this function comes from lens vcam with same function name
std::shared_ptr<uint8_t> DrawPlaceholderBitmap(Gdiplus::Bitmap &image, int width, int height)
{
	if (width <= 0 || height <= 0) {
		assert(false);
		return nullptr;
	}

	const size_t bitsCount = width * height * 4; // BGRA
	std::shared_ptr<uint8_t> data(new (std::nothrow) uint8_t[bitsCount], [](uint8_t *ptr) { delete[] ptr; });
	Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB, (unsigned char *)data.get());

	Gdiplus::Graphics graphics(&bitmap);
	Gdiplus::SolidBrush brush(Gdiplus::Color(255, 2, 2, 7)); // defined in lens vcam

	Gdiplus::Region ellipseRegion(Gdiplus::Rect(0.f, 0.f, bitmap.GetWidth(), bitmap.GetHeight()));
	graphics.FillRegion(&brush, &ellipseRegion);

	Gdiplus::RectF destRect;
	if (width == height) {
		destRect = Gdiplus::RectF((bitmap.GetWidth() - image.GetWidth()) / 2, (bitmap.GetHeight() - image.GetHeight()) / 2, image.GetWidth(), image.GetHeight());
	} else if (width > height) {
		auto destWidth = static_cast<float>(bitmap.GetWidth()) * 0.37f;
		auto destHeight = static_cast<float>(bitmap.GetHeight()) * 0.52f;
		destRect = Gdiplus::RectF((static_cast<float>(bitmap.GetWidth()) - destWidth) / 2.f, (static_cast<float>(bitmap.GetHeight()) - destHeight) / 2, destWidth, destHeight);
	} else {
		auto destWidth = static_cast<float>(bitmap.GetWidth()) * 0.52f;
		auto destHeight = static_cast<float>(bitmap.GetHeight()) * 0.24f;
		destRect = Gdiplus::RectF((static_cast<float>(bitmap.GetWidth()) - destWidth) / 2.f, (static_cast<float>(bitmap.GetHeight()) - destHeight) / 2, destWidth, destHeight);
	}

	graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	graphics.DrawImage(&image, destRect);

	return data;
}

void init_lens_logo_tex(Gdiplus::Bitmap &image, int width, int height)
{
	std::shared_ptr<uint8_t> data = DrawPlaceholderBitmap(image, width, height);
	if (!data) {
		assert(false);
		return;
	}

	const uint8_t *buf = (uint8_t *)data.get();
	obs_enter_graphics();
	gs_texture_t *tex = gs_texture_create(width, height, GS_BGRA, 1, &buf, GS_DYNAMIC);
	obs_leave_graphics();

	if (!tex) {
		PLS_WARN("lens capture", "%s lens: failed to create logo texture %dx%d", __FUNCTION__, width, height);
		assert(false);
		return;
	}

	std::shared_ptr<lens_logo> logo = std::make_shared<lens_logo>();
	logo->width = width;
	logo->height = height;
	logo->tex = tex;

	std::lock_guard<std::recursive_mutex> lock(lock_logo);
	lens_logos.push_back(logo);
}

void init_lens_logo()
{
	logo_thread = new std::thread([]() {
		char *logo_path = obs_module_file("lens_logo.png");
		if (!logo_path) {
			PLS_WARN("lens capture", "%s lens: can not find logo image", __FUNCTION__);
			return;
		}

		std::shared_ptr<int> auto_clear(nullptr, [=](int *) { bfree(logo_path); });

		wchar_t wpath[MAX_PATH] = {0};
		MultiByteToWideChar(CP_UTF8, 0, logo_path, -1, wpath, MAX_PATH);

		Gdiplus::Bitmap image(wpath);
		if (image.GetLastStatus() != Gdiplus::Ok) {
			PLS_WARN("lens capture", "%s lens: failed to load logo, status=%d", __FUNCTION__, (int)image.GetLastStatus());
			assert(false);
			return;
		}

		// Prepare logo texture for every resolution supported by lens.
		// If new resolution is added in lens, assert in {get_lens_logo} will be triggered.
		init_lens_logo_tex(image, 1920, 1080);
		init_lens_logo_tex(image, 1280, 720);
		init_lens_logo_tex(image, 1080, 1080);
		init_lens_logo_tex(image, 1080, 1920);
		init_lens_logo_tex(image, 720, 1280);

		init_lens_logo_tex(image, 854, 480);
		init_lens_logo_tex(image, 640, 360);
		init_lens_logo_tex(image, 480, 854);
		init_lens_logo_tex(image, 360, 640);

		PLS_INFO("lens capture", "%s lens: logo texture is initialized", __FUNCTION__);
		logo_inited = true;
	});
}

void clear_lens_logo()
{
	if (logo_thread) {
		if (logo_thread->joinable())
			logo_thread->join();

		delete logo_thread;
		logo_thread = nullptr;
	}

	std::vector<std::shared_ptr<lens_logo>> temp;

	{
		std::lock_guard<std::recursive_mutex> lock(lock_logo);
		// In destructor of lens_logo, we will destroy the texture during obs_enter_graphics and obs_leave_graphics.
		// Here we move the logo list to a temporary vector to avoid request obs_enter_graphics under lock_logo
		temp = std::move(lens_logos);
	}

	temp.clear();
}

const std::shared_ptr<lens_logo> get_lens_logo(uint32_t index)
{
	if (index >= MAX_LENS_COUNT) {
		assert(false);
		return nullptr;
	}

	int width, height;
	pls_get_lens_resolution(index, &width, &height);

	return get_lens_logo(width, height);
}

const std::shared_ptr<lens_logo> get_lens_logo(int width, int height)
{
	if (!logo_inited)
		return nullptr;

	{
		std::lock_guard<std::recursive_mutex> lock(lock_logo);
		for (const auto &item : lens_logos) {
			if (item->width == width && item->height == height) {
				return item;
			}
		}
	}

	static bool log_saved = false;
	if (!log_saved) {
		log_saved = true;
		PLS_WARN("lens capture", "%s lens: found new resolution %dx%d", __FUNCTION__, width, height);

		assert(false);
		if (pls_is_dev_mode()) { // This helps find missed resolution in dev mode
			MessageBox(0, L"found new resolution in lens app", L"ERROR", 0);
		}
	}

	assert(false && "new lens resolution ?");
	return nullptr;
}

}; // namespace LensLogo
