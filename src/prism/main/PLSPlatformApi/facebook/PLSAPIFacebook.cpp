#include "PLSAPIFacebook.h"
#include <QUrl>
#include <QMap>
#include <QUrlQuery>
#include <QPair>
#include <QMetaEnum>
#include "PLSLiveInfoFacebook.h"
#include "../PLSPlatformBase.hpp"
#include "../ChannelsDataApi/ChannelConst.h"
#include "../PLSPlatformApi.h"
#include "../vlive/PLSAPIVLive.h"
#include "../../pls-common-define.hpp"
#include "../../frontend-api/frontend-api.h"

#define page_list_permission "pages_show_list"
#define granted_status "granted"
#define declined_status "declined"
#define granted_scopes "granted_scopes"

PLSAPIFacebook *PLSAPIFacebook::instance()
{
	static PLSAPIFacebook *_instance = nullptr;

	if (nullptr == _instance) {
		_instance = new PLSAPIFacebook();
		_instance->moveToThread(qApp->thread());
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
			delete _instance;
			_instance = nullptr;
		});
	}

	return _instance;
}

void PLSAPIFacebook::getLongLiveUserAccessToken(PLSAPI requestType, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::getUserInfo(PLSAPI requestType, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::checkPermission(PLSAPI requestType, QStringList permissionList, MyRequestTypeFunction onFinished, QWidget *parent)
{
	
}

void PLSAPIFacebook::getMyGroupListRequestAndCheckPermission(PLSAPI requestType, QList<QMap<QString, QString>> &list, MyRequestTypeFunction onFinished, QWidget *parent)
{
	
}

void PLSAPIFacebook::getMyGroupListRequest(PLSAPI requestType, QList<QMap<QString, QString>> &list, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::getMyPageListRequestAndCheckPermission(PLSAPI requestType, QList<QMap<QString, QString>> &list, MyRequestTypeFunction onFinished, QWidget *parent)
{
	
}

void PLSAPIFacebook::getMyPageListRequest(PLSAPI requestType, QList<QMap<QString, QString>> &list, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::searchGameTagListByKeyword(PLSAPI requestType, QList<QMap<QString, QString>> &list, MyRequestTypeFunction onFinished, const QString &keyword)
{
	
}

void PLSAPIFacebook::startLiving(PLSAPI requestType, const QString &itemId, const QString &title, const QString &description, const QString &privacy, const QString &game_id,
				 const QString &accessToken, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::getFacebookItemUserInfo(PLSAPI requestType, const QString &itemId, ItemInfoRequestFunction onFinished)
{
	
}

void PLSAPIFacebook::getLiveVideoInfoRequest(PLSAPI requestType, const QString &liveVideoId, const QString &field, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::getTimelinePrivacyRequest(PLSAPI requestType, const QString &videoId, TimelinePrivacyFunction onFinished)
{
	
}

void PLSAPIFacebook::updateFacebookLiving(PLSAPI requestType, const QString &liveVideoId, const QString &title, const QString &description, const QString &privacy, const QString &game_id,
					  MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::stopFacebookLiving(PLSAPI requestType, const QString &liveVideoId, MyRequestTypeFunction onFinished)
{
	
}

void PLSAPIFacebook::downloadSyncImage(const QString &url, QString &imagePath)
{
	
}

QString PLSAPIFacebook::getFaceboolURL(const QString &endpoint)
{
	return (FACEBOOK_GRAPHA_DOMAIN + endpoint);
}

QUrl PLSAPIFacebook::getPermissionRequestUrl(const QString &permission)
{
	QUrl url(CHANNEL_FACEBOOK_LOGIN_URL);
	return url;
}

bool PLSAPIFacebook::goFacebookRequestPermission(const QString &permission, QWidget *parent)
{
	return true;
}

void PLSAPIFacebook::startRequestApi(PLSAPI requestType, QNetworkReply *requestReply, MyRequestSuccessFunction successFunction, MyRequestTypeFunction failedFunction)
{
	
}

PLSAPIFacebookType PLSAPIFacebook::handleApiErrorCode(PLSAPI requestType, int statusCode, QByteArray data, QNetworkReply::NetworkError error)
{
	return PLSAPIFacebookType::PLSFacebookFailed;
}

const char *PLSAPIFacebook::getApiName(PLSAPI requestType)
{
	QMetaEnum metaRequestType = QMetaEnum::fromType<PLSAPIFacebook::PLSAPI>();
	return metaRequestType.valueToKey(requestType);
}
