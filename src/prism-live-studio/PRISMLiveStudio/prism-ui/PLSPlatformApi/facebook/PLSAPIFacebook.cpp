#include "PLSAPIFacebook.h"
#include <QUrl>
#include <QMap>
#include <QUrlQuery>
#include <QPair>
#include <QMetaEnum>
#include "PLSLiveInfoFacebook.h"
#include "PLSPlatformBase.hpp"
#include "pls-channel-const.h"
#include "PLSPlatformApi.h"
#include "PLSAPICommon.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "pls-gpop-data-struct.hpp"
#include "liblog.h"

using namespace common;

constexpr auto facebookMoudule = "PLSLiveInfoFacebook";

constexpr auto granted_status = "granted";
constexpr auto declined_status = "declined";
constexpr auto granted_scopes = "granted_scopes";

const QString TimelinePublicId = "{value:'EVERYONE'}";
const QString TimelineFriendId = "{value:'ALL_FRIENDS'}";
const QString TimelineOnlymeId = "{value:'SELF'}";

PLSAPIFacebook *PLSAPIFacebook::instance()
{
	static PLSAPIFacebook *_instance = nullptr;
	if (nullptr == _instance) {
		_instance = pls_new<PLSAPIFacebook>();
		_instance->moveToThread(qApp->thread());
		QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] { pls_delete(_instance, nullptr); });
	}
	return _instance;
}

void PLSAPIFacebook::getLongLiveUserAccessToken(const GetLongAccessTokenCallback &onFinished)
{
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetLongLiveUserAccessToken;
	QString url = getFaceboolURL("oauth/access_token");

	QVariantMap params;
	params.insert(HTTP_CLIENT_ID, CHANNEL_FACEBOOK_CLIENT_ID);
	params.insert(HTTP_CLIENT_SECRET, CHANNEL_FACEBOOK_SECRET);
	params.insert("grant_type", "fb_exchange_token");
	params.insert("fb_exchange_token", PLS_PLATFORM_FACEBOOK->getAccessToken());
	pls::http::Request request;
	request.url(url).urlParams(params);
	auto successCallBack = [onFinished, this, requestType](QJsonObject root) {
		QString accessToken = root.value("access_token").toString();
		QString log = QString("get long time access token is %1").arg(accessToken.length() > 0 ? "success" : "false");
		printRequestSuccessLog(requestType, log);
		if (accessToken.length() > 0) {
			onFinished(makeRetData(PLSErrorHandler::SUCCESS), accessToken);
			return;
		}
		onFinished(PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::COMMON_DEFAULT_UPDATELIVEINFOFAILED_NOSERVICE, FACEBOOK, QString()), QString());
	};
	auto failCallBack = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, QString()); };
	printRequestStartLog(requestType, url);
	startRequestApi(requestType, request, successCallBack, failCallBack);
}

void PLSAPIFacebook::getUserInfo(const GetUserInfoCallback &onFinished)
{
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetUserInfo;
	QString url = getFaceboolURL("me");

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	params.insert("fields", "name,picture.type(large)");
	pls::http::Request request;
	request.url(url).urlParams(params);

	auto successCallBack = [onFinished, this, requestType](QJsonObject root) {
		QString username = root.value(name2str(name)).toString();
		auto url_ = pls_get_attr<QString>(root, name2str(url));
		QString imagePath;
		downloadSyncImage(url_, imagePath);
		pls_check_app_exiting();
		if (imagePath.length() > 0) {
			QString originPath = PLS_PLATFORM_FACEBOOK->getSrcInfo().value(ChannelData::g_userIconCachePath).toString();
			if (originPath != imagePath) {
				QFile::remove(originPath);
			}
		}
		QString userId = root.value(name2str(id)).toString();
		QString log = QString("download the user avatar is succeed %1").arg(BOOL2STR(!imagePath.isEmpty()));
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), username, imagePath, userId);
	};
	auto failCallBack = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, QString(), QString(), QString()); };
	printRequestStartLog(requestType, url);
	startRequestApi(requestType, request, successCallBack, failCallBack);
}

void PLSAPIFacebook::checkPermission(PLSAPI requestType, QStringList permissionList, const MyRequestTypeFunction &onFinished, QWidget *parent)
{
	QString url = getFaceboolURL("me/permissions");

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successCallBack = [onFinished, permissionList, parent, requestType, this](QJsonObject root) {
		auto retData = checkPermissionSuccess(root, permissionList, requestType, parent);
		onFinished(retData);
	};
	startRequestApi(requestType, request, successCallBack, onFinished);
}

