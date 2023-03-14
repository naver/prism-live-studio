#include "PLSPerfCounter.hpp"
#include "PLSPerfHelper.hpp"
#if USED_INSIDE_PRISM
#include "obs.h"
#endif
#include <tlhelp32.h>
#include <pdhmsg.h>
#include <exception>
#include <string>
#include "liblog.h"
#include "log/module_names.h"

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "IPHlpApi.lib")

namespace PLSPerf {
/* clang-format off */
static const int   QUERY_INTERVAL_MS				= 1000;
static LPCWSTR sGpuEngineUtilizationPercentageCounterPath	= L"\\GPU Engine(pid_*_*)\\Utilization Percentage";
static LPCWSTR sGpuAdapterMemoryDedicatedUsageCounterPath	= L"\\GPU Adapter Memory(luid_*_*)\\Dedicated Usage";
static LPCWSTR sGpuAdapterMemorySharedUsageCounterPath		= L"\\GPU Adapter Memory(luid_*_*)\\Shared Usage";
static LPCWSTR sGpuProcessMemoryDedicatedUsageCounterPath	= L"\\GPU Process Memory(pid_*_*)\\Dedicated Usage";
static LPCWSTR sGpuProcessMemorySharedUsageCounterPath		= L"\\GPU Process Memory(pid_*_*)\\Shared Usage";
static LPCWSTR sProcessProcessorTimeCounterPath			= L"\\Process(*)\\% Processor Time";
static LPCWSTR sProcessWorkingSetPrivateCounterPath		= L"\\Process(*)\\Working Set - Private";
static LPCWSTR sProcessPrivateBytesCounterPath			= L"\\Process(*)\\Private Bytes";
static LPCWSTR sProcessHandleCountCounterPath			= L"\\Process(*)\\Handle Count";
static LPCWSTR sProcessThreadCountCounterPath			= L"\\Process(*)\\Thread Count";
static LPCWSTR sProcessorTimeCounterPath			= L"\\Processor(_Total)\\% Processor Time";
static LPCWSTR sProcessorPerformanceCounterPath			= L"\\Processor Information(_Total)\\% Processor Performance";
static LPCWSTR sProcessorUtilityCounterPath			= L"\\Processor Information(_Total)\\% Processor Utility";
static LPCWSTR sProcessorFrequencyCounterPath			= L"\\Processor Information(_Total)\\Processor Frequency";
static LPCWSTR sCommitLimitCounterPath				= L"\\Memory\\Commit Limit";
static LPCWSTR sCommittedBytesCounterPath			= L"\\Memory\\Committed Bytes";
static LPCWSTR sNetworkInterfaceBytesReceivedCounterPath	= L"\\Network Interface(*)\\Bytes Received/sec";
static LPCWSTR sNetworkInterfaceBytesSentCounterPath		= L"\\Network Interface(*)\\Bytes Sent/sec";
static LPCWSTR sNetworkInterfaceCurrentBandwidthCounterPath	= L"\\Network Interface(*)\\Current Bandwidth";
static LPCWSTR sPhysicalDiskWriteBytesCounterPath		= L"\\PhysicalDisk(* *)\\Disk Write Bytes/sec";
static LPCWSTR sPhysicalDiskReadBytesCounterPath		= L"\\PhysicalDisk(* *)\\Disk Read Bytes/sec";


static LPCWSTR sGpuEngine3DString				= L"_engtype_3D";
static LPCWSTR sGpuEngineComputeString				= L"_engtype_Compute";
static LPCWSTR sGpuEngineVideoDecodeString			= L"_engtype_VideoDecode";
static LPCWSTR sGpuEngineVideoEncodeString			= L"_engtype_VideoEncode";
static LPCWSTR sGpuEngineCopyString				= L"_engtype_Copy";
/* clang-format on */

inline bool SucceededPDH(PDH_STATUS pdhStatus, const char *function, const std::wstring &counter, const char *msg = nullptr)
{
	if (pdhStatus != ERROR_SUCCESS) {
#if USED_INSIDE_PRISM
		if (msg)
			PLS_INFO(MAIN_PERFORMANCE, "[PLSPerfCounter] '%s' failed (0x%x) for '%s' (%s).", function, pdhStatus, WS2S(counter).c_str(), msg);
		else
			PLS_INFO(MAIN_PERFORMANCE, "[PLSPerfCounter] '%s' failed (0x%x) for '%s'.", function, pdhStatus, WS2S(counter).c_str());
#endif
		return false;
	}
	return true;
}

bool PerfCounter::Start()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	pdhStatus = PdhOpenQuery(NULL, 0, &mQuery);

