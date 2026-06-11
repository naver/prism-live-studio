#ifndef PLSBACKGROUNDMUSICDATAMGR_H
#define PLSBACKGROUNDMUSICDATAMGR_H

#include <QByteArray>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <map>
#include <set>
#include <vector>

#include "obs.hpp"
#include "libresource.h"

enum class DurationType { FifteenSeconds, ThirtySeconds, SixtySeconds, FullSeconds, Unknown };

enum class CustomDataRole { DataRole = Qt::UserRole, NeedPaintRole, MediaStatusRole, DropIndicatorRole, CoverPathRole, RowStatusRole };
Q_DECLARE_METATYPE(CustomDataRole)

enum class MediaStatus { stateNormal, stateLoading, statePlaying, statePause, stateInvalid };
Q_DECLARE_METATYPE(MediaStatus)

enum class RowStatus { stateNormal, stateHover, stateSelected };
Q_DECLARE_METATYPE(RowStatus)

enum class ButtonState { Normal, Hover, Pressed, Disabled };

enum class DropIndicator { None, Top, Bottom };
Q_DECLARE_METATYPE(DropIndicator)

class PLSBgmItemView;
class PLSBgmItemCoverView;
class PLSBgmItemData;

using PushButtonMapType = std::map<QString, QPointer<QPushButton>>; //key: group
using BgmItemMapType = std::map<QString, QPointer<PLSBgmItemView>>; //key: title

using BgmItemCoverType = std::vector<std::pair<quint64, QPointer<PLSBgmItemCoverView>>>; //key: sceneitem ptr
using BgmItemVectorType = std::vector<QPointer<PLSBgmItemView>>;
using BgmItemCacheType = QVector<PLSBgmItemData>;

// music library data struct
using BgmLibraryData = std::map<QString, PLSBgmItemData>;

class PLSBgmItemData {
	pls::rsm::Item _item;

public:
	PLSBgmItemData(const QString &group, const pls::rsm::Item &item);
	PLSBgmItemData() = default;
	~PLSBgmItemData() = default;

	void SetUrl(const QString &url, const int &id);
	QString GetUrl(const int &id) const;
	int GetDuration(const int &id) const;
	void SetDuration(const int &id, const int &duration);
	QString GetKey() const;
	QString ConvertIntToTimeString(const int &seconds) const;

	QString title{};
	QString producer{};
	QString group{};

	QString fullUrl{};
	QString fifteenUrl{};
	QString thirtyUrl{};
	QString sixtyUrl{};
	int fullDuration{0};
	int fifteenDuration{0};
	int thirtyDuration{0};
	int sixtyDuration{0};
	int id{-1};
	bool isLocalFile{false};
	bool isChecked{false};
	bool isCurrent{false};
	bool isDisable{false};
	bool haveCover{false};
	std::map<QString, int> urls;
	QString coverPath;
	MediaStatus mediaStatus{MediaStatus::stateNormal};
	DropIndicator dropIndicator{DropIndicator::None};
	RowStatus rowStatus{RowStatus::stateNormal};
};
Q_DECLARE_METATYPE(PLSBgmItemData)

class PLSBgmDataViewManager {
public:
	static PLSBgmDataViewManager *Instance();
	bool IsSupportFormat(const QString &url) const;
	bool IsSupportFormat(const QList<QUrl> urls) const;
	QString ConvertIntToTimeString(const int &seconds) const;

	//group button
	QPushButton *GetGroupButton(const QString &group);
	void AddGroupButton(const QString group, QPushButton *button);
	void DeleteGroupButton();
	PushButtonMapType GetGroupButtonList() const;

	// cache play list
	void AddCachePlayList(const PLSBgmItemData &data);
	void ClearCachePlayList();
	void DeleteCachePlayList(const PLSBgmItemData &data);
	bool CachePlayListExisted(const PLSBgmItemData &data) const;
	size_t GetCachePlayListSize() const;
	BgmItemCacheType GetCachePlayList() const;

private:
	PLSBgmDataViewManager() = default;
	~PLSBgmDataViewManager() = default;

	PushButtonMapType groupButtonList{};
	BgmItemCacheType cachePlayList{}; //cache playlist when user clicked add button
};

#endif // PLSBACKGROUNDMUSICDATAMGR_H