PLSErrorHandler::RetData PLSAPIFacebook::checkPermissionSuccess(const QJsonObject &root, const QStringList &permissionList, PLSAPIFacebook::PLSAPI requestType, QWidget *parent) const
{
	QList<QString> grantedPermissionList;
	QList<QString> allPermissionList;
	QJsonArray json_array = root.value(name2str(data)).toArray();
	for (const QJsonValue &info : json_array) {
		QJsonObject json = info.toObject();
		QString userPermission = json.value(name2str(permission)).toString();
		allPermissionList.append(userPermission);
		QString status = json.value(name2str(status)).toString();
		if (status == granted_status) {
			grantedPermissionList.append(userPermission);
		}
	}
	if (QSet<QString> intersection = QSet<QString>(grantedPermissionList.begin(), grantedPermissionList.end()).intersect(QSet<QString>(permissionList.begin(), permissionList.end()));
	    intersection.size() == permissionList.size()) {
		PLS_INFO(facebookMoudule, "PLSAPIFacebook %s Include requested permissions, authorized permissions: [%s], requested permissions: [%s], all permissions: [%s]", getApiName(requestType),
			 grantedPermissionList.join(",").toStdString().c_str(), permissionList.join(",").toStdString().c_str(), allPermissionList.join(",").toStdString().c_str());
		return makeRetData(PLSErrorHandler::SUCCESS);
	}
	if (bool granted = goFacebookRequestPermission(permissionList, parent); granted) {
		PLS_INFO(facebookMoudule, "PLSAPIFacebook %s Go to the Facebook window to re-authorize successfully, authorized permissions: [%s], requested permissions: [%s], all permissions: [%s]",
			 getApiName(requestType), grantedPermissionList.join(",").toStdString().c_str(), permissionList.join(",").toStdString().c_str(),
			 allPermissionList.join(",").toStdString().c_str());
		return makeRetData(PLSErrorHandler::SUCCESS);
	}
	PLS_INFO(facebookMoudule, "PLSAPIFacebook %s Go to Facebook window to re-authorize failed", getApiName(requestType));
	return PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::CHANNEL_FACEBOOK_DECLINED, FACEBOOK, QString());
}

void PLSAPIFacebook::getMyGroupListRequestAndCheckPermission(const GetMyGroupListCallback &onFinished, QWidget *parent)
{
	auto finished = [onFinished, this](const PLSErrorHandler::RetData &retData) {
		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			getMyGroupListRequest(onFinished);
			return;
		}
		onFinished(retData, QList<FacebookGroupInfo>());
	};
	QStringList permissionList;
	permissionList << group_living_permission;
	checkPermission(PLSAPICheckMyGroupListPermission, permissionList, finished, parent);
}

void PLSAPIFacebook::getMyGroupListRequest(const GetMyGroupListCallback &onFinished)
{
	QString url = getFaceboolURL("me/groups");
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetMyGroupListRequest;

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	params.insert("fields", "name,picture");
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successCallBack = [onFinished, requestType, this](QJsonObject root) {
		QJsonArray json_array = root.value(name2str(data)).toArray();
		QList<FacebookGroupInfo> list;
		for (const QJsonValue &info : json_array) {
			QJsonObject json = info.toObject();
			FacebookGroupInfo groupInfo(json);
			list.append(groupInfo);
		}
		QString log = QString("get my group count is %1").arg(list.size());
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), list);
	};
	auto failCallBack = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, QList<FacebookGroupInfo>()); };
	startRequestApi(requestType, request, successCallBack, failCallBack);
}

void PLSAPIFacebook::getMyPageListRequestAndCheckPermission(const GetMyPageListCallback &onFinished, QWidget *parent)
{
	auto finished = [onFinished, this](const PLSErrorHandler::RetData &retData) {
		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			getMyPageListRequest(onFinished);
			return;
		}
		onFinished(retData, QList<FacebookPageInfo>());
	};
	QStringList permissionList;
	permissionList << page_show_list_permission;
	permissionList << business_management_permission;
	checkPermission(PLSAPICheckMyPageListPermission, permissionList, finished, parent);
}

