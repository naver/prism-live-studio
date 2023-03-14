#include <Windows.h>

#include "PLSPlatformNaverTV.h"

#include <ctime>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../../PLSHttpApi/PLSHmacNetworkReplyBuilder.h"
#include "../../PLSHttpApi/PLSHttpHelper.h"

#include "../../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../../channels/ChannelsDataApi/ChannelCommonFunctions.h"

#include "../common/PLSDateFormate.h"
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"

#include "../PLSScheLiveNotice.h"

#include "PLSLiveInfoNaverTV.h"

#include "pls-app.hpp"
#include "main-view.hpp"
#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"

#define CSTR_OBJECT_URL "http://tv.naver.com"

#define CSTR_CODE "code"
#define CSTR_MESSAGE "message"
#define CSTR_ERROR_CODE "errorCode"
#define CSTR_ERROR_MESSAGE "errorMessage"
#define CSTR_RTN_CD "rtn_cd"
#define CSTR_RTN_MSG "rtn_msg"
#define CSTR_HEADER "header"

#define CSTR_NAME "name"
#define CSTR_OBJECTID "objectId"
#define CSTR_GROUPID "groupId"
#define CSTR_LANG "lang"
#define CSTR_TICKET "ticket"
#define CSTR_TEMPLATEID "templateId"
#define CSTR_USERTYPE "userType"
#define CSTR_URI "URI"
#define CSTR_GLOBAL_URI "GLOBAL_URI"
#define CSTR_HMAC_SALT "HMAC_SALT"
#define CSTR_NID_SES "NID_SES"
#define CSTR_NID_INF "nid_inf"
#define CSTR_NID_AUT "NID_AUT"
#define CSTR_LIVENO "liveNo"
#define CSTR_OLIVEID "oliveId"
#define CSTR_STARTDATE "startDate"
#define CSTR_ENDDATE "endDate"
#define CSTR_TITLE "title"
#define CSTR_STATUS "status"
#define CSTR_CHANNELID "channelId"
#define CSTR_CATEGORYCODE "categoryCode"
#define CSTR_OPENYN "openYn"
#define CSTR_NOTICE "notice"
#define CSTR_THUMBNAILIMAGEURL "thumbnailImageUrl"
#define CSTR_CHANNELNAME "channelName"
#define CSTR_CHANNELEMBLEM "channelEmblem"
#define CSTR_STREAMKEY "streamKey"
#define CSTR_SERVERURL "serverUrl"
#define CSTR_PUBLISHING "publishing"
#define CSTR_REHEARSALYN "rehearsalYn"
#define CSTR_TICKETID "ticketId"
#define CSTR_LIVECOMMENTOPTIONS "liveCommentOptions"
#define CSTR_IDNO "idNo"
#define CSTR_MANAGER "manager"
#define CSTR_OLIVEMODEL "oliveModel"
#define CSTR_ID "id"
#define CSTR_LINK "link"
#define CSTR_PCLIVEURL "pcLiveUrl"
#define CSTR_MOBILELIVEURL "mobileLiveUrl"
#define CSTR_CURRENTSTAT "currentStat"
#define CSTR_PV "pv"
#define CSTR_THUMBNAIL "thumbnail"
#define CSTR_LIKECOUNT "likeCount"
#define CSTR_COMMENTCOUNT "commentCount"
#define CSTR_MANAGER_U "MANAGER"
#define CSTR_OBJECTURL "objectUrl"
#define CSTR_USERID "userId"
#define CSTR_USERNAME "username"
#define CSTR_REFERER "Referer"
#define CSTR_CLIENT_ID "client_id"
#define CSTR_CLIENT_ID_VALUE "jZNIoee3IBi6sujuw4w0"
#define CSTR_CLIENT_SECRET "client_secret"
#define CSTR_CLIENT_SECRET_VALUE "8nJdGLYNMb"
#define CSTR_GRANT_TYPE "grant_type"
#define CSTR_GRANT_TYPE_AUTHORIZATION_CODE "authorization_code"
#define CSTR_RESPONSE_TYPE "response_type"
#define CSTR_REDIRECT_URL "redirect_url"
#define CSTR_ERROR "error"
#define CSTR_ERROR_DESCRIPTION "error_description"
#define CSTR_LOCATION_REPLACE "location.replace"
#define CSTR_CODE_E "code="
#define CSTR_IS_REHEARSAL "isRehearsal"
#define CSTR_DISABLE "disable"
#define CSTR_LIVE_ID "liveId"
#define CSTR_SCHEDULED "scheduled"
#define CSTR_SIMULCAST_CHANNEL "simulcastChannel"
#define CSTR_NAVERTV_TOKEN "NaverTVToken"
#define CSTR_ACCESS_TOKEN "access_token"
#define CSTR_REFRESH_TOKEN "refresh_token"
#define CSTR_TOKEN_TYPE "token_type"
#define CSTR_EXPIRES_IN "expires_in"

static const QString LANGUAGE = "ko";

static const QString DEFAULT_NICK_NAME = "No Name";
static const QString KEY_NICK_NAME = "nick_name";
static const QString KEY_IMAGE_URL = "image_url";
static const QString IMAGE_FILE_NAME_PREFIX = "navertv-";

static const int CHECK_PUBLISHING_MAX_TIMES = 15;
static const int CHECK_PUBLISHING_TIMESPAN = 1000;
static const int CHECK_STATUS_TIMESPAN = 5000;

namespace {
class NoDefualtHeaderNetworkReplyBuilder : public PLSHmacNetworkReplyBuilder {
public:
	NoDefualtHeaderNetworkReplyBuilder(const QString &value, HmacType hmacType = HmacType::HT_PRISM) : PLSHmacNetworkReplyBuilder(value, hmacType) {}
	NoDefualtHeaderNetworkReplyBuilder(const QUrl &value, HmacType hmacType = HmacType::HT_PRISM) : PLSHmacNetworkReplyBuilder(value, hmacType) {}

public:
	QUrl buildUrl(const QUrl &url)
	{
		QVariantMap headers = this->rawHeaders;
		auto urlEncrypted = PLSHmacNetworkReplyBuilder::buildUrl(url);
		setRawHeaders(headers);
		return urlEncrypted;
	}
};

QString getValue(const QJsonObject &object, const QString &name, const QString &defaultValue = QString())
{
	QString value = object[name].toString();
	if (!value.isEmpty()) {
		return value;
	} else {
		return defaultValue;
	}
}
void deleteTimer(QTimer *&timer)
{
	if (timer) {
		timer->stop();
		delete timer;
		timer = nullptr;
	}
}
template<typename T> void deleteObject(T *&object)
{
	if (object) {
		delete object;
		object = nullptr;
	}
}
template<typename Callback> QTimer *startTimer(PLSPlatformNaverTV *platform, QTimer *&timer, int timeout, Callback callback, bool singleShot = true)
{
	deleteTimer(timer);
	timer = new QTimer(platform);
	timer->setSingleShot(singleShot);
	QObject::connect(timer, &QTimer::timeout, platform, callback);
	timer->start(timeout);
	return timer;
}
QMap<QString, QString> parseCookie(const QString &cookie)
{
	QMap<QString, QString> map;
	QList<QString> list = cookie.split('\n');
	for (const auto &c : list) {
		int i = c.indexOf('=');
		if (i <= 0) {
			continue;
		}

		QString key = c.left(i), value = c.mid(i + 1);
		if (!key.isEmpty() && !value.isEmpty()) {
			map.insert(key, value);
		}
	}
	return map;
}
bool titleIsEmtpy(const QString &title)
{
	if (title.isEmpty()) {
		return true;
	}

	for (const auto &ch : title) {
		if (!ch.isSpace()) {
			return false;
		}
	}
	return true;
}
QString processThumbnailImageUrl(const QString &thumbnailImageUrl)
{
	return thumbnailImageUrl.isEmpty() ? "null" : thumbnailImageUrl;
}
bool findLocationUrl(QString &url, const QByteArray &body)
{
	int index = body.indexOf(CSTR_LOCATION_REPLACE);
	if (index < 0) {
		return false;
	}

	int start, end;
	start = index + sizeof(CSTR_LOCATION_REPLACE) + 1;
	for (end = start; end < body.length(); ++end) {
		if (body[end] == '\"' || body[end] == '\'') {
			break;
		}
	}
	url = QString::fromUtf8(body.mid(start, end - start));
	return true;
}
QString getUrlCode(const QString &url)
{
	QStringList items = QUrl(url).query(QUrl::FullyDecoded).split('&');
	for (const auto &item : items) {
		QStringList param = item.split('=');
		if (param.length() == 2 && param.at(0) == CSTR_CODE) {
			return param.at(1);
		}
	}
	return QString();
}
void loadToken(PLSPlatformNaverTV::Token &token)
{
	auto config = App()->CookieConfig();
	token.accessToken = QString::fromUtf8(config_get_string(config, CSTR_NAVERTV_TOKEN, CSTR_ACCESS_TOKEN));
	token.refreshToken = QString::fromUtf8(config_get_string(config, CSTR_NAVERTV_TOKEN, CSTR_REFRESH_TOKEN));
	token.tokenType = QString::fromUtf8(config_get_string(config, CSTR_NAVERTV_TOKEN, CSTR_TOKEN_TYPE));
	token.expiresIn = config_get_int(config, CSTR_NAVERTV_TOKEN, CSTR_EXPIRES_IN);
}
void saveToken(const PLSPlatformNaverTV::Token &token)
{
	auto config = App()->CookieConfig();
	config_set_string(config, CSTR_NAVERTV_TOKEN, CSTR_ACCESS_TOKEN, token.accessToken.toUtf8().constData());
	config_set_string(config, CSTR_NAVERTV_TOKEN, CSTR_REFRESH_TOKEN, token.refreshToken.toUtf8().constData());
	config_set_string(config, CSTR_NAVERTV_TOKEN, CSTR_TOKEN_TYPE, token.tokenType.toUtf8().constData());
	config_set_int(config, CSTR_NAVERTV_TOKEN, CSTR_EXPIRES_IN, token.expiresIn);
	config_save(config);
}
void clearToken()
{
	auto config = App()->CookieConfig();
	config_remove_value(config, CSTR_NAVERTV_TOKEN, CSTR_ACCESS_TOKEN);
	config_remove_value(config, CSTR_NAVERTV_TOKEN, CSTR_REFRESH_TOKEN);
	config_remove_value(config, CSTR_NAVERTV_TOKEN, CSTR_TOKEN_TYPE);
	config_remove_value(config, CSTR_NAVERTV_TOKEN, CSTR_EXPIRES_IN);
	config_save(config);
}
}

