#include "GPUUsage.h"
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#include <vector>
#include <map>
#include <string>
#include <regex>
#include <thread>
#include <assert.h>

#include <liblog.h>
#include "log/module_names.h"

#ifdef SHOW_CONSOULE
#include <conio.h>
#endif

#pragma comment(lib, "pdh.lib")

struct EngineItem {
	int pid = 0;
	std::wstring luid = L"";
	std::wstring engtype = L"";
	double usage = 0.0;
};

class GPUUtil {
public:
	GPUUtil() { Init(); }
	virtual ~GPUUtil() { Uninit(); }

	bool UpdateEngineList(std::vector<EngineItem> &output);

protected:
	bool Init();
	void Uninit();
	bool ParseEngineItem(const std::wstring &input, int &pid, std::wstring &luid, std::wstring &engtype);

private:
	bool initialized = false;
	HQUERY hQuery = nullptr;
	HCOUNTER hCounter = nullptr;
};

bool GPUUtil::Init()
{
	auto status = PdhOpenQuery(nullptr, 0, &hQuery);
	if (status != ERROR_SUCCESS) {
		PLS_WARN(MAIN_PERFORMANCE, "[%s] PdhOpenQuery failed with status=0x%p", __FUNCTION__, status);
		assert(false);
		return false;
	}

	status = PdhAddEnglishCounter(hQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &hCounter);
	if (status != ERROR_SUCCESS) {
		PLS_WARN(MAIN_PERFORMANCE, "[%s] PdhAddEnglishCounter failed with status=0x%p", __FUNCTION__, status);
		assert(false);
		return false;
	}

	status = PdhCollectQueryData(hQuery);
	if (status != ERROR_SUCCESS && status != PDH_CSTATUS_INVALID_DATA) {
		PLS_WARN(MAIN_PERFORMANCE, "[%s] PdhCollectQueryData failed with status=0x%p", __FUNCTION__, status);
		assert(false);
		return false;
	}

	PLS_INFO(MAIN_PERFORMANCE, "[%s] successed to init", __FUNCTION__);
	initialized = true;

	return true;
}

void GPUUtil::Uninit()
{
	initialized = false;
	hCounter = nullptr;
	if (hQuery) {
		PdhCloseQuery(hQuery);
		hQuery = nullptr;
	}
}

bool GPUUtil::ParseEngineItem(const std::wstring &input, int &pid, std::wstring &luid, std::wstring &engtype)
{
	// eg: "pid_1372_luid_0x00000000_0x00013865_phys_0_eng_1_engtype_Copy"
	static const std::wregex re(L"pid_(\\d+)_luid_(.*?)_phys_.*?_engtype_(.*)");

	std::wsmatch match;
	if (std::regex_search(input, match, re)) {
		pid = std::stoi(match[1].str());
		luid = match[2].str();
		engtype = match[3].str();
		return true;
	}
	assert(false);
	return false;
}

