#ifndef __PLS_PERF_HELPER_HPP__
#define __PLS_PERF_HELPER_HPP__
#include <string>
#include <vector>
#include "PLSPerfDefine.hpp"

namespace PLSPerf {
/* clang-format off */
#define BITS_ONE_BYTE		(8)
#define BYTES_ONE_KBYTES	(1024)
#define BYTES_ONE_MBYTES	(1024 * 1024)
#define BYTES_ONE_GBYTES	(1024 * 1024 * 1024)

std::string	WS2S(const std::wstring &wstr);
std::wstring	S2WS(const std::string &str);

int		EnumAdapter(std::vector<Gpu> &gpus);
int		EnumPhysicalDisk(std::vector<PhysicalDisk> &disks);
int		GetLogicalCores();
int		GetPhysicalCores();
void		CheckPhysicalMemory(MemoryValue &memory);
std::string	GetBestNetworkInterface();
std::wstring	CutProcessName(const std::wstring& name);
LUID		GetLuidFromFormattedCounterNameString(const std::wstring &str);
std::string	GetEngineName(EngineType type);
/* clang-format on */

inline bool SameLuid(const LUID &luid1, const LUID &luid2)
{
	return (luid1.highPart == luid2.highPart && luid1.lowPart == luid2.lowPart);
}
}

#endif
