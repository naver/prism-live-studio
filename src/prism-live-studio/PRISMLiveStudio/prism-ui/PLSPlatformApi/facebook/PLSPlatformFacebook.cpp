#include "PLSPlatformFacebook.h"
#include <QVariantMap>
#include "PLSLiveInfoFacebook.h"
#include "PLSPlatformApi.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../PLSLiveInfoDialogs.h"
#include "PLSAlertView.h"
#include "prism/PLSPlatformPrism.h"
#include "log/log.h"


constexpr auto facebookMoudule = "PLSLiveInfoFacebook";

const QString FacebookPrivacyItemType = "FacebookPrivacyItemType";
const QString FacebookTimelineItemId = "me";
const QString FacebookGroupItemType = "FacebookGroupItemType";
const QString FacebookPageItemType = "FacebookPageItemType";
const QString FacebookGameItemType = "FacebookGameItemType";
const QString REMOVE_DASHBOARD_KEY = "REMOVE_DASHBOARD_KEY";

PLSPlatformFacebook::PLSPlatformFacebook() : PLSPlatformBase()
{
	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEndedForUi, this, [this]() { resetLiveInfo(); }, Qt::DirectConnection);
	QList<QString> privacyNames;
	privacyNames.append(TimelinePublicName);
	privacyNames.append(TimelineFriendName);
	privacyNames.append(TimelineOnlymeName);
	QList<QString> privacyIds;
	privacyIds.append(TimelinePublicId);
	privacyIds.append(TimelineFriendId);
	privacyIds.append(TimelineOnlymeId);
	for (int i = 0; i < privacyNames.size(); i++) {
		FacebookPrivacyInfo info;
		info.privacyName = privacyNames.at(i);
		info.privacyId = privacyIds.at(i);
		m_timelinePrivacyList.append(info);
	}
}

void PLSPlatformFacebook::setSrcInfo(const QVariantMap &srcInfo)
{
	m_srcInfo = srcInfo;
	clearInfoData();
}

const QVariantMap &PLSPlatformFacebook::getSrcInfo() const
{
	return m_srcInfo;
}

void PLSPlatformFacebook::insertSrcInfo(const QString &key, const QVariant &value)
{
	m_srcInfo[key] = value;
}

QString PLSPlatformFacebook::getTimelinePrivacy() const
{
	if (QString shareObjectName = m_prepareInfo.firstObjectName; shareObjectName == TimelineObjectFlags) {
		return m_prepareInfo.secondObjectId;
	}
	return QString();
}

QString PLSPlatformFacebook::getAccessToken() const
{
	auto accessToken = m_srcInfo.value(ChannelData::g_channelToken).toString();
	return accessToken;
}

QString PLSPlatformFacebook::getLiveAccessToken() const
{
	QString shareObjectName = m_prepareInfo.firstObjectName;
	QString accessToken = getAccessToken();
	QString itemId = m_prepareInfo.secondObjectId;
	if (shareObjectName == PageObjectFlags) {
		for (const FacebookPageInfo &info : m_pageList) {
			if (info.pageId == itemId) {
				accessToken = info.pageAccessToken;
			}
		}
	}
	return accessToken;
}

QString PLSPlatformFacebook::getGameId(const QString &gameName) const
{
	for (const FacebookGameInfo &info : m_gameList) {
		if (info.gameName == gameName) {
			return info.gameId;
		}
	}
	return QString();
}

const QStringList &PLSPlatformFacebook::getShareObjectList() const
{
	return m_shareObjectList;
}

QList<QString> PLSPlatformFacebook::getItemNameList(const QString &itemType) const
{
	QList<QString> name_list;
	if (itemType == FacebookGameItemType) {
		for (const auto &info : m_gameList) {
			name_list.append(info.gameName);
		}
	} else if (itemType == FacebookGroupItemType) {
		for (const auto &info : m_groupList) {
			name_list.append(info.groupName);
		}
	} else if (itemType == FacebookPageItemType) {
		for (const auto &info : m_pageList) {
			name_list.append(info.pageName);
		}
	} else if (itemType == FacebookPrivacyItemType) {
		for (const auto &info : m_timelinePrivacyList) {
			name_list.append(info.privacyName);
		}
	}
	return name_list;
}