PLSPlatformNaverTV::Token::Token() {}
PLSPlatformNaverTV::Token::Token(const QJsonObject &object)
{
	accessToken = object[CSTR_ACCESS_TOKEN].toString();
	refreshToken = object[CSTR_REFRESH_TOKEN].toString();
	tokenType = object[CSTR_TOKEN_TYPE].toString();
	expiresIn = object["expires_in"].toString().toLongLong();
}
PLSPlatformNaverTV::Token::~Token() {}

PLSPlatformNaverTV::Token::Token(const Token &token) : accessToken(token.accessToken), refreshToken(token.refreshToken), tokenType(token.tokenType), expiresIn(token.expiresIn) {}
PLSPlatformNaverTV::Token &PLSPlatformNaverTV::Token::operator=(const Token &token)
{
	accessToken = token.accessToken;
	refreshToken = token.refreshToken;
	tokenType = token.tokenType;
	expiresIn = token.expiresIn;
	return *this;
}

PLSPlatformNaverTV::LiveInfo::LiveInfo() : isScheLive(false), isRehearsal(false), liveNo(-1), oliveId(-1), startDate(0), endDate(0) {}
PLSPlatformNaverTV::LiveInfo::LiveInfo(const QJsonObject &object)
{
	isScheLive = true;
	isRehearsal = false;
	liveNo = object[CSTR_LIVENO].toInt();
	oliveId = object[CSTR_OLIVEID].toInt();
	startDate = (qint64)object[CSTR_STARTDATE].toDouble();
	endDate = (qint64)object[CSTR_ENDDATE].toDouble();
	title = object[CSTR_TITLE].toString();
	status = object[CSTR_STATUS].toString();
	channelId = object[CSTR_CHANNELID].toString();
	categoryCode = object[CSTR_CATEGORYCODE].toString();
	openYn = object[CSTR_OPENYN].toString();
	notice = object[CSTR_NOTICE].toString();
	thumbnailImageUrl = object[CSTR_THUMBNAILIMAGEURL].toString();
	if (!thumbnailImageUrl.isEmpty() && thumbnailImageUrl.contains(QLatin1String("?type="))) {
		thumbnailImageUrl += "?type=f880_495";
	}
}
PLSPlatformNaverTV::LiveInfo::~LiveInfo() {}

PLSPlatformNaverTV::LiveInfo::LiveInfo(const LiveInfo &liveInfo)
	: isScheLive(liveInfo.isScheLive),
	  isRehearsal(liveInfo.isRehearsal),
	  liveNo(liveInfo.liveNo),
	  oliveId(liveInfo.oliveId),
	  startDate(liveInfo.startDate),
	  endDate(liveInfo.endDate),
	  title(liveInfo.title),
	  status(liveInfo.status),
	  channelId(liveInfo.channelId),
	  categoryCode(liveInfo.categoryCode),
	  openYn(liveInfo.openYn),
	  notice(liveInfo.notice),
	  thumbnailImageUrl(liveInfo.thumbnailImageUrl),
	  thumbnailImagePath(liveInfo.thumbnailImagePath)
{
}
PLSPlatformNaverTV::LiveInfo &PLSPlatformNaverTV::LiveInfo::operator=(const LiveInfo &liveInfo)
{
	isScheLive = liveInfo.isScheLive;
	isRehearsal = liveInfo.isRehearsal;
	liveNo = liveInfo.liveNo;
	oliveId = liveInfo.oliveId;
	startDate = liveInfo.startDate;
	endDate = liveInfo.endDate;
	title = liveInfo.title;
	status = liveInfo.status;
	channelId = liveInfo.channelId;
	categoryCode = liveInfo.categoryCode;
	openYn = liveInfo.openYn;
	notice = liveInfo.notice;
	thumbnailImageUrl = liveInfo.thumbnailImageUrl;
	thumbnailImagePath = liveInfo.thumbnailImagePath;
	return *this;
}

bool PLSPlatformNaverTV::LiveInfo::needUploadThumbnail() const
{
	return !thumbnailImagePath.isEmpty() && thumbnailImageUrl.isEmpty();
}

PLSPlatformNaverTV::Live::Live() : manager(false), /*liveStarted(false), allLiveStarted(false),*/ oliveId(-1), likeCount(0), watchCount(0), commentCount(0) {}
PLSPlatformNaverTV::Live::~Live() {}

PLSPlatformNaverTV::PLSPlatformNaverTV() {}
PLSPlatformNaverTV::~PLSPlatformNaverTV() {}

