#include "PLSPerfHelper.hpp"
#include <d3d11_2.h>
#include <comdef.h>
#include <IPHlpApi.h>
#include <cmath>
#include <windows.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "IPHlpApi.lib")

namespace PLSPerf {
typedef _com_ptr_t<_com_IIID<IDXGIFactory2, &IID_IDXGIFactory2>> CDXGIFactory2;
typedef _com_ptr_t<_com_IIID<IDXGIAdapter1, &IID_IDXGIAdapter1>> CDXGIAdapter1;
typedef _com_ptr_t<_com_IIID<IDXGIAdapter2, &IID_IDXGIAdapter2>> CDXGIAdapter2;

static int sLogicalCores = 0;
static int sPhysicalCores = 0;
static bool sCoreCountInitialized = false;
static std::string sEngineName[] = {"3D", "DEC", "ENC", "COPY", "COMPUTE"};

static inline bool HasUTF8Bom(const char *str)
{
	uint8_t *in = (uint8_t *)str;
	return (in && in[0] == 0xef && in[1] == 0xbb && in[2] == 0xbf);
}

static size_t UTF8ToWChar(const char *in, size_t inSize, wchar_t *out, size_t outSize)
{
	size_t size = inSize;
	if (inSize == 0)
		size = strlen(in);

	/* prevent bom from being used in the string */
	if (HasUTF8Bom(in)) {
		if (size >= 3) {
			in += 3;
			size -= 3;
		}
	}

	int ret = MultiByteToWideChar(CP_UTF8, 0, in, (int)size, out, (int)outSize);

	return (ret > 0) ? ret : 0;
}

static size_t WCharToUTF8(const wchar_t *in, size_t inSize, char *out, size_t outSize)
{
	size_t size = inSize;
	if (size == 0)
		size = wcslen(in);

	int ret = WideCharToMultiByte(CP_UTF8, 0, in, (int)inSize, out, (int)outSize, NULL, NULL);
	return (ret > 0) ? ret : 0;
}

std::wstring S2WS(const std::string &str)
{
	std::wstring outStr;
	if (!str.empty()) {
		const char *pIn = str.c_str();
		size_t inSize = str.size();

		size_t outLen = UTF8ToWChar(pIn, inSize, nullptr, 0);
		wchar_t *out = new (std::nothrow) wchar_t[outLen + 1];
		if (out) {
			memset(out, 0, sizeof(wchar_t) * (outLen + 1));
			outLen = UTF8ToWChar(pIn, inSize, out, outLen + 1);
			out[outLen] = 0;
			outStr = out;
			delete[] out;
		}
	}

	return outStr;
}

std::string WS2S(const std::wstring &wstr)
{
	std::string outStr;
	if (!wstr.empty()) {
		const wchar_t *pIn = wstr.c_str();
		size_t inSize = wstr.size();

		size_t outLen = WCharToUTF8(pIn, inSize, nullptr, 0);
		char *out = new (std::nothrow) char[outLen + 1];
		if (out) {
			memset(out, 0, sizeof(char) * (outLen + 1));
			outLen = WCharToUTF8(pIn, inSize, out, outLen + 1);
			out[outLen] = 0;
			outStr = out;
			delete[] out;
		}
	}

	return outStr;
}

int EnumAdapter(std::vector<Gpu> &gpus)
{
	CDXGIFactory2 DXGIFactory;
	CDXGIAdapter1 Adapter1;
	CDXGIAdapter2 Adapter2;

	int adapterCount = 0;
	HRESULT hr = CreateDXGIFactory2(0, __uuidof(CDXGIFactory2), (void **)&DXGIFactory);
	if (SUCCEEDED(hr)) {
		int index = 0;
		while (DXGIFactory->EnumAdapters1(index++, &Adapter1) == S_OK) {
			hr = Adapter1->QueryInterface(__uuidof(Adapter1), (void **)&Adapter2);
			if (FAILED(hr))
				continue;
			DXGI_ADAPTER_DESC2 desc = {0};
			hr = Adapter2->GetDesc2(&desc);
			if (FAILED(hr))
				continue;

			// ignore microsoft's 'basic' renderer
			if (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c)
				continue;

			Gpu info = {0};
			info.name = WS2S(desc.Description);
			info.luid.highPart = desc.AdapterLuid.HighPart;
			info.luid.lowPart = desc.AdapterLuid.LowPart;
			info.dedicatedMemory.totalMB = static_cast<int>(desc.DedicatedVideoMemory / BYTES_ONE_MBYTES);
			info.sharedMemory.totalMB = static_cast<int>(desc.SharedSystemMemory / BYTES_ONE_MBYTES);

			info.engines.resize(static_cast<size_t>(std::log2((double)ENGINE_TYPE_COUNT)));

			gpus.push_back(info);

			adapterCount++;
		}
	}
	return adapterCount;
}

int EnumPhysicalDisk(std::vector<PhysicalDisk> &disks)
{
	HANDLE hDevice = INVALID_HANDLE_VALUE;
	BOOL bResult = FALSE;
	DWORD junk = 0;

	std::wstring prefix = L"\\\\.\\PhysicalDrive";

	int count = 0;
	int index = 0;
	do {
		std::wstring indexStr = std::to_wstring(index);
		std::wstring diskName = prefix + indexStr;
		hDevice = CreateFileW(diskName.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hDevice == INVALID_HANDLE_VALUE) {
			break;
		}
		DISK_GEOMETRY pdg = {0};
		bResult = DeviceIoControl(hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &pdg, sizeof(pdg), &junk, (LPOVERLAPPED)NULL);

		CloseHandle(hDevice);
		if (bResult) {
			ULONGLONG DiskSize = pdg.Cylinders.QuadPart * (ULONG)pdg.TracksPerCylinder * (ULONG)pdg.SectorsPerTrack * (ULONG)pdg.BytesPerSector;
			PhysicalDisk disk;
			disk.index = index;
			disk.totalGB = static_cast<int>(DiskSize / BYTES_ONE_GBYTES);
			disks.push_back(disk);
			count++;
		}

		index++;
	} while (true);

	return count;
}

LUID GetLuidFromFormattedCounterNameString(const std::wstring &str)
{
	LUID luid = {0, 0};

	// find and extract LUID parts
	size_t pos = str.find(L"_0x");
	if (pos != std::string::npos) {
		std::wstring strHighPart = str.substr(pos + 1, 10);

		pos = str.find(L"_0x", pos + 1);
		if (pos != std::string::npos) {
			std::wstring strLowPart = str.substr(pos + 1, 10);

			// convert from string
			luid.highPart = stol(strHighPart, 0, 16);
			luid.lowPart = stoul(strLowPart, 0, 16);
		}
	}

	return luid;
}

void CheckPhysicalMemory(MemoryValue &memory)
{
	MEMORYSTATUSEX memStatusEx;
	memStatusEx.dwLength = sizeof(memStatusEx);
	if (GlobalMemoryStatusEx(&memStatusEx)) {
		memory.totalMB = static_cast<int>(memStatusEx.ullTotalPhys / BYTES_ONE_MBYTES);
		memory.freeMB = static_cast<int>(memStatusEx.ullAvailPhys / BYTES_ONE_MBYTES);
	}
}

static DWORD NBLogicalCores(ULONG_PTR mask)
{
	DWORD leftShift = sizeof(ULONG_PTR) * 8 - 1;
	DWORD bitSetCount = 0;
	ULONG_PTR bitTest = (ULONG_PTR)1 << leftShift;

	for (DWORD i = 0; i <= leftShift; ++i) {
		bitSetCount += ((mask & bitTest) ? 1 : 0);
		bitTest /= 2;
	}

	return bitSetCount;
}

static void GetCoresInfo()
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL, temp = NULL;
	DWORD len = 0;

