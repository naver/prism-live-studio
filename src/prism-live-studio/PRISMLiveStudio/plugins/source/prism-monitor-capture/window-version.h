#pragma once
#include <Windows.h>

using get_file_version_info_size_wt = DWORD(WINAPI *)(LPCWSTR md, LPDWORD unused);
using get_file_version_info_wt = BOOL(WINAPI *)(LPCWSTR md, DWORD unused, DWORD len, LPVOID data);
using ver_query_value_wt = BOOL(WINAPI *)(LPVOID data, LPCWSTR subblock, LPVOID *buf, PUINT sizeout);

class PLSWindowVersion {
public:
	PLSWindowVersion();
	PLSWindowVersion(const PLSWindowVersion &obj);
	PLSWindowVersion &operator=(const PLSWindowVersion &obj);
	virtual ~PLSWindowVersion();

	PLSWindowVersion(PLSWindowVersion &&) = delete;
	PLSWindowVersion &operator=(PLSWindowVersion &&) = delete;

	static bool is_support_monitor_duplicate();
	static VS_FIXEDFILEINFO get_dll_versiton(const wchar_t *dll_name);
	static unsigned get_win_version();

private:
	bool is_support_monitor_duplicate_inner();
	static bool init_function(get_file_version_info_size_wt &pFunc1, get_file_version_info_wt &pFunc2, ver_query_value_wt &pFunc3);

	HMODULE free_library = nullptr;
	bool is_inited = false;
	bool is_support = false;
};
