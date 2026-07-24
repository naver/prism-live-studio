#include "PLSChannelDataHandlerFunctions.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRunnable>
#include <QThreadPool>
#include <QUrl>

#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "pls-channel-const.h"

#include "PLSChannelDataAPI.h"
#include "frontend-api.h"

#include "../../PLSPlatformApi/NCB2B/PLSNCB2BDataHandler.h"
#include "../../PLSPlatformApi/band/PLSBandDataHandler.h"
#include "../../PLSPlatformApi/facebook/PLSFacebookDataHandler.h"
#include "../../PLSPlatformApi/naver-shopping-live/PLSNaverShoppingLIVEDataHandler.h"
#include "../../PLSPlatformApi/navertv/PLSNaverTVDataHandler.h"
#include "PLSChzzkDataHandler.h"

#include "PLSAfreecaTVDataHandler.h"
#include "PLSChannelDataHandler.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSPlatformApi.h"
#include "libhttp-client.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"

using namespace ChannelData;
using namespace common;
extern void httpRequestHead(QVariantMap &headMap, bool hasGacc);

void registerAllPlatforms()
{
	PLSCHANNELS_API->registerPlatformHandler(new PLSAfreecaTVDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new TwitchDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new YoutubeHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSNaverTVDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSBandDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSFacebookDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSNaverShoppingLIVEDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSNCB2BDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSChzzkDataHandler);
}

QString getYoutubeImageUrl(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}

	return list[0].toMap()["snippet"].toMap()["thumbnails"].toMap()["high"].toMap()["url"].toString();
}

QString getYoutubeName(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}

	return list[0].toMap()["snippet"].toMap()["title"].toString();
}

QString getYoutubeFirstID(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}
	return list[0].toMap()["id"].toString();
}

bool isItemsExists(const QVariantMap &src)
{
	return src.contains("items");
}

bool isChannelItemEmpty(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return true;
	}

	return list[0].toMap().isEmpty();
}

QString getYoutubePriacyStatus(const QVariantMap &src)
{
	auto list = src.value("items").toList();
	if (list.isEmpty()) {
		return "";
	}
	auto mapS = list[0].toMap()["status"].toMap();
	return mapS["privacyStatus"].toString();
}

bool isTokenValid(const QString &mSrcUUID)
{
	const auto channelInfo = PLSCHANNELS_API->getChannelInfo(mSrcUUID);
	return isTokenValid(channelInfo);
}

bool isTokenValid(const QVariantMap &channelInfo)
{
	const static int expiresDiff = 3 * 60; //3min
	auto lastTokenCreateTime = getInfo(channelInfo, g_createTime, QDateTime());
	int expiresSeconds = getInfo(channelInfo, g_expires_in, g_defaultExpiresSeconds);
	if (lastTokenCreateTime.isValid()) {
		auto differ = lastTokenCreateTime.secsTo(QDateTime::currentDateTime());
		return ((expiresSeconds - differ - expiresDiff) > 0);
	}
	return false;
}

QJsonObject createJsonArrayFromInfo(const QString &uuid)
{

	QJsonObject obj, orgData;
	if (const auto &info = PLSCHANNELS_API->getChannelInfo(uuid); !info.isEmpty()) {
		obj = QJsonObject::fromVariantMap(info);
		obj.remove(g_channelUserStatus);
		obj.remove(g_customUserDataSeq);
		obj.remove(g_isUpdated);
		obj.remove(g_displayState);
		obj.remove(g_displayOrder);
		obj.remove(g_channelDualOutput);
		obj.insert("resolution", "720");
		obj.insert("bitrate", "2000");
		obj.insert("framerate", "30");
		obj.insert("interval", 2);
		orgData.insert(g_customData, obj);
	} else {
		PLS_INFO(CHANNELDATAHANDLER, "getChanelInfoRef return null,uuid is %d", uuid.toUtf8().constData());
		if (uuid.isEmpty()) {
			QVariantMap map = {{g_isUseNewAPI, true}};
			obj = QJsonObject::fromVariantMap(map);
			orgData.insert(g_customData, obj);
		}
	}

	return orgData;
}

