#ifndef __PLS_PERF_DEFINE_HPP__
#define __PLS_PERF_DEFINE_HPP__

#include <string>
#include <vector>

#if _DEBUG
//#define ENABLE_FULL_FEATURE
#endif
#define USED_INSIDE_PRISM true

namespace PLSPerf {
/* clang-format off */
enum EngineType {
	ENGINE_TYPE_3D			= 0x1,
	ENGINE_TYPE_VIDEO_DECODE	= ENGINE_TYPE_3D << 1,
	ENGINE_TYPE_VIDEO_ENCODE	= ENGINE_TYPE_3D << 2,
	ENGINE_TYPE_COPY		= ENGINE_TYPE_3D << 3,
	ENGINE_TYPE_COMPUTE		= ENGINE_TYPE_3D << 4,

	/* TO BE SUPPORTED */
#if 0
	ENGINE_TYPE_VIDEO_PROCESSING	= ENGINE_TYPE_3D << 5,
	ENGINE_TYPE_OVERLAY		= ENGINE_TYPE_3D << 6,
	ENGINE_TYPE_CRYPTO		= ENGINE_TYPE_3D << 7,
	ENGINE_TYPE_MAX			= ENGINE_TYPE_3D << 8,
	ENGINE_TYPE_SCENE_ASSEMBLY	= ENGINE_TYPE_3D << 9,
	ENGINE_TYPE_VR			= ENGINE_TYPE_3D << 10,
#endif
	ENGINE_TYPE_COUNT		= ENGINE_TYPE_3D << 5,
};
/* clang-format on */

struct MemoryValue {
	union {
		int usedMB = 0;
		int freeMB;
	};
	int totalMB = 0;
};

/*--------------------------------GPU----------------------------------*/
struct LUID {
	unsigned long lowPart = 0;
	long highPart = 0;
};

struct Engine {
	enum EngineType type = ENGINE_TYPE_3D;
	double usage = 0.0;
};

struct ProcessGpu {
	double gpuUsage = 0.0;
	int sharedMemoryMB = 0;
	int dedicatedMemoryMB = 0;
	std::vector<Engine> engines;
};

struct Gpu {
	LUID luid;
	std::string name;
	struct MemoryValue sharedMemory;    // {used, total}
	struct MemoryValue dedicatedMemory; // {used, total}
	std::vector<Engine> engines;
};

/*--------------------------------CPU----------------------------------*/
struct ProcessCpu {
	double usage = 0.0;
	int handleNB = 0;
	int threadNB = 0;
};

struct Cpu {
	int physicalCoreNB = 0;
	int logicalCoreNB = 0;
	double usage = 0.0;
	double baseFrequencyGHz = 0.0;
	double performance = 0.0;
	double utility = 0.0; // this is only supported on Windows 10
};

/*--------------------------------Memory--------------------------------*/
struct ProcessMemory {
	int workingSetPrivateMB = 0;
	int committedMB = 0;
};

struct Memory {
	MemoryValue physicalMemory; // {free, total}
	int committedMB = 0;
	int commitLimitMB = 0;
};

/*--------------------------------Network--------------------------------*/
struct Network {
	std::string name;
	int bandwidthMB = 0;
	double sentKbps = 0.0;
	double receivedKbps = 0.0;
};

/*--------------------------------Disk------------------------------------*/
struct PhysicalDisk {
	std::string name;
	int index = 0;
	int totalGB = 0;
	double readKBps = 0.0;
	double writeKBps = 0.0;
};

struct Disk {
	std::vector<PhysicalDisk> physicalDisks;
	// TODO: Logical Disks
};

/*--------------------------------System Stats Wrapper---------------------*/
struct SystemStats {
	Cpu cpu;
	std::vector<Gpu> gpus;
	Memory memory;
	Network network;
	Disk disk;
};

/*--------------------------------Process Stats Wrapper-------------------*/
struct ProcessStats {
	unsigned long pid = 0;
	std::string name;
	ProcessCpu cpu;
	ProcessGpu gpu;
	ProcessMemory memory;
};

/*--------------------------------Overall---------------------------------*/
struct PerfStats {
	SystemStats system;
	ProcessStats process;
};

}

#endif
