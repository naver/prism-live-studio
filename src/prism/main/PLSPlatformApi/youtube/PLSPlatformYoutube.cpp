#include "PLSPlatformYoutube.h"
#include <QObject>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-app.hpp"
#include "log.h"
#include "alert-view.hpp"
#include "window-basic-main.hpp"
#include "frontend-api.h"
#include "pls-common-define.hpp"
#include "PLSHttpApi\PLSHttpHelper.h"
#include "../PLSPlatformApi.h"
#include "../PLSLiveInfoDialogs.h"
#include "PLSChannelDataAPI.h"
#include "pls-net-url.hpp"
#include <QTimer>
#include <QDebug>
#include <PLSChannelDataHandler.h>
#include <cassert>

PLSPlatformYoutube::PLSPlatformYoutube() : m_bTempSchedule(false)
{
	m_vecLocalPrivacy.clear();
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.public"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.unlisted"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.private"));

	m_vecEnglishPrivacy.clear();
	m_vecEnglishPrivacy.push_back(QObject::tr("youtube.privacy.public.en"));
	m_vecEnglishPrivacy.push_back(QObject::tr("youtube.privacy.unlisted.en"));
	m_vecEnglishPrivacy.push_back(QObject::tr("youtube.privacy.private.en"));

	m_statusTimer = new QTimer(this);
	connect(m_statusTimer, &QTimer::timeout, this, &PLSPlatformYoutube::requestStatisticsInfo);
}

PLSServiceType PLSPlatformYoutube::getServiceType() const
{
	return PLSServiceType::ST_YOUTUBE;
}

const QString PLSPlatformYoutube::getSelectID()
{
	return m_selectData._id;
}

const vector<QString> PLSPlatformYoutube::getPrivacyDatas()
{
	return m_vecLocalPrivacy;
}
const vector<QString> PLSPlatformYoutube::getPrivacyEnglishDatas()
{
	return m_vecEnglishPrivacy;
}

const vector<PLSYoutubeCategory> PLSPlatformYoutube::getCategoryDatas()
{
	return m_vecCategorys;
}

const vector<PLSScheduleData> PLSPlatformYoutube::getScheduleDatas()
{
	return m_vecSchedules;
}

const PLSScheduleData PLSPlatformYoutube::getNomalLiveDatas()
{
	return m_noramlData;
}
const PLSScheduleData PLSPlatformYoutube::getTempSelectDatas()
{
	PLSScheduleData *data = nullptr;
	if (!m_bTempSchedule) {
		data = &m_noramlData;
	} else {
		for (auto scheuleData : getScheduleDatas()) {
			if (0 == getTempSelectID().compare(scheuleData._id)) {
				data = &scheuleData;
				break;
			}
		}
	}

	if (data == nullptr && 0 == getTempSelectID().compare(m_selectData._id)) {
		data = &m_selectData;
	}

	if (nullptr == data) {
		data = &m_noramlData;
	}

	return *data;
}

const PLSScheduleData PLSPlatformYoutube::getSelectDatas()
{
	return m_selectData;
}

void PLSPlatformYoutube::setSelectData(PLSScheduleData data)
{
	m_bTempSchedule = !data.isNormalLive;
	m_selectData = data;
	if (!data.isNormalLive) {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(data._id) == 0) {
				scheduleData = data;
			}
		}
	}
	const QString &uuid = getChannelUUID();
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrl, getShareUrl());

	for (int i = 0; i < m_vecEnglishPrivacy.size(); i++) {
		if (m_selectData.privacyStatus.compare(m_vecEnglishPrivacy[i], Qt::CaseInsensitive) != 0) {
			continue;
		}
		PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_catogryTemp, m_vecEnglishPrivacy[i]);
		PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl());
		PLSCHANNELS_API->channelModified(uuid);
		break;
	}
	setTitle(data.title.toStdString());
	emit selectIDChanged();
}

