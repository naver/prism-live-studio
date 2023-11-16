#include "libutils-api.h"
#include <pls/pls-obs-api.h>
#include "util/platform.h"
#if defined(_WIN32)
#include <Windows.h>
constexpr auto VST_DLL = "obs-vst.dll";
#elif defined(__APPLE__)
constexpr auto VST_DLL = "obs-vst.plugin";
#endif
#include <QDir>

using scanVstInterface = int (*)(const char *);

#if defined(__APPLE__)
#define LOCAL_DEBUG 0
#endif

int checkScanVst(const char *vst_path, const char *dll_path)
{
    QThread::msleep(2 * 1000);
	void *library;
	library = os_dlopen(dll_path);
	if (!library) {
		assert(false);
		return VST_STATUS_UNKNOWN_ERROR;
	}

	obs_vst_verify_state result = VST_STATUS_AVAILABLE;
	auto func = (scanVstInterface)os_dlsym(library, "ScanVstPlugin");
	if (!func) {
		assert(false);
		os_dlclose(library);
		return VST_STATUS_UNKNOWN_ERROR;
	} else {
		result = (obs_vst_verify_state)func(vst_path); //get the result from function
	}

	os_dlclose(library);
	return result;
}

int main(int argc, char *argv[])
{
	auto vst_path = pls_cmdline_get_arg(argc, argv, "--vst-path=");
#if LOCAL_DEBUG
    vst_path = "/Library/Audio/Plug-Ins/VST/Auburn Sounds Graillon 2.vst";
#endif
	if (!vst_path.has_value()) {
		return 0;
	}
	auto dll_path = pls_cmdline_get_arg(argc, argv, "--vst-module-path=");
#if LOCAL_DEBUG
    dll_path = "/Users/ncc/Documents/1/prism-live-studio/src/obs-studio/build/plugins/obs-vst/Debug/obs-vst.plugin/Contents/MacOS/obs-vst";
#endif
	if (!dll_path.has_value()) {
		return 0;
	}

	auto result = checkScanVst(vst_path.value(), dll_path.value());
	return result;
}