QList<QString> PLSPlatformFacebook::getItemIdList(const QString &itemType) const
{
	QList<QString> id_list;
	if (itemType == FacebookGameItemType) {
		for (const auto &info : m_gameList) {
			id_list.append(info.gameId);
		}
	} else if (itemType == FacebookGroupItemType) {
		for (const auto &info : m_groupList) {
			id_list.append(info.groupId);
		}
	} else if (itemType == FacebookPageItemType) {
		for (const auto &info : m_pageList) {
			id_list.append(info.pageId);
		}
	} else if (itemType == FacebookPrivacyItemType) {
		for (const auto &info : m_timelinePrivacyList) {
			id_list.append(info.privacyId);
		}
	}
	return id_list;
}

void PLSPlatformFacebook::getItemInfo(const QString &itemType, const int &index, QString &name, QString &id) const
{
	if (itemType == FacebookGameItemType) {
		FacebookGameInfo info = m_gameList.at(index);
		name = info.gameName;
		id = info.gameId;
	} else if (itemType == FacebookGroupItemType) {
		FacebookGroupInfo info = m_groupList.at(index);
		name = info.groupName;
		id = info.groupId;
	} else if (itemType == FacebookPageItemType) {
		FacebookPageInfo info = m_pageList.at(index);
		name = info.pageName;
		id = info.pageId;
	} else if (itemType == FacebookPrivacyItemType) {
		FacebookPrivacyInfo info = m_timelinePrivacyList.at(index);
		name = info.privacyName;
		id = info.privacyId;
	}
}

QString PLSPlatformFacebook::getItemName(const QString &itemId, const QString &itemType) const
{
	if (itemType == FacebookGameItemType) {
		for (const auto &info : m_gameList) {
			if (info.gameId == itemId) {
				return info.gameName;
			}
		}
	} else if (itemType == FacebookGroupItemType) {
		for (const auto &info : m_groupList) {
			if (info.groupId == itemId) {
				return info.groupName;
			}
		}
	} else if (itemType == FacebookPageItemType) {
		for (const auto &info : m_pageList) {
			if (info.pageId == itemId) {
				return info.pageName;
			}
		}
	} else if (itemType == FacebookPrivacyItemType) {
		for (const auto &info : m_timelinePrivacyList) {
			if (info.privacyId == itemId) {
				return info.privacyName;
			}
		}
	}
	return QString();
}

PLSServiceType PLSPlatformFacebook::getServiceType() const
{
	return PLSServiceType::ST_FACEBOOK;
}

bool PLSPlatformFacebook::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}

void PLSPlatformFacebook::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(facebookMoudule, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_facebook(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(facebookMoudule, "%s %s liveinfo closed value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));

	prepareLiveCallback(value);
}

void PLSPlatformFacebook::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}

	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	latInfo.insert(ChannelData::g_shareUrl, m_prepareInfo.shareLink);
	latInfo.insert(ChannelData::g_viewers, "0");
	latInfo.insert(ChannelData::g_likes, "0");
	latInfo.insert(ChannelData::g_comments, "0");
	PLSCHANNELS_API->setChannelInfos(latInfo, true);
	PLSCHANNELS_API->setChannelInfos(latInfo, true);
}

void PLSPlatformFacebook::onLiveEnded()
{
	auto callBack = [this](PLSAPIFacebookType) { liveEndedCallback(); };
	PLSFaceBookRquest->stopFacebookLiving(m_prepareInfo.liveId, callBack);
}