void PLSAPIFacebook::getMyPageListRequest(const GetMyPageListCallback &onFinished)
{
	QString url = getFaceboolURL("me/accounts");
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetMyPageListRequest;

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successCallBack = [onFinished, this, requestType](QJsonObject root) {
		QJsonArray json_array = root.value(name2str(data)).toArray();
		QList<FacebookPageInfo> list;
		for (const QJsonValue &info : json_array) {
			QJsonObject json = info.toObject();
			FacebookPageInfo pageInfo(json);
			list.append(pageInfo);
		}
		QString log = QString("get my page count is %1").arg(list.size());
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), list);
	};
	auto failCallBack = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, QList<FacebookPageInfo>()); };
	startRequestApi(requestType, request, successCallBack, failCallBack);
}

void PLSAPIFacebook::searchGameTagListByKeyword(const GetMyGameListCallback &onFinished, const QString &keyword)
{
	QString url = getFaceboolURL("search");
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetMyGameListRequest;

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	params.insert("type", "game");
	params.insert("q", keyword);
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successCallBack = [onFinished, this, requestType](QJsonObject root) {
		QJsonArray json_array = root.value(name2str(data)).toArray();
		QList<FacebookGameInfo> list;
		for (const QJsonValue &info : json_array) {
			QJsonObject json = info.toObject();
			FacebookGameInfo gameInfo(json);
			list.append(gameInfo);
		}
		QString log = QString("get search game count is %1").arg(list.size());
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), list);
	};
	auto failCallBack = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, QList<FacebookGameInfo>()); };
	startRequestApi(requestType, request, successCallBack, failCallBack);
}

void PLSAPIFacebook::startLiving(PLSAPI requestType, const QString &itemId, const QString &privacy, const QString &accessToken, const StartLivingCallback &onFinished)
{
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = PLS_PLATFORM_FACEBOOK->getPrepareInfo();
	QString url = getFaceboolURL(itemId + "/live_videos");

	pls::http::Request request;
	QVariantMap params;
	params.insert("status", "LIVE_NOW");
	params.insert("title", prepareInfo.title);
	params.insert("description", prepareInfo.description);
	params.insert("privacy", privacy);
	params.insert(COOKIE_ACCESS_TOKEN, accessToken);
	params.insert("fields", "embed_html,secure_stream_url,video");
	params.insert("game_id", prepareInfo.gameId);
	request.form(params);
	request.url(url).method(pls::http::Method::Post);

	auto successFunction = [onFinished, this, requestType](QJsonObject root) {
		QString streamURL = root.value(name2str(secure_stream_url)).toString();
		QString liveId = root.value(name2str(id)).toString();
		QString videoId = root.value(name2str(video)).toObject().value(name2str(id)).toString();
		QString shareLink;
		getFacebookShareLink(root, shareLink);
		QString log = QString("liveId: %1, videoId: %2").arg(liveId).arg(videoId);
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), streamURL, liveId, videoId, shareLink);
	};
	auto failedFunction = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, "", "", "", ""); };
	QString log = QString("first name is %1, second name is %2, title is %3, description is %4, privacy is %5, gameName is %6")
			      .arg(prepareInfo.firstObjectName)
			      .arg(prepareInfo.secondObjectName)
			      .arg(prepareInfo.title.isEmpty() ? "empty" : prepareInfo.title)
			      .arg(prepareInfo.description.isEmpty() ? "empty" : prepareInfo.description)
			      .arg(privacy.isEmpty() ? "empty" : privacy)
			      .arg(prepareInfo.gameName.isEmpty() ? "empty" : prepareInfo.gameName);
	printRequestStartLog(requestType, url, log);
	startRequestApi(requestType, request, successFunction, failedFunction);
}

void PLSAPIFacebook::getFacebookShareLink(const QJsonObject &root, QString &shareLink) const
{
	QString embedHtml = root.value(name2str(embed_html)).toString();
	QStringList urlList = embedHtml.split(" ");
	for (auto queryURL : urlList) {
		if (!queryURL.startsWith("src=")) {
			continue;
		}
		QByteArray queryURLArray = queryURL.toUtf8();
		queryURL = QUrl::fromPercentEncoding(queryURLArray);
		queryURL = queryURL.replace("src=", "");
		QUrl qurl(queryURL);
		QUrlQuery query(qurl.query());
		QList<QPair<QString, QString>> list = query.queryItems();
		for (auto pair : list) {
			if (pair.first.compare("href", Qt::CaseInsensitive) == 0) {
				shareLink = pair.second.replace("\"", "");
				break;
			}
		}
	}
}