	if (pdhStatus != ERROR_SUCCESS) {
#if USED_INSIDE_PRISM
		PLS_INFO(MAIN_PERFORMANCE, "[PLSPerfCounter] Fail to open pdh query, error 0x%x.", pdhStatus);
#endif
		return false;
	}

	AddAdapterQuery();
	UpdateProcessQuery();

#ifdef ENABLE_FULL_FEATURE
	AddCpuQuery();
	AddMemoryQuery();
	AddNetworkInterfaceQuery();
	AddPhysicalDiskQuery();

	mCpu.physicalCoreNB = GetPhysicalCores();
	mCpu.logicalCoreNB = GetLogicalCores();
#endif

	mRunning = true;
	mTimer.start(QUERY_INTERVAL_MS);

	return true;
}

void PerfCounter::Stop()
{
	if (mRunning)
		mTimer.stop();
	Close();
}

void PerfCounter::Close()
{
	if (mQuery) {
		PdhCloseQuery(mQuery);
		mQuery = nullptr;
	}
}

void PerfCounter::Update()
{
	if (mNBCounters) {
		PDH_STATUS pdhStatus = PdhCollectQueryData(mQuery);
		if (pdhStatus == ERROR_SUCCESS) {
			QueryAdapter();
			UpdateProcessQuery(true);
#ifdef ENABLE_FULL_FEATURE
			QueryCpu();
			QueryMemory();
			QueryNetwork();
			QueryPhysicalDisk();
#endif
			DumpQueryResult();
			mReady2Output = true;
		}
	}
}

void PerfCounter::AddAdapterQuery()
{
	EnumAdapter(mGpuQuery.gpus);
	if (mGpuQuery.gpus.size() <= 0)
		return;

	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	pdhStatus = PdhAddEnglishCounter(mQuery, sGpuEngineUtilizationPercentageCounterPath, 0, &mGpuQuery.usageCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sGpuEngineUtilizationPercentageCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sGpuAdapterMemoryDedicatedUsageCounterPath, 0, &mGpuQuery.dedicatedMemoryCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sGpuAdapterMemoryDedicatedUsageCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sGpuAdapterMemorySharedUsageCounterPath, 0, &mGpuQuery.sharedMemoryCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sGpuAdapterMemorySharedUsageCounterPath))
		mNBCounters++;
}

void PerfCounter::QueryAdapter()
{
	DWORD bufferSize = 0;
	DWORD itemCount = 0;
	PDH_STATUS pdhStatus = ERROR_SUCCESS;

	if (mGpuQuery.usageCounter) {
		pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.usageCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.usageCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS)
				UpdateAdapterGpuUsageInternal(itemCount, items);
		}
	}

	bufferSize = 0;
	itemCount = 0;
	if (mGpuQuery.dedicatedMemoryCounter) {
		pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.dedicatedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.dedicatedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS)
				UpdateAdapterGpuMemoryInternal(itemCount, items);
		}
	}

	bufferSize = 0;
	itemCount = 0;
	if (mGpuQuery.sharedMemoryCounter) {
		pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.sharedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(mGpuQuery.sharedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS)
				UpdateAdapterGpuMemoryInternal(itemCount, items, false);
		}
	}
}

void PerfCounter::UpdateAdapterGpuUsageInternal(DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items)
{
	for (int index = 0; index < mGpuQuery.gpus.size(); index++) {
		double renderLoad = 0.0;
		double videoDecoderLoad = 0.0;
		double videoEncoderLoad = 0.0;
		double copyLoad = 0.0;
		double computeLoad = 0.0;
		for (DWORD i = 0; i < itemCount; i++) {
			std::wstring itemName(items[i].szName);
			LUID itemLuid = GetLuidFromFormattedCounterNameString(itemName);
			if (SameLuid(itemLuid, mGpuQuery.gpus[index].luid)) {
				if (itemName.find(sGpuEngine3DString) != std::string::npos)
					renderLoad += items[i].FmtValue.doubleValue;
				else if (itemName.find(sGpuEngineVideoDecodeString) != std::string::npos)
					videoDecoderLoad += items[i].FmtValue.doubleValue;
				else if (itemName.find(sGpuEngineVideoEncodeString) != std::string::npos)
					videoEncoderLoad += items[i].FmtValue.doubleValue;
				else if (itemName.find(sGpuEngineCopyString) != std::string::npos)
					copyLoad += items[i].FmtValue.doubleValue;
				else if (itemName.find(sGpuEngineComputeString) != std::string::npos)
					computeLoad += items[i].FmtValue.doubleValue;
			}
		}

		mGpuQuery.gpus[index].engines[0].type = ENGINE_TYPE_3D;
		mGpuQuery.gpus[index].engines[0].usage = renderLoad;
		mGpuQuery.gpus[index].engines[1].type = ENGINE_TYPE_VIDEO_DECODE;
		mGpuQuery.gpus[index].engines[1].usage = videoDecoderLoad;
		mGpuQuery.gpus[index].engines[2].type = ENGINE_TYPE_VIDEO_ENCODE;
		mGpuQuery.gpus[index].engines[2].usage = videoEncoderLoad;
		mGpuQuery.gpus[index].engines[3].type = ENGINE_TYPE_COPY;
		mGpuQuery.gpus[index].engines[3].usage = copyLoad;
		mGpuQuery.gpus[index].engines[4].type = ENGINE_TYPE_COMPUTE;
		mGpuQuery.gpus[index].engines[4].usage = computeLoad;
	}
}