void PLSPlatformFacebook::getLongLivedUserAccessToken(const MyRequestTypeFunction &onFinished)
{
	auto callBack = [onFinished, this](PLSAPIFacebookType type, const QString &accessToken) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			insertSrcInfo(ChannelData::g_channelToken, accessToken);
			getUserInfo(onFinished);
			return;
		}
		if (type == PLSAPIFacebookType::PLSFacebookInvalidAccessToken) {
			insertSrcInfo(ChannelData::g_channelSreLoginFailed , QStringLiteral("facebook get long access token api request invalid access token"));
			insertSrcInfo(ChannelData::g_channelStatus, ChannelData::ChannelStatus::Expired);
			onFinished(type);
			return;
		}
		insertSrcInfo(ChannelData::g_channelStatus, ChannelData::ChannelStatus::Error);
		if (type == PLSAPIFacebookType::PLSFacebookNetworkError) {
			insertSrcInfo(ChannelData::g_channelSreLoginFailed, QStringLiteral("facebook get long access token api network error failed"));
			insertSrcInfo(ChannelData::g_errorType, ChannelData::NetWorkErrorType::NetWorkNoStable);
			onFinished(type);
			return;
		}
		insertSrcInfo(ChannelData::g_channelSreLoginFailed, QStringLiteral("facebook get long access token api unkown reason failed"));
		insertSrcInfo(ChannelData::g_errorType, ChannelData::NetWorkErrorType::UnknownError);
		onFinished(type);
	};
	PLSFaceBookRquest->getLongLiveUserAccessToken(callBack);
}

void PLSPlatformFacebook::getUserInfo(const MyRequestTypeFunction &onFinished)
{
	auto callBack = [onFinished, this](PLSAPIFacebookType type, const QString &username, const QString &imagePath, const QString &userId) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			getUserInfoSuccess(userId, username, imagePath);
			onFinished(type);
			return;
		}
		if (type == PLSAPIFacebookType::PLSFacebookInvalidAccessToken) {
			insertSrcInfo(ChannelData::g_channelStatus, ChannelData::ChannelStatus::Expired);
			insertSrcInfo(ChannelData::g_channelSreLoginFailed, QStringLiteral("facebook get user info api request token expired failed"));
			onFinished(type);
			return;
		}
		insertSrcInfo(ChannelData::g_channelStatus, ChannelData::ChannelStatus::Error);
		if (type == PLSAPIFacebookType::PLSFacebookNetworkError) {
			insertSrcInfo(ChannelData::g_channelSreLoginFailed, QStringLiteral("facebook get user info api request network error failed"));
			insertSrcInfo(ChannelData::g_errorType, ChannelData::NetWorkErrorType::NetWorkNoStable);
			onFinished(type);
			return;
		}
		insertSrcInfo(ChannelData::g_channelSreLoginFailed, QStringLiteral("facebook get user info api request unkown error failed"));
		insertSrcInfo(ChannelData::g_errorType, ChannelData::NetWorkErrorType::UnknownError);
		onFinished(type);
	};
	PLSFaceBookRquest->getUserInfo(callBack);
}

void PLSPlatformFacebook::getUserInfoSuccess(const QString &userId, const QString &username, const QString &imagePath)
{
	m_userId = userId;
	m_timelineNickname = username;
	insertSrcInfo(ChannelData::g_nickName, username);
	insertSrcInfo(ChannelData::g_displayLine1, username);
	insertSrcInfo(ChannelData::g_channelStatus, ChannelData::ChannelStatus::Valid);
	if (imagePath.length() > 0) {
		insertSrcInfo(ChannelData::g_userIconCachePath, imagePath);
		m_timelineProfilePath = imagePath;
	}
	QString privacyName = TimelinePublicName;
	insertSrcInfo(ChannelData::g_catogry, privacyName);
	insertSrcInfo(ChannelData::g_displayLine2, privacyName);
	insertSrcInfo(ChannelData::g_shareUrl, "");
	insertSrcInfo(ChannelData::g_likes, "0");
	insertSrcInfo(ChannelData::g_viewers, "0");
	insertSrcInfo(ChannelData::g_comments, "0");
}