PLSServiceType PLSPlatformNaverTV::getServiceType() const
{
	return PLSServiceType::ST_NAVERTV;
}
void PLSPlatformNaverTV::onPrepareLive(bool value)
{
	clearLive();

	if (!value) {
		PLSPlatformBase::onPrepareLive(value);
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_navertv(this) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s liveinfo closed value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	PLSPlatformBase::onPrepareLive(value);

	if (!value) {
		return;
	}
	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (isRehearsal()) {
		latInfo.remove(ChannelData::g_viewers);
	} else {
		latInfo.insert(ChannelData::g_viewers, "0");
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, true);

	// notify golive button change status
	if (value && isRehearsal()) {
		PLSCHANNELS_API->rehearsalBegin();
	}
}
void PLSPlatformNaverTV::onAllPrepareLive(bool value)
{
	if (!value && (getLiveId() >= 0) && !isRehearsal() && !getLiveInfo()->isScheLive) {
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());

		// cancel or fail
		// immediate live
		liveClose([=](bool) { PLSPlatformBase::onAllPrepareLive(value); });
	} else {
		PLSPlatformBase::onAllPrepareLive(value);
	}
}
void PLSPlatformNaverTV::onLiveStarted(bool value)
{
	// auto live = getLive();
	// live->liveStarted = true;

	if (PLSCHANNELS_API->currentSelectedCount() > 1) {
		// multiple platform
		if (!isRehearsal() && getLiveInfo()->isScheLive) {
			// scheduled live
			scheduleStart([=](bool ok, int) {
				if (!ok) {
					clearLive();
					PLSCHANNELS_API->setChannelStatus(getChannelUUID(), ChannelData::ChannelStatus::Error);
				}

				PLSPlatformBase::onLiveStarted(value);
			});
		} else {
			PLSPlatformBase::onLiveStarted(value);
		}
	} else {
		// single platform
		if (!isRehearsal() && getLiveInfo()->isScheLive) {
			// scheduled live
			scheduleStart([=](bool ok, int) {
				if (!ok) {
					PLS_PLATFORM_API->showEndView(false, false);
					clearLive();
				}

				PLSPlatformBase::onLiveStarted(ok);
			});
		} else {
			PLSPlatformBase::onLiveStarted(true);
		}
	}
}
void PLSPlatformNaverTV::onAlLiveStarted(bool value)
{
	// auto live = getLive();
	// live->allLiveStarted = true;
	__super::setIsScheduleLive(isScheduleLive());
	PLSPlatformBase::onAlLiveStarted(value);

	// live start clear others
	// for (auto platform : PLS_PLATFORM_API->getPlatformNaverTV()) {
	// 	if (!platform->isActive()) {
	// 		platform->clearLiveInfo();
	// 	}
	// }
}
void PLSPlatformNaverTV::onLiveEnded()
{
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());

	checkStreamPublishingRetryTimes = 0;
	deleteTimer(checkStreamPublishingTimer);
	deleteTimer(checkStatusTimer);

	// if (isRehearsal() && !isLiveStarted() && !isAllLiveStarted()) {
	// 	PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("LiveInfo.Rehearsal.Start.Fail.Alert"));
	// }

	if (!this->isRehearsal() && (getLiveId() >= 0)) {
		if (isLiveEndedByMqtt) {
			PLSPlatformBase::onLiveEnded();
			clearLiveInfo();
			clearLive();
		} else {
			liveClose([=](bool) {
				PLSPlatformBase::onLiveEnded();
				clearLiveInfo();
			});
		}
	} else {
		PLSPlatformBase::onLiveEnded();
		clearLive();
	}
}
void PLSPlatformNaverTV::onActive()
{
	PLSPlatformBase::onActive();

	QMetaObject::invokeMethod(this, &PLSPlatformNaverTV::onShowScheLiveNotice, Qt::QueuedConnection);
}
QString PLSPlatformNaverTV::getShareUrl()
{
	return live ? live->pcLiveUrl : QString();
}
QJsonObject PLSPlatformNaverTV::getWebChatParams()
{
	QJsonObject object;
	object[CSTR_NAME] = "NAVERTV";

	if (live) {
		auto cookies = parseCookie(getChannelCookie());
		object[CSTR_OBJECTID] = live->objectId;
		object[CSTR_GROUPID] = live->groupId;
		object[CSTR_LANG] = LANGUAGE;
		object[CSTR_TICKET] = live->ticketId;
		object[CSTR_TEMPLATEID] = live->templateId;
		object[CSTR_USERTYPE] = live->manager ? CSTR_MANAGER_U : "";
		// object[CSTR_URI] = PRISM_SSL;
		object[CSTR_GLOBAL_URI] = PRISM_SSL;
		object[CSTR_HMAC_SALT] = PLS_PC_HMAC_KEY;
		object[CSTR_NID_SES] = cookies[CSTR_NID_SES];
		object[CSTR_NID_INF] = cookies[CSTR_NID_INF];
		object[CSTR_NID_AUT] = cookies[CSTR_NID_AUT];
		object[CSTR_OBJECTURL] = CSTR_OBJECT_URL;
		object[CSTR_USERID] = live->idNo;
		object[CSTR_USERNAME] = nickName;
		object[CSTR_IS_REHEARSAL] = isRehearsal();
		object[CSTR_DISABLE] = getLiveId() == -1;
	}

	return object;
}
bool PLSPlatformNaverTV::isSendChatToMqtt() const
{
	return true;
}
QJsonObject PLSPlatformNaverTV::getLiveStartParams()
{
	QJsonObject platform = PLSPlatformBase::getLiveStartParams();

	platform[CSTR_LIVE_ID] = getLiveId();
	platform[CSTR_IS_REHEARSAL] = isRehearsal();
	platform[CSTR_CHANNELID] = getSubChannelId();
	platform[CSTR_SCHEDULED] = isScheduleLive();
	platform[CSTR_SIMULCAST_CHANNEL] = getSubChannelName();

	return platform;
}
void PLSPlatformNaverTV::onInitDataChanged()
{
	auto platforms = PLS_PLATFORM_API->getPlatformNaverTV();
	auto iter = std::find_if(platforms.begin(), platforms.end(), [](PLSPlatformNaverTV *platform) { return platform->isPrimary(); });
	if (iter == platforms.end()) {
		return;
	}

	auto primary = *iter;
	if (this != primary) {
		token = primary->token;
		nickName = primary->nickName;
		headImageUrl = primary->headImageUrl;
	}
}
void PLSPlatformNaverTV::onMqttStatus(PLSPlatformMqttStatus status)
{
	if (status == PLSPlatformMqttStatus::PMS_END_BROADCAST) {
		PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV live end by mqtt, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV live end by mqtt, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

		isLiveEndedByMqtt = true;

		// multi-streaming
		if (PLSCHANNELS_API->currentSelectedCount() > 1) {
			PLSCHANNELS_API->setChannelStatus(getChannelUUID(), ChannelData::ChannelStatus::Error);

			pls_toast_message(pls_toast_info_type::PLS_TOAST_ERROR, tr("LiveInfo.live.error.stoped.byRemote").arg("NAVER TV"));
		}
	}
}
bool PLSPlatformNaverTV::isMqttChatCanShow(const QJsonObject &)
{
	return getLiveId() > 0;
}

QMap<QString, QString> PLSPlatformNaverTV::getDirectEndParams()
{
	return {{"serviceLiveLink", live ? live->pcLiveUrl : ""}};
}

QPair<bool, QString> PLSPlatformNaverTV::getChannelEmblemSync(const QString &url)
{
	PLSNetworkReplyBuilder builder(url);
	builder.setCookie(getChannelCookie());
	QNetworkReply *reply = builder.get();
	reply->setProperty("urlMasking", pls_masking_person_info(url));
	return PLSHttpHelper::downloadImageSync(reply, this, getTmpCacheDir(), IMAGE_FILE_NAME_PREFIX);
}
QList<QPair<bool, QString>> PLSPlatformNaverTV::getChannelEmblemSync(const QList<QString> &urls)
{
	if (urls.isEmpty()) {
		return QList<QPair<bool, QString>>();
	}

	QString cookie = getChannelCookie();

	QList<QNetworkReply *> replyList;
	for (auto &url : urls) {
		QNetworkReply *reply = PLSNetworkReplyBuilder(url).setCookie(cookie).get();
		reply->setProperty("urlMasking", pls_masking_person_info(url));
		replyList.append(reply);
	}

	return PLSHttpHelper::downloadImagesSync(replyList, this, getTmpCacheDir(), IMAGE_FILE_NAME_PREFIX);
}

void PLSPlatformNaverTV::getAuth(const QString &channelName, const QVariantMap &srcInfo, UpdateCallback finishedCall, bool isClearLiveInfo)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get channel list, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get channel list, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	for (auto *platform : PLS_PLATFORM_API->getPlatformNaverTV()) {
		platform->primary = false;
	}

	if (isClearLiveInfo) {
		for (auto platform : PLS_PLATFORM_API->getPlatformNaverTV()) {
			platform->clearLiveInfo();
		}
	}

	primary = true;

	getToken([=](bool ok, QNetworkReply::NetworkError error) {
		if (!ok) {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get token failed, channel: %s, load lastest saved.", getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get token failed, channel: %s, load lastest saved.", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

			loadToken(token);
			if (token.accessToken.isEmpty()) {
				PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV lastest saved token is empty, channel: %s.", getSubChannelName().toUtf8().constData());
				PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV lastest saved token is empty, channel: %s.", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

				QVariantMap info = srcInfo;
				info[ChannelData::g_channelStatus] = (error >= QNetworkReply::ConnectionRefusedError && error <= QNetworkReply::UnknownNetworkError)
									     ? ChannelData::ChannelStatus::Error
									     : ChannelData::ChannelStatus::Expired;
				finishedCall({info});
				return;
			}
		}

		getJson(
			QUrl(CHANNEL_NAVERTV_GET_AUTH), "Naver TV get channel list",
			[=](const QJsonDocument &json) {
				QJsonObject object = json.object();
				QString representChannelId = object.value("representChannelId").toString();
				QJsonArray meta = object.value("meta").toArray();
				if (!meta.isEmpty()) {
					QList<QVariantMap> infos;

					// download image
					QList<QString> channelEmblemList;
					for (auto i : meta) {
						channelEmblemList.append(i.toObject()[CSTR_CHANNELEMBLEM].toString());
					}
					auto channelEmblems = getChannelEmblemSync(channelEmblemList);

					// save sub channels
					for (int i = 0, count = meta.count(); i < count; ++i) {
						auto object = meta[i].toObject();

						QVariantMap info;
						info[ChannelData::g_channelCookie] = srcInfo[ChannelData::g_channelCookie];
						info[ChannelData::g_channelToken] = token.accessToken;
						info[ChannelData::g_platformName] = channelName;
						info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
						info[ChannelData::g_subChannelId] = object[CSTR_CHANNELID].toString();
						info[ChannelData::g_nickName] = object[CSTR_CHANNELNAME].toString();
						info[ChannelData::g_shareUrl] = QString();
						info[ChannelData::g_viewers] = "0";
						info[ChannelData::g_viewersPix] = ChannelData::g_naverTvViewersIcon;
						if (i < channelEmblems.count()) {
							if (auto channelEmblem = channelEmblems.at(i); channelEmblem.first) {
								info[ChannelData::g_userIconCachePath] = channelEmblem.second;
							}
						}
						infos.append(info);
					}

					getUserInfo([=](bool) { finishedCall(infos); });
				} else {
					QVariantMap info = srcInfo;
					info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
					finishedCall({info});
				}
			},
			[=](bool expired, int code) {
				QVariantMap info = srcInfo;
				if (expired) {
					info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Expired;
				} else if (code == ERROR_PERMITEXCEPTION) {
					info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
				} else {
					info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
				}
				finishedCall({info});
			},
			this, nullptr, ApiId::Other, QVariantMap(), false, false);
	});
}