void PerfCounter::UpdateAdapterGpuMemoryInternal(DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items, bool dedicated)
{
	for (int index = 0; index < mGpuQuery.gpus.size(); index++) {
		LONGLONG load = 0;
		for (DWORD i = 0; i < itemCount; i++) {
			std::wstring itemName(items[i].szName);
			LUID itemLuid = GetLuidFromFormattedCounterNameString(itemName);
			if (SameLuid(itemLuid, mGpuQuery.gpus[index].luid)) {
				load = items[i].FmtValue.largeValue;
			}
		}

		if (dedicated)
			mGpuQuery.gpus[index].dedicatedMemory.usedMB = static_cast<int>(load / BYTES_ONE_MBYTES);
		else
			mGpuQuery.gpus[index].sharedMemory.usedMB = static_cast<int>(load / BYTES_ONE_MBYTES);
	}
}

void PerfCounter::NewProcess(DWORD pid, std::wstring name)
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	ProcessQueryWrapper queryWrapper;
	// set active count 2 for new process to check if process alive
	queryWrapper.active = 2;
	queryWrapper.pid = pid;
	queryWrapper.name = WS2S(name);
	queryWrapper.gpuStats.engines.resize((int)std::log2((double)ENGINE_TYPE_COUNT));

	std::wstring pidStr = std::to_wstring(pid);

	std::wstring usageCounterPath = sGpuEngineUtilizationPercentageCounterPath;
	std::wstring dedicatedMemoryCounterPath = sGpuProcessMemoryDedicatedUsageCounterPath;
	std::wstring sharedMemoryCounterPath = sGpuProcessMemorySharedUsageCounterPath;
	usageCounterPath.replace(usageCounterPath.find('*'), 1, pidStr);
	dedicatedMemoryCounterPath.replace(dedicatedMemoryCounterPath.find('*'), 1, pidStr);
	sharedMemoryCounterPath.replace(sharedMemoryCounterPath.find('*'), 1, pidStr);

	pdhStatus = PdhAddEnglishCounter(mQuery, usageCounterPath.c_str(), 0, &queryWrapper.gpuUsageCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", usageCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, dedicatedMemoryCounterPath.c_str(), 0, &queryWrapper.dedicatedMemoryCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", dedicatedMemoryCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sharedMemoryCounterPath.c_str(), 0, &queryWrapper.sharedMemoryCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sharedMemoryCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;

	std::wstring nameStr = CutProcessName(name);
	std::wstring cpuUsageCounterPath = sProcessProcessorTimeCounterPath;
	std::wstring workingSetPrivateCounterPath = sProcessWorkingSetPrivateCounterPath;
	std::wstring privateBytesCounterPath = sProcessPrivateBytesCounterPath;
	std::wstring handleCounterPath = sProcessHandleCountCounterPath;
	std::wstring threadCounterPath = sProcessThreadCountCounterPath;
	cpuUsageCounterPath.replace(cpuUsageCounterPath.find('*'), 1, nameStr);
	workingSetPrivateCounterPath.replace(workingSetPrivateCounterPath.find('*'), 1, nameStr);
	privateBytesCounterPath.replace(privateBytesCounterPath.find('*'), 1, nameStr);
	handleCounterPath.replace(handleCounterPath.find('*'), 1, nameStr);
	threadCounterPath.replace(threadCounterPath.find('*'), 1, nameStr);

	pdhStatus = PdhAddEnglishCounter(mQuery, cpuUsageCounterPath.c_str(), 0, &queryWrapper.cpuUsageCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", cpuUsageCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, workingSetPrivateCounterPath.c_str(), 0, &queryWrapper.workingSetPrivateCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", workingSetPrivateCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, privateBytesCounterPath.c_str(), 0, &queryWrapper.privateBytesCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", privateBytesCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, handleCounterPath.c_str(), 0, &queryWrapper.handleCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", handleCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, threadCounterPath.c_str(), 0, &queryWrapper.threadCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", threadCounterPath, queryWrapper.name.c_str()))
		mNBCounters++;

	// put the parent process in the first place
	if (pid == mCurrentPID)
		mProcessQuery.insert(mProcessQuery.begin(), queryWrapper);
	else
		mProcessQuery.push_back(queryWrapper);
}

void PerfCounter::UpdateProcessQuery(bool ready2Query)
{
	if (!mCurrentPID) {
#if USED_INSIDE_PRISM
		mCurrentPID = GetCurrentProcessId();
#else
		return;
#endif
	}

	HANDLE procSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (procSnap == INVALID_HANDLE_VALUE) {
		return;
	}

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);
	if (Process32First(procSnap, &pe)) {
		do {
			if (pe.th32ProcessID == mCurrentPID || pe.th32ParentProcessID == mCurrentPID) {
				std::vector<ProcessQueryWrapper>::iterator iter = std::find_if(mProcessQuery.begin(), mProcessQuery.end(), [=](std::vector<ProcessQueryWrapper>::value_type &obj) {
					if (obj.pid == pe.th32ProcessID) {
						// add active count to check if process alive
						obj.active++;
						return true;
					} else
						return false;
				});

				// process is already in vector
				if (iter != mProcessQuery.end())
					continue;

				NewProcess(pe.th32ProcessID, pe.szExeFile);
			}
		} while (Process32Next(procSnap, &pe));
	}

	CheckProcessActive();

	if (ready2Query)
		QueryProcess();
}

