#include <Windows.h>
#include <assert.h>
#include <tchar.h>
#include <string>
#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

enum class pcl_error_define {
	pcl_ok = 0,
	pcl_params_error,
	pcl_unknown_method,
	pcl_open_process_fail,
	pcl_create_file_fail,
	pcl_write_dump_fail,
};

enum pcl_error_define RunCaptureDump(LPWSTR *szArgList, int argCount);

// Return 0 if sucess
int main()
{
	enum pcl_error_define ret = pcl_error_define::pcl_ok;
	int argCount = 0;
	LPWSTR *szArgList = nullptr;

	do {
		szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);
		if (!szArgList) {
			ret = pcl_error_define::pcl_params_error;
			break;
		}

		// At least need two params: exe and method
		if (argCount < 2 || !szArgList[1]) {
			ret = pcl_error_define::pcl_params_error;
			break;
		}

		const LPWSTR &method = szArgList[1];
		if (0 == wcscmp(method, L"CaptureDump")) {
			ret = RunCaptureDump(szArgList, argCount);
			break;
		} else {
			ret = pcl_error_define::pcl_unknown_method;
			break;
		}

	} while (false);

	if (szArgList)
		LocalFree(szArgList);

	assert(ret == pcl_error_define::pcl_ok);
	return (int)ret;
}

enum pcl_error_define RunCaptureDump(LPWSTR *szArgList, int argCount)
{
	enum pcl_error_define ret = pcl_error_define::pcl_ok;
	HANDLE prismHandle = nullptr;

	do {
		if (argCount != 4) {
			ret = pcl_error_define::pcl_params_error;
			break;
		}

		if (!szArgList[2] || !szArgList[3]) {
			ret = pcl_error_define::pcl_params_error;
			break;
		}

		const LPWSTR &dumpPath = szArgList[2];
		int prismID = std::stoi(szArgList[3]);

		prismHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, prismID);
		if (!prismHandle) {
			ret = pcl_error_define::pcl_open_process_fail;
			break;
		}

		HANDLE hDumpFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hDumpFile == INVALID_HANDLE_VALUE) {
			ret = pcl_error_define::pcl_create_file_fail;
			break;
		}

		BOOL bOK = MiniDumpWriteDump(prismHandle, prismID, hDumpFile, MiniDumpWithFullMemoryInfo, nullptr, nullptr, nullptr);
		CloseHandle(hDumpFile);
		if (!bOK) {
			ret = pcl_error_define::pcl_write_dump_fail;
			break;
		}

	} while (false);

	if (prismHandle)
		CloseHandle(prismHandle);

	return ret;
}