void PLSPlatformNaverTV::getCode(CodeCallback callback, const QString &url, bool redirectUrl)
{
	getAuth2(
		url, "Naver TV get code",
		[=](bool ok, QNetworkReply::NetworkError error, const QByteArray &body) {
			if (!ok) {
				callback(false, error, QString());
				return;
			}

			QString url;
			if (!findLocationUrl(url, body)) {
				PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get code failed, channel: %s, reason: location url not found.", getSubChannelName().toUtf8().constData());
				PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get code failed, channel: %s, reason: location url not found.",
					  pls_masking_person_info(getSubChannelName()).toUtf8().constData());
				callback(false, error, QString());
			} else if (url.contains(CHANNEL_NAVERTV_AUTHORIZE_REDIRECT) && url.contains(CSTR_CODE_E)) {
				callback(true, error, getUrlCode(url));
			} else {
				getCode(callback, url, true);
			}
		},
		!redirectUrl ? QVariantMap{{CSTR_CLIENT_ID, CSTR_CLIENT_ID_VALUE}, {CSTR_RESPONSE_TYPE, CSTR_CODE}, {CSTR_REDIRECT_URL, CHANNEL_NAVERTV_AUTHORIZE_REDIRECT}} : QVariantMap{});
}
void PLSPlatformNaverTV::getToken(TokenCallback callback)
{
	getCode(
		[=](bool ok, QNetworkReply::NetworkError error, const QString &code) {
			if (ok) {
				getToken(code, callback);
			} else {
				callback(false, error);
			}
		},
		CHANNEL_NAVERTV_AUTHORIZE, false);
}
void PLSPlatformNaverTV::getToken(const QString &code, TokenCallback callback)
{
	getAuth2(CHANNEL_NAVERTV_TOKEN, "Naver TV get token",
		 [=](bool ok, QNetworkReply::NetworkError error, const QByteArray &body) {
			 if (!ok) {
				 callback(false, error);
				 return;
			 }

			 QJsonParseError jsonError;
			 QJsonDocument respjson = QJsonDocument::fromJson(body, &jsonError);
			 if (jsonError.error == QJsonParseError::NoError) {
				 PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get token success, channel: %s", getSubChannelName().toUtf8().constData());
				 PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get token success, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
				 token = Token(respjson.object());
				 saveToken(token);
				 callback(true, error);
			 } else {
				 PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get token failed, channel: %s, reason: %s", getSubChannelName().toUtf8().constData(),
					      jsonError.errorString().toUtf8().constData());
				 PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get token failed, channel: %s, reason: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
					   jsonError.errorString().toUtf8().constData());
				 callback(false, error);
			 }
		 },
		 {{CSTR_CODE, code}, {CSTR_GRANT_TYPE, CSTR_GRANT_TYPE_AUTHORIZATION_CODE}, {CSTR_CLIENT_ID, CSTR_CLIENT_ID_VALUE}, {CSTR_CLIENT_SECRET, CSTR_CLIENT_SECRET_VALUE}});
}
void PLSPlatformNaverTV::getAuth2(const QString &url, const char *log, Auth2Callback callback, const QVariantMap &urlQueries)
{
	auto okCallback = [=](QNetworkReply *, int, QByteArray data, void *) {
		if (data.contains(CSTR_ERROR) && data.contains(CSTR_ERROR_DESCRIPTION)) {
			QJsonParseError jsonError;
			QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
			if (jsonError.error == QJsonParseError::NoError) {
				PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(),
					     respJson.object()[CSTR_ERROR_DESCRIPTION].toString().toUtf8().constData());
				PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
					  respJson.object()[CSTR_ERROR_DESCRIPTION].toString().toUtf8().constData());
				callback(false, QNetworkReply::NoError, QByteArray());
			} else {
				callback(true, QNetworkReply::NoError, data);
			}
		} else {
			callback(true, QNetworkReply::NoError, data);
		}
	};
	auto failCallback = [=](QNetworkReply *reply, int, QByteArray data, QNetworkReply::NetworkError error, void *) {
		if (!data.isEmpty()) {
			QJsonParseError jsonError;
			QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
			if (jsonError.error == QJsonParseError::NoError) {
				PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(),
					     respJson.object()[CSTR_ERROR_DESCRIPTION].toString().toUtf8().constData());
				PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
					  respJson.object()[CSTR_ERROR_DESCRIPTION].toString().toUtf8().constData());
				callback(false, error, QByteArray());
			} else {
				PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, getSubChannelName().toUtf8().constData());
				PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
				callback(false, error, QByteArray());
			}
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(), reply->errorString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  reply->errorString().toUtf8().constData());
			callback(false, error, QByteArray());
		}
	};

	PLSNetworkReplyBuilder builder(url);
	builder.setCookie(getChannelCookie());
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	builder.setQuerys(urlQueries);
	PLSHttpHelper::connectFinished(builder.get(), this, okCallback, failCallback);
}
void PLSPlatformNaverTV::getUserInfo(UserInfoCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get user info, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	auto okCallback = [=](QNetworkReply *, int, QByteArray data, void *) {
		QJsonParseError jsonError;
		QJsonDocument respjson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info success, channel: %s", getSubChannelName().toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get user info success, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			QJsonObject object = respjson.object();
			nickName = getValue(object, KEY_NICK_NAME, DEFAULT_NICK_NAME);
			headImageUrl = getValue(object, KEY_IMAGE_URL, CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL);
			callback(true);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info failed, channel: %s, reason: %s", getSubChannelName().toUtf8().constData(),
				     jsonError.errorString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info failed, channel: %s, reason: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  jsonError.errorString().toUtf8().constData());
			nickName = DEFAULT_NICK_NAME;
			headImageUrl = CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL;
			callback(false);
		}
	};
	auto failCallback = [=](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError networkError, void *) {
		QJsonParseError jsonError;
		QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			processFailed(
				"Naver TV get user info", respJson,
				[=](bool, int) {
					nickName = DEFAULT_NICK_NAME;
					headImageUrl = CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL;
					callback(false);
				},
				this, nullptr, networkError, statusCode, false, false, false, ApiId::Other);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info failed, channel: %s, reason: unknown", getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get user info failed, channel: %s, reason: unknown", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			nickName = DEFAULT_NICK_NAME;
			headImageUrl = CHANNEL_NAVERTV_DEFAULT_HEAD_IMAGE_URL;
			callback(false);
		}
	};

	NoDefualtHeaderNetworkReplyBuilder builder(QUrl(CHANNEL_NAVERTV_GET_USERINFO), HmacType::HT_PRISM);
	builder.setRawHeaders({{CSTR_REFERER, CSTR_OBJECT_URL}});
	builder.setCookie(getChannelCookie());
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	PLSHttpHelper::connectFinished(builder.get(), this, okCallback, failCallback);
}