void PLSPlatformFacebook::getMyGroupListRequestAndCheckPermission(const MyRequestTypeFunction &onFinished, QWidget *parent)
{
	auto callback = [onFinished, this](PLSAPIFacebookType type, const QList<FacebookGroupInfo> &list) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_groupList = list;
		}
		onFinished(type);
	};
	PLSFaceBookRquest->getMyGroupListRequestAndCheckPermission(callback, parent);
}

void PLSPlatformFacebook::getMyPageListRequestAndCheckPermission(const MyRequestTypeFunction &onFinished, QWidget *parent)
{
	auto callback = [onFinished, this](PLSAPIFacebookType type, const QList<FacebookPageInfo> &list) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_pageList = list;
		}
		onFinished(type);
	};
	PLSFaceBookRquest->getMyPageListRequestAndCheckPermission(callback, parent);
}

void PLSPlatformFacebook::getGameTagListByKeyword(const MyRequestTypeFunction &onFinished, const QString &keyword)
{
	auto callback = [onFinished, this](PLSAPIFacebookType type, const QList<FacebookGameInfo> &list) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_gameList = list;
		}
		onFinished(type);
	};
	PLSFaceBookRquest->searchGameTagListByKeyword(callback, keyword);
}

void PLSPlatformFacebook::startLiving(const MyRequestTypeFunction &onFinished)
{
	QString title = m_prepareInfo.title;
	m_oldTimelinePrivacy = getTimelinePrivacy();
	PLSPlatformBase::setTitle(title.toStdString());
	QString shareObjectName = m_prepareInfo.firstObjectName;
	QString accessToken = getAccessToken();
	QString itemId = m_prepareInfo.secondObjectId;
	PLSAPIFacebook::PLSAPI apiType = PLSAPIFacebook::PLSAPIStartTimelineLiving;
	if (shareObjectName == TimelineObjectFlags) {
		itemId = FacebookTimelineItemId;
		apiType = PLSAPIFacebook::PLSAPIStartTimelineLiving;
	} else if (shareObjectName == GroupObjectFlags) {
		apiType = PLSAPIFacebook::PLSAPIStartGroupLiving;
	} else if (shareObjectName == PageObjectFlags) {
		for (const FacebookPageInfo &info : m_pageList) {
			if (info.pageId == itemId) {
				accessToken = info.pageAccessToken;
			}
		}
		apiType = PLSAPIFacebook::PLSAPIStartPageLiving;
	}

	auto livingFinished = [this, onFinished, shareObjectName](PLSAPIFacebookType type, const QString &streamURL, const QString &liveId, const QString &videoId, const QString &shareLink) {
		if (type != PLSAPIFacebookType::PLSFacebookSuccess) {
			PLS_LOGEX(PLS_LOG_ERROR, facebookMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "facebook create live api failed"}},
				  "facebook start live failed");
			onFinished(type);
			return;
		}
		auto timeLinePrivacyFunction = [onFinished](PLSAPIFacebookType type) {
			if (type != PLSAPIFacebookType::PLSFacebookSuccess) {
				PLS_LOGEX(PLS_LOG_ERROR, facebookMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "facebook get timeline privacy api request failed"}},
					  "facebook start live failed");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, facebookMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Success"}}, "facebook start live success");
			}
			onFinished(type);
		};
		auto itemInfoFunction = [onFinished](PLSAPIFacebookType type) {
			if (type != PLSAPIFacebookType::PLSFacebookSuccess) {
				PLS_LOGEX(PLS_LOG_ERROR, facebookMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Failed"}, {"startLiveFailed", "facebook get group/page/timeline info api request failed"}},
					  "facebook start live failed");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, facebookMoudule, {{"platformName", "facebook"}, {"startLiveStatus", "Success"}}, "facebook start live success");
			}
			onFinished(type);
		};
		m_prepareInfo.streamURL = streamURL;
		m_prepareInfo.liveId = liveId;
		m_prepareInfo.videoId = videoId;
		m_prepareInfo.shareLink = shareLink;
		setStreamUrlAndStreamKey();
		if (shareObjectName == TimelineObjectFlags) {
			getTimelinePrivacyRequest(timeLinePrivacyFunction);
			return;
		}
		requestItemInfoRequest(itemInfoFunction);
	};
	PLSFaceBookRquest->startLiving(apiType, itemId, getTimelinePrivacy(), accessToken, livingFinished);
}

