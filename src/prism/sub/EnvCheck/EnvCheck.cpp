// EnvCheck.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <obs.hpp>
#include "util/util.hpp"

#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <vector>

bool is_dev_test_env()
{
	HKEY hOpen;
	LPCWSTR key = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	LPCWSTR dev_name = L"DevServer";
	LPCWSTR testmode_name = L"TestMode";
	TCHAR buf[64];
	DWORD size = sizeof(buf);
	bool is_dev = false;
	bool is_test_mode = false;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &hOpen)) {
		if (ERROR_SUCCESS == RegQueryValueEx(hOpen, dev_name, NULL, NULL, (LPBYTE)buf, &size)) {
			if (_tcscmp(buf, _T("true")) == 0) {
				is_dev = true;
			}
		}

		memset(buf, 0, sizeof(buf));
		if (ERROR_SUCCESS == RegQueryValueEx(hOpen, testmode_name, NULL, NULL, (LPBYTE)buf, &size)) {
			if (_tcscmp(buf, _T("true")) == 0) {
				is_test_mode = true;
			}
		}

		RegCloseKey(hOpen);
	}

	return is_dev && is_test_mode;
}

bool get_crash_test_token(const LPCWSTR check_name)
{
	HKEY hOpen;
	LPCWSTR key = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	TCHAR buf[64];
	DWORD size = sizeof(buf);
	bool res = false;
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_QUERY_VALUE, &hOpen)) {
		if (ERROR_SUCCESS == RegQueryValueEx(hOpen, check_name, NULL, NULL, (LPBYTE)buf, &size)) {
			if (_tcscmp(buf, _T("true")) == 0) {
				res = true;
			}
		}
		RegCloseKey(hOpen);
	}
	return res;
}

LONG PrismUnhandledExceptionFilter(struct _EXCEPTION_POINTERS *pExceptionPointers)
{
	TerminateProcess(GetCurrentProcess(), 2);
	return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, wchar_t *argv[], wchar_t *envp[])
{
	SetUnhandledExceptionFilter(PrismUnhandledExceptionFilter);

	struct obs_video_info ovi = {0};
	ovi.graphics_module = "libobs-d3d11.dll"; //needed only
	ovi.base_width = 1920;
	ovi.base_height = 1080;
	ovi.output_width = 1280;
	ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.colorspace = VIDEO_CS_601;
	ovi.range = VIDEO_RANGE_FULL;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.scale_type = OBS_SCALE_BILINEAR;

	int ret = obs_check_init_video(&ovi);
	if (ret == 0) {
		std::cout << "init success" << std::endl;
	} else {
		std::cout << "init failed" << std::endl;
	}

	if (is_dev_test_env()) {
		bool crash_test = get_crash_test_token(L"EnvCheckCrash");
		if (crash_test) {
			std::vector<int> vec_test;
			vec_test.at(2);
			return 2;
		}

		bool timeout_test = get_crash_test_token(L"EnvCheckTimeout");
		if (timeout_test) {
			Sleep(7000);
		}
	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