void PLSAPIFacebook::getFacebookItemUserInfo(const QString &itemId, const ItemInfoRequestFunction &onFinished)
{
	PLSAPI requestType = PLSAPIGetFacebookItemUserInfo;
	QString url = getFaceboolURL(itemId);

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getLiveAccessToken());
	params.insert("fields", "name,picture.type(large)");
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successFunction = [onFinished, this, requestType](QJsonObject root) {
		QString name = root.value(name2str(name)).toString();
		auto url_ = pls_get_attr<QString>(root, name2str(url));
		QString imagePath;
		downloadSyncImage(url_, imagePath);
		QString log = QString("title: %1, download image is succeed: %2").arg(name).arg(!imagePath.isEmpty());
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), name, imagePath);
	};
	auto failedFunction = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, "", ""); };
	startRequestApi(requestType, request, successFunction, failedFunction);
}

void PLSAPIFacebook::getLiveVideoTitleDesRequest(const QString &liveVideoId, const GetLiveVideoTitleDesCallback &onFinished)
{
	PLSAPI requestType = PLSAPIGetLiveVideoTitleDescRequest;
	QString url = getFaceboolURL(liveVideoId);

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	params.insert("fields", "title,description");
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successFunction = [onFinished, this, requestType](QJsonObject root) {
		QString title = root.value(name2str(title)).toString();
		QString des = root.value(name2str(description)).toString();
		QString log = QString("title is %1, description is %2").arg(title.length() > 0 ? title : "empty").arg(des);
		printRequestSuccessLog(requestType, log);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), title, des);
	};
	auto failedFunction = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, "", ""); };
	startRequestApi(requestType, request, successFunction, failedFunction);
}

void PLSAPIFacebook::getTimelinePrivacyRequest(const QString &videoId, const TimelinePrivacyFunction &onFinished)
{
	PLSAPI requestType = PLSAPIFacebook::PLSAPIGetLiveVideoPrivacyRequest;
	QString url = getFaceboolURL(videoId);

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getAccessToken());
	params.insert("fields", "privacy");
	pls::http::Request request;
	request.url(url).urlParams(params);

	printRequestStartLog(requestType, url);
	auto successFunction = [onFinished, this, requestType](QJsonObject root) {
		QString privacyId = TimelineOnlymeId;
		if (QString privacyKey = "privacy"; root.contains(privacyKey)) {
			QJsonObject privacyObject = root.value(privacyKey).toObject();
			QString privacyValue = privacyObject.value(name2str(value)).toString();
			if (privacyValue == "ALL_FRIENDS") {
				privacyId = TimelineFriendId;
			} else if (privacyValue == "EVERYONE") {
				privacyId = TimelinePublicId;
			}
			QString log = QString(" privacy:%1").arg(privacyValue);
			printRequestSuccessLog(requestType, log);
		}
		onFinished(makeRetData(PLSErrorHandler::SUCCESS), privacyId);
	};
	auto failedFunction = [onFinished](const PLSErrorHandler::RetData &retData) { onFinished(retData, ""); };
	startRequestApi(requestType, request, successFunction, failedFunction);
}

void PLSAPIFacebook::updateFacebookLiving(const QString &liveVideoId, const QString &privacy, const MyRequestTypeFunction &onFinished)
{
	PLSAPIFacebook::FacebookPrepareLiveInfo prepareInfo = PLS_PLATFORM_FACEBOOK->getPrepareInfo();
	PLSAPI requestType = PLSAPIFacebook::PLSAPIUpdateFacebookLiving;
	QString url = getFaceboolURL(liveVideoId);

	pls::http::Request request;
	QVariantMap params;
	params.insert("title", prepareInfo.title);
	params.insert("description", prepareInfo.description);
	params.insert("privacy", privacy);
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getLiveAccessToken());
	params.insert("fields", "embed_html,secure_stream_url,video");
	params.insert("game_id", prepareInfo.gameId);
	request.form(params);
	request.url(url).method(pls::http::Method::Post);

	QString log = QString("first public name %1, second public name %2, title %3, description %4, privacy %5, gameName %6")
			      .arg(prepareInfo.firstObjectName)
			      .arg(prepareInfo.secondObjectName)
			      .arg(prepareInfo.title)
			      .arg(prepareInfo.description)
			      .arg(privacy)
			      .arg(prepareInfo.gameName);
	printRequestStartLog(requestType, url, log);
	auto successFunction = [onFinished, this, requestType](QJsonObject) {
		printRequestSuccessLog(requestType);
		onFinished(makeRetData(PLSErrorHandler::SUCCESS));
	};
	startRequestApi(requestType, request, successFunction, onFinished);
}