bool PLSPlatformYoutube::isModifiedWithNewData(QString title, QString description, int categotyIndex, int privacyIndex, PLSScheduleData *uiData)
{
	bool isModified = false;
	PLSScheduleData sData = getTempSelectDatas();
	if (getPrivacyEnglishDatas().size() <= privacyIndex || privacyIndex < 0) {
		isModified = false;
	} else if (getCategoryDatas().size() <= categotyIndex || categotyIndex < 0) {
		isModified = false;
	} else if (title.compare(sData.title) != 0) {
		isModified = true;
	} else if (description.compare(sData.description) != 0) {
		isModified = true;
	} else if (getPrivacyEnglishDatas()[privacyIndex].compare(sData.privacyStatus, Qt::CaseInsensitive) != 0) {
		isModified = true;
	} else if (getCategoryDatas()[categotyIndex]._id.compare(sData.categoryID) != 0) {
		isModified = true;
	}

	if (isModified && uiData != NULL) {
		*uiData = sData;
		uiData->title = title;
		uiData->description = description;
		uiData->categoryID = getCategoryDatas()[categotyIndex]._id;
		uiData->privacyStatus = getPrivacyEnglishDatas()[privacyIndex];
	}

	return isModified;
}

void PLSPlatformYoutube::saveSettings(function<void(bool)> onNext, bool isNeedUpdate, PLSScheduleData uiData)
{
	PLS_INFO(MODULE_PlatformService, "Youtube call saveSettings");
	auto _onNext = [=](bool isSucceed) {
		onNext(isSucceed);
		if (!isSucceed) {
			return;
		}

		if (!getIsTempSchedule()) {
			m_noramlData = uiData;
		}

		setSelectData(uiData);
	};

	if (!isNeedUpdate) {
		_onNext(true);
		return;
	}
	requestUpdateData(_onNext, uiData);
}

QString PLSPlatformYoutube::getShareUrl()
{
	return QString(g_plsYoutubeShareUrl).arg(m_selectData._id);
}

void PLSPlatformYoutube::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ "show liveinfo");
	value = pls_exec_live_Info_youtube(getChannelUUID()) == QDialog::Accepted;
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ "liveinfo closed with:%d", value);
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	auto _onToeknNext = [=](bool value) { prepareLiveCallback(value); };

	auto _onAPINext = [=](bool value) {
		if (value == true) {
			PLS_PLATFORM_API->saveStreamSettings(getNameForSettingId(), getStreamServer(), getStreamKey());
		}
		if (value == true) {
			//when start live will force to update youtube token. because prism server will update token each hour.
			forceToRefreshToken(_onToeknNext);
		} else {
			_onToeknNext(true);
		}
	};

	if (m_selectData.isNormalLive) {
		//update broadcast id now, otherwise maybe will get last broadcast address.
		requestLiveStreamKey(_onAPINext);
	} else {
		//schedule live
		requestDisabelMonitor(_onAPINext);
	}
}

void PLSPlatformYoutube::requestUserInfo(function<void(bool)> onNext, bool isStartLive)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto itemFirst = root["items"].toArray().first().toObject();
			if (root["items"].toArray().size() <= 0) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			}
			auto snippet = itemFirst["snippet"].toObject();

			m_noramlData.isNormalLive = true;
			m_noramlData._id = itemFirst["id"].toString();
			m_noramlData.privacyStatus = itemFirst["status"].toObject()["privacyStatus"].toString();
			m_noramlData.title = snippet["title"].toString();
			m_noramlData.description = snippet["description"].toString();
			m_noramlData.boundStreamId = itemFirst["contentDetails"].toObject()["boundStreamId"].toString();

			if (isStartLive) {
				setSelectData(m_noramlData);
				requestLiveStreamKey(onNext);
			} else {
				emit onGetTitleDescription();
				requestVideo(onNext, false, nullptr);
			}

		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE)
			.setQuerys({{"access_token", getChannelToken()}, {"broadcastType", "persistent"}, {"maxResults", 50}, {"mine", true}, {"part", "contentDetails, snippet, status"}});
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestVideo(function<void(bool)> onNext, bool isSchedule, QWidget *widget)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto items = root["items"].toArray();
			if (items.size() <= 0) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			}
			if (isSchedule) {
				for (int i = 0; i < items.size(); i++) {
					auto originData = items[i].toObject();
					auto &scheduleData = m_vecSchedules[i];
					scheduleData.categoryID = originData["snippet"].toObject()["categoryId"].toString();
				}
				onNext(true);
			} else {
				if (items.size() > 0) {
					auto itemFirst = items[0].toObject();
					auto snippet = itemFirst["snippet"].toObject();
					m_noramlData.categoryID = snippet["categoryId"].toString();
					m_noramlData.channelID = snippet["channelId"].toString();
				}
				requestCategory(onNext);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}

		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto _getNetworkReply = [=] {
		const char *lang = static_cast<PLSApp *>(QCoreApplication::instance())->GetLocale();
		QString currentLangure(lang);
		bool isKr = 0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive);
		QString langStr = isKr ? "ko-kr" : "en-us";

		QString liveID = m_noramlData._id;
		if (isSchedule) {
			liveID = "";
			for (int i = 0; i < m_vecSchedules.size(); i++) {
				auto sche = m_vecSchedules[i];
				liveID = liveID.append("%1").arg(i == 0 ? "" : ",").append(sche._id);
			}
		}
		PLS_INFO(MODULE_PlatformService, "Youtube requestVideo start");
		PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"access_token", getChannelToken()}, {"id", liveID}, {"part", "snippet"}});
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, nullptr != widget ? static_cast<QObject *>(widget) : static_cast<QObject *>(this), _onSucceed, _onFail, nullptr,
					 reinterpret_cast<void *>(m_iContext));
}

