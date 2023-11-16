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
	do {
		hFileMiniDump = CreateFileA(pszDumpPath, FILE_GENERIC_READ, 0, nullptr, OPEN_EXISTING, NULL, nullptr);
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
			PathStripPath(info.ModuleName.data());
			auto name = std::wstring(info.ModuleName.data());
			auto moduleName = name.substr(0, name.rfind(L"."));
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

	IDebugClient *pDebugClient = nullptr;
	IDebugSymbols *pDebugSymbols = nullptr;
	IDebugControl *pDebugControl = nullptr;
	do {
		if (S_OK != DebugCreate(__uuidof(IDebugClient), (void **)&pDebugClient)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugSymbols), (void **)&pDebugSymbols)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugControl), (void **)&pDebugControl)) {
			break;
		}

		std::array<char, MAX_PATH> szImagePath{};
		GetModuleFileNameA(nullptr, szImagePath.data(), MAX_PATH);
		PathRemoveFileSpecA(szImagePath.data());
		pDebugSymbols->AppendImagePath(szImagePath.data());

		if (S_OK != pDebugClient->OpenDumpFile(pszDumpPath)) {
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
	do {
		hFileMiniDump = CreateFileA(pszDumpPath, FILE_GENERIC_READ, 0, nullptr, OPEN_EXISTING, NULL, nullptr);
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

bool FindModule(const std::vector<ModuleInfo> &modules,  ULONG64 addr, ULONG64 &offset, ModuleInfo &moduleInfo)
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