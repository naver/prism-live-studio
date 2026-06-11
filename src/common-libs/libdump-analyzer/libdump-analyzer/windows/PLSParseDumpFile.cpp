#if _WIN32
#include <Windows.h>
#include <stdlib.h>
#include <DbgEng.h>
#include <Shlwapi.h>

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "DbgEng.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

#include "PLSParseDumpFile.h"
#include "libutils-api.h"
#include <algorithm>
#include <array>

constexpr static int MAX_BACKTRACE = 256;
using namespace std;

#if _WIN32
vector<ModuleInfo> GetModuleInfo(const char *pszDumpPath)
{
	vector<ModuleInfo> vctModuleInfo;

	auto hFileMiniDump = INVALID_HANDLE_VALUE;
	HANDLE hFileMapping = nullptr;
	LPVOID pMappingBase = nullptr;
	auto wPath = pls_utf8_to_unicode(pszDumpPath);
	if (wPath.empty())
		return vctModuleInfo;
	do {
		hFileMiniDump = CreateFileW(wPath.c_str(), FILE_GENERIC_READ, 0, nullptr, OPEN_EXISTING, NULL, nullptr);
		if (INVALID_HANDLE_VALUE == hFileMiniDump) {
			break;
		}

		hFileMapping = CreateFileMapping(hFileMiniDump, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (nullptr == hFileMapping) {
			break;
		}

		pMappingBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
		if (nullptr == pMappingBase) {
			break;
		}

		LPVOID pStreamStart = nullptr;
		ULONG uStreamSize = 0;
		if (!MiniDumpReadDumpStream(pMappingBase, ModuleListStream, nullptr, &pStreamStart, &uStreamSize) || nullptr == pStreamStart) {
			break;
		}

		auto pModuleStream = (MINIDUMP_MODULE_LIST *)pStreamStart;
		vctModuleInfo.reserve(pModuleStream->NumberOfModules);
		for (auto i = 0; i < pModuleStream->NumberOfModules; ++i) {
			ModuleInfo info = {0};
			info.BaseOfImage = pModuleStream->Modules[i].BaseOfImage;
			info.SizeOfImage = pModuleStream->Modules[i].SizeOfImage;
			wcscpy_s(info.ModuleName.data(), info.ModuleName.size(), ((MINIDUMP_STRING *)((LPBYTE)pMappingBase + pModuleStream->Modules[i].ModuleNameRva))->Buffer);
			vctModuleInfo.push_back(info);
		}
	} while (false);

	if (nullptr != pMappingBase) {
		UnmapViewOfFile(pMappingBase);
		pMappingBase = nullptr;
	}
	if (nullptr != hFileMapping) {
		CloseHandle(hFileMapping);
		hFileMapping = nullptr;
	}
	if (INVALID_HANDLE_VALUE != hFileMiniDump) {
		CloseHandle(hFileMiniDump);
		hFileMiniDump = nullptr;
	}

	return vctModuleInfo;
}

vector<ULONG64> GetStackTrace(const char *pszDumpPath)
{
	vector<ULONG64> vctStackTrace;

	IDebugClient4 *pDebugClient = nullptr;
	IDebugSymbols4 *pDebugSymbols = nullptr;
	IDebugControl4 *pDebugControl = nullptr;
	do {
		if (S_OK != DebugCreate(__uuidof(IDebugClient4), (void **)&pDebugClient)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugSymbols4), (void **)&pDebugSymbols)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugControl4), (void **)&pDebugControl)) {
			break;
		}

		std::array<wchar_t, MAX_PATH> szImagePath{};
		GetModuleFileNameW(nullptr, szImagePath.data(), MAX_PATH);
		PathRemoveFileSpecW(szImagePath.data());
		pDebugSymbols->AppendImagePathWide(szImagePath.data());

		auto wPath = pls_utf8_to_unicode(pszDumpPath);
		if (wPath.empty())
			break;
		if (S_OK != pDebugClient->OpenDumpFileWide(wPath.c_str(), 0)) {
			break;
		}
		if (S_OK != pDebugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE)) {
			break;
		}

		if (S_OK != pDebugControl->Execute(DEBUG_OUTCTL_THIS_CLIENT, ".ecxr", 0)) {
			break;
		}
		std::array<DEBUG_STACK_FRAME, MAX_BACKTRACE> frames{0};
		ULONG ulFramesFilled = 0;
		if (S_OK != pDebugControl->GetStackTrace(0, 0, 0, frames.data(), frames.size(), &ulFramesFilled)) {
			break;
		}

		for (auto i = 0; i < ulFramesFilled; ++i) {
			vctStackTrace.push_back(frames[i].InstructionOffset);
		}
	} while (false);

	if (nullptr != pDebugControl) {
		pDebugControl->Release();
		pDebugControl = nullptr;
	}
	if (nullptr != pDebugSymbols) {
		pDebugSymbols->Release();
		pDebugSymbols = nullptr;
	}
	if (nullptr != pDebugClient) {
		pDebugClient->EndSession(DEBUG_END_PASSIVE);
		pDebugClient->Release();
		pDebugClient = nullptr;
	}

	return vctStackTrace;
}

ULONG64 GetThreadId(const char *pszDumpPath)
{
	ULONG64 threadId = 0;

	auto hFileMiniDump = INVALID_HANDLE_VALUE;
	HANDLE hFileMapping = nullptr;
	LPVOID pMappingBase = nullptr;
	auto wPath = pls_utf8_to_unicode(pszDumpPath);
	if (wPath.empty())
		return 0;
	do {
		hFileMiniDump = CreateFileW(wPath.c_str(), FILE_GENERIC_READ, 0, nullptr, OPEN_EXISTING, NULL, nullptr);
		if (INVALID_HANDLE_VALUE == hFileMiniDump) {
			break;
		}

		hFileMapping = CreateFileMappingW(hFileMiniDump, nullptr, PAGE_READONLY, 0, 0, nullptr);
		if (nullptr == hFileMapping) {
			break;
		}

		pMappingBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
		if (nullptr == pMappingBase) {
			break;
		}

		LPVOID pStreamStart = nullptr;
		ULONG uStreamSize = 0;
		if (!MiniDumpReadDumpStream(pMappingBase, ExceptionStream, nullptr, &pStreamStart, &uStreamSize) || nullptr == pStreamStart) {
			break;
		}
		auto pExceptionStream = (MINIDUMP_EXCEPTION_STREAM *)pStreamStart;
		if (pExceptionStream != nullptr && uStreamSize >= sizeof(MINIDUMP_EXCEPTION_STREAM))
			threadId = pExceptionStream->ThreadId;
	} while (false);

	if (nullptr != pMappingBase) {
		UnmapViewOfFile(pMappingBase);
		pMappingBase = nullptr;
	}
	if (nullptr != hFileMapping) {
		CloseHandle(hFileMapping);
		hFileMapping = nullptr;
	}
	if (INVALID_HANDLE_VALUE != hFileMiniDump) {
		CloseHandle(hFileMiniDump);
		hFileMiniDump = nullptr;
	}

	return threadId;
}

bool FindModule(const std::vector<ModuleInfo> &modules, ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo)
{
	bool find = false;
	for (const auto &item : modules) {
		if (addr >= item.BaseOfImage && addr <= item.BaseOfImage + item.SizeOfImage) {
			moduleInfo = item;
			offset = addr - item.BaseOfImage;
			find = true;
			break;
		}
	}

	return find;
}

#endif