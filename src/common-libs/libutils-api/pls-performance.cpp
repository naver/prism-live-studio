#include "pls-performance.h"
#include <libutils-api.h>

namespace pls {
namespace performance {

template<typename Key, typename Value, typename Less> Value &get_value(std::shared_mutex &mutex, std::map<Key, Value, Less> &map, const Key &key)
{
	{
		std::shared_lock lock(mutex);
		if (auto iter = map.find(key); iter != map.end())
			return iter->second;
	}

	std::unique_lock lock(mutex);
	return map[key];
}
template<typename Key, typename Value, typename Less> Value &get_value(std::pair<std::shared_mutex, std::map<Key, Value, Less>> &map, const Key &key)
{
	return get_value(map.first, map.second, key);
}
template<typename Key, typename Value, typename Less, typename Create> Value get_value(std::map<Key, Value, Less> &map, const Key &key, Create create)
{
	if (auto iter = map.find(key); iter != map.end())
		return iter->second;

	auto data = create();
	map[key] = data;
	return data;
}
template<typename Key, typename Value, typename Less, typename Create> Value get_value(std::shared_mutex &mutex, std::map<Key, Value, Less> &map, const Key &key, Create create)
{
	{
		std::shared_lock lock(mutex);
		if (auto iter = map.find(key); iter != map.end())
			return iter->second;
	}

	std::unique_lock lock(mutex);
	return get_value(map, key, create);
}

PLSStatsData::PLSStatsData(const std::string &comment_, const std::string &function_, const std::string &file_, std::int32_t line_) : comment(comment_), function(function_), file(file_), line(line_)
{
}

std::pair<std::shared_mutex, std::map<std::uint32_t, PLSStatsDataPtr>> &PLSStatsData::statsData()
{
	static std::pair<std::shared_mutex, std::map<std::uint32_t, PLSStatsDataPtr>> statsData;
	return statsData;
}
void PLSStatsData::clear()
{
	auto &[mutex, map] = PLSStatsData::statsData();
	std::unique_lock lock(mutex);
	map.clear();
}
PLSStatsDataPtr PLSStatsData::findTopLevel(const std::string &function, const std::string &comment, const std::string &file, std::int32_t line)
{
	auto key = std::make_tuple(function, comment, file, line);
	auto &[mutex, root] = PLSStatsData::statsData();
	std::shared_lock lock1(mutex);
	for (auto [tid, data] : root) {
		std::unique_lock lock2(data->mutex);
		if (auto i = data->map.find(key); i != data->map.end()) {
			return i->second;
		}
	}
	return nullptr;
}
PLSStatsDataPtr PLSStatsData::statsData(PLSStatsDataPtr parent, const std::string &function, const std::string &file, std::int32_t line, const std::string &comment)
{
	return statsData(pls_current_thread_id(), parent, function, file, line, comment);
}
PLSStatsDataPtr PLSStatsData::statsData(std::uint32_t tid, PLSStatsDataPtr parent, const std::string &function, const std::string &file, std::int32_t line, const std::string &comment)
{
	if (!parent) {
		auto &[mutex, root] = PLSStatsData::statsData();
		parent = get_value(mutex, root, tid, []() { return std::make_shared<PLSStatsData>("root", "", "", 0); });
	}

	auto key = std::make_tuple(function, comment, file, line);
	std::shared_lock lock(parent->mutex);
	return get_value(parent->map, key, [parent, comment, function, file, line]() {
		auto data = std::make_shared<PLSStatsData>(comment, function, file, line);
		parent->children.push_back(data);
		return data;
	});
}

void PLSStatsData::setElapsed(std::int64_t elapsed_)
{
	std::shared_lock lock(mutex);
	++count;
	elapsed = elapsed_;
	minElapsed = (minElapsed >= 0) ? std::min(minElapsed, elapsed) : elapsed;
	maxElapsed = std::max(maxElapsed, elapsed);
	if (averageElapsed >= 0)
		averageElapsed = averageElapsed + (elapsed - averageElapsed) / count;
	else
		averageElapsed = elapsed;
}
std::int64_t PLSStatsData::getElapsed() const
{
	if (elapsed >= 0)
		return elapsed;

	std::int64_t value = 0;
	for (auto child : children)
		value += child->getElapsed();
	return value;
}
std::int64_t PLSStatsData::getMinElapsed() const
{
	return (minElapsed >= 0) ? minElapsed : getElapsed();
}
std::int64_t PLSStatsData::getMaxElapsed() const
{
	return (maxElapsed >= 0) ? maxElapsed : getElapsed();
}
std::int64_t PLSStatsData::getAverageElapsed() const
{
	return (averageElapsed >= 0) ? averageElapsed : getElapsed();
}

PLSStats::PLSStats(PLSStats *parent_, PLSStatsDataPtr data_, const char *function_, const char *file_, std::int32_t line_, const char *comment_, bool global)
	: parent(parent_), comment(comment_), function(function_), file(file_), line(line_), data(data_)
{
	if (!global)
		push(this);
}

std::pair<std::shared_mutex, std::map<uint32_t, PLSStatsList>> &PLSStats::stack()
{
	static std::pair<std::shared_mutex, std::map<uint32_t, PLSStatsList>> stack;
	return stack;
}
void PLSStats::clear()
{
	auto &[mutex, map] = PLSStats::stack();
	std::unique_lock lock(mutex);
	map.clear();
}
PLSStats *PLSStats::top()
{
	if (auto &stack = get_value(PLSStats::stack(), pls_current_thread_id()); !stack.empty())
		return stack.back();
	return nullptr;
}
void PLSStats::push(PLSStats *stats)
{
	get_value(PLSStats::stack(), pls_current_thread_id()).push_back(stats);
}
void PLSStats::pop(const PLSStats *stats)
{
	if (auto &stack = get_value(PLSStats::stack(), pls_current_thread_id()); stack.empty())
		return;
	else if (stack.back() == stats)
		stack.pop_back();
}

PLSRangeStats::PLSRangeStats(PLSStats *parent, const char *id, const char *function, const char *file, std::int32_t line, const char *comment)
	: PLSStats(parent, PLSStatsData::statsData(parent->data.lock(), id, file, line, comment), id, file, line, comment)
{
	parent->children.push_back(this);
	this->start = std::chrono::steady_clock::now();
}
void PLSRangeStats::complete()
{
	this->end = std::chrono::steady_clock::now();
	this->elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(this->end - this->start).count();
	if (auto d = this->data.lock(); d)
		d->setElapsed(this->elapsed);
	pop(this);
}

PLSFunctionStats::PLSFunctionStats(PLSStats *parent, const char *function, const char *file, std::int32_t line, const char *comment)
	: PLSStats(parent, PLSStatsData::statsData(parent ? parent->data.lock() : nullptr, function, file, line, comment), function, file, line, comment)
{
	if (parent)
		parent->children.push_back(this);
	this->start = std::chrono::steady_clock::now();
}

PLSFunctionStats::~PLSFunctionStats()
{
	this->end = std::chrono::steady_clock::now();
	this->elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(this->end - this->start).count();
	if (auto d = this->data.lock(); d)
		d->setElapsed(this->elapsed);
	pop(this);
}

PLSGlobalStats::PLSGlobalStats(PLSStatsDataPtr parent, const char *id, const char *file, std::int32_t line, const char *comment)
	: PLSStats(nullptr, PLSStatsData::statsData(0, parent, id, file, line, comment), id, file, line, comment, true)
{
	this->start = std::chrono::steady_clock::now();
}

PLSGlobalStats::~PLSGlobalStats()
{
	this->end = std::chrono::steady_clock::now();
	this->elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(this->end - this->start).count();
	if (auto d = this->data.lock(); d)
		d->setElapsed(this->elapsed);
}

std::pair<std::shared_mutex, std::map<std::string, std::pair<std::mutex, PLSGlobalStatsList>>> &PLSGlobalStats::globalStats()
{
	static std::pair<std::shared_mutex, std::map<std::string, std::pair<std::mutex, PLSGlobalStatsList>>> globalStats;
	return globalStats;
}
void PLSGlobalStats::clear()
{
	auto &[mutex, map] = PLSGlobalStats::globalStats();
	std::unique_lock lock(mutex);
	map.clear();
}
PLSStatsDataPtr PLSGlobalStats::globalParent(const char *parent)
{
	if (pls_is_empty(parent))
		return nullptr;
	std::string key = parent;
	auto &[mutex, list] = get_value(PLSGlobalStats::globalStats(), key);
	std::lock_guard lock(mutex);
	if (!list.empty())
		return list.front()->data.lock();
	return nullptr;
}
void PLSGlobalStats::globalStart(const char *id, const char *file, std::int32_t line, const char *parent, const char *comment)
{
	std::string key = id;
	auto &[mutex, list] = get_value(PLSGlobalStats::globalStats(), key);
	auto stats = std::make_shared<PLSGlobalStats>(globalParent(parent), id, file, line, comment);
	std::lock_guard lock(mutex);
	list.push_back(stats);
}

void PLSGlobalStats::globalEnd(const char *id)
{
	std::string key = id;
	auto &[mutex, list] = get_value(PLSGlobalStats::globalStats(), key);
	std::lock_guard lock(mutex);
	if (!list.empty())
		list.pop_front();
}

void PLSGlobalStats::global(const std::string &id, const std::string &file, std::int32_t line, const std::string &comment, std::int64_t elapsed)
{
	auto data = PLSStatsData::statsData(-1, nullptr, id, file, line, comment);
	data->setElapsed(elapsed);
}
void PLSGlobalStats::global(const std::list<std::string> &tree, const std::string &id, const std::string &file, std::int32_t line, const std::string &comment, std::int64_t elapsed)
{
	PLSStatsDataPtr parent = nullptr;
	for (const auto &name : tree) {
		parent = PLSStatsData::statsData(-1, parent, name, "tree-group", -1000, comment);
	}

	auto data = PLSStatsData::statsData(-1, parent, id, file, line, comment);
	data->setElapsed(elapsed);
}

LIBUTILSAPI_API void clear()
{
	PLSStatsData::clear();
	PLSStats::clear();
	PLSGlobalStats::clear();
}

}
}