void PerfCounter::QueryProcess()
{
	int dedicatedMemoryMB = 0;
	int sharedMemoryMB = 0;

	double renderLoad = 0.0;
	double videoDecoderLoad = 0.0;
	double videoEncoderLoad = 0.0;
	double copyLoad = 0.0;
	double computeLoad = 0.0;

	double cpuLoad = 0.0;
	int handlesNB = 0;
	int threadsNB = 0;

	int workingSetPrivateMB = 0;
	int committedMB = 0;

	for (int i = 0; i < mProcessQuery.size(); i++) {
		QueryProcessGpu(mProcessQuery[i]);
		QueryProcessCpu(mProcessQuery[i]);
		QueryProcessMemory(mProcessQuery[i]);

		renderLoad += mProcessQuery[i].gpuStats.engines[0].usage;
		videoDecoderLoad += mProcessQuery[i].gpuStats.engines[1].usage;
		videoEncoderLoad += mProcessQuery[i].gpuStats.engines[2].usage;
		copyLoad += mProcessQuery[i].gpuStats.engines[3].usage;
		computeLoad += mProcessQuery[i].gpuStats.engines[4].usage;
		dedicatedMemoryMB += mProcessQuery[i].gpuStats.dedicatedMemoryMB;
		sharedMemoryMB += mProcessQuery[i].gpuStats.sharedMemoryMB;

		cpuLoad += mProcessQuery[i].cpuStats.usage;
		handlesNB += mProcessQuery[i].cpuStats.handleNB;
		threadsNB += mProcessQuery[i].cpuStats.threadNB;

		workingSetPrivateMB += mProcessQuery[i].memStats.workingSetPrivateMB;
		committedMB += mProcessQuery[i].memStats.committedMB;
	}

	// use the render load as default gpu usage
	mProcessGpu.gpuUsage = renderLoad;
	mProcessGpu.dedicatedMemoryMB = dedicatedMemoryMB;
	mProcessGpu.sharedMemoryMB = sharedMemoryMB;
	mProcessGpu.engines[0].usage = renderLoad;
	mProcessGpu.engines[0].type = ENGINE_TYPE_3D;
	mProcessGpu.engines[1].usage = videoDecoderLoad;
	mProcessGpu.engines[1].type = ENGINE_TYPE_VIDEO_DECODE;
	mProcessGpu.engines[2].usage = videoEncoderLoad;
	mProcessGpu.engines[2].type = ENGINE_TYPE_VIDEO_ENCODE;
	mProcessGpu.engines[3].usage = copyLoad;
	mProcessGpu.engines[3].type = ENGINE_TYPE_COPY;
	mProcessGpu.engines[4].usage = computeLoad;
	mProcessGpu.engines[4].type = ENGINE_TYPE_COMPUTE;

	mProcessCpu.usage = cpuLoad;
	mProcessCpu.handleNB = handlesNB;
	mProcessCpu.threadNB = threadsNB;

	mProcessMemory.workingSetPrivateMB = workingSetPrivateMB;
	mProcessMemory.committedMB = committedMB;
}

