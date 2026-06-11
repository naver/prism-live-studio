#pragma once
#if _WIN32
#include <TlHelp32.h>
#include <DbgHelp.h>
#include <vector>
#include <array>
#endif

#include <QJsonArray>

#if _WIN32
struct ModuleInfo {
	BOOL bInternal;
	ULONG64 BaseOfImage;
	ULONG32 SizeOfImage;
	std::array<WCHAR, MAX_MODULE_NAME32> ModuleName;
};

std::vector<ModuleInfo> GetModuleInfo(const char *pszDumpPath);

std::vector<ULONG64> GetStackTrace(const char *pszDumpPath);

ULONG64 GetThreadId(const char *pszDumpPath);

bool FindModule(const std::vector<ModuleInfo> &modules, ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo);

#endif