void PLSPlatformFacebook::updateLiving(const MyRequestTypeFunction &onFinished)
{
	QString title = m_prepareInfo.title;
	QString description = m_prepareInfo.description;
	QString gameId = m_prepareInfo.gameId;
	m_oldTimelinePrivacy = getTimelinePrivacy();
	auto updateFinished = [onFinished, this](PLSAPIFacebookType type) {
		if (type != PLSAPIFacebookType::PLSFacebookSuccess) {
			onFinished(type);
			return;
		}
		if (QString shareObjectName = m_prepareInfo.firstObjectName; shareObjectName == TimelineObjectFlags) {
			getTimelinePrivacyRequest(onFinished);
			return;
		}
		requestItemInfoRequest(onFinished);
	};
	PLSFaceBookRquest->updateFacebookLiving(m_prepareInfo.liveId, getTimelinePrivacy(), updateFinished);
}

void PLSPlatformFacebook::requestItemInfoRequest(const MyRequestTypeFunction &onFinished)
{
	//get the avatar and name corresponding to the selected Timeline, group, page
	QString shareObjectName = m_prepareInfo.firstObjectName;
	QString itemId = m_prepareInfo.secondObjectId;
	if (shareObjectName == TimelineObjectFlags) {
		itemId = FacebookTimelineItemId;
	}
	auto itemInfoFinished = [onFinished, shareObjectName, this](PLSAPIFacebookType type, QString nickname, QString profilePath) {
		this->getFacebookItemUserInfoFinished(type, nickname, profilePath, shareObjectName);
		onFinished(type);
	};
	PLSFaceBookRquest->getFacebookItemUserInfo(itemId, itemInfoFinished);
}

void PLSPlatformFacebook::getFacebookItemUserInfoFinished(PLSAPIFacebookType type, const QString &nickname, const QString &profilePath, const QString &shareObjectName)
{
	QVariantMap map;
	if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
		//update dashboard
		map.insert(ChannelData::g_nickName, nickname);
		map.insert(ChannelData::g_displayLine1, nickname);
		map.insert(ChannelData::g_userIconCachePath, profilePath.length() > 0 ? profilePath : REMOVE_DASHBOARD_KEY);
		if (shareObjectName == TimelineObjectFlags) {
			m_timelineNickname = nickname;
			m_timelineProfilePath = profilePath;
		}
	}

	if (shareObjectName == TimelineObjectFlags) {
		QString privacyName = m_prepareInfo.secondObjectName;
		map.insert(ChannelData::g_catogry, privacyName);
		map.insert(ChannelData::g_displayLine2, privacyName);
		updateChannelInfos(map, true);
		return;
	}

	if (shareObjectName == GroupObjectFlags || shareObjectName == PageObjectFlags) {
		QString gameId = m_prepareInfo.gameId;
		map.insert(ChannelData::g_catogry, gameId.length() > 0 ? m_prepareInfo.gameName : "");
		map.insert(ChannelData::g_displayLine2, gameId.length() > 0 ? m_prepareInfo.gameName : "");
		updateChannelInfos(map, true);
	}
}

void PLSPlatformFacebook::updateChannelInfo(const QString &key, const QVariant &value, bool refresh) const
{
	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	latInfo.insert(key, value);
	PLSCHANNELS_API->setChannelInfos(latInfo, refresh);
}

void PLSPlatformFacebook::updateChannelInfos(const QVariantMap &channelInfos, bool refresh) const
{
	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	QVariantMap::const_iterator i;
	for (i = channelInfos.constBegin(); i != channelInfos.constEnd(); ++i) {
		if (i.value().toString() == REMOVE_DASHBOARD_KEY) {
			latInfo.remove(i.key());
			continue;
		}
		latInfo.insert(i.key(), i.value());
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, refresh);
}

