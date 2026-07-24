#pragma once
#include "PLSDrawPenStroke.h"
#include "obs.h"
#include <QString>
#include <unordered_map>

#define MAX_CACHE_COUNT 128 // 1080P * 128 = 1GB

struct CacheWriteInfo {
	// output
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t linesize = 0;
	std::shared_ptr<uint8_t> data = nullptr;
	bool result = false;
};

struct CacheReadInfo {
	// intput
	gs_texture_t *target = nullptr;
	std::shared_ptr<uint8_t> data = nullptr;
	uint32_t linesize = 0;

	// output
	bool result = false;
};

class PLSUndoRedoCache {
public:
	static size_t GetHashValue(const std::vector<Stroke> &strokes);

	PLSUndoRedoCache();
	~PLSUndoRedoCache();

	void ClearCache();
	bool NeedInsertCache(size_t hash);
	void InsertCache(size_t hash, uint32_t linesize, uint32_t height, std::shared_ptr<uint8_t> data);
	std::shared_ptr<uint8_t> ReadCache(size_t hash, uint32_t &linesize, uint32_t &length);
	void RemoveCache(size_t hash);

private:
	bool InitCachePath();
	void DeleteCacheFiles();
	void CheckCacheCount();
	bool HashExist(size_t hash);

private:
	struct CacheInfo {
		std::wstring file = L"";
		uint32_t linesize = 0;
		uint32_t length = 0;
	};

	QString cacheDir;

	CCSection lockCache;
	std::vector<size_t> cacheKeys;
	std::unordered_map<size_t, CacheInfo> cacheImage; // key: hash
};
