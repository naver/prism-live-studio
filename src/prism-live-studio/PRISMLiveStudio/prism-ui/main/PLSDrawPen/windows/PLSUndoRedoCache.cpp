#include "PLSUndoRedoCache.h"
#include "frontend-api.h"
#include <functional> // std::hash
#include <assert.h>
#include <memory>
#include <QDir>
#include <QFile>

inline size_t GenerateHash(const std::string &input)
{
	std::hash<std::string> hasher;
	return hasher(input);
}

size_t PLSUndoRedoCache::GetHashValue(const std::vector<Stroke> &strokes)
{
	if (strokes.empty()) {
		assert(false);
		return 0;
	}

	std::string key;
	key.reserve(1024);

	for (const auto &item : strokes) {
		key += item.id;
		key += "|";
	}

	return GenerateHash(key);
}

PLSUndoRedoCache::PLSUndoRedoCache()
{
	if (InitCachePath())
		DeleteCacheFiles();
}

PLSUndoRedoCache::~PLSUndoRedoCache()
{
	DeleteCacheFiles();
}

void PLSUndoRedoCache::ClearCache()
{
	DeleteCacheFiles();

	CAutoLockCS lock(lockCache);
	cacheImage.clear();
	cacheKeys.clear();
}

bool PLSUndoRedoCache::NeedInsertCache(size_t hash)
{
	if (cacheDir.isEmpty() || !hash)
		return false;

	if (HashExist(hash))
		return false;

	return true;
}

void PLSUndoRedoCache::InsertCache(size_t hash, uint32_t linesize, uint32_t height, std::shared_ptr<uint8_t> data)
{
	if (cacheDir.isEmpty())
		return;

	if (!data || !linesize || !height || !hash) {
		assert(false);
		return;
	}

	QDir dir;
	if (!dir.mkpath(cacheDir)) {
		assert(false);
		return;
	}

	if (HashExist(hash))
		return;

	static int index = 0;
	auto strDir = cacheDir.toStdWString();
	wchar_t path[MAX_PATH];
	swprintf_s(path, MAX_PATH, L"%s%d-%zu.cache", (wchar_t *)strDir.c_str(), ++index, hash);

	FILE *fp = _wfopen(path, L"wb+");
	if (!fp) {
		assert(false);
		return;
	}

	auto length = linesize * height;
	auto isOk = fwrite(data.get(), 1, length, fp) == length;
	fclose(fp);

	if (!isOk) {
		assert(false);
		return;
	}

	CacheInfo ci;
	ci.length = length;
	ci.linesize = linesize;
	ci.file = path;

	CAutoLockCS lock(lockCache);
	cacheImage[hash] = ci;
	cacheKeys.push_back(hash);

	CheckCacheCount();
}

std::shared_ptr<uint8_t> PLSUndoRedoCache::ReadCache(size_t hash, uint32_t &linesize, uint32_t &length)
{
	if (!hash)
		return nullptr;

	linesize = length = 0;

	CacheInfo info;
	{
		CAutoLockCS lock(lockCache);
		auto itr = cacheImage.find(hash);
		if (itr == cacheImage.end())
			return nullptr;

		info = itr->second;
	}

	if (!info.length || !info.linesize || info.file.empty()) {
		RemoveCache(hash);
		assert(false);
		return nullptr;
	}

	auto data = std::shared_ptr<uint8_t>(new (std::nothrow) uint8_t[info.length], std::default_delete<uint8_t[]>());
	if (!data)
		return nullptr;

	FILE *fp = _wfopen(info.file.c_str(), L"rb+");
	if (!fp) {
		RemoveCache(hash);
		return nullptr;
	}

	auto isOk = fread(data.get(), 1, info.length, fp) == info.length;
	fclose(fp);

	if (!isOk) {
		RemoveCache(hash);
		assert(false);
		return nullptr;
	}

	linesize = info.linesize;
	length = info.length;
	return data;
}

bool PLSUndoRedoCache::InitCachePath()
{
	cacheDir = pls_get_user_path("PRISMLiveStudio\\drawpen\\");
	if (cacheDir.isEmpty()) {
		assert(false);
		return false;
	}

	QDir dir;
	if (!dir.mkpath(cacheDir)) {
		cacheDir.clear();
		assert(false);
		return false;
	}

	return true;
}

void PLSUndoRedoCache::DeleteCacheFiles()
{
	if (cacheDir.isEmpty())
		return;

	QDir dir(cacheDir);
	if (!dir.exists())
		return;

	QFileInfoList files = dir.entryInfoList(QDir::Files);
	for (const QFileInfo &item : files) {
		QString filePath = item.absoluteFilePath();
		QFile file(filePath);
		if (file.remove()) {
			continue; // deleted
		} else {
			assert(false);
			auto errorString = file.errorString().toStdString();
			auto name = file.fileName().toStdString();
			blog(LOG_WARNING, "failed to delete cache file '%s', error=%d, desc=%s", name.c_str(), (int)file.error(), errorString.c_str());
		}
	}
}

void PLSUndoRedoCache::RemoveCache(size_t hash)
{
	CAutoLockCS lock(lockCache);

	auto itr = cacheImage.find(hash);
	if (itr != cacheImage.end()) {
		DeleteFileW(itr->second.file.c_str());
		cacheImage.erase(itr);
	}

	auto it = find(cacheKeys.begin(), cacheKeys.end(), hash);
	if (it != cacheKeys.end())
		cacheKeys.erase(it);
}

void PLSUndoRedoCache::CheckCacheCount()
{
	CAutoLockCS lock(lockCache);

	if (cacheImage.size() <= MAX_CACHE_COUNT)
		return;

	auto itr = cacheKeys.begin();
	while (itr != cacheKeys.end() && cacheImage.size() > MAX_CACHE_COUNT) {
		auto temp = cacheImage.find(*itr);
		if (temp != cacheImage.end()) {
			DeleteFileW(temp->second.file.c_str());
			cacheImage.erase(temp);
		}

		itr = cacheKeys.erase(itr);
	}
}

bool PLSUndoRedoCache::HashExist(size_t hash)
{
	CAutoLockCS lock(lockCache);

	auto itr = cacheImage.find(hash);
	if (itr == cacheImage.end())
		return false; // never find

	if (!itr->second.length || !itr->second.linesize || itr->second.file.empty())
		return false;

	QString file = QString::fromStdWString(itr->second.file);
	if (!QFile::exists(file))
		return false;

	return true;
}
