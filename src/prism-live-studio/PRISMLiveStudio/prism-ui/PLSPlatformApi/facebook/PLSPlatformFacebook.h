/*
* @file		PLSPlatformFacebook.h
* @brief	All facebook relevant api is implemented in this file
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include "../PLSPlatformBase.hpp"
#include "PLSAPIFacebook.h"

constexpr auto MODULE_PLATFORM_FACEBOOK = "Platform/Facebook";

#define GROUP_COMBOX_DEFAULT_TEXT tr("facebook.liveinfo.Select.Group")
#define PAGE_COMBOX_DEFAULT_TEXT tr("facebook.liveinfo.Select.Page")

extern const QString FacebookPrivacyItemType;
extern const QString FacebookTimelineItemId;
extern const QString FacebookGroupItemType;
extern const QString FacebookPageItemType;
extern const QString FacebookGameItemType;

class PLSLiveInfoFacebook;

class PLSPlatformFacebook : public PLSPlatformBase {

	Q_OBJECT

public:
	PLSPlatformFacebook();
	void setSrcInfo(const QVariantMap &srcInfo);
	const QVariantMap &getSrcInfo() const;
	void insertSrcInfo(const QString &key, const QVariant &value);
	QString getAccessToken() const;
	QString getLiveAccessToken() const;
	QString getGameId(const QString &gameName) const;
	const QStringList &getShareObjectList() const;
	QList<QString> getItemNameList(const QString &itemType) const;
	QList<QString> getItemIdList(const QString &itemType) const;
	void getItemInfo(const QString &itemType, const int &index, QString &name, QString &id) const;
	QString getItemName(const QString &itemId, const QString &itemType) const;
	PLSServiceType getServiceType() const override;
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	void onPrepareLive(bool value) override;
	void onAlLiveStarted(bool) override;
	void onLiveEnded() override;
	void getLongLivedUserAccessToken(const MyRequestTypeFunction &onFinished);
	void getUserInfo(const MyRequestTypeFunction &onFinished);
	void getUserInfoSuccess(const QString &userId, const QString &username, const QString &imagePath);
	void getMyGroupListRequestAndCheckPermission(const MyRequestTypeFunction &onFinished, QWidget *parent);
	void getMyPageListRequestAndCheckPermission(const MyRequestTypeFunction &onFinished, QWidget *parent);
	void getGameTagListByKeyword(const MyRequestTypeFunction &onFinished, const QString &keyword);
	void startLiving(const MyRequestTypeFunction &onFinished);
	void updateLiving(const MyRequestTypeFunction &onFinished);
	void requestItemInfoRequest(const MyRequestTypeFunction &onFinished);
	void getFacebookItemUserInfoFinished(PLSAPIFacebookType type, const QString &nickname, const QString &profilePath, const QString &shareObjectName);
	void updateChannelInfo(const QString &key, const QVariant &value, bool refresh) const;
	void updateChannelInfos(const QVariantMap &channelInfos, bool refresh) const;
	void setPrepareInfo(const PLSAPIFacebook::FacebookPrepareLiveInfo &prepareInfo);
	const PLSAPIFacebook::FacebookPrepareLiveInfo &getPrepareInfo() const;
	void resetLiveInfo();
	void setStreamUrlAndStreamKey();
	void getLivingVideoTitleDescRequest(const MyRequestTypeFunction &onFinished);
	void insertParentPointer(PLSLiveInfoFacebook *parent);

	const PLSLiveInfoFacebook *getParentPointer() const;
	QJsonObject getWebChatParams() override;
	QJsonObject getLiveStartParams() override;
	bool isPrivateChat() const;
	void getTimelinePrivacyRequest(const MyRequestTypeFunction &onFinished);
	void getLivingTimelinePrivacyRequest(const TimelinePrivacyFunction &onFinished);
	void currentPrivacyInconsistentDisplay(const QString &privacyId);
	QString getServiceLiveLink() override;
	void setPrivacyToTimeline();

signals:
	void privateChatChanged(bool oldPrivate, bool newPrivate);

private:
	QString getTimelinePrivacy() const;
	void clearInfoData();

	QStringList m_shareObjectList{TimelineObjectFlags, GroupObjectFlags, PageObjectFlags};
	QList<FacebookPrivacyInfo> m_timelinePrivacyList;
	QList<FacebookGroupInfo> m_groupList;
	QList<FacebookPageInfo> m_pageList;
	QList<FacebookGameInfo> m_gameList;
	QVariantMap m_srcInfo;
	QPointer<PLSLiveInfoFacebook> m_parent;
	QString m_timelineNickname;
	QString m_timelineProfilePath;
	QString m_oldTimelinePrivacy;
	QString m_userId;
	PLSAPIFacebook::FacebookPrepareLiveInfo m_prepareInfo;
};
