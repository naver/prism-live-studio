#ifndef PLSPERFORMANCE_H
#define PLSPERFORMANCE_H

#include "libutils-export.h"

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <map>
#include <shared_mutex>
#include <string>

#if defined(PLS_PERFORMANCE_STATS)
#define __macro_concat_i(a, b) a##b
#define __macro_concat(a, b) __macro_concat_i(a, b)
#define PLS_PERFORMANCE_FUNCTION(/*comment*/...) \
	using namespace pls::performance;        \
	PLSFunctionStats __performance_function_stats(PLSStats::top(), __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define PLS_PERFORMANCE_START(id, /*comment*/...) PLSRangeStats __performance_range_stats_##id(&__performance_function_stats, #id, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define PLS_PERFORMANCE_END(id) __performance_range_stats_##id.complete()
#define PLS_PERFORMANCE_GLOBAL_START(id, /*parent, comment*/...) pls::performance::PLSGlobalStats::globalStart(id, __FILE__, __LINE__, ##__VA_ARGS__)
#define PLS_PERFORMANCE_GLOBAL_END(id) pls::performance::PLSGlobalStats::globalEnd(id)
#else
#define PLS_PERFORMANCE_FUNCTION(/*comment*/...)
#define PLS_PERFORMANCE_START(id, /*comment*/...)
#define PLS_PERFORMANCE_END(id)
#define PLS_PERFORMANCE_GLOBAL_START(id, /*parent, comment*/...)
#define PLS_PERFORMANCE_GLOBAL_END(id)
#endif

namespace pls {
namespace performance {

struct CommentFileLineLess {
	bool operator()(const std::tuple<std::string, std::string, std::string, std::int32_t> &a, const std::tuple<std::string, std::string, std::string, std::int32_t> &b) const
	{
		if (auto cmp3 = std::get<3>(a) - std::get<3>(b); cmp3 != 0) {
			return cmp3 < 0;
		} else if (auto cmp2 = std::get<2>(a).compare(std::get<2>(b)); cmp2 != 0) {
			return cmp2 < 0;
		} else if (auto cmp1 = std::get<1>(a).compare(std::get<1>(b)); cmp1 != 0) {
			return cmp1 < 0;
		} else if (auto cmp0 = std::get<0>(a).compare(std::get<0>(b)); cmp0 != 0) {
			return cmp0 < 0;
		} else {
			return false;
		}
	}
};

struct PLSStatsData;
struct PLSStats;
struct PLSRangeStats;
struct PLSFunctionStats;
struct PLSGlobalStats;
using PLSStatsDataPtr = std::shared_ptr<PLSStatsData>;
using PLSStatsDataWeakPtr = std::weak_ptr<PLSStatsData>;
using PLSStatsDataList = std::list<PLSStatsDataPtr>;
using PLSStatsDataMap = std::map<std::tuple<std::string, std::string, std::string, std::int32_t>, PLSStatsDataPtr, CommentFileLineLess>;
using PLSStatsList = std::list<PLSStats *>;
using PLSGlobalStatsPtr = std::shared_ptr<PLSGlobalStats>;
using PLSGlobalStatsList = std::list<PLSGlobalStatsPtr>;

struct LIBUTILSAPI_API PLSStatsData {
	std::shared_mutex mutex;
	std::string comment;
	std::string function;
	std::string file;
	std::int32_t line;
	std::int64_t count = 0;
	std::int64_t elapsed = -1;
	std::int64_t minElapsed = -1;
	std::int64_t maxElapsed = -1;
	std::int64_t averageElapsed = -1;
	PLSStatsDataList children;
	PLSStatsDataMap map;

	explicit PLSStatsData(const std::string &comment, const std::string &function, const std::string &file, std::int32_t line);

	static std::pair<std::shared_mutex, std::map<std::uint32_t, PLSStatsDataPtr>> &statsData();
	static void clear();
	static PLSStatsDataPtr findTopLevel(const std::string &function, const std::string &comment, const std::string &file, std::int32_t line);
	static PLSStatsDataPtr statsData(PLSStatsDataPtr parent, const std::string &function, const std::string &file, std::int32_t line, const std::string &comment);
	static PLSStatsDataPtr statsData(std::uint32_t tid, PLSStatsDataPtr parent, const std::string &function, const std::string &file, std::int32_t line, const std::string &comment);

	void setElapsed(std::int64_t elapsed);
	std::int64_t getElapsed() const;
	std::int64_t getMinElapsed() const;
	std::int64_t getMaxElapsed() const;
	std::int64_t getAverageElapsed() const;
};
struct LIBUTILSAPI_API PLSStats {
	PLSStats *parent;
	const char *comment;
	const char *function;
	const char *file;
	std::int32_t line;
	std::int64_t elapsed = -1;
	std::chrono::steady_clock::time_point start;
	std::chrono::steady_clock::time_point end;
	PLSStatsDataWeakPtr data;
	PLSStatsList children;

	explicit PLSStats(PLSStats *parent, PLSStatsDataPtr data, const char *function, const char *file, std::int32_t line, const char *comment, bool global = false);
	~PLSStats() = default;

	static std::pair<std::shared_mutex, std::map<uint32_t, PLSStatsList>> &stack();
	static void clear();
	static PLSStats *top();
	static void push(PLSStats *stats);
	static void pop(const PLSStats *stats);
};
struct LIBUTILSAPI_API PLSRangeStats : public PLSStats {
	explicit PLSRangeStats(PLSStats *parent, const char *id, const char *function, const char *file, std::int32_t line, const char *comment = "");
	~PLSRangeStats() = default;

	void complete();
};
struct LIBUTILSAPI_API PLSFunctionStats : public PLSStats {
	explicit PLSFunctionStats(PLSStats *parent, const char *function, const char *file, std::int32_t line, const char *comment = "");
	~PLSFunctionStats();
};
struct LIBUTILSAPI_API PLSGlobalStats : public PLSStats {
	explicit PLSGlobalStats(PLSStatsDataPtr parent, const char *id, const char *file, std::int32_t line, const char *comment = "");
	~PLSGlobalStats();

	static std::pair<std::shared_mutex, std::map<std::string, std::pair<std::mutex, PLSGlobalStatsList>>> &globalStats();
	static void clear();
	static PLSStatsDataPtr globalParent(const char *parent);
	static void globalStart(const char *id, const char *file, std::int32_t line, const char *parent = nullptr, const char *comment = "");
	static void globalEnd(const char *id);

	static void global(const std::string &id, const std::string &file, std::int32_t line, const std::string &comment, std::int64_t elapsed);
	static void global(const std::list<std::string> &tree, const std::string &id, const std::string &file, std::int32_t line, const std::string &comment, std::int64_t elapsed);
};

LIBUTILSAPI_API void clear();

}
}

#endif // PLSPERFORMANCE_H