void PLSPlatformYoutube::requestCategory(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		m_vecCategorys.clear();
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto items = root["items"].toArray();
			if (items.size() <= 0) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			}
			for (int i = 0; i < items.size(); i++) {
				auto data = items[i].toObject();
				bool isShow = data["snippet"].toObject()["assignable"].toBool();
				if (!isShow) {
					continue;
				}
				PLSYoutubeCategory category = PLSYoutubeCategory();
				category.title = data["snippet"].toObject()["title"].toString();
				category._id = data["id"].toString();
				m_vecCategorys.push_back(category);
			}

			emit onGetCategorys();
			if (nullptr != onNext) {
				onNext(true);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	if (m_vecCategorys.size() > 0) {
		emit onGetCategorys();
		if (nullptr != onNext) {
			onNext(true);
		}
		return;
	}
	auto _getNetworkReply = [=] {
		const char *lang = static_cast<PLSApp *>(QCoreApplication::instance())->GetLocale();
		QString currentLangure(lang);
		bool isKr = 0 == currentLangure.compare(KO_KR, Qt::CaseInsensitive);
		QString langStr = isKr ? "ko-kr" : "en-us";
		PLSNetworkReplyBuilder builder(QString("%1/videoCategories").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"access_token", getChannelToken()}, {"hl", langStr}, {"regionCode", "kr"}, {"part", "snippet"}});
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestSchedule(function<void(bool)> onNext, QWidget *widget)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}

		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			PLSScheduleData selectData = getTempSelectDatas();
			bool isContainSelectData = false;

			m_vecSchedules.clear();
			auto root = doc.object();
			auto items = root["items"].toArray();
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with itemCount:%i", items.count());
			QString boundStreamIDNils = "";
			for (int i = 0; i < items.size(); i++) {
				auto data = items[i].toObject();
				auto snippet = data["snippet"].toObject();
				auto contentDetails = data["contentDetails"].toObject();

				PLSScheduleData scheduleDta = PLSScheduleData();
				scheduleDta.boundStreamId = contentDetails["boundStreamId"].toString();
				scheduleDta._id = data["id"].toString();
				scheduleDta.title = snippet["title"].toString();
				scheduleDta.description = snippet["description"].toString();
				scheduleDta.startTimeOrigin = snippet["scheduledStartTime"].toString();
				int timeStamp = timeStringToStamp(scheduleDta.startTimeOrigin);
				scheduleDta.startTimeShort = timeStampToShortString(timeStamp);
				scheduleDta.startTimeUTC = timeStampToUTCString(timeStamp);
				scheduleDta.privacyStatus = data["status"].toObject()["privacyStatus"].toString();
				scheduleDta.liveChatId = snippet["liveChatId"].toString();
				scheduleDta.enableMonitorStream = contentDetails["monitorStream"].toObject()["enableMonitorStream"].toBool();
				if (scheduleDta.boundStreamId.isEmpty()) {
					//if boundStreamId is nil, then call get streamkey and url will failed.
					boundStreamIDNils.append(scheduleDta.title).append(",   ");
				} else {
					m_vecSchedules.push_back(scheduleDta);
				}

				if (scheduleDta._id == selectData._id) {
					isContainSelectData = true;
				}
			}

			if (!boundStreamIDNils.isEmpty()) {
				PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with ignore titles:%s", boundStreamIDNils.toStdString().c_str());
			}

			PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with itemCount:%i", items.count());
			if (isContainSelectData = false && !selectData.isNormalLive) {
				//if the remote not found selected schedule, so add it.
				m_vecSchedules.push_back(selectData);
			}

			requestVideo(onNext, true, widget);
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}

		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE)
			.setQuerys({{"access_token", getChannelToken()}, {"part", "contentDetails,snippet,status"}, {"broadcastStatus", "upcoming"}, {"maxResults", "50"}, {"pageToken", "@"}});

		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, widget, _onSucceed, _onFail, nullptr, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformYoutube::requestLiveStreamKey(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto firstItem = root["items"].toArray().first().toObject();
			if (root["items"].toArray().size() <= 0) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			}
			auto info = firstItem["cdn"].toObject()["ingestionInfo"].toObject();
			m_selectData.streamKey = info["streamName"].toString();
			m_selectData.streamUrl = info["ingestionAddress"].toString();

			setStreamKey(m_selectData.streamKey.toStdString());
			setStreamServer(m_selectData.streamUrl.toStdString());

			if (nullptr != onNext) {
				onNext(true);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	auto _getNetworkReply = [=] {
		QString boundStreamID = m_selectData.boundStreamId;
		PLSNetworkReplyBuilder builder(QString("%1/liveStreams").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"access_token", getChannelToken()}, {"part", "cdn"}, {"id", boundStreamID}, {"maxResults", "50"}});
		return builder.get();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");

	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestUpdateData(function<void(bool)> onNext, PLSScheduleData data)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();

			if (nullptr != onNext) {
				onNext(true);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			showApiUpdateError(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		showApiUpdateError(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE).setQuerys({{"access_token", getChannelToken()}, {"part", "snippet,status"}});
		builder.addJsonObject("status", QVariantMap({{"privacyStatus", data.privacyStatus}}));
		builder.addJsonObject("id", data._id);
		builder.addJsonObject("snippet", QVariantMap({{"description", data.description}, {"title", data.title}, {"categoryId", data.categoryID}}));
		return builder.put();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestDisabelMonitor(function<void(bool)> onNext)
{
	requestLiveStreamKey(onNext);
	return;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		requestLiveStreamKey(onNext);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	auto _getNetworkReply = [=] {
		QVariant monitorStream = QVariantMap({{"broadcastStreamDelayMs", 0}, {"enableMonitorStream", false}});
		QVariant contentDetails = QVariantMap(
			{{"enableEmbed", false}, {"enableDvr", false}, {"enableContentEncryption", false}, {"recordFromStart", true}, {"startWithSlate", false}, {"monitorStream", monitorStream}});

		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.setQuerys({{"access_token", getChannelToken()}, {"part", "contentDetails"}});
		builder.addJsonObject("id", m_selectData._id);
		builder.addJsonObject("contentDetails", contentDetails);
		return builder.put();
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");

	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::openScheduleStatus(function<void(bool)> onNext)
{
	QTimer::singleShot(5000, this, [=] { requestOpenScheduleStatus(onNext); });
}

void PLSPlatformYoutube::requestOpenScheduleStatus(function<void(bool)> onNext)
{
	const static int youtubeMaxCount = 5;
	static int youtubeStartTryCount = youtubeMaxCount;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with leftCount:%d", youtubeMaxCount);
		youtubeStartTryCount = youtubeMaxCount;
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code);

		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed with leftCount:%d", youtubeStartTryCount);
		if (youtubeStartTryCount > 0) {
			youtubeStartTryCount--;
			openScheduleStatus(onNext);
		} else {
			youtubeStartTryCount = youtubeMaxCount;
			auto result = getApiResult(code, error, data);
			setupApiFailedWithCode(result);
			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _getNetworkReply = [=] {
		PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts/transition").arg(g_plsYoutubeAPIHost));
		builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
		builder.setQuerys({{"access_token", getChannelToken()}, {"broadcastStatus", "live"}, {"id", m_selectData._id}, {"part", "status"}});
		return builder.post();
	};

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(false, _getNetworkReply, this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestStopLive(function<void()> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		if (nullptr != onNext) {
			onNext();
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)

		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");

		if (nullptr != onNext) {
			onNext();
		}
	};
	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	PLSNetworkReplyBuilder builder(QString("%1/liveBroadcasts/transition").arg(g_plsYoutubeAPIHost));
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	builder.setQuerys({{"access_token", getChannelToken()}, {"broadcastStatus", "complete"}, {"id", m_selectData._id}, {"part", "status"}});
	PLS_HTTP_HELPER->connectFinished(builder.post(), this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::requestStatisticsInfo()
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto firstItem = root["items"].toArray().first().toObject();
			auto likeCount = firstItem["statistics"].toObject()["likeCount"].toString();
			auto watchCount = firstItem["liveStreamingDetails"].toObject()["concurrentViewers"].toString();
			auto totalWatchCount = firstItem["statistics"].toObject()["viewCount"].toString();

			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_viewers, watchCount);
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_likes, likeCount);
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)

		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
	};

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	PLSNetworkReplyBuilder builder(QString("%1/videos").arg(g_plsYoutubeAPIHost));
	builder.setContentType(HTTP_CONTENT_TYPE_VALUE);
	builder.setQuerys({{"access_token", getChannelToken()}, {"part", "statistics, liveStreamingDetails"}, {"id", m_selectData._id}});
	PLS_HTTP_HELPER->connectFinished(builder.get(), this, _onSucceed, _onFail);
}

void PLSPlatformYoutube::forceToRefreshToken(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		onNext(true);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));
		onNext(false);
	};

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	refreshYoutubeTokenBeforeRequest(true, nullptr, this, _onSucceed, _onFail);
}

