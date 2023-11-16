#ifndef PLSAPIFacebook_H
#define PLSAPIFacebook_H

#include <QObject>
#include "libhttp-client.h"

enum class PLSAPIFacebookType {
	PLSFacebookSuccess,
	PLSFacebookFailed,
	PLSFacebookNotObject,
	PLSFacebookDeclined,
	PLSFacebookGranted,
	PLSFacebookInvalidAccessToken,
	PLSFacebookObjectDontExist,
	PLSRequestPermissionReject,
	PLSLivingPermissionReject,
	PLSUpdateLivingPermissionReject,
	PLSUpdateLiveInfoFailed,
	PLSFacebookNetworkError,
};

constexpr auto timeline_living_permission = "publish_video";
constexpr auto group_living_permission = "publish_to_groups";
constexpr auto pages_manage_posts_permission = "pages_manage_posts";
constexpr auto pages_read_engagement_permission = "pages_read_engagement";
constexpr auto pages_read_user_content_permission = "pages_read_user_content";

#define TimelineObjectFlags tr("facebook.channel.type.timeline")
#define GroupObjectFlags tr("facebook.channel.type.group")
#define PageObjectFlags tr("facebook.channel.type.page")

#define TimelinePublicName tr("facebook.privacy.type.all")
#define TimelineFriendName tr("facebook.privacy.type.all.friends")
#define TimelineOnlymeName tr("facebook.privacy.type.self")

extern const QString TimelinePublicId;
extern const QString TimelineFriendId;
extern const QString TimelineOnlymeId;

struct FacebookGroupInfo {
	QString groupId;
	QString groupName;
	QString groupCover;

	FacebookGroupInfo() = default;
	explicit FacebookGroupInfo(const QJsonObject &object);
};

struct FacebookPageInfo {
	QString pageId;
	QString pageName;
	QString pageAccessToken;

	FacebookPageInfo() = default;
	explicit FacebookPageInfo(const QJsonObject &object);
};

struct FacebookGameInfo {
	QString gameId;
	QString gameName;

	FacebookGameInfo() = default;
	explicit FacebookGameInfo(const QJsonObject &object);
};

struct FacebookPrivacyInfo {
	QString privacyId;
	QString privacyName;
};

using MyRequestTypeFunction = std::function<void(PLSAPIFacebookType type)>;
using GetLongAccessTokenCallback = std::function<void(PLSAPIFacebookType type, const QString &accessToken)>;
using GetUserInfoCallback = std::function<void(PLSAPIFacebookType type, const QString &username, const QString &imagePath, const QString &userId)>;
using GetMyGroupListCallback = std::function<void(PLSAPIFacebookType type, const QList<FacebookGroupInfo> &list)>;
using GetMyPageListCallback = std::function<void(PLSAPIFacebookType type, const QList<FacebookPageInfo> &list)>;
using GetMyGameListCallback = std::function<void(PLSAPIFacebookType type, const QList<FacebookGameInfo> &list)>;
using GetLiveVideoTitleDesCallback = std::function<void(PLSAPIFacebookType type, const QString &title, const QString &description)>;
using GetLiveVideoStatisticCallback = std::function<void(PLSAPIFacebookType type, const QString &status, const QString &live_views, const QString &comments, const QString &reactions)>;
using StartLivingCallback = std::function<void(PLSAPIFacebookType type, const QString &streamURL, const QString &liveId, const QString &videoId, const QString &shareLink)>;
using MyRequestSuccessFunction = std::function<void(QJsonObject root)>;
using ItemInfoRequestFunction = std::function<void(PLSAPIFacebookType type, QString username, QString profilePath)>;
using TimelinePrivacyFunction = std::function<void(PLSAPIFacebookType type, QString privacyId)>;

#define PLSFaceBookRquest PLSAPIFacebook::instance()

