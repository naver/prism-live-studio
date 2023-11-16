#pragma once

#include <util/platform.h>
#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

//PRISM/WangShaohui/20220314/none/modify log
#include <Psapi.h>
#include <string>

#include <obs.h>

#define do_log(level, format, ...) \
	do_log_source(level, "[win-capture-audio] (%s) " format, __func__, ##__VA_ARGS__)

inline static void do_log_source(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	//PRISM/WangShaohui/20220310/none/integrate plugin
	plogva(level, format, args);

	va_end(args);
}

#define warn(format, ...) do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...) do_log(LOG_INFO, format, ##__VA_ARGS__)

//PRISM/WangShaohui/20220314/none/modify log
static std::string get_exe_by_pid(DWORD id)
{
	std::string file_name = "";

	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, id);
	if (!process)
		return "(return-fail)";

	char name[MAX_PATH] = {0};
	if (!GetProcessImageFileNameA(process, name, MAX_PATH)) {
		CloseHandle(process);
		return "(return-fail)";
	}

	char temp[MAX_PATH] = {0};
	os_extract_file_name(name, temp, MAX_PATH);

	CloseHandle(process);
	return temp;
}

//PRISM/WangShaohui/20220314/none/modify log
static std::string get_exe_by_wnd(HWND window)
{
	DWORD id = 0;
	GetWindowThreadProcessId(window, &id);

	return get_exe_by_pid(id);
}
