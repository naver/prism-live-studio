#include <obs-module.h>
#include "prism-lens-source.h"
#include "lens-logo.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-lens-av", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "prism lens v2";
}

ULONG_PTR gdip_token = 0;

extern void register_lens_source();
extern void register_mobile_source();

bool obs_module_load(void)
{
	pls_init_lens_resolution();

	const Gdiplus::GdiplusStartupInput gdip_input;
	Gdiplus::GdiplusStartup(&gdip_token, &gdip_input, nullptr);

	LensLogo::init_lens_logo();
	ICaptureSession::init_lens_module();

	register_lens_source();
	register_mobile_source();
	return true;
}

void obs_module_unload(void)
{
	ICaptureSession::uninit_lens_module();
	LensLogo::clear_lens_logo();
	Gdiplus::GdiplusShutdown(gdip_token);
}