void PLSPlatformNaverTV::getScheLives(LiveInfosCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid, bool popupNeedShow, bool popupGenericError)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get schedule live list, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get schedule live list, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getScheLives(30 * 60, 3 * 24 * 60 * 60, callback, receiver ? receiver : this, receiverIsValid, popupNeedShow, popupGenericError);
}
void PLSPlatformNaverTV::getScheLives(LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid, bool popupNeedShow, bool popupGenericError)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get schedule live list, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get schedule live list, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getScheLives(30 * 60, 3 * 24 * 60 * 60, callback, receiver ? receiver : this, receiverIsValid, popupNeedShow, popupGenericError);
}
void PLSPlatformNaverTV::getScheLives(int before, int after, LiveInfosCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid, bool popupNeedShow, bool popupGenericError)
{
	getScheLives(
		before, after, [callback](bool ok, int, const QList<LiveInfo> &scheLiveInfos) { callback(ok, scheLiveInfos); }, receiver ? receiver : this, receiverIsValid, popupNeedShow,
		popupGenericError);
}
void PLSPlatformNaverTV::getScheLives(int before, int after, LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid, bool popupNeedShow, bool popupGenericError)
{
	setlocale(LC_TIME, "en_US");
	time_t now = std::time(nullptr) + 9 * 60 * 60;

	struct tm tm;

	time_t fromTime = now - before;
	gmtime_s(&tm, &fromTime);
	char fromDate[128] = {0};
	strftime(fromDate, sizeof(fromDate), "%FT%T+09:00", &tm);

	time_t toTime = now + after;
	gmtime_s(&tm, &toTime);
	char toDate[128] = {0};
	strftime(toDate, sizeof(toDate), "%FT%T+09:00", &tm);

	getScheLives(QString::fromUtf8(fromDate), QString::fromUtf8(toDate), callback, receiver ? receiver : this, receiverIsValid, popupNeedShow, popupGenericError);
}
void PLSPlatformNaverTV::getScheLives(const QString &fromDate, const QString &toDate, LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid, bool popupNeedShow,
				      bool popupGenericError)
{
	QJsonObject reqJson;
	if (!fromDate.isEmpty()) {
		reqJson["fromDate"] = fromDate;
	}
	if (!toDate.isEmpty()) {
		reqJson["toDate"] = toDate;
	}

	postJson(
		Url(CHANNEL_NAVERTV_GET_LIVES.arg(PRISM_SSL).arg(getSubChannelId()), CHANNEL_NAVERTV_GET_LIVES.arg(PRISM_SSL).arg(pls_masking_person_info(getSubChannelId()))), reqJson,
		"Naver TV get schedule live list",
		[=](const QJsonDocument &json) {
			QList<LiveInfo> liveInfos;
			QJsonObject object = json.object();
			QJsonArray meta = object["meta"].toArray();
			for (auto i : meta) {
				liveInfos.append(LiveInfo(i.toObject()));
			}

			QList<QString> urls;
			for (auto &liveInfo : liveInfos) {
				urls.append(liveInfo.thumbnailImageUrl);
			}
			auto thumbnails = getChannelEmblemSync(urls);
			for (int i = 0; i < thumbnails.size(); ++i) {
				const auto &thumbnail = thumbnails[i];
				if (thumbnail.first) {
					liveInfos[i].thumbnailImagePath = thumbnail.second;
				}
			}

			callback(true, 0, liveInfos);
		},
		[=](bool, int code) { callback(false, code, QList<LiveInfo>()); }, receiver ? receiver : this, receiverIsValid, ApiId::Other, QVariantMap(), true, popupNeedShow, popupGenericError);
}
void PLSPlatformNaverTV::checkScheLive(int oliveId, CheckScheLiveCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV check schedule live is valid, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV check schedule live is valid, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getScheLives(
		[=](bool ok, const QList<LiveInfo> &scheLiveInfos) {
			if (ok) {
				if (std::find_if(scheLiveInfos.begin(), scheLiveInfos.end(), [=](const LiveInfo &liveInfo) { return liveInfo.oliveId == oliveId; }) != scheLiveInfos.end()) {
					callback(true, true);
				} else {
					callback(true, false);
				}
			} else {
				callback(false, false);
			}
		},
		this);
}

void PLSPlatformNaverTV::getStreamInfo(StreamInfoCallback callback, const char *log, bool popupGenericError)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "%s, channel: %s", log, getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s, channel: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getJson(
		Url(CHANNEL_NAVERTV_GET_STREAM_INFO.arg(PRISM_SSL).arg(getSubChannelId()), CHANNEL_NAVERTV_GET_STREAM_INFO.arg(PRISM_SSL).arg(pls_masking_person_info(getSubChannelId()))), log,
		[=](const QJsonDocument &json) {
			QJsonObject object = json.object();
			QString streamKey = object[CSTR_STREAMKEY].toString();
			QString serverUrl = object[CSTR_SERVERURL].toString();
			bool publishing = object[CSTR_PUBLISHING].toBool();
			QString channelId = object[CSTR_CHANNELID].toString();
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV stream info, stream key: %s, server url: %s, publishing: %s channel: %s", streamKey.toUtf8().constData(),
				    serverUrl.toUtf8().constData(), publishing ? "true" : "false", getSubChannelName().toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV stream info, stream key: %s, server url: %s, publishing: %s channel: %s", pls_masking_person_info(streamKey).toUtf8().constData(),
				 serverUrl.toUtf8().constData(), publishing ? "true" : "false", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			setStreamKey(streamKey.toStdString());
			setStreamServer(serverUrl.toStdString());
			PLS_PLATFORM_API->checkDirectPush();
			callback(true, publishing, 0);
		},
		[=](bool, int code) { callback(false, false, code); }, this, nullptr, ApiId::Other, QVariantMap(), true, true, popupGenericError);
}
void PLSPlatformNaverTV::checkStreamPublishing(CheckStreamPublishingCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV check stream publishing, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV check stream publishing, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
	getStreamInfo([=](bool ok, bool publishing, int code) { callback(ok, publishing, code); }, "Naver TV check stream publishing");
}

void PLSPlatformNaverTV::immediateStart(QuickStartCallback callback, bool isShareOnFacebook, bool isShareOnTwitter, bool popupGenericError)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV immediate live, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV immediate live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	auto quickStartCallback = [=](bool ok, int code) {
		if (!ok) {
			callback(false, code);
		} else if (!PLS_PLATFORM_API->isPrismLive()) {
			::startTimer(this, checkStatusTimer, CHECK_STATUS_TIMESPAN, &PLSPlatformNaverTV::onCheckStatus, false);
			callback(true, 0);
		} else {
			callback(true, 0);
		}
	};

	auto liveInfo = getLiveInfo();
	if (liveInfo->needUploadThumbnail()) {
		uploadImage(liveInfo->thumbnailImagePath, [=](bool ok, const QString &imageUrl) {
			if (ok) {
				liveInfo->thumbnailImageUrl = imageUrl;
				quickStart(quickStartCallback, isShareOnFacebook, isShareOnTwitter, popupGenericError);
			} else {
				callback(false, ERROR_OTHERS);
			}
		});
	} else {
		quickStart(quickStartCallback, isShareOnFacebook, isShareOnTwitter, popupGenericError);
	}
}
void PLSPlatformNaverTV::scheduleStart(LiveOpenCallback callback, bool isShareOnFacebook, bool isShareOnTwitter)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV schedule live, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV schedule live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	checkStreamPublishingRetryTimes = 0;
	deleteTimer(checkStreamPublishingTimer);
	deleteTimer(checkStatusTimer);

	scheduleStartWithCheck(callback, isShareOnFacebook, isShareOnTwitter);
}
void PLSPlatformNaverTV::scheduleStartWithCheck(LiveOpenCallback callback, bool isShareOnFacebook, bool isShareOnTwitter)
{
	auto liveOpenCallback = [=](bool ok, int code) {
		if (!ok) {
			callback(false, code);
		} else {
			getCommentOptions([=](bool ok, int code) {
				if (ok) {
					if (!PLS_PLATFORM_API->isPrismLive()) {
						::startTimer(this, checkStatusTimer, CHECK_STATUS_TIMESPAN, &PLSPlatformNaverTV::onCheckStatus, false);
						callback(true, 0);
					} else {
						callback(true, 0);
					}
				} else {
					PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());
					liveClose([=](bool) { callback(false, code); });

					checkStreamPublishingRetryTimes = 0;
					deleteTimer(checkStreamPublishingTimer);
					deleteTimer(checkStatusTimer);
				}
			});
		}
	};

	this->checkStreamPublishing([=](bool ok, bool publishing, int code) {
		if (!ok) {
			callback(false, code);
		} else if (publishing) {
			liveOpen(liveOpenCallback, isShareOnFacebook, isShareOnTwitter);
		} else if (checkStreamPublishingRetryTimes < CHECK_PUBLISHING_MAX_TIMES) {
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV stream is not publishing, wait 5s and retry, channel: %s", getSubChannelName().toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV stream is not publishing, wait 5s and retry, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			::startTimer(this, checkStreamPublishingTimer, CHECK_PUBLISHING_TIMESPAN, [=]() { scheduleStartWithCheck(callback, isShareOnFacebook, isShareOnTwitter); });
			++checkStreamPublishingRetryTimes;
		} else {
			callback(false, ERROR_OTHERS);
		}
	});
}

void PLSPlatformNaverTV::quickStart(QuickStartCallback callback, bool isShareOnFacebook, bool isShareOnTwitter, bool popupGenericError)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV quick start, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV quick start, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	auto liveInfo = getLiveInfo();

	QJsonObject reqJson;
	reqJson[CSTR_TITLE] = liveInfo->title;
	reqJson[CSTR_REHEARSALYN] = liveInfo->isRehearsal ? "Y" : "N";
	reqJson[CSTR_THUMBNAILIMAGEURL] = processThumbnailImageUrl(liveInfo->thumbnailImageUrl);
	if (!liveInfo->isRehearsal && (isShareOnFacebook || isShareOnTwitter)) {
		QJsonObject share;
		if (isShareOnFacebook) {
			// share["facebookToken"] = ;
		}

		if (isShareOnTwitter) {
			// share["twitterConsumerKey"] = ;
			// share["twitterConsumerSecret"] = ;
			// share["twitterToken"] = ;
			// share["twitterTokenSecret"] = ;
		}

		reqJson["share"] = share;
	}

	postJson(
		Url(CHANNEL_NAVERTV_QUICK_START.arg(PRISM_SSL).arg(getSubChannelId()), CHANNEL_NAVERTV_QUICK_START.arg(PRISM_SSL).arg(pls_masking_person_info(getSubChannelId()))), reqJson,
		"Post Naver TV quick start",
		[=](const QJsonDocument &respJson) {
			auto live = getLive();

			QJsonObject object = respJson.object();
			QJsonObject liveCommentOptions = object[CSTR_LIVECOMMENTOPTIONS].toObject();
			live->groupId = liveCommentOptions[CSTR_GROUPID].toString();
			live->ticketId = liveCommentOptions[CSTR_TICKETID].toString();
			live->objectId = liveCommentOptions[CSTR_OBJECTID].toString();
			live->templateId = liveCommentOptions[CSTR_TEMPLATEID].toString();
			live->idNo = liveCommentOptions[CSTR_IDNO].toString();
			live->manager = liveCommentOptions[CSTR_MANAGER].toBool();

			QJsonObject oliveModel = object[CSTR_OLIVEMODEL].toObject();
			live->oliveId = oliveModel[CSTR_ID].toInt();

			QJsonObject link = oliveModel[CSTR_LINK].toObject();
			live->pcLiveUrl = link[CSTR_PCLIVEURL].toString();
			live->mobileLiveUrl = link[CSTR_MOBILELIVEURL].toString();

			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, live->pcLiveUrl);
			callback(true, 0);
		},
		[=](bool, int code) { callback(false, code); }, this, nullptr, ApiId::Other, QVariantMap(), true, true, popupGenericError);
}