void PLSPlatformFacebook::setPrepareInfo(const PLSAPIFacebook::FacebookPrepareLiveInfo &prepareInfo)
{
	m_prepareInfo = prepareInfo;
}

const PLSAPIFacebook::FacebookPrepareLiveInfo &PLSPlatformFacebook::getPrepareInfo() const
{
	return m_prepareInfo;
}

void PLSPlatformFacebook::resetLiveInfo()
{
	clearInfoData();
	QVariantMap updateInfoMap;
	updateInfoMap.insert(ChannelData::g_nickName, m_timelineNickname);
	updateInfoMap.insert(ChannelData::g_displayLine1, m_timelineNickname);
	updateInfoMap.insert(ChannelData::g_userIconCachePath, m_timelineProfilePath.length() > 0 ? m_timelineProfilePath : REMOVE_DASHBOARD_KEY);
	QString privacyName = m_prepareInfo.secondObjectName;
	updateInfoMap.insert(ChannelData::g_catogry, privacyName);
	updateInfoMap.insert(ChannelData::g_displayLine2, privacyName);
	updateInfoMap.insert(ChannelData::g_shareUrl, "");
	updateChannelInfos(updateInfoMap, false);
}

void PLSPlatformFacebook::setStreamUrlAndStreamKey()
{
	QString pushUrl = m_prepareInfo.streamURL;
	QStringList list = pushUrl.split("/");
	QString streamKey = list.last();
	list.removeLast();
	QString streamUrl = list.join("/");
	setStreamKey(streamKey.toStdString());
	setStreamServer(streamUrl.toStdString());
}

void PLSPlatformFacebook::getLivingVideoTitleDescRequest(const MyRequestTypeFunction &onFinished)
{
	auto callBack = [onFinished, this](PLSAPIFacebookType type, const QString &title, const QString &description) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_prepareInfo.title = title;
			m_prepareInfo.description = description;
		}
		onFinished(type);
	};
	PLSFaceBookRquest->getLiveVideoTitleDesRequest(m_prepareInfo.liveId, callBack);
}

void PLSPlatformFacebook::insertParentPointer(PLSLiveInfoFacebook *parent)
{
	m_parent = parent;
}

const PLSLiveInfoFacebook *PLSPlatformFacebook::getParentPointer() const
{
	return m_parent;
}

QJsonObject PLSPlatformFacebook::getWebChatParams()
{
	QJsonObject platform;
	platform["name"] = "FACEBOOK";
	platform["isPrivate"] = isPrivateChat();
	platform["userId"] = m_userId;
	return platform;
}

QJsonObject PLSPlatformFacebook::getLiveStartParams()
{
	QJsonObject platform;
	bool isPrivate = false;
	if (QString shareObjectName = m_prepareInfo.firstObjectName; shareObjectName == TimelineObjectFlags) {
		if (QString privacy = m_prepareInfo.secondObjectId; privacy == TimelinePublicId) {
			platform["privacyStatus"] = "PUBLIC";
		} else if (privacy == TimelineFriendId) {
			platform["privacyStatus"] = "FRIEND";
		} else if (privacy == TimelineOnlymeId) {
			isPrivate = true;
			platform["privacyStatus"] = "PRIVATE";
		}
		platform["target"] = "TIMELINE";
		platform.insert("accessToken", getChannelToken());
	} else if (shareObjectName == GroupObjectFlags) {
		platform["target"] = "GROUP";
		platform.insert("accessToken", getChannelToken());
	} else if (shareObjectName == PageObjectFlags) {
		platform["target"] = "PAGE";
		QString pageId = m_prepareInfo.secondObjectId;
		QString pageAccessToken;
		for (const FacebookPageInfo &info : m_pageList) {
			if (info.pageId == pageId) {
				pageAccessToken = info.pageAccessToken;
				break;
			}
		}
		platform.insert("accessToken", pageAccessToken);
	}
	platform["isPrivate"] = isPrivate;
	platform["broadcastId"] = m_prepareInfo.liveId;
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	return platform;
}