void PLSAPIFacebook::stopFacebookLiving(const QString &liveVideoId, const MyRequestTypeFunction &onFinished) const
{
	PLSAPI requestType = PLSAPIFacebook::PLSAPIStopFacebookLiving;
	QString url = getFaceboolURL(liveVideoId);

	QVariantMap params;
	params.insert(COOKIE_ACCESS_TOKEN, PLS_PLATFORM_FACEBOOK->getLiveAccessToken());
	params.insert("end_live_video", "true");
	pls::http::Request request;
	request.url(url).urlParams(params).method(pls::http::Method::Post);

	printRequestStartLog(requestType, url);
	request.receiver(App()->getMainView());
	request.withLog();
	request.okResult([requestType, onFinished, this](const pls::http::Reply &) {
		       printRequestSuccessLog(requestType);
		       onFinished(makeRetData(PLSErrorHandler::SUCCESS));
	       })
		.failResult([onFinished, this, requestType](const pls::http::Reply &reply) {
			auto retData = handleApiErrorCode(requestType, reply.statusCode(), reply.data(), reply.error());
			onFinished(retData);
		})
		.timeout(PRISM_NET_REQUEST_TIMEOUT);
	pls::http::request(request);
}

void PLSAPIFacebook::downloadSyncImage(const QString &url, QString &imagePath) const
{
	if (auto icon = PLSAPICommon::downloadImageSync(this, url); icon.first) {
		imagePath = icon.second;
		return;
	}
	imagePath = QString();
}

QString PLSAPIFacebook::getFaceboolURL(const QString &endpoint)
{
	return (FACEBOOK_GRAPHA_DOMAIN + endpoint);
}

QUrl PLSAPIFacebook::getPermissionRequestUrl(const QString &permission) const
{
	QUrl url(CHANNEL_FACEBOOK_LOGIN_URL);
	QUrlQuery query;
	query.addQueryItem(HTTP_CLIENT_ID, CHANNEL_FACEBOOK_CLIENT_ID);
	query.addQueryItem(HTTP_REDIRECT_URI, CHANNEL_FACEBOOK_REDIRECTURL);
	query.addQueryItem("auth_type", "rerequest");
	query.addQueryItem("response_type", "granted_scopes");
	query.addQueryItem("scope", permission);
	url.setQuery(query);
	return url;
}

bool PLSAPIFacebook::goFacebookRequestPermission(const QStringList &permissionList, QWidget *parent) const
{
	auto check = [this, permissionList](const QVariantHash &, const QString &url, const QMap<QString, QString> &) {
		if (!url.startsWith(CHANNEL_FACEBOOK_REDIRECTURL)) {
			return PLSResultCheckingResult::Continue;
		}
		if (bool containsPermission = containsRequestPermissionList(url, permissionList); containsPermission) {
			return PLSResultCheckingResult::Ok;
		}
		return PLSResultCheckingResult::Close;
	};

	QVariantHash result;
	QUrl url = getPermissionRequestUrl(permissionList.join(","));
	bool bResult = pls_browser_view(result, url, {}, FACEBOOK, check, parent);
	return bResult;
}

void PLSAPIFacebook::startRequestApi(PLSAPI requestType, const pls::http::Request &request, const MyRequestSuccessFunction &successFunction, const MyRequestTypeFunction &failedFunction)
{
	if (m_reply.contains(requestType)) {
		pls::http::Request cancelRequest = m_reply.take(requestType);
		cancelRequest.abort();
	}
	if (PLS_PLATFORM_FACEBOOK->getParentPointer() != nullptr) {
		request.receiver(PLS_PLATFORM_FACEBOOK->getParentPointer());
	} else {
		request.receiver(App()->getMainView());
	}
	request.id(QStringLiteral("Facebook"))
		.okResult([requestType, successFunction, failedFunction, this](const pls::http::Reply &reply) {
		       if (pls_get_app_exiting()) {
			       return;
		       }
		       auto doc = QJsonDocument::fromJson(reply.data());
		       if (!doc.isObject()) {
			       PLS_ERROR(facebookMoudule, "PLSAPIFacebook %s is not object", getApiName(requestType));
			       failedFunction(PLSErrorHandler::getAlertStringByPrismCode(PLSErrorHandler::COMMON_DEFAULT_UPDATELIVEINFOFAILED_NOSERVICE, FACEBOOK, QString()));
			       return;
		       }
		       m_reply.take(requestType);
		       auto root = doc.object();
		       successFunction(root);
	       })
		.failResult([failedFunction, this, requestType](const pls::http::Reply &reply) {
			if (pls_get_app_exiting()) {
				return;
			}
			if (reply.isAborted()) {
				return;
			}
			m_reply.take(requestType);
			auto retData = handleApiErrorCode(requestType, reply.statusCode(), reply.data(), reply.error());
			failedFunction(retData);
		})
		.timeout(PRISM_NET_REQUEST_TIMEOUT)
		.withLog()
		.workInMainThread();
	pls::http::request(request);
	m_reply.insert(requestType, request);
}