void PerfCounter::QueryProcessGpu(ProcessQueryWrapper &queryWrapper)
{
	DWORD bufferSize = 0;
	DWORD itemCount = 0;
	PDH_STATUS pdhStatus = ERROR_SUCCESS;

	double gpuUsage = 0.0;
	int dedicatedMemoryMB = 0;
	int sharedMemoryMB = 0;

	if (queryWrapper.gpuUsageCounter) {
		pdhStatus = PdhGetFormattedCounterArray(queryWrapper.gpuUsageCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(queryWrapper.gpuUsageCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS) {
				UpdateProcessGpuUsageInternal(queryWrapper, itemCount, items);

				// use the 3D engine usage as default gpu usage
				gpuUsage = queryWrapper.gpuStats.engines[0].usage;
			}
		}
	}

	bufferSize = 0;
	itemCount = 0;
	if (queryWrapper.dedicatedMemoryCounter) {
		pdhStatus = PdhGetFormattedCounterArray(queryWrapper.dedicatedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(queryWrapper.dedicatedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS) {
				LONGLONG maxValue = 0;
				for (size_t i = 0; i < itemCount; ++i) {
					maxValue = items[i].FmtValue.largeValue > maxValue ? items[i].FmtValue.largeValue : maxValue;
				}
				dedicatedMemoryMB = static_cast<int>(maxValue / BYTES_ONE_MBYTES);
			}
		}
	}

	bufferSize = 0;
	itemCount = 0;
	if (queryWrapper.sharedMemoryCounter) {
		pdhStatus = PdhGetFormattedCounterArray(queryWrapper.sharedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, nullptr);
		if (pdhStatus == PDH_MORE_DATA) {
			std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
			PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
			pdhStatus = PdhGetFormattedCounterArray(queryWrapper.sharedMemoryCounter, PDH_FMT_LARGE, &bufferSize, &itemCount, items);
			if (pdhStatus == ERROR_SUCCESS) {
				LONGLONG maxValue = 0;
				for (size_t i = 0; i < itemCount; ++i) {
					maxValue = items[i].FmtValue.largeValue > maxValue ? items[i].FmtValue.largeValue : maxValue;
				}
				sharedMemoryMB = static_cast<int>(maxValue / BYTES_ONE_MBYTES);
			}
		}
	}

	queryWrapper.gpuStats.gpuUsage = gpuUsage;
	queryWrapper.gpuStats.dedicatedMemoryMB = dedicatedMemoryMB;
	queryWrapper.gpuStats.sharedMemoryMB = sharedMemoryMB;
}

