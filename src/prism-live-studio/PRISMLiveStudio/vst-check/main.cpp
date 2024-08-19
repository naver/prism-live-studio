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

#if defined(_WIN32)
static std::optional<const wchar_t *> cmdline_get_arg_w(int argc, wchar_t **argv, const wchar_t *name)
{
	auto nameLength = wcslen(name);
	for (int i = 1; i < argc; ++i) {
		if (!wcsncmp(argv[i], name, nameLength)) {
			return argv[i] + nameLength;
		}
	}
	return std::nullopt;
}
#endif

int checkScanVst(const char *vst_path, const char *dll_path)
{
#if defined(__APPLE__)
	QThread::msleep(2 * 1000);
#endif
	void *library;
	library = os_dlopen(dll_path);
	if (!library) {
		assert(false);
		return VST_STATUS_PROCESS_OPEN_DLL_ERROR;
	}

	obs_vst_verify_state result = VST_STATUS_AVAILABLE;
	auto func = (scanVstInterface)os_dlsym(library, "ScanVstPlugin");
	if (!func) {
		assert(false);
		os_dlclose(library);
		return VST_STATUS_PROCESS_GET_SCAN_FUNC_ERROR;
	} else {
		result = (obs_vst_verify_state)func(vst_path); //get the result from function
	}

	os_dlclose(library);

#if defined(_WIN32)
	pls_process_terminate(result);
#endif

	return result;
}

#if defined(_WIN32)
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#if defined(__APPLE__)
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
#elif defined(_WIN32)
	auto vst_path_w = cmdline_get_arg_w(argc, argv, L"--vst-path=");
	if (!vst_path_w.has_value()) {
		return 0;
	}
	auto dll_path_w = cmdline_get_arg_w(argc, argv, L"--vst-module-path=");
	if (!dll_path_w.has_value()) {
		return 0;
	}

	char *vst_path = nullptr;
	char *dll_path = nullptr;
	os_wcs_to_utf8_ptr(vst_path_w.value(), 0, &vst_path);
	os_wcs_to_utf8_ptr(dll_path_w.value(), 0, &dll_path);

	if (!vst_path || !dll_path) {
		bfree(vst_path);
		bfree(dll_path);
		return 0;
	}

	auto result = checkScanVst(vst_path, dll_path);
	bfree(vst_path);
	bfree(dll_path);
#endif
	return result;
}