class PLSAPIFacebook : public QObject {
	Q_OBJECT

public:
	enum PLSAPI {
		PLSAPIGetLongLiveUserAccessToken,
		PLSAPIGetUserInfo,
		PLSAPIGetMyGroupListRequest,
		PLSAPIGetMyPageListRequest,
		PLSAPIGetMyGameListRequest,
		PLSAPICheckMyGroupListPermission,
		PLSAPICheckMyPageListPermission,
		PLSAPICheckTimelineLivingPermission,
		PLSAPICheckGroupLivingPermission,
		PLSAPICheckPageLivingPermission,
		PLSAPIStartTimelineLiving,
		PLSAPIStartGroupLiving,
		PLSAPIStartPageLiving,
		PLSAPIGetFacebookItemUserInfo,
		PLSAPIGetLiveVideoStatisticRequest,
		PLSAPIGetLiveVideoTitleDescRequest,
		PLSAPIGetLiveVideoPrivacyRequest,
		PLSAPIUpdateFacebookLiving,
		PLSAPIStopFacebookLiving,
	};
	Q_ENUM(PLSAPI)

	struct FacebookPrepareLiveInfo {
		QString title;
		QString description;
		QString firstObjectName = TimelineObjectFlags;
		QString secondObjectName = TimelinePublicName;
		QString secondObjectId = TimelinePublicId;
		QString gameName;
		QString gameId;
		QString liveId;
		QString videoId;
		QString shareLink;
		QString streamURL;
	};

	static PLSAPIFacebook *instance();
	void getLongLiveUserAccessToken(const GetLongAccessTokenCallback &onFinished);
	void getUserInfo(const GetUserInfoCallback &onFinished);
	void checkPermission(PLSAPI requestType, QStringList permission, const MyRequestTypeFunction &onFinished, QWidget *parent);
	PLSAPIFacebookType checkPermissionSuccess(const QJsonObject &root, const QStringList &permissionList, PLSAPIFacebook::PLSAPI requestType, QWidget *parent) const;
	void getMyGroupListRequestAndCheckPermission(const GetMyGroupListCallback &onFinished, QWidget *parent);
	void getMyGroupListRequest(const GetMyGroupListCallback &onFinished);
	void getMyPageListRequestAndCheckPermission(const GetMyPageListCallback &onFinished, QWidget *parent);
	void getMyPageListRequest(const GetMyPageListCallback &onFinished);
	void searchGameTagListByKeyword(const GetMyGameListCallback &onFinished, const QString &keyword);
	void startLiving(PLSAPI requestType, const QString &itemId, const QString &privacy, const QString &accessToken, const StartLivingCallback &onFinished);
	void getFacebookShareLink(const QJsonObject &root, QString &shareLink) const;
	void getFacebookItemUserInfo(const QString &itemId, const ItemInfoRequestFunction &onFinished);
	void getLiveVideoTitleDesRequest(const QString &liveVideoId, const GetLiveVideoTitleDesCallback &onFinished);
	void getTimelinePrivacyRequest(const QString &videoId, const TimelinePrivacyFunction &onFinished);
	void updateFacebookLiving(const QString &liveVideoId, const QString &privacy, const MyRequestTypeFunction &onFinished);
	void stopFacebookLiving(const QString &liveVideoId, const MyRequestTypeFunction &onFinished) const;
	void downloadSyncImage(const QString &url, QString &imagePath) const;

private:
	bool containsRequestPermissionList(const QString &url, const QStringList &requestPermissionList) const;
	static QString getFaceboolURL(const QString &endpoint);
	QUrl getPermissionRequestUrl(const QString &permission) const;
	bool goFacebookRequestPermission(const QStringList &permissionList, QWidget *parent) const;
	void startRequestApi(PLSAPI requestType, const pls::http::Request &request, const MyRequestSuccessFunction &successFunction, const MyRequestTypeFunction &failedFunction);
	PLSAPIFacebookType handleApiErrorCode(PLSAPI requestType, int statusCode, QByteArray data, QNetworkReply::NetworkError error) const;
	const char *getApiName(PLSAPI requestType) const;
	void printRequestStartLog(PLSAPI requestType, const QString &uri, const QString &log = QString()) const;
	void printRequestSuccessLog(PLSAPI requestType, const QString &log = QString()) const;
	QMap<PLSAPI, pls::http::Request> m_reply;
};

#endif // PLSAPIFacebook_H
