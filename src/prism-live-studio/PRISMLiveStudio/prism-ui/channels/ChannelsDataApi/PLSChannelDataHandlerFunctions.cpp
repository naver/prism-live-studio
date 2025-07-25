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
	FinishTaskReleaser finishAdd(uuid);
	return true;
}

bool AddOrgDataToNewApi(const QString &uuid, bool bAddFlag)
{
	return true;
}

bool RTMPUpdateToPrism(const QString &uuid)
{
	PRE_LOG_MSG_STEP("Update RTMP Begin ", g_updateChannelStep, INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	FinishTaskReleaser finishUpdate(uuid);
	return true;
}

bool RTMPDeleteToPrism(const QString &uuid)
{
	PRE_LOG_MSG_STEP("Remove RMP channel Begin", g_removeChannelStep, INFO)
	FinishTaskReleaser finishUpdate(uuid);
	PLSCHANNELS_API->removeChannelInfo(uuid, true, false);
	return true;
}

QNetworkCookie createPrismCookie()
{
	QNetworkCookie cookie;
	cookie.setName(COOKIE_NEO_SES);
	cookie.setValue(pls_get_prism_token().toUtf8());
	return cookie;
}

void updateAllRtmpsV1() {}

void updateAllRtmps()
{
	PRE_LOG(update all RTMPs begin, INFO)
	HolderReleaser releaser(&PLSChannelDataAPI::holdOnChannelArea);
	PLSCHANNELS_API->release();
	endRefresh();
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
