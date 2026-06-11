#include "PLSBgmDataManager.h"
#include "PLSBgmItemCoverView.h"
#include "PLSBgmItemView.h"
#include "PLSBgmDataManager.h"
#include <QDir>

#include "frontend-api.h"
#include "liblog.h"
#include "log/module_names.h"
#include "pls-common-define.hpp"
using namespace common;

std::vector<QString> musicFormat = {"*.mp3", "*.wav", "*.m4a", "*.ogg", "*.flac", "*.wma", "*.aac", "*.aif"};

PLSBgmDataViewManager *PLSBgmDataViewManager::Instance()
{
	static PLSBgmDataViewManager instance;
	return &instance;
}

// (*.mp3 *.aac *.ogg *.wav)  =>
// (*.mp3 *.wav *.m4a *.ogg *.flac *.wma *.aac *.aif)
bool PLSBgmDataViewManager::IsSupportFormat(const QString &url) const
{
	QString format = url.mid(url.lastIndexOf(".") + 1).toLower();

	auto iter = std::find_if(musicFormat.begin(), musicFormat.end(), [format](QString value) { return value.contains(format); });
	return iter != musicFormat.end();
}

bool PLSBgmDataViewManager::IsSupportFormat(const QList<QUrl> urls) const
{
	bool existedSupportFormat = false;
	for (auto &url : urls) {
		if (IsSupportFormat(url.path())) {
			existedSupportFormat = true;
			break;
		}
	}
	return existedSupportFormat;
}

QString PLSBgmDataViewManager::ConvertIntToTimeString(const int &seconds) const
{
	QTime currentTime((seconds / 3600) % 60, (seconds / 60) % 60, seconds % 60, (seconds * 1000) % 1000);

	QString format = "mm:ss";
	if (seconds >= 3600) {
		format = "hh:mm:ss";
	}
	return currentTime.toString(format);
}

QPushButton *PLSBgmDataViewManager::GetGroupButton(const QString &group)
{
	for (const auto &iter : groupButtonList) {
		if (0 == iter.first.compare(group)) {
			return iter.second;
		}
	}

	return nullptr;
}

void PLSBgmDataViewManager::AddGroupButton(const QString group, QPushButton *button)
{
	if (group.isEmpty() || !button) {
		return;
	}

	groupButtonList.emplace(group, button);
}

void PLSBgmDataViewManager::DeleteGroupButton()
{
	if (groupButtonList.empty()) {
		return;
	}

	for (auto iter = groupButtonList.begin(); iter != groupButtonList.end();) {
		QPushButton *item = iter->second;
		iter = groupButtonList.erase(iter);
		if (item) {
			item->deleteLater();
			item = nullptr;
		}
	}
}

PushButtonMapType PLSBgmDataViewManager::GetGroupButtonList() const
{
	return groupButtonList;
}

void PLSBgmDataViewManager::AddCachePlayList(const PLSBgmItemData &data)
{
	bool find = CachePlayListExisted(data);
	if (find) {
		return;
	}
	cachePlayList.push_front(data);
}

void PLSBgmDataViewManager::ClearCachePlayList()
{
	cachePlayList.clear();
}

void PLSBgmDataViewManager::DeleteCachePlayList(const PLSBgmItemData &data)
{
	for (auto iter = cachePlayList.begin(); iter != cachePlayList.end(); ++iter) {
		PLSBgmItemData data_ = *iter;
		if (data_.title == data.title && data_.id == data.id) {
			cachePlayList.erase(iter);
			break;
		}
	}
}

bool PLSBgmDataViewManager::CachePlayListExisted(const PLSBgmItemData &data) const
{
	bool find = false;
	for (const auto &data_ : cachePlayList) {
		if (data_.title == data.title && data_.id == data.id) {
			find = true;
			break;
		}
	}
	return find;
}

size_t PLSBgmDataViewManager::GetCachePlayListSize() const
{
	return cachePlayList.size();
}

BgmItemCacheType PLSBgmDataViewManager::GetCachePlayList() const
{
	return cachePlayList;
}

PLSBgmItemData::PLSBgmItemData(const QString &group_, const pls::rsm::Item &item) : group(group_), _item(item)
{
	title = item.attr("title").toString();
	producer = item.attr("producer").toString();
	id = (int)DurationType::FullSeconds;

	SetUrl(item.attr({"properties", "duration60SecondsMusicInfo", "url"}).toString(), (int)DurationType::SixtySeconds);
	SetDuration((int)DurationType::SixtySeconds, item.attr({"properties", "duration60SecondsMusicInfo", "duration"}).toInt());

	SetUrl(item.attr({"properties", "duration15SecondsMusicInfo", "url"}).toString(), (int)DurationType::FifteenSeconds);
	SetDuration((int)DurationType::FifteenSeconds, item.attr({"properties", "duration15SecondsMusicInfo", "duration"}).toInt());

	SetUrl(item.attr({"properties", "duration30SecondsMusicInfo", "url"}).toString(), (int)DurationType::ThirtySeconds);
	SetDuration((int)DurationType::ThirtySeconds, item.attr({"properties", "duration30SecondsMusicInfo", "duration"}).toInt());

	SetUrl(item.attr({"properties", "durationFullMusicInfo", "url"}).toString(), (int)DurationType::FullSeconds);
	SetDuration((int)DurationType::FullSeconds, item.attr({"properties", "durationFullMusicInfo", "duration"}).toInt());
}

void PLSBgmItemData::SetUrl(const QString &url, const int &id_)
{
	if (!url.isEmpty())
		urls.insert({url, id_});
	auto type = static_cast<DurationType>(id_);
	switch (type) {
	case DurationType::FifteenSeconds:
		fifteenUrl = url;
		break;
	case DurationType::ThirtySeconds:
		thirtyUrl = url;
		break;
	case DurationType::SixtySeconds:
		sixtyUrl = url;
		break;
	default:
		fullUrl = url;
		break;
	}
}

QString PLSBgmItemData::GetUrl(const int &id_) const
{
	auto type = static_cast<DurationType>(id_);
	if (type == DurationType::FifteenSeconds) {
		return fifteenUrl;
	} else if (type == DurationType::ThirtySeconds) {
		return thirtyUrl;
	} else if (type == DurationType::SixtySeconds) {
		return sixtyUrl;
	} else {
		return fullUrl;
	}
}

int PLSBgmItemData::GetDuration(const int &id_) const
{
	auto type = static_cast<DurationType>(id_);
	if (type == DurationType::FifteenSeconds) {
		return fifteenDuration;
	} else if (type == DurationType::ThirtySeconds) {
		return thirtyDuration;
	} else if (type == DurationType::SixtySeconds) {
		return sixtyDuration;
	} else {
		return fullDuration;
	}
}

void PLSBgmItemData::SetDuration(const int &id_, const int &duration)
{
	auto type = static_cast<DurationType>(id_);
	switch (type) {
	case DurationType::FifteenSeconds:
		fifteenDuration = duration;
		break;
	case DurationType::ThirtySeconds:
		thirtyDuration = duration;
		break;
	case DurationType::SixtySeconds:
		sixtyDuration = duration;
		break;
	default:
		fullDuration = duration;
		break;
	}
}

QString PLSBgmItemData::GetKey() const
{
	return title + QString::number(id);
}

QString PLSBgmItemData::ConvertIntToTimeString(const int &seconds) const
{
	QTime currentTime((seconds / 3600) % 60, (seconds / 60) % 60, seconds % 60, (seconds * 1000) % 1000);

	QString format = "mm:ss";
	if (seconds >= 3600) {
		format = "hh:mm:ss";
	}
	return currentTime.toString(format);
}
