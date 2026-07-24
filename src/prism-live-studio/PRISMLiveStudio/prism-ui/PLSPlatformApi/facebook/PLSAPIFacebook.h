#ifndef PLSAPIFacebook_H
#define PLSAPIFacebook_H

#include <QObject>
#include "libhttp-client.h"
#include "PLSErrorHandler.h"

#if defined(Q_OS_WIN)
class QLocalServer;
#endif

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
	PLSFacebookDeclined_60Days,
	PLSFacebookDeclined_100Followers,
};

constexpr auto timeline_living_permission = "publish_video";
constexpr auto group_living_permission = "publish_to_groups";
constexpr auto pages_manage_posts_permission = "pages_manage_posts";
constexpr auto pages_read_engagement_permission = "pages_read_engagement";
constexpr auto business_management_permission = "business_management";
constexpr auto pages_read_user_content_permission = "pages_read_user_content";
constexpr auto page_show_list_permission = "pages_show_list";

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

using MyRequestTypeFunction = std::function<void(const PLSErrorHandler::RetData &retData)>;
using GetLongAccessTokenCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QString &accessToken)>;
using GetUserInfoCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QString &username, const QString &imagePath, const QString &userId)>;
using GetUserIdByTokenCallback = std::function<void(bool ok, const QString &userId)>;
using GetMyGroupListCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QList<FacebookGroupInfo> &list)>;
using GetMyPageListCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QList<FacebookPageInfo> &list)>;
using GetMyGameListCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QList<FacebookGameInfo> &list)>;
using GetLiveVideoTitleDesCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QString &title, const QString &description)>;
using GetLiveVideoStatisticCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QString &status, const QString &live_views, const QString &comments, const QString &reactions)>;
using StartLivingCallback = std::function<void(const PLSErrorHandler::RetData &retData, const QString &streamURL, const QString &liveId, const QString &videoId, const QString &shareLink)>;
using MyRequestSuccessFunction = std::function<void(QJsonObject root)>;
using ItemInfoRequestFunction = std::function<void(const PLSErrorHandler::RetData &retData, QString username, QString profilePath)>;
using TimelinePrivacyFunction = std::function<void(const PLSErrorHandler::RetData &retData, QString privacyId)>;

#define PLSFaceBookRquest PLSAPIFacebook::instance()

class PLSAPIFacebook : public QObject {
	Q_OBJECT

public:
	enum PLSAPI {
		PLSAPIGetLongLiveUserAccessToken,
		PLSAPIGetUserInfo,
		// PRISM_PC-6311: separate request-type key from PLSAPIGetUserInfo so a token-explicit
		// identity lookup (e.g. for a fresh, not-yet-associated OAuth login) never aborts, or
		// gets aborted by, an in-flight getUserInfo() call for the currently loaded channel.
		PLSAPIGetUserIdByToken,
		PLSAPIGetMyGroupListRequest,
		PLSAPIGetMyPageListRequest,
		PLSAPIGetMyGameListRequest,
		PLSAPICheckMyGroupListPermission,
		PLSAPICheckMyPageListPermission,
		PLSAPICheckTimelineLivingPermission,
		PLSAPICheckGroupLivingPermission,
		PLSAPICheckGroupGetInfoPermission,
		PLSAPICheckPageLivingPermission,
		PLSAPICheckPageGetInfoPermission,
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
	// PRISM_PC-6311: both take the access token (and, for getUserInfo(), the channel UUID for its
	// icon-cache lookup) explicitly instead of reading PLS_PLATFORM_FACEBOOK->getAccessToken()/
	// getChannelUUID(): that macro always resolves to the FIRST Facebook platform instance in
	// platformList, which is only correct by accident when at most one Facebook channel exists.
	// With multiple connected Facebook channels, a caller refreshing any channel other than the
	// first would otherwise silently exchange/fetch using the first channel's token and overwrite
	// the intended channel with the first channel's identity.
	void getLongLiveUserAccessToken(const QString &accessToken, const GetLongAccessTokenCallback &onFinished);
	void getUserInfo(const QString &accessToken, const QString &channelUUID, const GetUserInfoCallback &onFinished);
	// PRISM_PC-6311: same /me identity lookup as getUserInfo(), but takes the access token
	// explicitly instead of reading PLS_PLATFORM_FACEBOOK->getAccessToken(), so it is safe to
	// call for a fresh, not-yet-associated OAuth code without risking reading the wrong
	// channel's token.
	void getUserIdByToken(const QString &accessToken, const GetUserIdByTokenCallback &onFinished);
	// onRequestingPermission (optional): invoked exactly once, right before opening the Facebook
	// consent browser flow, i.e. only when the me/permissions pre-check finds a genuinely missing
	// permission. Not invoked when the requested permissions are already all granted (the common
	// fast path). Lets a caller (e.g. the GoLive button) disable itself only for the slow,
	// browser-round-trip path instead of for every permission check.
	void checkPermission(PLSAPI requestType, QStringList permission, const MyRequestTypeFunction &onFinished, QWidget *parent, const std::function<void()> &onRequestingPermission = nullptr);
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

	static PLSErrorHandler::RetData makeRetData(PLSErrorHandler::ErrCode prismCode);
	QString customErrorUpdateLiveinfoFailed() const { return QStringLiteral("UpdateLiveInfoFailedNoService"); }
	// PRISM_PC-6307: invokes the pending callback synchronously and inline (never queued or
	// deferred) if one is pending. Callers (e.g. PLSLiveInfoFacebook's timeout-suppression
	// flag) rely on this to distinguish a synthesized cancellation from a later real one.
	void cancelFacebookRequestPermission() const;

private:
	void checkPermissionSuccess(const QJsonObject &root, const QStringList &permissionList, PLSAPIFacebook::PLSAPI requestType, QWidget *parent, const MyRequestTypeFunction &onFinished,
				     const std::function<void()> &onRequestingPermission) const;
	static QString getFaceboolURL(const QString &endpoint);
	QUrl getPermissionRequestUrl(const QString &permission) const;
	void goFacebookRequestPermission(const QStringList &permissionList, QWidget *parent, std::function<void(bool)> callback) const;
	void startRequestApi(PLSAPI requestType, const pls::http::Request &request, const MyRequestSuccessFunction &successFunction, const MyRequestTypeFunction &failedFunction);
	PLSErrorHandler::RetData handleApiErrorCode(PLSAPI requestType, int statusCode, QByteArray data, QNetworkReply::NetworkError error) const;
	const char *getApiName(PLSAPI requestType) const;
	void printRequestStartLog(PLSAPI requestType, const QString &uri, const QString &log = QString()) const;
	void printRequestSuccessLog(PLSAPI requestType, const QString &log = QString()) const;
	QString downloadImageAsync(const QJsonObject &root);
	QMap<PLSAPI, pls::http::Request> m_reply;

	// PRISM_PC-6307: in-flight Facebook re-permission request state, so an external
	// timeout can cancel it and the pending callback is only ever consumed once.
#if defined(Q_OS_WIN)
	mutable QLocalServer *m_permissionServer = nullptr;
#elif defined(Q_OS_MACOS)
	mutable QMetaObject::Connection *m_permissionConnPtr = nullptr;
#endif
	mutable std::function<void(bool)> m_permissionCallback;
};

#endif // PLSAPIFacebook_H