QString PLSPlatformYoutube::timeStampToUTCString(long timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}

	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);

	int timeOffsest = dateTime.offsetFromUtc() / 3600;
	QString timeZoneString = QString(" UTC%1%2").arg(timeOffsest < 0 ? "" : "+").arg(timeOffsest);

	dateTime.setTimeSpec(Qt::UTC);

	QLocale local = QLocale::English;
	QString formateStr = "MM/dd/yyyy hh:mm AP";
	QString enTimeStr = local.toString(dateTime, formateStr) + timeZoneString;

	return enTimeStr;
}

QString PLSPlatformYoutube::timeStampToShortString(long timeStamp)
{
	if (timeStamp <= 0) {
		return "";
	}
	QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timeStamp);
	dateTime.setTimeSpec(Qt::UTC);
	QLocale local = QLocale::English;
	QString formateStr = "MM/dd hh:mm AP";
	QString enTimeStr = local.toString(dateTime, formateStr);

	return enTimeStr;
}

long PLSPlatformYoutube::timeStringToStamp(QString time)
{
	QDateTime stDTime = QDateTime::fromString(time, "yyyy-MM-dd'T'HH:mm:ss.zzz'Z'");
	return stDTime.toTime_t() + QDateTime::currentDateTime().offsetFromUtc();
}

