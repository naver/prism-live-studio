#include <Windows.h>
#include "ParseDumpFile.h"
#include "TargetList.h"

#include <stdlib.h>
#include <dbgeng.h>
#include <Shlwapi.h>
#include <algorithm>

#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "DbgEng.lib")
#pragma comment(lib, "shlwapi.lib")

#define MAX_BACKTRACE 256

using namespace std;

vector<ModuleInfo> GetModuleInfo(const char *pszDumpPath)
{
	vector<ModuleInfo> vctModuleInfo;

	HANDLE hFileMiniDump = INVALID_HANDLE_VALUE;
	HANDLE hFileMapping = nullptr;
	LPVOID pMappingBase = nullptr;
	for (;;) {
		hFileMiniDump = CreateFileA(pszDumpPath, FILE_GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
		if (INVALID_HANDLE_VALUE == hFileMiniDump) {
			break;
		}

		hFileMapping = CreateFileMapping(hFileMiniDump, NULL, PAGE_READONLY, 0, 0, NULL);
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

		MINIDUMP_MODULE_LIST *pModuleStream = (MINIDUMP_MODULE_LIST *)pStreamStart;
		vctModuleInfo.reserve(pModuleStream->NumberOfModules);
		for (auto i = 0; i < pModuleStream->NumberOfModules; ++i) {
			ModuleInfo info = {0};
			info.BaseOfImage = pModuleStream->Modules[i].BaseOfImage;
			info.SizeOfImage = pModuleStream->Modules[i].SizeOfImage;
			wcscpy_s(info.ModuleName, ((MINIDUMP_STRING *)((LPBYTE)pMappingBase + pModuleStream->Modules[i].ModuleNameRva))->Buffer);
			PathStripPath(info.ModuleName);
			info.bInternal = any_of(begin(TargetList), end(TargetList), [=](auto szTarget) { return wcsncmp(info.ModuleName, szTarget, wcslen(szTarget)) == 0; });
			vctModuleInfo.push_back(info);
		}

		break;
	}
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
		hFileMiniDump = INVALID_HANDLE_VALUE;
	}

	return vctModuleInfo;
}

vector<ULONG64> GetStackTrace(const char *pszDumpPath)
{
	vector<ULONG64> vctStackTrace;

	IDebugClient *pDebugClient = nullptr;
	IDebugSymbols *pDebugSymbols = nullptr;
	IDebugControl *pDebugControl = nullptr;
	for (;;) {
		if (S_OK != DebugCreate(__uuidof(IDebugClient), (void **)&pDebugClient)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugSymbols), (void **)&pDebugSymbols)) {
			break;
		}
		if (S_OK != pDebugClient->QueryInterface(__uuidof(IDebugControl), (void **)&pDebugControl)) {
			break;
		}

		char szImagePath[MAX_PATH];
		GetModuleFileNameA(NULL, szImagePath, MAX_PATH);
		PathRemoveFileSpecA(szImagePath);
		pDebugSymbols->AppendImagePath(szImagePath);

		if (S_OK != pDebugClient->OpenDumpFile(pszDumpPath)) {
			break;
		}
		if (S_OK != pDebugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE)) {
			break;
		}

		if (S_OK != pDebugControl->Execute(DEBUG_OUTCTL_THIS_CLIENT, ".ecxr", 0)) {
			break;
		}
		DEBUG_STACK_FRAME frames[MAX_BACKTRACE] = {0};
		ULONG ulFramesFilled = 0;
		if (S_OK != pDebugControl->GetStackTrace(0, 0, 0, frames, sizeof(frames) / sizeof(frames[0]), &ulFramesFilled)) {
			break;
		}

		for (auto i = 0; i < ulFramesFilled; ++i) {
			vctStackTrace.push_back(frames[i].InstructionOffset);
		}

		break;
	}
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