bool PLSPlatformFacebook::isPrivateChat() const
{
	bool isPrivate = false;
	if (QString shareObjectName = m_prepareInfo.firstObjectName; shareObjectName == TimelineObjectFlags) {
		QString privacy = m_prepareInfo.secondObjectId;
		if (privacy == TimelineOnlymeId || privacy == TimelineFriendId) {
			isPrivate = true;
		}
	} else if (shareObjectName == GroupObjectFlags) {
		isPrivate = true;
	}
	return isPrivate;
}

void PLSPlatformFacebook::getTimelinePrivacyRequest(const MyRequestTypeFunction &onFinished)
{
	auto privacyFinished = [onFinished, this](PLSAPIFacebookType type, QString privacyId) {
		if (QString shareObjectName = m_prepareInfo.firstObjectName; shareObjectName != TimelineObjectFlags) {
			onFinished(PLSAPIFacebookType::PLSFacebookFailed);
			return;
		}
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_prepareInfo.secondObjectId = privacyId;
			m_prepareInfo.secondObjectName = getItemName(privacyId, FacebookPrivacyItemType);
			currentPrivacyInconsistentDisplay(privacyId);
			requestItemInfoRequest(onFinished);
			return;
		}
		onFinished(type);
	};
	PLSFaceBookRquest->getTimelinePrivacyRequest(m_prepareInfo.videoId, privacyFinished);
}

void PLSPlatformFacebook::getLivingTimelinePrivacyRequest(const TimelinePrivacyFunction &onFinished)
{
	auto privacyFinished = [onFinished, this](PLSAPIFacebookType type, QString privacyId) {
		if (type == PLSAPIFacebookType::PLSFacebookSuccess) {
			m_prepareInfo.secondObjectId = privacyId;
			m_prepareInfo.secondObjectName = getItemName(privacyId, FacebookPrivacyItemType);
			currentPrivacyInconsistentDisplay(privacyId);
		}
		onFinished(type, privacyId);
	};
	PLSFaceBookRquest->getTimelinePrivacyRequest(m_prepareInfo.videoId, privacyFinished);
}

void PLSPlatformFacebook::currentPrivacyInconsistentDisplay(const QString &privacyId)
{
	PLSPlatformApi::instance()->forwardWebMessagePrivateChanged(this, isPrivateChat());
	if (m_oldTimelinePrivacy != privacyId) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("facebook.living.privacy.inconsistent.permissions"));
		m_oldTimelinePrivacy = privacyId;
	}
}

QString PLSPlatformFacebook::getServiceLiveLink()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto shareUrl = info.value(ChannelData::g_shareUrl).toString();
	return shareUrl;
}

void PLSPlatformFacebook::setPrivacyToTimeline()
{
	m_prepareInfo.firstObjectName = TimelineObjectFlags;
	m_prepareInfo.secondObjectId = TimelinePublicId;
	m_prepareInfo.secondObjectName = TimelinePublicName;
	QVariantMap updateInfoMap;
	updateInfoMap.insert(ChannelData::g_nickName, m_timelineNickname);
	updateInfoMap.insert(ChannelData::g_displayLine1, m_timelineNickname);
	if (m_timelineProfilePath.length() > 0) {
		updateInfoMap.insert(ChannelData::g_userIconCachePath, m_timelineProfilePath);
	} else {
		updateInfoMap.insert(ChannelData::g_userIconCachePath, REMOVE_DASHBOARD_KEY);
	}
	updateInfoMap.insert(ChannelData::g_catogry, m_prepareInfo.secondObjectName);
	updateInfoMap.insert(ChannelData::g_displayLine2, m_prepareInfo.secondObjectName);
	updateChannelInfos(updateInfoMap, true);
}

void PLSPlatformFacebook::clearInfoData()
{
	m_groupList.clear();
	m_pageList.clear();
	m_gameList.clear();
	m_prepareInfo = PLSAPIFacebook::FacebookPrepareLiveInfo();
}
