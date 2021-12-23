#pragma once

#include <TlHelp32.h>
#include <DbgHelp.h>
#include <vector>

struct ModuleInfo {
	ULONG64 BaseOfImage;
	ULONG32 SizeOfImage;
	WCHAR ModuleName[MAX_MODULE_NAME32 + 1];
	BOOL bInternal;
};

std::vector<ModuleInfo> GetModuleInfo(const char *pszDumpPath);

std::vector<ULONG64> GetStackTrace(const char *pszDumpPath);