bool GPUUtil::UpdateEngineList(std::vector<EngineItem> &output)
{
	output.clear();

	if (!initialized)
		return false;

	auto status = PdhCollectQueryData(hQuery);
	if (status != ERROR_SUCCESS) {
		if (status != PDH_CSTATUS_INVALID_DATA) {
			assert(false);
		}
		return false;
	}

	DWORD bufSize = 0, itemCount = 0;
	status = PdhGetFormattedCounterArray(hCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
	if (status != PDH_MORE_DATA) {
		return false;
	}

	std::vector<BYTE> buffer(bufSize);
	PPDH_FMT_COUNTERVALUE_ITEM items = (PPDH_FMT_COUNTERVALUE_ITEM)buffer.data();
	status = PdhGetFormattedCounterArray(hCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
	if (status != ERROR_SUCCESS) {
		return false;
	}

	for (DWORD i = 0; i < itemCount; ++i) {
		if (items[i].FmtValue.doubleValue > 0.0) {
			EngineItem item;
			item.usage = items[i].FmtValue.doubleValue;

			if (ParseEngineItem(items[i].szName, item.pid, item.luid, item.engtype)) {
				output.push_back(item);
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------------------------------------------
class GPUImpl {
	friend class GPUUsage;

public:
	GPUImpl();
	~GPUImpl();

	void Start(DWORD topProcessId);
	void Stop();

protected:
	void UpdateThread(DWORD topProcessId);
	void CalcSystemGPU(std::vector<EngineItem> items);
	void CalcProcessGPU(DWORD topProcessId, std::vector<EngineItem> items);
	std::map<DWORD, bool> GetChildProcessIds(DWORD parentPid);

private:
	double systemGPU = 0.0;
	double processGPU = 0.0;

	HANDLE exitEvent = 0;
	std::thread *workerThread = nullptr;
};

GPUImpl::GPUImpl()
{
#ifdef SHOW_CONSOULE
	AllocConsole();
#endif

	exitEvent = CreateEventW(nullptr, true, false, nullptr);
}

GPUImpl::~GPUImpl()
{
	Stop();

	if (exitEvent) {
		CloseHandle(exitEvent);
		exitEvent = nullptr;
	}

#ifdef SHOW_CONSOULE
	FreeConsole();
#endif
}

void GPUImpl::Start(DWORD topProcessId)
{
	if (workerThread != nullptr || topProcessId == 0) {
		assert(false);
		return;
	}

	if (!exitEvent) {
		assert(false);
		return;
	}

	ResetEvent(exitEvent);
	workerThread = new std::thread(&GPUImpl::UpdateThread, this, topProcessId);
}

void GPUImpl::Stop()
{
	if (exitEvent) {
		SetEvent(exitEvent);
	}

	if (workerThread) {
		if (workerThread->joinable())
			workerThread->join();

		delete workerThread;
		workerThread = nullptr;
	}
}

void GPUImpl::UpdateThread(DWORD topProcessId)
{
	PLS_INFO(MAIN_PERFORMANCE, "[%s] enter", __FUNCTION__);

	GPUUtil util;
	while (WAIT_OBJECT_0 != WaitForSingleObject(exitEvent, UPDATE_GPU_INTERVAL)) {
		std::vector<EngineItem> items;
		if (util.UpdateEngineList(items)) {
			CalcSystemGPU(items);
			CalcProcessGPU(topProcessId, items);
		}

#ifdef SHOW_CONSOULE
		_cprintf("process-gpu:%lf system-gpu: %lf \n", processGPU, systemGPU);
#endif
	}

	PLS_INFO(MAIN_PERFORMANCE, "[%s] leave", __FUNCTION__);
}

void GPUImpl::CalcSystemGPU(std::vector<EngineItem> items)
{
	double result = 0.0;
	std::map<std::wstring, double> values;
	for (const auto &item : items) {
		auto key = item.luid + item.engtype;
		auto &temp = values[key];
		temp += item.usage;
		if (result < temp)
			result = temp;
	}

	systemGPU = result;
	if (systemGPU > 100.0)
		systemGPU = 100.0;
}

void GPUImpl::CalcProcessGPU(DWORD topProcessId, std::vector<EngineItem> items)
{
	std::map<DWORD, bool> pids = GetChildProcessIds(topProcessId);
	if (pids.empty()) {
		processGPU = 0.0;
		return;
	}

	double result = 0.0;
	std::map<std::wstring, double> values;
	for (const auto &item : items) {
		if (!pids[item.pid]) {
			continue;
		}

		auto key = item.engtype;
		auto &temp = values[key];
		temp += item.usage;
		if (result < temp)
			result = temp;
	}

	processGPU = result;
	if (processGPU > 100.0)
		processGPU = 100.0;
}

std::map<DWORD, bool> GPUImpl::GetChildProcessIds(DWORD parentPid)
{
	std::map<DWORD, bool> childPids;

	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return childPids;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(hSnap, &pe)) {
		do {
			if (pe.th32ParentProcessID == parentPid || pe.th32ProcessID == parentPid) {
				childPids[pe.th32ProcessID] = true;
			}
		} while (Process32Next(hSnap, &pe));
	}

	CloseHandle(hSnap);
	return childPids;
}

//-----------------------------------------------------------------------------------------------------------------
GPUUsage *GPUUsage::Instance()
{
	static GPUUsage instance;
	return &instance;
}

GPUUsage::GPUUsage()
{
	self = new GPUImpl();
}

GPUUsage::~GPUUsage()
{
	delete self;
}

void GPUUsage::Start(DWORD topProcessId)
{
	self->Start(topProcessId);
}

void GPUUsage::Stop()
{
	self->Stop();
}

void GPUUsage::GetUsage(double &systemGPU, double &processGPU)
{
	if (self) {
		systemGPU = self->systemGPU;
		processGPU = self->processGPU;
	} else {
		assert(false);
		systemGPU = processGPU = 0.0;
	}
}