	GetLogicalProcessorInformation(info, &len);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return;

	info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(new char[len]);

	if (GetLogicalProcessorInformation(info, &len)) {
		DWORD num = len / sizeof(*info);
		temp = info;

		for (DWORD i = 0; i < num; i++) {
			if (temp->Relationship == RelationProcessorCore) {
				ULONG_PTR mask = temp->ProcessorMask;

				sPhysicalCores++;
				sLogicalCores += NBLogicalCores(mask);
			}

			temp++;
		}
	}

	delete[] info;
	sCoreCountInitialized = true;
}

int GetLogicalCores()
{
	if (!sCoreCountInitialized)
		GetCoresInfo();

	return sLogicalCores;
}

int GetPhysicalCores()
{
	if (!sCoreCountInitialized)
		GetCoresInfo();

	return sPhysicalCores;
}

std::wstring CutProcessName(std::wstring name)
{
	size_t suffixPos = name.find_last_of(L".");
	std::wstring subStr = name.substr(0, suffixPos);
	return subStr;
}

std::string GetBestNetworkInterface()
{
	unsigned long size = 0;
	std::string strInterface;
	PIP_ADAPTER_INFO adapter = nullptr;
	if (GetAdaptersInfo(adapter, &size) == ERROR_BUFFER_OVERFLOW) {
		adapter = (PIP_ADAPTER_INFO) new (std::nothrow) char[size];
		if (!adapter)
			return strInterface;
	} else {
		return strInterface;
	}

	if (GetAdaptersInfo(adapter, &size) != ERROR_SUCCESS) {
		delete[] adapter;
		return strInterface;
	}

	IPAddr ipAddr = {0};
	DWORD dwIndex = 0;
	DWORD nRet = GetBestInterface(ipAddr, &dwIndex);
	if (nRet != NO_ERROR) {
		delete[] adapter;
		return strInterface;
	}

	for (auto cur = adapter; cur != nullptr; cur = cur->Next) {

		if (cur->Index == dwIndex) {
			strInterface = cur->Description;
			break;
		}
	}

	if (adapter)
		delete[] adapter;

	return strInterface;
}

std::string GetEngineName(EngineType type)
{
	return sEngineName[(int)std::log2((double)type)];
}
}