QVariantMap createPrismHeader()
{
	QVariantMap headerMap;
	headerMap[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_VALUE;
	httpRequestHead(headerMap, true);
	return headerMap;
}

bool RTMPAddToPrism(const QString &uuid)
{
	PRE_LOG_MSG_STEP("add rtmp begin", g_addChannelStep, INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::addingHold);

	if (PLSCHANNELS_API->isChannelInfoExists(uuid)) {

		PLSCHANNELS_API->release();
		auto headerMap = createPrismHeader();
		if (headerMap.isEmpty()) {
			PLSCHANNELS_API->acquire();
			PRE_LOG_MSG_STEP("add rtmp failed", g_addChannelStep, ERROR)
			return false;
		}
		pls::http::Request request;
		request.rawHeaders(headerMap);
		request.timeout(PRISM_NET_REQUEST_TIMEOUT);
		auto obj = createJsonArrayFromInfo(uuid);
		request.body(obj);

		request.jsonContentType();

		request.cookie(createPrismCookie());
		request.url(PLS_RTMP_ADD_V2.arg(PRISM_SSL));
		request.hmacKey(PLS_PC_HMAC_KEY.toUtf8());
		request.withLog();
		request.id(CUSTOM_RTMP);

		auto handleSuccess = [uuid](const pls::http::Reply &reply) {
			PLSCHANNELS_API->acquire();
			auto &lastInfo = PLSCHANNELS_API->getChanelInfoRef(uuid);
			auto jsonDoc = QJsonDocument::fromJson(reply.data());
			auto jsonMap = jsonDoc.toVariant().toMap();
			auto seq = jsonMap[g_customUserDataSeq].toString();
			auto customData = jsonMap[g_customData].toMap();
			addToMap(lastInfo, customData);
			lastInfo[g_customUserDataSeq] = seq;
			FinishTaskReleaser finishAdd(uuid);
			PRE_LOG_MSG_STEP("add rtmp success:" + seq, g_addChannelStep, INFO)
		};

		auto handleFail = [uuid](const pls::http::Reply &reply) {
			PLSCHANNELS_API->acquire();
			PLSCHANNELS_API->removeChannelInfo(uuid, false, false);
			FinishTaskReleaser finishUpdate(uuid);
			PRE_LOG_MSG_STEP("add rtmp failed", g_addChannelStep, ERROR)
			ChannelsNetWorkPretestWithAlerts(reply);
		};

		request.okResult(handleSuccess);
		request.failResult(handleFail);
		request.receiver(PLSCHANNELS_API);
		request.method(pls::http::Method::Post);
		pls::http::request(request);
	}
	return true;
}

bool AddOrgDataToNewApi(const QString &uuid, bool bAddFlag)
{
	if (bAddFlag) {
		PLS_INFO(CHANNELDATAHANDLER, "Add flag to new API");
	} else {
		PLS_INFO(CHANNELDATAHANDLER, "Add OrgData to new API");
	}
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PLSCHANNELS_API->release();
	auto headerMap = createPrismHeader();
	if (headerMap.isEmpty()) {
		endRefresh();
		if (bAddFlag) {
			PLS_ERROR(CHANNELDATAHANDLER, "Add flag to new API failed");
		} else {
			PLS_ERROR(CHANNELDATAHANDLER, "Add OrgData to new API failed");
		}
		return false;
	}
	pls::http::Request request;
	request.rawHeaders(headerMap);
	request.timeout(PRISM_NET_REQUEST_TIMEOUT);
	QJsonObject obj;
	if (bAddFlag) {
		obj = createJsonArrayFromInfo("");
	} else {
		obj = createJsonArrayFromInfo(uuid);
	}

	request.body(obj);

	request.jsonContentType();

	request.cookie(createPrismCookie());
	request.url(PLS_RTMP_ADD_V2.arg(PRISM_SSL));
	request.hmacKey(PLS_PC_HMAC_KEY.toUtf8());
	request.withLog();
	request.id(CUSTOM_RTMP);

	auto handleSuccess = [bAddFlag, uuid](const pls::http::Reply &reply) {
		endRefresh();
		auto jsonDoc = QJsonDocument::fromJson(reply.data());
		auto jsonMap = jsonDoc.toVariant().toMap();
		auto seq = jsonMap[g_customUserDataSeq].toString();
		if (bAddFlag) {
			PLS_INFO(CHANNELDATAHANDLER, "Add flag to new API success,seq is %s", seq.toUtf8().constData());
		} else {
			PLS_INFO(CHANNELDATAHANDLER, "Add OrgData to new API success,seq is %s", seq.toUtf8().constData());
			auto &lastInfo = PLSCHANNELS_API->getChanelInfoRef(uuid);
			auto customData = jsonMap[g_customData].toMap();
			addToMap(lastInfo, customData);
			lastInfo[g_customUserDataSeq] = seq;
		}
	};

	auto handleFail = [bAddFlag](const pls::http::Reply &reply) {
		endRefresh();
		if (bAddFlag) {
			PLS_ERROR(CHANNELDATAHANDLER, "Add flag to new API failed");
		} else {
			PLS_ERROR(CHANNELDATAHANDLER, "Add OrgData to new API failed");
		}
		ChannelsNetWorkPretestWithAlerts(reply);
	};

	request.okResult(handleSuccess);
	request.failResult(handleFail);
	request.receiver(PLSCHANNELS_API);
	request.method(pls::http::Method::Post);
	pls::http::request(request);

	return true;
}

bool RTMPUpdateToPrism(const QString &uuid)
{

	PRE_LOG_MSG_STEP("Update RTMP Begin ", g_updateChannelStep, INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);

	if (PLSCHANNELS_API->isChannelInfoExists(uuid)) {
		PLSCHANNELS_API->release();
		auto headerMap = createPrismHeader();
		if (headerMap.isEmpty()) {
			PLSCHANNELS_API->acquire();
			PLSCHANNELS_API->recoverInfo(uuid);
			PRE_LOG_MSG_STEP("Update RTMP failed for Prism Header is empty ", g_updateChannelStep, ERROR)
			return false;
		}

		pls::http::Request request;
		request.rawHeaders(headerMap);
		request.timeout(PRISM_NET_REQUEST_TIMEOUT);
		auto obj = createJsonArrayFromInfo(uuid);
		request.body(obj);
		request.jsonContentType();

		request.cookie(createPrismCookie());
		auto sqNo = PLSCHANNELS_API->getValueOfChannel(uuid, g_customUserDataSeq, QString());
		QString url = QString(PLS_RTMP_MODIFY_V2).arg(PRISM_SSL, sqNo);
		request.url(url);
		request.hmacKey(PLS_PC_HMAC_KEY.toUtf8());
		request.withLog();
		request.id(CUSTOM_RTMP);

		auto handleSuccess = [uuid](const pls::http::Reply &reply) {
			PLSCHANNELS_API->acquire();
			auto &lastInfo = PLSCHANNELS_API->getChanelInfoRef(uuid);
			auto jsonDoc = QJsonDocument::fromJson(reply.data());
			auto jsonMap = jsonDoc.toVariant().toMap();
			auto customData = jsonMap[g_customData].toMap();
			addToMap(lastInfo, customData);
			PLSCHANNELS_API->clearBackup(uuid);
			PRE_LOG_MSG_STEP("Update RTMP success ", g_updateChannelStep, INFO)
			FinishTaskReleaser finishUpdate(uuid);
		};

		auto handleFail = [uuid](const pls::http::Reply &reply) {
			PLSCHANNELS_API->acquire();
			PLSCHANNELS_API->recoverInfo(uuid);
			PRE_LOG_MSG_STEP("Update RTMP failed ", g_updateChannelStep, ERROR)
			FinishTaskReleaser finishUpdate(uuid);

			ChannelsNetWorkPretestWithAlerts(reply);
		};

		request.okResult(handleSuccess);
		request.failResult(handleFail);
		request.receiver(PLSCHANNELS_API);
		request.method(pls::http::Method::Put);
		pls::http::request(request);
	}
	return true;
}

bool RTMPDeleteToPrism(const QString &uuid)
{

	PRE_LOG_MSG_STEP("Remove RMP channel Begin", g_removeChannelStep, INFO)
	if (PLSCHANNELS_API->isChannelInfoExists(uuid)) {
		PLSCHANNELS_API->release();
		auto headerMap = createPrismHeader();
		if (headerMap.isEmpty()) {
			PLSCHANNELS_API->acquire();
			PRE_LOG_MSG_STEP("Remove RMP channel Error for Prism header is empty", g_removeChannelStep, ERROR)
			return false;
		}
		pls::http::Request request;
		request.rawHeaders(headerMap);
		request.timeout(PRISM_NET_REQUEST_TIMEOUT);
		QString seq = PLSCHANNELS_API->getValueOfChannel(uuid, g_customUserDataSeq, QString());
		QString url = QString(PLS_RTMP_DELETE_V2).arg(PRISM_SSL, seq);
		PLS_INFO(CHANNELDATAHANDLER, "Delete data form new API seq is %s", seq.toUtf8().constData());

		request.cookie(createPrismCookie());
		request.url(url);
		request.hmacKey(PLS_PC_HMAC_KEY.toUtf8());
		request.withLog();
		request.id(CUSTOM_RTMP);

		auto handleSuccess = [uuid](const pls::http::Reply &) {
			PLSCHANNELS_API->acquire();
			PRE_LOG_MSG_STEP("Remove RMP channel end ,success", g_removeChannelStep, INFO)
			PLSCHANNELS_API->removeChannelInfo(uuid, true, false);
			FinishTaskReleaser finishUpdate(uuid);
		};

		auto handleFail = [uuid](const pls::http::Reply &reply) {
			PLSCHANNELS_API->acquire();
			FinishTaskReleaser finishUpdate(uuid);
			PRE_LOG_MSG_STEP("Remove RMP channel end ,falied", g_removeChannelStep, ERROR)
			ChannelsNetWorkPretestWithAlerts(reply);
		};

		request.okResult(handleSuccess);
		request.failResult(handleFail);
		request.receiver(PLSCHANNELS_API);
		request.method(pls::http::Method::Delete);
		pls::http::request(request);
	} else {
		PLS_ERROR(CHANNELDATAHANDLER, "ChannelInfo not exist,uuid is %s", uuid.toUtf8().constData());
	}
	return true;
}

QNetworkCookie createPrismCookie()
{
	QNetworkCookie cookie;
	cookie.setName(COOKIE_NEO_SES);
	cookie.setValue(pls_get_prism_token().toUtf8());
	return cookie;
}

void updateAllRtmpsV1()
{
	PRE_LOG("update all old API RTMPs begin", INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PLSCHANNELS_API->release();

	auto headerMap = createPrismHeader();
	if (headerMap.isEmpty()) {
		PRE_LOG("update all RTMPs error when prism token is not right", INFO)
		PLSCHANNELS_API->acquire();
		if (!PLSCHANNELS_API->isInitilized()) {
			PLSCHANNELS_API->resetInitializeState(true);
		}
		return;
	}
	pls::http::Request request;
	request.timeout(PRISM_NET_REQUEST_TIMEOUT);
	request.rawHeaders(headerMap);

	request.cookie(createPrismCookie());

	request.url(PLS_RTMP_LIST.arg(PRISM_SSL));
	request.hmacKey(PLS_PC_HMAC_KEY.toUtf8());
	request.withLog();
	request.id(CUSTOM_RTMP);
	auto handleSuccess = [](const pls::http::Reply &reply) {
		PRE_LOG("use old API update all rtmp ok", INFO)
		updateRTMPCallback(reply.data(), false);
	};

	auto handleFail = [](const pls::http::Reply &reply) {
		PRE_LOG("use old API update all rtmp error", INFO)
		ChannelsNetWorkPretestWithAlerts(reply);
		endRefresh();
		PLSCHANNELS_API->sigEndRefreshRtmp();
	};

	request.okResult(handleSuccess);
	request.failResult(handleFail);
	request.receiver(App()->getMainView());
	request.method(pls::http::Method::Get);
	pls::http::request(request);
}

void updateAllRtmps()
{
	PRE_LOG("update all new API RTMPs begin", INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PLSCHANNELS_API->release();

	auto headerMap = createPrismHeader();
	if (headerMap.isEmpty()) {
		PRE_LOG("update all RTMPs error when prism token is not right", INFO)
		PLSCHANNELS_API->acquire();
		if (!PLSCHANNELS_API->isInitilized()) {
			PLSCHANNELS_API->resetInitializeState(true);
		}
		return;
	}
	auto handleSuccess = [](const pls::http::Reply &reply) {
		PRE_LOG("use new API update all rtmp ok", INFO)
		auto jsonDoc = QJsonDocument::fromJson(reply.data());
		auto jsArray = jsonDoc.array().toVariantList();
		if (jsArray.size() == 0) {
			PRE_LOG("use new API update all rtmp,return size 0, request old API data", INFO)
			PLSCHANNELS_API->acquire();
			updateAllRtmpsV1();
			PLSCHANNELS_API->addRISTandSRT2RtmpServer();
			AddOrgDataToNewApi("", true);
		} else {
			updateRTMPCallback(reply.data(), true);
		}
	};

	auto handleFail = [](const pls::http::Reply &reply) {
		PRE_LOG("use new API update all rtmp error", INFO)
		ChannelsNetWorkPretestWithAlerts(reply);
		endRefresh();
		PLSCHANNELS_API->sigEndRefreshRtmp();
	};

	pls::http::Request request;
	request.method(pls::http::Method::Get)
		.rawHeaders(headerMap)
		.timeout(PRISM_NET_REQUEST_TIMEOUT)
		.cookie(createPrismCookie())
		.url(PLS_RTMP_LIST_V2.arg(PRISM_SSL))
		.hmacKey(PLS_PC_HMAC_KEY.toUtf8())
		.withLog()
		.receiver(App()->getMainView())
		.jsonContentType()
		.okResult(handleSuccess)
		.failResult(handleFail)
		.id(CUSTOM_RTMP);
	pls::http::request(request);
}

void endRefresh()
{

	PLSCHANNELS_API->acquire();
	bool isEmpty = PLSCHANNELS_API->isEmptyToAcquire();
	if (isEmpty && !PLSCHANNELS_API->isInitilized()) {
		PLSCHANNELS_API->resetInitializeState(true);
	}
	PLSCHANNELS_API->holdOnChannelArea(false);
	if (PLSCHANNELS_API->hasError()) {
		PLSCHANNELS_API->networkInvalidOcurred();
	}

	if (isEmpty) {
		PLSCHANNELS_API->endTransactions();
	}
}

struct RtmpRun : public QRunnable {
	using QRunnable::QRunnable;
	~RtmpRun() override
	{
		PRE_LOG(End RTMP update..., INFO)
		PLSCHANNELS_API->sigEndRefreshRtmp();
		endRefresh();
	}
	Q_DISABLE_COPY(RtmpRun)

	void run() override
	{
		auto jsonDoc = QJsonDocument::fromJson(taskData);
		auto jsArray = jsonDoc.array().toVariantList();
		auto allChannels = PLSCHANNELS_API->getAllChannelInfo();
		QSet<QString> matched;

		if (!bNewAPIData) {
			PLS_INFO("RTMPUpdate", "Old API Data processing");
			QVariantMap mapper;
			pls_read_json(mapper, ":/configs/configs/RTMPJsonMapper.json");
			//match
			auto checkMatched = [&mapper, &allChannels, this, &matched](const QVariant &obj) { return this->searchMatched(mapper, allChannels, matched, obj); };
			std::for_each(jsArray.rbegin(), jsArray.rend(), checkMatched);

		} else {
			PLS_INFO("RTMPUpdate", "New API Data processing");
			auto checkMatchedV2 = [&allChannels, this, &matched](const QVariant &obj) { return this->searchMatchedV2(allChannels, matched, obj); };
			std::for_each(jsArray.rbegin(), jsArray.rend(), checkMatchedV2);
		}
		// remove
		removeExpiredData(allChannels, matched);
	}

	void searchMatchedV2(ChannelsMap &allChannels, QSet<QString> &matched, const QVariant &obj) const
	{
		QVariantMap objMap = obj.toMap();
		auto customData = objMap[g_customData].toMap();
		QString seq = objMap[g_customUserDataSeq].toString();

		if (customData.contains(g_isUseNewAPI)) {
			return;
		}
		customData.insert(g_isUpdated, true);
		auto isSeqMatched = [&](const QVariantMap &src) {
			int type = getInfo(src, g_data_type, ChannelType);
			auto srcSeq = getInfo(src, g_customUserDataSeq);
			if (type >= RTMPType && srcSeq == seq) {
				return true;
			}
			return false;
		};
		auto retIte = std::find_if(allChannels.begin(), allChannels.end(), isSeqMatched);
		if (retIte == allChannels.end()) {
			auto uuid = getInfo(customData, g_channelUUID);
			matched.insert(uuid);
			customData[g_customUserDataSeq] = seq;
			PLSCHANNELS_API->addChannelInfo(customData, false);
			PLSCHANNELS_API->sortAllChannels();
			PLSCHANNELS_API->channelAdded(uuid);
		} else {
			auto &lastMap = retIte.value();
			addToMap(lastMap, customData);
			matched.insert(getInfo(lastMap, g_channelUUID));
			PLSCHANNELS_API->setChannelInfos(lastMap);
		}
	};

	void searchMatched(const QVariantMap &mapper, ChannelsMap &allChannels, QSet<QString> &matched, const QVariant &obj) const
	{
		auto tmpMap = obj.toMap();
		QVariantMap objMap;
		addToMap(objMap, tmpMap, mapper);

		auto urlstream = getInfo(objMap, g_channelRtmpUrl);
		auto streamKey = getInfo(objMap, g_streamKey);
		auto platformName = getInfo(objMap, g_channelName);
		auto guessPlaform = guessPlatformFromRTMP(urlstream);

		if (platformName.isEmpty()) {
			platformName = CUSTOM_RTMP;
		} else {
			platformName = toPlatformCodeID(platformName);
		}

		if (guessPlaform != platformName && guessPlaform != BAND && guessPlaform != NOW && guessPlaform != CHZZK && guessPlaform != NAVER_SHOPPING_LIVE) {

			platformName = guessPlaform;
		}

		PRE_LOG_MSG("platform :" + platformName + " rtmp:" + urlstream + "->stream key: " + streamKey, INFO_KR)
		PRE_LOG_MSG("platform :" + platformName + " rtmp:" + urlstream + "->stream key: " + pls_masking_person_info(streamKey), INFO)

		objMap[g_channelName] = platformName;
		objMap[g_displayLine1] = objMap[g_nickName];
		objMap[g_isUpdated] = true;
		auto seq = getInfo(objMap, g_rtmpSeq);
		auto isSeqMatched = [&](const QVariantMap &src) {
			int type = getInfo(src, g_data_type, ChannelType);

			if (auto srcSeq = getInfo(src, g_rtmpSeq); type == RTMPType && srcSeq == seq) {
				return true;
			}
			return false;
		};
		auto retIte = std::find_if(allChannels.begin(), allChannels.end(), isSeqMatched);
		QString uuid;
		if (retIte == allChannels.end()) {
			auto defaultMap = createDefaultChannelInfoMap(platformName, RTMPType);
			addToMap(defaultMap, objMap);
			defaultMap[g_channelStatus] = Valid;
			uuid = getInfo(defaultMap, g_channelUUID);
			matched.insert(uuid);
			PLSCHANNELS_API->addChannelInfo(defaultMap, false);
			PLSCHANNELS_API->sortAllChannels();
			PLSCHANNELS_API->channelAdded(uuid);
		} else {
			auto &lastMap = retIte.value();
			addToMap(lastMap, objMap);
			lastMap[g_channelStatus] = Valid;
			uuid = getInfo(lastMap, g_channelUUID);
			matched.insert(uuid);
			PLSCHANNELS_API->setChannelInfos(lastMap);
		}
		AddOrgDataToNewApi(uuid, false);
	};

	void removeExpiredData(const ChannelsMap &allChannels, const QSet<QString> &matched) const
	{
		// remove
		auto removeIte = allChannels.cbegin();
		for (; removeIte != allChannels.cend(); ++removeIte) {
			auto info = removeIte.value();
			auto type = getInfo(info, g_data_type, RTMPType);
			if (bNewAPIData) {
				if (type >= RTMPType && !matched.contains(removeIte.key())) {
					PLSCHANNELS_API->removeChannelInfo(removeIte.key(), true, false);
				}
			} else {
				if (type == RTMPType && !matched.contains(removeIte.key())) {
					PLSCHANNELS_API->removeChannelInfo(removeIte.key(), true, false);
				}
			}
		}
	}

	//public:
	QByteArray taskData;
	bool bNewAPIData = true;
};

void updateRTMPCallback(const QByteArray &retData, bool bNewAPIData)
{
	auto run = new RtmpRun();
	run->taskData = retData;
	run->bNewAPIData = bNewAPIData;
	QThreadPool::globalInstance()->start(run);
}