void PLSPlatformNaverTV::liveOpen(LiveOpenCallback callback, bool isShareOnFacebook, bool isShareOnTwitter)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV open live, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV open live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	if (getLiveId() < 0) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV open live failed, live id invalid, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV open live failed, live id invalid, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		callback(false, 0);
		return;
	}

	QJsonObject reqJson;
	if (isShareOnFacebook || isShareOnTwitter) {
		QJsonObject share;
		if (isShareOnFacebook) {
			// share["facebookToken"] = ;
		}

		if (isShareOnTwitter) {
			// share["twitterConsumerKey"] = ;
			// share["twitterConsumerSecret"] = ;
			// share["twitterToken"] = ;
			// share["twitterTokenSecret"] = ;
		}

		reqJson["share"] = share;
	}

	postJson(
		QUrl(CHANNEL_NAVERTV_OPEN.arg(PRISM_SSL).arg(getLiveId())), reqJson, "Post Naver TV live open",
		[=](const QJsonDocument &respJson) {
			auto live = getLive();

			QJsonObject object = respJson.object();
			QJsonObject currentStat = object[CSTR_CURRENTSTAT].toObject();
			live->watchCount = currentStat[CSTR_PV].toInt();

			QJsonObject link = object[CSTR_LINK].toObject();
			live->pcLiveUrl = link[CSTR_PCLIVEURL].toString();
			live->mobileLiveUrl = link[CSTR_MOBILELIVEURL].toString();
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, live->pcLiveUrl);
			callback(true, 0);
		},
		[=](bool, int code) { callback(false, code); }, this, nullptr, ApiId::LiveOpen);
}
void PLSPlatformNaverTV::liveClose(LiveCloseCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV close live, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV close live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	if (getLiveId() < 0) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV close live failed, live id invalid, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV close live failed, live id invalid, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		callback(true);
		return;
	}

	QJsonObject reqJson;

	postJson(
		QUrl(CHANNEL_NAVERTV_CLOSE.arg(PRISM_SSL).arg(getLiveId())), reqJson, "Naver TV close live",
		[=](const QJsonDocument &) {
			clearLive();
			callback(true);
		},
		[=](bool, int) { callback(false); }, this, nullptr, ApiId::Other, QVariantMap(), true, false);
}
void PLSPlatformNaverTV::liveModify(const QString &title, const QString &thumbnailImageUrl, LiveModifyCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV modify live info, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV modify live info, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	QJsonObject reqJson;
	reqJson[CSTR_TITLE] = title;
	reqJson[CSTR_THUMBNAILIMAGEURL] = thumbnailImageUrl;

	postJson(
		QUrl(CHANNEL_NAVERTV_MODIFY.arg(PRISM_SSL).arg(getLiveId())), reqJson, "Naver TV modify live info",
		[=](const QJsonDocument &respJson) {
			QJsonObject object = respJson.object();
			auto liveInfo = getLiveInfo();
			liveInfo->title = object[CSTR_TITLE].toString();
			liveInfo->thumbnailImageUrl = object[CSTR_THUMBNAIL].toString();
			callback(true);
		},
		[=](bool, int) { callback(false); });
}
void PLSPlatformNaverTV::liveStatus(LiveStatusCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get live status, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get live status, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	if (getLiveId() < 0) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get live status failed, live id invalid, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV get live status failed, live id invalid, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		callback(false, 0, 0, 0, false);
		return;
	}

	auto live = getLive();

	QVariantMap headers;
	headers[CSTR_TICKET] = live->ticketId;
	headers[CSTR_OBJECTID] = live->objectId;
	headers[CSTR_TEMPLATEID] = live->templateId;
	headers[CSTR_LANG] = LANGUAGE;

	getJson(
		QUrl(CHANNEL_NAVERTV_STATUS.arg(PRISM_SSL).arg(getLiveId())), "Naver TV get live status",
		[=](const QJsonDocument &respJson) {
			QJsonObject object = respJson.object();
			QString status = object[CSTR_STATUS].toString();
			live->watchCount = (qint64)object[CSTR_PV].toDouble();
			live->likeCount = (qint64)object[CSTR_LIKECOUNT].toDouble();
			live->commentCount = (qint64)object[CSTR_COMMENTCOUNT].toDouble();
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_viewers, QString::number(live->watchCount));
			callback(true, live->watchCount, live->likeCount, live->commentCount, status == "CLOSED");
		},
		[=](bool, int) { callback(false, 0, 0, 0, false); }, this, nullptr, ApiId::Other, headers, true, false, false);
}

void PLSPlatformNaverTV::getCommentOptions(CommentOptionsCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV get live comment options, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV get live comment options, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getJson(
		QUrl(CHANNEL_NAVERTV_COMMENT_OPTIONS.arg(getLiveId())), "Naver TV get live comment options",
		[=](const QJsonDocument &respJson) {
			auto live = getLive();
			QJsonObject object = respJson.object();
			live->groupId = object[CSTR_GROUPID].toString();
			live->ticketId = object[CSTR_TICKETID].toString();
			live->objectId = object[CSTR_OBJECTID].toString();
			live->templateId = object[CSTR_TEMPLATEID].toString();
			live->idNo = object[CSTR_IDNO].toString();
			live->manager = object[CSTR_MANAGER].toBool();
			callback(true, 0);
		},
		[=](bool, int code) { callback(false, code); });
}

void PLSPlatformNaverTV::uploadImage(const QString &imageFilePath, UploadImageCallback callback)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV upload image, channel: %s", pls_masking_person_info(getSubChannelName().toUtf8().constData()));

	QFile imageFile(imageFilePath);
	if (!imageFile.open(QFile::ReadOnly)) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, open file failed, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, open file failed, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		callback(false, QString());
		return;
	}

	auto okCallback = [=](QNetworkReply *, int, QByteArray data, void *) {
		QJsonParseError jsonError;
		QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error != QJsonParseError::NoError) {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s, reason: %s", getSubChannelName().toUtf8().constData(),
				     jsonError.errorString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s, reason: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  jsonError.errorString().toUtf8().constData());
			callback(false, QString());
			return;
		}

		QJsonObject object = respJson.object();
		QString imageUrl = object["imageUrl"].toString();
		if (!imageUrl.isEmpty()) {
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image success, channel: %s, image url: %s", getSubChannelName().toUtf8().constData(), imageUrl.toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV upload image success, channel: %s, image url: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				 pls_masking_person_info(imageUrl).toUtf8().constData());
			callback(true, imageUrl);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s, image url is empty", getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s, image url is empty", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			callback(false, QString());
		}
	};
	auto failCallback = [=](QNetworkReply *, int, QByteArray data, QNetworkReply::NetworkError, void *) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s", getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "Naver TV upload image failed, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		callback(false, QString());
	};

	QByteArray imageData = imageFile.readAll();
	QString fileName = PLSHttpHelper::getFileName(imageFilePath);
	QString boundary = QString::asprintf("-------------------%u%u", GetTickCount(), GetTickCount());
	QString contentType = QString("multipart/form-data; boundary=%1").arg(boundary);
	QString bodyHeader = QString("--%1\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"%2\"\r\nContent-Type: %3\r\n\r\n")
				     .arg(boundary, fileName, PLSHttpHelper::imageFileNameToContentType(fileName));
	QString bodyFooter = QString("\r\n--%1--\r\n").arg(boundary);
	QByteArray body = bodyHeader.toUtf8() + imageData + bodyFooter.toUtf8();

	PLSHmacNetworkReplyBuilder builder(CHANNEL_NAVERTV_UPLOAD_IMAGE.arg(getSubChannelId()), HmacType::HT_PRISM);
	builder.setCookie(getChannelCookie());
	builder.setContentType(contentType);
	builder.setBody(body);
	QNetworkReply *reply = builder.post();
	reply->setProperty("urlMasking", CHANNEL_NAVERTV_UPLOAD_IMAGE.arg(pls_masking_person_info(getSubChannelId())));
	PLSHttpHelper::connectFinished(reply, this, okCallback, failCallback);
}