PLSPlatformApiResult PLSPlatformYoutube::getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data)
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {

	} else if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		switch (code) {
		case 401:
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			break;
		case 403: {
			auto doc = QJsonDocument::fromJson(data);
			if (!doc.isObject()) {
				result = PLSPlatformApiResult::PAR_API_FAILED;
				break;
			}
			auto root = doc.object();
			auto errorMsg = root["error"].toObject()["message"].toString();
			if (errorMsg.compare("The user is not enabled for live streaming.") == 0) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_NO_CHANNEL;
			} else if (errorMsg.compare("Redundant transition") == 0) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_REDUNDANT_TRANSITION;
			} else if (errorMsg.compare("Invalid transition") == 0) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_INVALID_TRANSITION;
			} else if (errorMsg.compare("Live broadcast not found") == 0 ||
				   errorMsg.compare(
					   "The video that you are trying to update cannot be found. Check the value of the <code>id</code> field in the request body to ensure that it is correct.") ==
					   0) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_LIVE_BROADCAST_NOT_FOUND;
			} else {
				result = PLSPlatformApiResult::PAR_API_FAILED;
			}
			break;
		}
		default:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		}
	}
	return result;
}

void PLSPlatformYoutube::setupApiFailedWithCode(PLSPlatformApiResult result)
{
	showApiRefreshError(result);
}

