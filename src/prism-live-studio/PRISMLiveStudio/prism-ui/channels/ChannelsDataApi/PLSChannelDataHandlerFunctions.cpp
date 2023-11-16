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

#include "../../PLSPlatformApi/band/PLSBandDataHandler.h"
#include "../../PLSPlatformApi/facebook/PLSFacebookDataHandler.h"
#include "../../PLSPlatformApi/naver-shopping-live/PLSNaverShoppingLIVEDataHandler.h"
#include "../../PLSPlatformApi/navertv/PLSNaverTVDataHandler.h"

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

QVariantMap loadMapFromJsonFile(const QString &fileName)
{
	QVariantMap tmpMaper;

	if (QFile file(fileName); file.open(QIODevice::ReadOnly)) {
		auto data = file.readAll();
		auto jsonDoc = QJsonDocument::fromJson(data);
		tmpMaper = jsonDoc.toVariant().toMap();
	}

	return tmpMaper;
}

void registerAllPlatforms()
{
	PLSCHANNELS_API->registerPlatformHandler(new PLSAfreecaTVDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new TwitchDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new YoutubeHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSNaverTVDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSBandDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSFacebookDataHandler);
	PLSCHANNELS_API->registerPlatformHandler(new PLSNaverShoppingLIVEDataHandler);
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

bool isInvalidGrant(const QVariantMap &src)
{
	if (src.contains("error")) {
		return src.value("error").toString().contains("invalid_grant", Qt::CaseInsensitive);
	}

	return false;
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

bool isReplyContainExpired(const QByteArray &body, const QStringList &keys)
{
	int matchCount = 0;
	for (const QString &key : keys) {
		if (body.contains(key.toUtf8())) {
			++matchCount;
		}
	}

	return keys.count();
}

QJsonObject createJsonArrayFromInfo(const QString &uuid)
{
	QJsonObject obj;

	if (const auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid); !info.isEmpty()) {
		auto platformName = getInfo(info, g_platformName);

		platformName = platformName.remove("RTMP").remove(" ").toUpper();
		auto userID = getInfo(info, g_rtmpUserID);
		auto disName = getInfo(info, g_nickName);
		auto rtmpUrl = getInfo(info, g_channelRtmpUrl);
		auto description = getInfo(info, g_otherInfo);
		auto streamKey = getInfo(info, g_streamKey);

		obj.insert("streamName", disName);
		obj.insert("rtmpUrl", rtmpUrl);
		obj.insert("streamKey", streamKey);
		obj.insert("username", userID);
		obj.insert(g_publishService, platformName);
		obj.insert("description", description);
		obj.insert("resolution", "720");
		obj.insert("bitrate", "2000");
		obj.insert("framerate", "30");
		obj.insert("interval", 2);
	}

	return obj;
};

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
	if (PLSCHANNELS_API->isEmptyToAcquire() && !PLSCHANNELS_API->isInitilized()) {
		PLSCHANNELS_API->resetInitializeState(true);
	}
	PLSCHANNELS_API->holdOnChannelArea(false);
	if (PLSCHANNELS_API->hasError()) {
		PLSCHANNELS_API->networkInvalidOcurred();
	}

	if (PLSCHANNELS_API->isEmptyToAcquire()) {
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
		auto mapper = loadMapFromJsonFile(":/configs/configs/RTMPJsonMapper.json");

		auto allChannels = PLSCHANNELS_API->getAllChannelInfo();
		QSet<QString> matched;
		//match
		auto checkMatched = [&mapper, &allChannels, this, &matched](const QVariant &obj) { return this->searchMatched(mapper, allChannels, matched, obj); };
		std::for_each(jsArray.rbegin(), jsArray.rend(), checkMatched);
		// remove
		removeExpiredData(allChannels, matched);
	}

	void searchMatched(const QVariantMap &mapper, ChannelsMap &allChannels, QSet<QString> &matched, const QVariant &obj) const
	{
		auto tmpMap = obj.toMap();
		QVariantMap objMap;
		addToMap(objMap, tmpMap, mapper);

		auto urlstream = getInfo(objMap, g_channelRtmpUrl);
		auto streamKey = getInfo(objMap, g_streamKey);
		auto platformName = getInfo(objMap, g_platformName);
		auto guessPlaform = guessPlatformFromRTMP(urlstream);

		if (platformName.isEmpty()) {
			platformName = CUSTOM_RTMP;
		} else {
			platformName = toPlatformCodeID(platformName);
		}

		if (guessPlaform != platformName && guessPlaform != BAND && guessPlaform != NOW) {

			platformName = guessPlaform;
		}

		PRE_LOG_MSG("platform :" + platformName + " rtmp:" + urlstream + "->stream key: " + streamKey, INFO_KR)
		PRE_LOG_MSG("platform :" + platformName + " rtmp:" + urlstream + "->stream key: " + pls_masking_person_info(streamKey), INFO)

		objMap[g_platformName] = platformName;
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
		if (retIte == allChannels.end()) {
			auto defaultMap = createDefaultChannelInfoMap(platformName, RTMPType);
			addToMap(defaultMap, objMap);
			defaultMap[g_channelStatus] = Valid;
			auto uuid = getInfo(defaultMap, g_channelUUID);
			matched.insert(uuid);
			PLSCHANNELS_API->addChannelInfo(defaultMap, false);
			PLSCHANNELS_API->sortAllChannels();
			PLSCHANNELS_API->channelAdded(uuid);
		} else {
			auto &lastMap = retIte.value();
			addToMap(lastMap, objMap);
			lastMap[g_channelStatus] = Valid;
			matched.insert(getInfo(lastMap, g_channelUUID));
			PLSCHANNELS_API->setChannelInfos(lastMap);
		}
	};

	void removeExpiredData(const ChannelsMap &allChannels, const QSet<QString> &matched) const
	{
		// remove
		auto removeIte = allChannels.cbegin();
		for (; removeIte != allChannels.cend(); ++removeIte) {
			auto info = removeIte.value();
			if (getInfo(info, g_data_type, RTMPType) == RTMPType && !matched.contains(removeIte.key())) {
				PLSCHANNELS_API->removeChannelInfo(removeIte.key(), true, false);
			}
		}
	}

	//public:
	QByteArray taskData;
};

void updateRTMPCallback(const QByteArray &retData)
{
	auto run = new RtmpRun();
	run->taskData = retData;
	QThreadPool::globalInstance()->start(run);
}
