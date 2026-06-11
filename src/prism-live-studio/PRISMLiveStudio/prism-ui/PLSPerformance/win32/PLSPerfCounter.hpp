#ifndef __PLS_PERF_COUNTER_HPP__
#define __PLS_PERF_COUNTER_HPP__
#include <Pdh.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <atomic>
#include <cmath>
#include "PLSPerfDefine.hpp"

namespace PLSPerf {
class PerfCounter {
public:
	static PerfCounter &Instance(unsigned long pid = 0)
	{
		static PerfCounter counter(pid);
		return counter;
	}

	bool GetPerfStats(PerfStats &stats);
	void Stop();

private:
	struct GpuQueryWrapper {
		std::vector<Gpu> gpus;
		HCOUNTER usageCounter = nullptr;
		HCOUNTER dedicatedMemoryCounter = nullptr;
		HCOUNTER sharedMemoryCounter = nullptr;
	};

	struct ProcessQueryWrapper {
		int active = 0;
		unsigned long pid = 0;
		std::string name;
		ProcessGpu gpuStats;
		ProcessCpu cpuStats;
		ProcessMemory memStats;
		HCOUNTER gpuUsageCounter = nullptr;
		HCOUNTER dedicatedMemoryCounter = nullptr;
		HCOUNTER sharedMemoryCounter = nullptr;
		HCOUNTER cpuUsageCounter = nullptr;
		HCOUNTER handleCounter = nullptr;
		HCOUNTER threadCounter = nullptr;
		HCOUNTER workingSetPrivateCounter = nullptr;
		HCOUNTER privateBytesCounter = nullptr;
	};

	struct CpuQueryWrapper {
		double usage = 0.0;
		double baseFrequencyGHz = 0.0;
		double performance = 0.0;
		HCOUNTER processorTimeCounter = nullptr;
		HCOUNTER processorFrequencyCounter = nullptr;
		HCOUNTER processorPerformanceCounter = nullptr;
		HCOUNTER processorUtilityCounter = nullptr;
	};

	struct MemoryQueryWrapper {
		int commitLimitMB = 0;
		int committedMB = 0;
		MemoryValue physicalMemory;
		HCOUNTER commitLimitCounter = nullptr;
		HCOUNTER committedBytesCounter = nullptr;
	};

	struct NetworkQueryWrapper {
		Network stats;
		HCOUNTER sentCounter = nullptr;
		HCOUNTER receivedCounter = nullptr;
		HCOUNTER bandwidthCounter = nullptr;
	};

	struct PhysicalDiskQueryWrapper {
		PhysicalDisk disk;
		HCOUNTER readCounter = nullptr;
		HCOUNTER writeCounter = nullptr;
	};

private:
	explicit PerfCounter(unsigned long pid) : mCurrentPID(pid)
	{
		mProcessGpu.engines.resize((int)std::log2((double)ENGINE_TYPE_COUNT));
		Start();
	}
	PerfCounter() = delete;
	PerfCounter(const PerfCounter &) = delete;
	PerfCounter &operator=(const PerfCounter &) = delete;
	bool Start();
	void Close();
	void Update();

	void AddAdapterQuery();
	void QueryAdapter();
	void UpdateAdapterGpuUsageInternal(DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items);
	void UpdateAdapterGpuMemoryInternal(DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items, bool dedicated = true /*false for shared*/);

	void CheckProcessActive();
	void UpdateProcessQuery(bool ready2Query = false);
	void NewProcess(DWORD pid, const std::wstring &name);
	void QueryProcess();
	void QueryProcessGpu(ProcessQueryWrapper &queryWrapper);
	void QueryProcessCpu(ProcessQueryWrapper &queryWrapper);
	void QueryProcessMemory(ProcessQueryWrapper &queryWrapper);
	void UpdateProcessGpuUsageInternal(ProcessQueryWrapper &queryWrapper, DWORD itemCount, const PDH_FMT_COUNTERVALUE_ITEM *items);

	void AddCpuQuery();
	void QueryCpu();

	void AddMemoryQuery();
	void QueryMemory();

	void AddPhysicalDiskQuery();
	void QueryPhysicalDisk();

	void AddNetworkInterfaceQuery();
	void QueryNetwork();

	void DumpQueryResult();

private:
	HQUERY mQuery = nullptr;
	DWORD mCurrentPID = 0;
	std::atomic_long mNBCounters = 0;
	std::atomic_bool mReady2Output = false;
	bool mResultReady = false;
	std::mutex mMutex;
	GpuQueryWrapper mGpuQuery;
	ProcessGpu mProcessGpu;
	ProcessCpu mProcessCpu;
	ProcessMemory mProcessMemory;
	Cpu mCpu;

	std::vector<ProcessQueryWrapper> mProcessQuery;
	std::vector<PhysicalDiskQueryWrapper> mPhysicalDiskQuery;
	CpuQueryWrapper mCpuQuery;
	MemoryQueryWrapper mMemoryQuery;
	NetworkQueryWrapper mNetwork;

	HANDLE m_hEventUpdate;
	std::thread m_threadUpdate;

	// query result for output
	PerfStats mPerfStats;
};

}

#endif
