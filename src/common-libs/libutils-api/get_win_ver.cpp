#include "libutils-api.h"
#include "libutils-api-log.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <VersionHelpers.h>

#include <shared_mutex>

namespace {
const auto GET_WIN_VER_MODULE = "libutils-api/get_win_ver";

using get_file_version_info_size_w_t = DWORD(WINAPI *)(LPCWSTR mod, LPDWORD unused);
using get_file_version_info_w_t = BOOL(WINAPI *)(LPCWSTR mod, DWORD unused, DWORD len, LPVOID data);
using ver_query_value_w_t = BOOL(WINAPI *)(LPVOID data, LPCWSTR subblock, LPVOID *buf, PUINT sizeout);

struct LocalGlobalVars {
	static std::optional<pls_win_ver_t> g_win_ver;
	static get_file_version_info_size_w_t g_get_file_version_info_size;
	static get_file_version_info_w_t g_get_file_version_info;
	static ver_query_value_w_t g_ver_query_value;
	static bool g_ver_initialized;
	static bool g_ver_initialize_success;
};
std::optional<pls_win_ver_t> LocalGlobalVars::g_win_ver = std::nullopt;
get_file_version_info_size_w_t LocalGlobalVars::g_get_file_version_info_size = nullptr;
get_file_version_info_w_t LocalGlobalVars::g_get_file_version_info = nullptr;
ver_query_value_w_t LocalGlobalVars::g_ver_query_value = nullptr;
bool LocalGlobalVars::g_ver_initialized = false;
bool LocalGlobalVars::g_ver_initialize_success = false;
}

static bool initialize_version_functions(void)
{
	LocalGlobalVars::g_ver_initialized = true;

	HMODULE ver = GetModuleHandleW(L"version");
	if (!ver) {
		ver = LoadLibraryW(L"version");
		if (!ver) {
			PLS_ERROR(GET_WIN_VER_MODULE, "Failed to load windows version library");
			return false;
		}
	}

	LocalGlobalVars::g_get_file_version_info_size = (get_file_version_info_size_w_t)GetProcAddress(ver, "GetFileVersionInfoSizeW");
	LocalGlobalVars::g_get_file_version_info = (get_file_version_info_w_t)GetProcAddress(ver, "GetFileVersionInfoW");
	LocalGlobalVars::g_ver_query_value = (ver_query_value_w_t)GetProcAddress(ver, "VerQueryValueW");
	if (!LocalGlobalVars::g_get_file_version_info_size || !LocalGlobalVars::g_get_file_version_info || !LocalGlobalVars::g_ver_query_value) {
		PLS_ERROR(GET_WIN_VER_MODULE, "Failed to load windows version functions");
		return false;
	}

	LocalGlobalVars::g_ver_initialize_success = true;
	return true;
}

LIBUTILSAPI_API bool pls_get_win_dll_ver(pls_win_ver_t &ver, const QString &dll)
{
	if (!LocalGlobalVars::g_ver_initialized && !initialize_version_functions())
		return false;
	if (!LocalGlobalVars::g_ver_initialize_success)
		return false;

	std::string dlla = dll.toStdString();
	std::wstring dllw = dll.toStdWString();

	DWORD size = LocalGlobalVars::g_get_file_version_info_size(dllw.c_str(), nullptr);
	if (!size) {
		PLS_ERROR(GET_WIN_VER_MODULE, "Failed to get %s version info size", dlla.c_str());
		return false;
	}

	LPVOID data = pls_malloc<VOID>(size);
	if (!data) {
		PLS_ERROR(GET_WIN_VER_MODULE, "Failed to get %s version info, malloc failed", dlla.c_str());
		return false;
	}

	if (!LocalGlobalVars::g_get_file_version_info(dllw.c_str(), 0, size, data)) {
		PLS_ERROR(GET_WIN_VER_MODULE, "Failed to get %s version info", dlla.c_str());
		pls_free(data);
		return false;
	}

	VS_FIXEDFILEINFO *ffi = nullptr;
	UINT len = 0;
	BOOL success = LocalGlobalVars::g_ver_query_value(data, L"\\", (LPVOID *)&ffi, &len);
	if (!success || !ffi || !len) {
		PLS_ERROR(GET_WIN_VER_MODULE, "Failed to get %s version info value", dlla.c_str());
		pls_free(data);
		return false;
	}

	ver.major = (int)HIWORD(ffi->dwFileVersionMS);
	ver.minor = (int)LOWORD(ffi->dwFileVersionMS);
	ver.build = (int)HIWORD(ffi->dwFileVersionLS);
	ver.revis = (int)LOWORD(ffi->dwFileVersionLS);
	pls_free(data);
	return true;
}

static void pls_get_win_ver_i()
{
	pls_win_ver_t ver{0, 0, 0, 0};
	pls_get_win_dll_ver(ver, QString::fromUtf8("kernel32.dll"));
	if (ver.major == 10) {
		HKEY key;
		if (RegOpenKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", &key) == ERROR_SUCCESS) {

			DWORD win10_revision = 0;
			DWORD size = sizeof(win10_revision);
			if (RegQueryValueExW(key, L"UBR", nullptr, nullptr, (LPBYTE)&win10_revision, &size) == ERROR_SUCCESS) {
				ver.revis = qMax((int)win10_revision, ver.revis);
			}

			RegCloseKey(key);
		}
	}

	LocalGlobalVars::g_win_ver = ver;
}

LIBUTILSAPI_API pls_win_ver_t pls_get_win_ver()
{
	if (!LocalGlobalVars::g_win_ver.has_value()) {
		std::lock_guard guard(pls_global_mutex());
		if (!LocalGlobalVars::g_win_ver.has_value()) {
			pls_get_win_ver_i();
		}
	}

	return LocalGlobalVars::g_win_ver.value();
}

LIBUTILSAPI_API bool pls_is_after_win10()
{
	return pls_get_win_ver().build >= 22000;
}

static const QString &conditonal_select(bool condition, const QString &true_text, const QString &false_text)
{
	return condition ? true_text : false_text;
}
LIBUTILSAPI_API QString pls_get_win_name()
{
	pls_win_ver_t ver = pls_get_win_ver();
	int version_info = (ver.major << 8) | ver.minor;
	if (version_info >= 0x0A00) {
		return conditonal_select(IsWindowsServer(), QStringLiteral("Windows Server 2016 Technical Preview"),
					 conditonal_select(ver.build >= 21664, QStringLiteral("Windows 11"), QStringLiteral("Windows 10")));
	} else if (version_info >= 0x0603) {
		return conditonal_select(IsWindowsServer(), QStringLiteral("Windows Server 2012 r2"), QStringLiteral("Windows 8.1"));
	} else if (version_info >= 0x0602) {
		return conditonal_select(IsWindowsServer(), QStringLiteral("Windows Server 2012"), QStringLiteral("Windows 8"));
	} else if (version_info >= 0x0601) {
		return conditonal_select(IsWindowsServer(), QStringLiteral("Windows Server 2008 r2"), QStringLiteral("Windows 7"));
	} else if (version_info >= 0x0600) {
		return conditonal_select(IsWindowsServer(), QStringLiteral("Windows Server 2008"), QStringLiteral("Windows Vista"));
	} else {
		return QStringLiteral("Windows Before Vista");
	}
}

LIBUTILSAPI_API QString pls_get_win_name_with_arch()
{
	return pls_get_win_name() + conditonal_select(sizeof(void *) == 8, QStringLiteral("(x64)"), QStringLiteral("(x86)"));
}