void PLSPlatformYoutube::showApiRefreshError(PLSPlatformApiResult value)
{
	auto alertParent = getAlertParent();

	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Network.Error"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Youtube.Expired"));
		emit closeDialogByExpired();
		PLSCHANNELS_API->channelExpired(getChannelUUID());
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_NO_CHANNEL:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Youtube_No_Channel_Alert"));
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_REDUNDANT_TRANSITION:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Youtube.Redundant.Broadcast"));
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_INVALID_TRANSITION:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Youtube.Failed"));
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_LIVE_BROADCAST_NOT_FOUND:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Youtube.Broadcast.Delete"));
		break;
	default:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Youtube.Failed"));
		break;
	}
}

void PLSPlatformYoutube::showApiUpdateError(PLSPlatformApiResult value)
{
	auto alertParent = getAlertParent();

	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Network.Error"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Youtube.Expired"));
		break;
	default:
		PLSAlertView::warning(alertParent, QTStr("Live.Check.Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Youtube.Failed"));
		break;
	}
}

void PLSPlatformYoutube::onLiveStopped()
{
	if (PLS_PLATFORM_API->isPrismLive() || !isActive()) {
		if (isActive() && !m_selectData.isNormalLive) {
			setTempSchedule(false);
			setSelectData(m_noramlData);
			setTempSelectID(m_noramlData._id);
		}
		liveStoppedCallback();
		return;
	}

	auto _onNext = [=]() {
		setTempSchedule(false);
		setSelectData(m_noramlData);
		setTempSelectID(m_noramlData._id);
		liveStoppedCallback();
	};

	if (m_statusTimer && m_statusTimer->isActive()) {
		m_statusTimer->stop();
	}

	requestStopLive(_onNext);
}

void PLSPlatformYoutube::onAlLiveStarted(bool value)
{
	if (value && !getSelectDatas().isNormalLive) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Youtube.Schedule.OnBroadcast"));
	}

	if (!value || PLS_PLATFORM_API->isPrismLive() || !isActive()) {
		return;
	}

	if (m_statusTimer && !m_statusTimer->isActive()) {
		m_statusTimer->start(5000);
	}
}
void PLSPlatformYoutube::refreshYoutubeTokenBeforeRequest(
	bool forceRefresh, function<QNetworkReply *()> originNetworkReplay, const QObject *originReceiver,
	function<void(QNetworkReply *networkReplay, int code, QByteArray data, void *const context)> originOnSucceed,
	function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *const context)> originOnFailed,
	function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *const context)> originOnFinished, void *const originContext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto access_token = root["access_token"].toString();
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_channelToken, access_token);
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_createTime, QDateTime::currentDateTime());
			PLSCHANNELS_API->channelModified(getChannelUUID());
			if (originNetworkReplay != nullptr) {
				PLS_HTTP_HELPER->connectFinished(originNetworkReplay(), originReceiver, originOnSucceed, originOnFailed, originOnFinished, originContext);
			} else if (nullptr != originOnSucceed) {
				originOnSucceed(networkReplay, code, data, context);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			if (nullptr != originOnFailed) {
				originOnFailed(networkReplay, 1999, data, QNetworkReply::InternalServerError, context);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		originOnFailed(networkReplay, code, data, error, context);
	};

	assert(!getChannelUUID().isEmpty());
	if (!forceRefresh && isTokenValid(getChannelUUID())) {
		if (originNetworkReplay != nullptr) {
			PLS_HTTP_HELPER->connectFinished(originNetworkReplay(), originReceiver, originOnSucceed, originOnFailed, originOnFinished, originContext);
		} else if (nullptr != originOnSucceed) {
			originOnSucceed(nullptr, 200, nullptr, originContext);
		}
		return;
	}

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");

	auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	auto token = channelData.value(ChannelData::g_refreshToken).toString();

	QString bodyStr(QString("refresh_token=%0&client_id=%1&client_secret=%2&grant_type=refresh_token").arg(token).arg(YOUTUBE_CLIENT_ID).arg(YOUTUBE_CLIENT_KEY));

	QByteArray bodyByte = bodyStr.toUtf8();
	PLSNetworkReplyBuilder builder(QString("%1/token").arg(g_plsYoutubeAPIHostV4));
	builder.setContentType(HTTP_CONTENT_TYPE_URL_ENCODED_VALUE);
	builder.setBody(bodyByte);
	PLS_HTTP_HELPER->connectFinished(builder.post(), originReceiver, _onSucceed, _onFail);
}