bool PLSPlatformNaverTV::isPrimary() const
{
	return primary;
}
QString PLSPlatformNaverTV::getSubChannelId() const
{
	return m_mapInitData[ChannelData::g_subChannelId].toString();
}
QString PLSPlatformNaverTV::getSubChannelName() const
{
	return m_mapInitData[ChannelData::g_nickName].toString();
}
int PLSPlatformNaverTV::getLiveId() const
{
	return live ? live->oliveId : -1;
}
bool PLSPlatformNaverTV::isRehearsal() const
{
	return liveInfo ? liveInfo->isRehearsal : false;
}
bool PLSPlatformNaverTV::isScheduleLive() const
{
	return liveInfo ? liveInfo->isScheLive : false;
}
// bool PLSPlatformNaverTV::isLiveStarted() const
// {
// 	return live ? live->liveStarted : false;
// }
// bool PLSPlatformNaverTV::isAllLiveStarted() const
// {
// 	return live ? live->allLiveStarted : false;
// }
bool PLSPlatformNaverTV::isKnownError(int code) const
{
	return code == ERROR_AUTHEXCEPTION || code == ERROR_LIVENOTFOUND || code == ERROR_PERMITEXCEPTION || code == ERROR_LIVESTATUSEXCEPTION || code == ERROR_START30MINEXCEPTION ||
	       code == ERROR_ALREADYONAIR || code == ERROR_PAIDSPONSORSHIPINFO || code == ERROR_NETWORK_ERROR;
}

const PLSPlatformNaverTV::Token *PLSPlatformNaverTV::getToken() const
{
	return &token;
}
PLSPlatformNaverTV::LiveInfo *PLSPlatformNaverTV::getLiveInfo() const
{
	return (!liveInfo) ? getNewLiveInfo() : liveInfo;
}
PLSPlatformNaverTV::LiveInfo *PLSPlatformNaverTV::getNewLiveInfo() const
{
	deleteObject(liveInfo);
	liveInfo = new LiveInfo(*getSelectedLiveInfo());
	return liveInfo;
}
void PLSPlatformNaverTV::clearLiveInfo()
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV clear live info, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV clear live info, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
	selectedScheLiveInfoId = -1;
	deleteObject(liveInfo);
}

PLSPlatformNaverTV::Live *PLSPlatformNaverTV::getLive() const
{
	if (live) {
		return live;
	}

	live = new Live();
	return live;
}
void PLSPlatformNaverTV::clearLive()
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV clear live, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV clear live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
	isLiveEndedByMqtt = false;
	deleteObject(live);
}

const PLSPlatformNaverTV::LiveInfo *PLSPlatformNaverTV::getSelectedLiveInfo() const
{
	auto scheLiveInfo = getScheLiveInfo(selectedScheLiveInfoId);
	if (scheLiveInfo) {
		return scheLiveInfo;
	}
	return getImmediateLiveInfo();
}
const PLSPlatformNaverTV::LiveInfo *PLSPlatformNaverTV::getScheLiveInfo(int scheLiveId) const
{
	for (auto &scheLive : scheLiveList) {
		if (scheLive.oliveId == scheLiveId) {
			return &scheLive;
		}
	}
	return nullptr;
}
const PLSPlatformNaverTV::LiveInfo *PLSPlatformNaverTV::getImmediateLiveInfo() const
{
	if (immediateLive.title.isEmpty()) {
		initImmediateLiveInfoTitle();
	}
	return &immediateLive;
}
const QList<PLSPlatformNaverTV::LiveInfo> &PLSPlatformNaverTV::getScheLiveInfoList() const
{
	return scheLiveList;
}
bool PLSPlatformNaverTV::isLiveInfoModified(int oliveId) const
{
	auto liveInfo = getLiveInfo();
	if (liveInfo->oliveId != oliveId) {
		return true;
	}
	return false;
}
bool PLSPlatformNaverTV::isLiveInfoModified(int oliveId, const QString &title, const QString &thumbnailImagePath) const
{
	auto liveInfo = getLiveInfo();
	if ((liveInfo->oliveId != oliveId) || (liveInfo->title != title) || (liveInfo->thumbnailImagePath != thumbnailImagePath)) {
		return true;
	}
	return false;
}
void PLSPlatformNaverTV::updateLiveInfo(const QList<LiveInfo> &scheLiveInfos, int oliveId, bool isRehearsal, const QString &title, const QString &thumbnailImagePath, UpdateLiveInfoCallback callback)
{
	if (selectedScheLiveInfoId != oliveId) {
		selectedScheLiveInfoId = oliveId;
		scheLiveList = scheLiveInfos;
	}

	auto liveInfo = getNewLiveInfo();
	liveInfo->isRehearsal = isRehearsal;
	liveInfo->title = titleIsEmtpy(title) ? getImmediateLiveInfo()->title : title;
	if (liveInfo->thumbnailImagePath != thumbnailImagePath) {
		liveInfo->thumbnailImagePath = thumbnailImagePath;
		liveInfo->thumbnailImageUrl.clear();
	}

	// save title to prism api
	setTitle(liveInfo->title.toStdString());

	if (PLS_PLATFORM_API->isPrepareLive()) {
		getStreamInfo(
			[=](bool ok, bool, int code) {
				if (ok) {
					if (!liveInfo->isScheLive) {
						// immediate live
						immediateStart(
							[=](bool ok, int code) {
								if (ok) {
									callback(ok, 0);
								} else {
									clearLive();
									callback(ok, code);
								}
							},
							false, false, false);
					} else {
						// scheduled live
						getLive()->oliveId = liveInfo->oliveId;
						callback(true, 0);
					}
				} else {
					callback(false, code);
				}
			},
			"Naver TV get stream key", false);
	} else {
		callback(true, 0);
	}
}
void PLSPlatformNaverTV::initImmediateLiveInfoTitle() const
{
	immediateLive.title = tr("LiveInfo.live.title.suffix").arg(getSubChannelName());
}

void PLSPlatformNaverTV::showScheLiveNotice(const LiveInfo &liveInfo)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV show schedule live notice, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV show schedule live notice, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	isScheLiveNoticeShown = true;

	PLSScheLiveNotice scheLiveNotice(this, liveInfo.title, PLSDateFormate::timeStampToUTCString(liveInfo.startDate / 1000), App()->getMainView());
	if (scheLiveNotice.exec() != PLSScheLiveNotice::Accepted) {
		return;
	}

	// getScheLives([=](bool ok, const QList<LiveInfo> &) {
	// 	if (ok) {
	// 		PLSCHANNELS_API->toStartBroadcast();
	// 		setSelectedScheLiveInfoId(liveInfo.oliveId);
	// 		clearLiveInfo();
	// 	} else {
	// 	}
	// });
}

void PLSPlatformNaverTV::onCheckStatus()
{
	liveStatus([=](bool ok, int, int, int, bool closed) {
		if (ok && closed) {
			//PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV server end live, channel: %s", getSubChannelName().toUtf8().constData());

			checkStreamPublishingRetryTimes = 0;
			deleteTimer(checkStreamPublishingTimer);
			deleteTimer(checkStatusTimer);

			clearLiveInfo();
			clearLive();
			PLS_LIVE_INFO_KR(MODULE_PLATFORM_NAVERTV, "FinishedBy Naver TV server end live, channel: %s", getSubChannelName().toUtf8().constData());
			PLS_LIVE_INFO(MODULE_PLATFORM_NAVERTV, "FinishedBy Naver TV server end live, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			PLSCHANNELS_API->toStopBroadcast();
		}
	});
}

void PLSPlatformNaverTV::onShowScheLiveNotice()
{
	if (isScheLiveNoticeShown) {
		return;
	}

	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV check schedule live notice, channel: %s", getSubChannelName().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV check schedule live notice, channel: %s", pls_masking_person_info(getSubChannelName()).toUtf8().constData());

	getScheLives(
		0, 30 * 60,
		[=](bool ok, const QList<LiveInfo> &scheLiveInfos) {
			if (!ok || scheLiveInfos.isEmpty()) {
				return;
			}

			qint64 current = QDateTime::currentMSecsSinceEpoch();
			const LiveInfo *liveInfo = nullptr;
			for (auto &sli : scheLiveInfos) {
				if (!liveInfo) {
					liveInfo = &sli;
				} else if (qAbs(current - sli.startDate) < qAbs(current - liveInfo->startDate)) {
					liveInfo = &sli;
				}
			}

			if (liveInfo) {
				showScheLiveNotice(*liveInfo);
			}
		},
		this, nullptr, false);
}