void PerfCounter::QueryProcessCpu(ProcessQueryWrapper &queryWrapper)
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	PDH_FMT_COUNTERVALUE pdhValue = {0};
	DWORD dwValue = 0;

	double cpuUsage = 0.0;
	int handlesNB = 0;
	int threadsNB = 0;

	if (queryWrapper.cpuUsageCounter) {
		pdhStatus = PdhGetFormattedCounterValue(queryWrapper.cpuUsageCounter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			cpuUsage = pdhValue.doubleValue / (double)GetLogicalCores();
	}

	pdhValue = {0};
	dwValue = 0;
	if (queryWrapper.handleCounter) {
		pdhStatus = PdhGetFormattedCounterValue(queryWrapper.handleCounter, PDH_FMT_LONG, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			handlesNB = pdhValue.longValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (queryWrapper.threadCounter) {
		pdhStatus = PdhGetFormattedCounterValue(queryWrapper.threadCounter, PDH_FMT_LONG, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			threadsNB = pdhValue.longValue;
	}

	queryWrapper.cpuStats.usage = cpuUsage;
	queryWrapper.cpuStats.handleNB = handlesNB;
	queryWrapper.cpuStats.threadNB = threadsNB;
}

void PerfCounter::QueryProcessMemory(ProcessQueryWrapper &queryWrapper)
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	PDH_FMT_COUNTERVALUE pdhValue = {0};
	DWORD dwValue = 0;

	int workingSetPrivateMB = 0;
	int committedMB = 0;

	if (queryWrapper.workingSetPrivateCounter) {
		pdhStatus = PdhGetFormattedCounterValue(queryWrapper.workingSetPrivateCounter, PDH_FMT_LARGE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			workingSetPrivateMB = static_cast<int>(pdhValue.largeValue / BYTES_ONE_MBYTES);
	}

	pdhValue = {0};
	dwValue = 0;
	if (queryWrapper.privateBytesCounter) {
		pdhStatus = PdhGetFormattedCounterValue(queryWrapper.privateBytesCounter, PDH_FMT_LARGE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			committedMB = static_cast<int>(pdhValue.largeValue / BYTES_ONE_MBYTES);
	}

	queryWrapper.memStats.workingSetPrivateMB = workingSetPrivateMB;
	queryWrapper.memStats.committedMB = committedMB;
}

void PerfCounter::UpdateProcessGpuUsageInternal(ProcessQueryWrapper &queryWrapper, DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items)
{
	double renderLoad = 0.0;
	double videoDecoderLoad = 0.0;
	double videoEncoderLoad = 0.0;
	double copyLoad = 0.0;
	double computeLoad = 0.0;
	for (DWORD i = 0; i < itemCount; i++) {
		std::wstring itemName(items[i].szName);
		if (itemName.find(sGpuEngine3DString) != std::string::npos)
			renderLoad += items[i].FmtValue.doubleValue;
		else if (itemName.find(sGpuEngineVideoDecodeString) != std::string::npos)
			videoDecoderLoad += items[i].FmtValue.doubleValue;
		else if (itemName.find(sGpuEngineVideoEncodeString) != std::string::npos)
			videoEncoderLoad += items[i].FmtValue.doubleValue;
		else if (itemName.find(sGpuEngineCopyString) != std::string::npos)
			copyLoad += items[i].FmtValue.doubleValue;
		else if (itemName.find(sGpuEngineComputeString) != std::string::npos)
			computeLoad += items[i].FmtValue.doubleValue;
	}

	queryWrapper.gpuStats.engines[0].type = ENGINE_TYPE_3D;
	queryWrapper.gpuStats.engines[0].usage = renderLoad;
	queryWrapper.gpuStats.engines[1].type = ENGINE_TYPE_VIDEO_DECODE;
	queryWrapper.gpuStats.engines[1].usage = videoDecoderLoad;
	queryWrapper.gpuStats.engines[2].type = ENGINE_TYPE_VIDEO_ENCODE;
	queryWrapper.gpuStats.engines[2].usage = videoEncoderLoad;
	queryWrapper.gpuStats.engines[3].type = ENGINE_TYPE_COPY;
	queryWrapper.gpuStats.engines[3].usage = copyLoad;
	queryWrapper.gpuStats.engines[4].type = ENGINE_TYPE_COMPUTE;
	queryWrapper.gpuStats.engines[4].usage = computeLoad;
}

void PerfCounter::CheckProcessActive()
{
	for (int i = 0; i < mProcessQuery.size(); i++) {
		ProcessQueryWrapper *pWrapper = &mProcessQuery[i];
		if (--pWrapper->active == 0) {
			if (pWrapper->gpuUsageCounter) {
				PdhRemoveCounter(pWrapper->gpuUsageCounter);
				mNBCounters--;
			}

			if (pWrapper->dedicatedMemoryCounter) {
				PdhRemoveCounter(pWrapper->dedicatedMemoryCounter);
				mNBCounters--;
			}

			if (pWrapper->sharedMemoryCounter) {
				PdhRemoveCounter(pWrapper->sharedMemoryCounter);
				mNBCounters--;
			}

			if (pWrapper->cpuUsageCounter) {
				PdhRemoveCounter(pWrapper->cpuUsageCounter);
				mNBCounters--;
			}

			if (pWrapper->workingSetPrivateCounter) {
				PdhRemoveCounter(pWrapper->workingSetPrivateCounter);
				mNBCounters--;
			}

			if (pWrapper->privateBytesCounter) {
				PdhRemoveCounter(pWrapper->privateBytesCounter);
				mNBCounters--;
			}

			if (pWrapper->handleCounter) {
				PdhRemoveCounter(pWrapper->handleCounter);
				mNBCounters--;
			}

			if (pWrapper->threadCounter) {
				PdhRemoveCounter(pWrapper->threadCounter);
				mNBCounters--;
			}

			mProcessQuery.erase(mProcessQuery.begin() + i);
		}
	}
}

void PerfCounter::AddCpuQuery()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	pdhStatus = PdhAddEnglishCounter(mQuery, sProcessorTimeCounterPath, 0, &mCpuQuery.processorTimeCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sProcessorTimeCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sProcessorPerformanceCounterPath, 0, &mCpuQuery.processorPerformanceCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sProcessorPerformanceCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sProcessorUtilityCounterPath, 0, &mCpuQuery.processorUtilityCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sProcessorUtilityCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sProcessorFrequencyCounterPath, 0, &mCpuQuery.processorFrequencyCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sProcessorFrequencyCounterPath))
		mNBCounters++;
}

void PerfCounter::QueryCpu()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	PDH_FMT_COUNTERVALUE pdhValue = {0};
	DWORD dwValue = 0;

	double cpuUsage = 0.0;
	double frequency = 0.0;
	double performance = 0.0;
	double utility = 0.0;

	if (mCpuQuery.processorTimeCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mCpuQuery.processorTimeCounter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			cpuUsage = pdhValue.doubleValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (mCpuQuery.processorFrequencyCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mCpuQuery.processorFrequencyCounter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			frequency = pdhValue.doubleValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (mCpuQuery.processorPerformanceCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mCpuQuery.processorPerformanceCounter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			performance = pdhValue.doubleValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (mCpuQuery.processorUtilityCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mCpuQuery.processorUtilityCounter, PDH_FMT_DOUBLE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			utility = pdhValue.doubleValue;
	}

	mCpu.usage = cpuUsage;
	mCpu.baseFrequencyGHz = frequency;
	mCpu.performance = performance;
	mCpu.utility = utility;
}

void PerfCounter::AddMemoryQuery()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	pdhStatus = PdhAddEnglishCounter(mQuery, sCommitLimitCounterPath, 0, &mMemoryQuery.commitLimitCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sCommitLimitCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, sCommittedBytesCounterPath, 0, &mMemoryQuery.committedBytesCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", sCommittedBytesCounterPath))
		mNBCounters++;
}

void PerfCounter::QueryMemory()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	PDH_FMT_COUNTERVALUE pdhValue = {0};
	DWORD dwValue = 0;

	if (mMemoryQuery.commitLimitCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mMemoryQuery.commitLimitCounter, PDH_FMT_LARGE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			mMemoryQuery.commitLimitMB = static_cast<int>(pdhValue.largeValue / BYTES_ONE_MBYTES);
	}

	pdhValue = {0};
	dwValue = 0;
	if (mMemoryQuery.committedBytesCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mMemoryQuery.committedBytesCounter, PDH_FMT_LARGE, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			mMemoryQuery.committedMB = static_cast<int>(pdhValue.largeValue / BYTES_ONE_MBYTES);
	}
	MemoryValue memVal;
	CheckPhysicalMemory(memVal);
	mMemoryQuery.physicalMemory = memVal;
}

void PerfCounter::AddNetworkInterfaceQuery()
{
	mNetwork.stats.name = GetBestNetworkInterface();
	if (mNetwork.stats.name.empty())
		return;

	std::wstring wstrBestInterface = S2WS(mNetwork.stats.name);
	std::wstring bytesReceivedCounterPath = sNetworkInterfaceBytesReceivedCounterPath;
	std::wstring bytesSentCounterPath = sNetworkInterfaceBytesSentCounterPath;
	std::wstring bandwidthCountPath = sNetworkInterfaceCurrentBandwidthCounterPath;
	bytesReceivedCounterPath.replace(bytesReceivedCounterPath.find('*'), 1, wstrBestInterface);
	bytesSentCounterPath.replace(bytesSentCounterPath.find('*'), 1, wstrBestInterface);
	bandwidthCountPath.replace(bandwidthCountPath.find('*'), 1, wstrBestInterface);

	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	pdhStatus = PdhAddEnglishCounter(mQuery, bytesReceivedCounterPath.c_str(), 0, &mNetwork.receivedCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", bytesReceivedCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, bytesSentCounterPath.c_str(), 0, &mNetwork.sentCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", bytesSentCounterPath))
		mNBCounters++;
	pdhStatus = PdhAddEnglishCounter(mQuery, bandwidthCountPath.c_str(), 0, &mNetwork.bandwidthCounter);
	if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", bandwidthCountPath))
		mNBCounters++;
}
void PerfCounter::QueryNetwork()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	PDH_FMT_COUNTERVALUE pdhValue = {0};
	DWORD dwValue = 0;

	int received = 0;
	int sent = 0;
	int bandwidth = 0;

	if (mNetwork.receivedCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mNetwork.receivedCounter, PDH_FMT_LONG, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			received = pdhValue.longValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (mNetwork.sentCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mNetwork.sentCounter, PDH_FMT_LONG, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			sent = pdhValue.longValue;
	}

	pdhValue = {0};
	dwValue = 0;
	if (mNetwork.bandwidthCounter) {
		pdhStatus = PdhGetFormattedCounterValue(mNetwork.bandwidthCounter, PDH_FMT_LONG, &dwValue, &pdhValue);
		if (pdhStatus == ERROR_SUCCESS)
			bandwidth = pdhValue.longValue;
	}

	mNetwork.stats.receivedKbps = (double)received * BITS_ONE_BYTE / BYTES_ONE_KBYTES;
	mNetwork.stats.sentKbps = (double)sent * BITS_ONE_BYTE / BYTES_ONE_KBYTES;
	mNetwork.stats.bandwidthMB = bandwidth / BYTES_ONE_MBYTES;
}

void PerfCounter::AddPhysicalDiskQuery()
{
	PDH_STATUS pdhStatus = ERROR_SUCCESS;
	std::vector<PhysicalDisk> physicalDisks;
	int physicalDiskCount = EnumPhysicalDisk(physicalDisks);
	mPerfStats.system.disk.physicalDisks.resize(physicalDiskCount);
	for (int i = 0; i < physicalDiskCount; i++) {
		PhysicalDiskQueryWrapper queryWrapper;
		queryWrapper.disk = physicalDisks[i];

		std::wstring wstrIndex = std::to_wstring(physicalDisks[i].index);
		std::wstring readCountPath = sPhysicalDiskReadBytesCounterPath;
		std::wstring writeCountPath = sPhysicalDiskWriteBytesCounterPath;
		readCountPath.replace(readCountPath.find('*'), 1, wstrIndex);
		writeCountPath.replace(writeCountPath.find('*'), 1, wstrIndex);

		pdhStatus = PdhAddEnglishCounter(mQuery, readCountPath.c_str(), 0, &queryWrapper.readCounter);
		if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", readCountPath.c_str()))
			mNBCounters++;
		pdhStatus = PdhAddEnglishCounter(mQuery, writeCountPath.c_str(), 0, &queryWrapper.writeCounter);
		if (SucceededPDH(pdhStatus, "PdhAddEnglishCounter", writeCountPath.c_str()))
			mNBCounters++;

		mPhysicalDiskQuery.push_back(queryWrapper);
	}
}

void PerfCounter::QueryPhysicalDisk()
{
	DWORD bufferSize = 0;
	DWORD itemCount = 0;
	PDH_STATUS pdhStatus = ERROR_SUCCESS;

	for (int i = 0; i < mPhysicalDiskQuery.size(); i++) {
		bufferSize = 0;
		itemCount = 0;
		if (mPhysicalDiskQuery[i].readCounter) {
			pdhStatus = PdhGetFormattedCounterArray(mPhysicalDiskQuery[i].readCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
			if (pdhStatus == PDH_MORE_DATA) {
				std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
				PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
				pdhStatus = PdhGetFormattedCounterArray(mPhysicalDiskQuery[i].readCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
				if (pdhStatus == ERROR_SUCCESS) {
					if (mPhysicalDiskQuery[i].disk.name.empty())
						mPhysicalDiskQuery[i].disk.name = WS2S(items->szName);
					mPhysicalDiskQuery[i].disk.readKBps = items->FmtValue.doubleValue / 1024.0;
				}
			}
		}

		bufferSize = 0;
		itemCount = 0;
		if (mPhysicalDiskQuery[i].writeCounter) {
			pdhStatus = PdhGetFormattedCounterArray(mPhysicalDiskQuery[i].writeCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
			if (pdhStatus == PDH_MORE_DATA) {
				std::unique_ptr<uint8_t[]> itemBuffer = std::unique_ptr<uint8_t[]>{new uint8_t[bufferSize]};
				PDH_FMT_COUNTERVALUE_ITEM *items = (PDH_FMT_COUNTERVALUE_ITEM *)itemBuffer.get();
				pdhStatus = PdhGetFormattedCounterArray(mPhysicalDiskQuery[i].writeCounter, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
				if (pdhStatus == ERROR_SUCCESS) {
					if (mPhysicalDiskQuery[i].disk.name.empty())
						mPhysicalDiskQuery[i].disk.name = WS2S(items->szName);
					mPhysicalDiskQuery[i].disk.writeKBps = items->FmtValue.doubleValue / 1024.0;
				}
			}
		}
	}
}

bool PerfCounter::GetPerfStats(PerfStats &stats)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mResultReady)
		return false;

	stats = mPerfStats;

	return true;
}

void PerfCounter::DumpQueryResult()
{
	if (mReady2Output) {
		std::lock_guard<std::mutex> lock(mMutex);
		// process
		mPerfStats.process.pid = mCurrentPID;
		mPerfStats.process.name = mProcessQuery[0].name;
		mPerfStats.process.gpu = mProcessGpu;
		mPerfStats.process.cpu = mProcessCpu;
		mPerfStats.process.memory.committedMB = mProcessMemory.committedMB;
		mPerfStats.process.memory.workingSetPrivateMB = mProcessMemory.workingSetPrivateMB;
		// system
		mPerfStats.system.cpu = mCpu;
		mPerfStats.system.gpus = mGpuQuery.gpus;
		mPerfStats.system.memory.commitLimitMB = mMemoryQuery.commitLimitMB;
		mPerfStats.system.memory.committedMB = mMemoryQuery.committedMB;
		mPerfStats.system.memory.physicalMemory = mMemoryQuery.physicalMemory;
		mPerfStats.system.network = mNetwork.stats;

		for (int i = 0; i < mPhysicalDiskQuery.size(); i++)
			mPerfStats.system.disk.physicalDisks[i] = mPhysicalDiskQuery[i].disk;

		mResultReady = true;
	}
}
}