bool PLSAPIFacebook::containsRequestPermissionList(const QString &url, const QStringList &requestPermissionList) const
{
	QUrl qurl(url);
	bool containsPermission = false;
	QStringList list = qurl.query(QUrl::FullyDecoded).split("&");
	QString grantedString;
	for (auto str : list) {
		if (str.startsWith(granted_scopes, Qt::CaseInsensitive)) {
			grantedString = str;
			break;
		}
	}
	for (auto requestPermission : requestPermissionList) {
		if (!grantedString.contains(requestPermission, Qt::CaseInsensitive)) {
			containsPermission = false;
			break;
		}
		containsPermission = true;
	}
	return containsPermission;
}

PLSErrorHandler::RetData PLSAPIFacebook::handleApiErrorCode(PLSAPI requestType, int statusCode, QByteArray data, QNetworkReply::NetworkError error) const
{
	PLSErrorHandler::ExtraData extraData;

	QString strRequestType;
	switch (requestType)
	{
	case PLSAPIStartTimelineLiving:
	case PLSAPIStartGroupLiving:
	case PLSAPIStartPageLiving:
		strRequestType = "StartLiving";
		break;
	case PLSAPIUpdateFacebookLiving:
		strRequestType = "UpdatLiving";
		break;
	default:
		break;
	}
	extraData.pathValueMap = {{"requestType", strRequestType}};

	return PLSErrorHandler::getAlertString({statusCode, error, data}, FACEBOOK, customErrorUpdateLiveinfoFailed(), extraData);
}

const char *PLSAPIFacebook::getApiName(PLSAPI requestType) const
{
	QMetaEnum metaRequestType = QMetaEnum::fromType<PLSAPIFacebook::PLSAPI>();
	return metaRequestType.valueToKey(requestType);
}

void PLSAPIFacebook::printRequestStartLog(PLSAPI requestType, const QString &uri, const QString &log) const
{
	if (log.length() > 0) {
		PLS_INFO(facebookMoudule, "PLSAPIFacebook %s start request url : %s, %s", getApiName(requestType), uri.toStdString().c_str(), log.toStdString().c_str());
		return;
	}
	PLS_INFO(facebookMoudule, "PLSAPIFacebook %s start request url : %s", getApiName(requestType), uri.toStdString().c_str());
}

void PLSAPIFacebook::printRequestSuccessLog(PLSAPI requestType, const QString &log) const
{
	if (log.length() > 0) {
		PLS_INFO(facebookMoudule, "PLSAPIFacebook %s request success, %s", getApiName(requestType), log.toStdString().c_str());
		return;
	}
	PLS_INFO(facebookMoudule, "PLSAPIFacebook %s request success", getApiName(requestType));
}

FacebookGroupInfo::FacebookGroupInfo(const QJsonObject &object) : groupId(JSON_getString(object, id)), groupName(JSON_getString(object, name))
{
	groupCover = pls_get_attr<QString>(object, name2str(url));
}

FacebookPageInfo::FacebookPageInfo(const QJsonObject &object) : pageId(JSON_getString(object, id)), pageName(JSON_getString(object, name)), pageAccessToken(JSON_getString(object, access_token)) {}

FacebookGameInfo::FacebookGameInfo(const QJsonObject &object) : gameId(JSON_getString(object, id)), gameName(JSON_getString(object, name)) {}

PLSErrorHandler::RetData PLSAPIFacebook::makeRetData(PLSErrorHandler::ErrCode prismCode)
{
	PLSErrorHandler::RetData data;
	data.prismCode = prismCode;

	return data;
}