QString PLSPlatformNaverTV::getChannelInfo(const QString &key, const QString &defaultValue) const
{
	auto iter = m_mapInitData.constFind(key);
	if (iter != m_mapInitData.constEnd()) {
		return iter.value().toString();
	}
	return defaultValue;
}
void PLSPlatformNaverTV::getJson(const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok, std::function<void(bool expired, int code)> fail, QObject *receiver,
				 ReceiverIsValid receiverIsValid, ApiId apiId, const QVariantMap &headers, bool expiredNotify, bool popupNeedShow, bool popupGenericError)
{
	auto okCallback = [=](QNetworkReply *, int statusCode, QByteArray data, void *) {
		QJsonParseError jsonError;
		QJsonDocument respjson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "%s success, channel: %s", log, getSubChannelName().toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s success, channel: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			ok(respjson);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(), jsonError.errorString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  jsonError.errorString().toUtf8().constData());
			bool expired = false;
			int code = processError(expired, receiver, receiverIsValid, QNetworkReply::NoError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
			fail(false, code);
		}
	};
	auto failCallback = [=](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError networkError, void *) {
		QJsonParseError jsonError;
		QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			processFailed(log, respJson, fail, receiver, receiverIsValid, networkError, statusCode, expiredNotify, popupNeedShow, popupGenericError, apiId);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			bool expired = false;
			int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
			fail(false, code);
		}
	};

	PLSHmacNetworkReplyBuilder builder(url.url, HmacType::HT_PRISM);
	builder.setRawHeaders(headers);
	builder.setCookie(getChannelCookie());
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	QNetworkReply *reply = builder.get();
	if (!url.maskingUrl.isEmpty())
		reply->setProperty("urlMasking", url.maskingUrl);
	PLSHttpHelper::connectFinished(reply, receiver ? receiver : this, okCallback, failCallback);
}
void PLSPlatformNaverTV::postJson(const Url &url, const QJsonObject &reqJson, const char *log, std::function<void(const QJsonDocument &)> ok, std::function<void(bool expired, int code)> fail,
				  QObject *receiver, ReceiverIsValid receiverIsValid, ApiId apiId, const QVariantMap &headers, bool expiredNotify, bool popupNeedShow, bool popupGenericError)
{
	auto okCallback = [=](QNetworkReply *, int statusCode, QByteArray data, void *) {
		QJsonParseError jsonError;
		QJsonDocument json = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "%s success, channel: %s", log, getSubChannelName().toUtf8().constData());
			PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s success, channel: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			ok(json);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(), jsonError.errorString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  jsonError.errorString().toUtf8().constData());
			bool expired = false;
			int code = processError(expired, receiver, receiverIsValid, QNetworkReply::NoError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
			fail(false, code);
		}
	};
	auto failCallback = [=](QNetworkReply *, int statusCode, QByteArray data, QNetworkReply::NetworkError networkError, void *) {
		QJsonParseError jsonError;
		QJsonDocument respJson = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error == QJsonParseError::NoError) {
			processFailed(log, respJson, fail, receiver, receiverIsValid, networkError, statusCode, expiredNotify, popupNeedShow, popupGenericError, apiId);
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
			bool expired = false;
			int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
			fail(false, code);
		}
	};

	PLSHmacNetworkReplyBuilder builder(url.url, HmacType::HT_PRISM);
	builder.setRawHeaders(headers);
	builder.setCookie(getChannelCookie());
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	builder.setJsonObject(reqJson);
	QNetworkReply *reply = builder.post();
	if (!url.maskingUrl.isEmpty())
		reply->setProperty("urlMasking", url.maskingUrl);
	PLSHttpHelper::connectFinished(reply, receiver ? receiver : this, okCallback, failCallback);
}
void PLSPlatformNaverTV::processFailed(const char *log, const QJsonDocument &respJson, std::function<void(bool expired, int code)> fail, QObject *receiver, ReceiverIsValid receiverIsValid,
				       QNetworkReply::NetworkError networkError, int statusCode, bool expiredNotify, bool popupNeedShow, bool popupGenericError, ApiId apiId)
{
	QJsonObject object = respJson.object();
	if (object.contains(CSTR_CODE) && object.contains(CSTR_MESSAGE)) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(), object.value(CSTR_MESSAGE).toString().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
			  object.value(CSTR_MESSAGE).toString().toUtf8().constData());

		bool expired = false;
		int code = object.value(CSTR_CODE).toInt();
		code = processError(expired, receiver, receiverIsValid, networkError, statusCode, code, expiredNotify, popupNeedShow, popupGenericError, apiId);
		fail(expired, code);
	} else if (object.contains(CSTR_ERROR_CODE) && object.contains(CSTR_ERROR_MESSAGE)) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(),
			     object.value(CSTR_ERROR_MESSAGE).toString().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
			  object.value(CSTR_ERROR_MESSAGE).toString().toUtf8().constData());

		bool expired = false;
		int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
		fail(expired, code);
	} else if (object.contains(CSTR_RTN_CD) && object.contains(CSTR_RTN_MSG)) {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(), object.value(CSTR_RTN_MSG).toString().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
			  object.value(CSTR_RTN_MSG).toString().toUtf8().constData());

		bool expired = false;
		int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
		fail(expired, code);
	} else if (object.contains(CSTR_HEADER)) {
		QJsonObject header = object[CSTR_HEADER].toObject();
		if (header.contains(CSTR_MESSAGE) && header.contains(CSTR_CODE)) {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, getSubChannelName().toUtf8().constData(),
				     header.value(CSTR_MESSAGE).toString().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: %s", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
				  header.value(CSTR_MESSAGE).toString().toUtf8().constData());
		} else {
			PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, getSubChannelName().toUtf8().constData());
			PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());
		}

		bool expired = false;
		int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
		fail(expired, code);
	} else {
		PLS_ERROR_KR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, getSubChannelName().toUtf8().constData());
		PLS_ERROR(MODULE_PLATFORM_NAVERTV, "%s failed, channel: %s, reason: unknown", log, pls_masking_person_info(getSubChannelName()).toUtf8().constData());

		bool expired = false;
		int code = processError(expired, receiver, receiverIsValid, networkError, statusCode, ERROR_OTHERS, expiredNotify, popupNeedShow, popupGenericError, apiId);
		fail(false, code);
	}
}
int PLSPlatformNaverTV::processError(bool &expired, QObject *receiver, ReceiverIsValid receiverIsValid, QNetworkReply::NetworkError networkError, int statusCode, int code, bool expiredNotify,
				     bool popupNeedShow, bool popupGenericError, ApiId apiId)
{
	if ((networkError >= QNetworkReply::ConnectionRefusedError) && (networkError <= QNetworkReply::UnknownNetworkError)) {
		if (popupNeedShow && ((receiver == this) || !receiver || !receiverIsValid || receiverIsValid(receiver))) {
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("login.check.note.network"));
		}
		return ERROR_NETWORK_ERROR;
	}

	if (statusCode == 401 && code == ERROR_AUTHEXCEPTION) {
		expired = true;
		emit apiRequestFailed(true);
		tokenExpired(expiredNotify, popupNeedShow);
		return code;
	}

	emit apiRequestFailed(false);
	if (popupNeedShow) {
		switch (code) {
		case ERROR_LIVENOTFOUND:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("broadcast.invalid.schedule"));
			break;
		case ERROR_PERMITEXCEPTION:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("main.message.error.navertv.service.10003"));
			break;
		case ERROR_LIVESTATUSEXCEPTION:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("broadcast.no.longer.valid"));
			break;
		case ERROR_START30MINEXCEPTION:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("main.message.error.navertv.service.10007"));
			break;
		case ERROR_ALREADYONAIR:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("main.message.error.navertv.service.10008"));
			break;
		case ERROR_PAIDSPONSORSHIPINFO:
			PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("main.message.error.navertv.service.10010"));
			break;
		default:
			if (popupGenericError) {
				if (apiId == ApiId::LiveOpen) {
					PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("main.message.error.navertv.service.unknown"));
				} else {
					PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("server.unknown.error"));
				}
			}
			break;
		}
	}
	return code;
}

void PLSPlatformNaverTV::tokenExpired(bool expiredNotify, bool popupNeedShow)
{
	PLS_INFO_KR(MODULE_PLATFORM_NAVERTV, "Naver TV platform token expired, channel: %s, UUID = %s.", getSubChannelName().toUtf8().constData(), getChannelUUID().toUtf8().constData());
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "Naver TV platform token expired, channel: %s, UUID = %s.", pls_masking_person_info(getSubChannelName()).toUtf8().constData(),
		 getChannelUUID().toUtf8().constData());

	if (popupNeedShow) {
		if (PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("Live.Check.LiveInfo.Refresh.NaverTV.Expired")) == PLSAlertView::Button::NoButton) {
			expiredNotify = false;
		}
	}

	if (expiredNotify) {
		PLSCHANNELS_API->channelExpired(getChannelUUID(), !popupNeedShow);
	}

	emit closeDialogByExpired();

	clearToken();
}
