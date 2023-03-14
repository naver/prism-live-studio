#include "PLSPlatformYoutube.h"
#include <QObject>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <PLSChannelDataHandler.h>
#include <QDebug>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <cassert>
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"
#include "../common/PLSDateFormate.h"
#include "PLSAPIYoutube.h"
#include "PLSServerStreamHandler.hpp"
#include "PLSChannelDataAPI.h"
#include "PLSChannelDataHandlerFunctions.h"
#include "PLSHttpApi\PLSHttpHelper.h"
#include "PLSPlatformApi/prism/PLSPlatformPrism.h"
#include "alert-view.hpp"
#include "frontend-api.h"
#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"
#include "pls-app.hpp"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
#include "window-basic-main.hpp"

const QString kDefaultCategoryID = "22"; //People & Blogs

const QString s_latencyNormal = "normal";
const QString s_latencyLow = "low";
const QString s_latencyUltraLow = "ultraLow";
const QString s_closed_captions_type = "closedCaptionsDisabled";
const QString s_youtube_private_en = "Private";
const QString s_description_default_add = "This stream is created with #PRISMLiveStudio";

PLSPlatformYoutube::PLSPlatformYoutube()
{
	setSingleChannel(true);

	m_vecLocalPrivacy.clear();
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.public"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.unlisted"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.private"));

	m_vecEnglishPrivacy.clear();
	m_vecEnglishPrivacy.push_back(QString("Public"));
	m_vecEnglishPrivacy.push_back(QString("Unlisted"));
	m_vecEnglishPrivacy.push_back(s_youtube_private_en);

	static int requestStatusCount = 0;
	m_statusTimer = new QTimer(this);
	m_statusTimer->setInterval(5000);
	connect(m_statusTimer, &QTimer::timeout, this, [=]() {
		requestStatusCount += 1;
		requestStatisticsInfo();
		if (requestStatusCount % 3 == 0) {
			//This api request is not frequent, so every 15 seconds
			requestLiveBroadcastStatus();
		}
	});

	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [=](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_platformName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

		if (dataType == ChannelData::ChannelType && platformName == YOUTUBE) {
			reInitLiveInfo();
		}
	});
}

PLSServiceType PLSPlatformYoutube::getServiceType() const
{
	return PLSServiceType::ST_YOUTUBE;
}

void PLSPlatformYoutube::liveInfoisShowing()
{

	if (getSelectData().isNormalLive && getSelectData().title.isEmpty()) {
		createNewNormalData();
	}
	setTempSelectID(getSelectData()._id);
	m_tempNoramlData = m_noramlData;
}

void PLSPlatformYoutube::reInitLiveInfo()
{
	m_bTempSelectID = "";
	createNewNormalData();
	setSelectData(m_noramlData);
	m_tempNoramlData = PLSYoutubeLiveinfoData();
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

const vector<PLSYoutubeCategory> &PLSPlatformYoutube::getCategoryDatas()
{
	return m_vecCategorys;
}

const vector<PLSYoutubeLiveinfoData> &PLSPlatformYoutube::getScheduleDatas()
{
	return m_vecSchedules;
}

const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getNomalLiveData()
{
	return m_noramlData;
}
const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getTempSelectData()
{
	for (auto &scheuleData : getScheduleDatas()) {
		if (0 == getTempSelectID().compare(scheuleData._id)) {
			return scheuleData;
		}
	}
	return m_tempNoramlData;
}

const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getSelectData()
{
	return m_selectData;
}

void PLSPlatformYoutube::setSelectData(PLSYoutubeLiveinfoData data)
{
	m_bTempSelectID = data._id;
	m_selectData = data;
	if (!data.isNormalLive) {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(data._id) == 0) {
				scheduleData = data;
			}
		}
	} else {
		m_noramlData = data;
		m_tempNoramlData = data;
	}
	const QString &uuid = getChannelUUID();

	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(PLS_PLATFORM_API->isLiving()));
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_youtube_latency, static_cast<int>(data.latency));

	for (int i = 0; i < m_vecEnglishPrivacy.size(); i++) {
		if (m_selectData.privacyStatus.compare(m_vecEnglishPrivacy[i], Qt::CaseInsensitive) != 0) {
			continue;
		}
		PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_displayLine2, m_vecEnglishPrivacy[i]);
		if (!PLS_PLATFORM_API->isGoLive()) {
			PLSCHANNELS_API->channelModified(uuid);
		} else if (PLS_PLATFORM_API->isGoLive() && PLS_PLATFORM_API->isPrepareLive()) {
			PLSCHANNELS_API->channelModified(uuid);
		}
		break;
	}
	__super::setTitle(m_selectData.title.toStdString());
	__super::setStreamKey(m_selectData.streamKey.toStdString());
	__super::setStreamServer(m_selectData.streamUrl.toStdString());

	emit selectIDChanged();
	if (PLS_PLATFORM_API->isLiving()) {
		emit privacyChangedWhenliving();
	}
}

bool PLSPlatformYoutube::isModifiedWithNewData(QString title, QString description, int categotyIndex, int privacyIndex, bool isKidSelect, bool isNotKidSelect, PLSYoutubeLatency nowLatency,
					       PLSYoutubeLiveinfoData *uiData)
{
	Q_UNUSED(isKidSelect);
	Q_UNUSED(isNotKidSelect);
	bool isModified = false;
	PLSYoutubeLiveinfoData sData = getTempSelectData();
	if (sData.isNormalLive) {
		sData = getNomalLiveData();
	}
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
	} else if (sData.latency != nowLatency) {
		isModified = true;
	} else {
		if (sData.iskidsUserSelect) {
			if (sData.isForKids != isKidSelect) {
				isModified = true;
			}
		} else {
			if (isKidSelect || isNotKidSelect) {
				isModified = true;
			}
		}
	}

	if (isModified && uiData != NULL) {
		*uiData = sData;
		uiData->title = title;
		uiData->description = description;
		uiData->categoryID = getCategoryDatas()[categotyIndex]._id;
		uiData->privacyStatus = getPrivacyEnglishDatas()[privacyIndex];
		uiData->latency = nowLatency;
		uiData->iskidsUserSelect = true;
		uiData->isForKids = isKidSelect;
	}

	return isModified;
}

void PLSPlatformYoutube::saveSettings(function<void(bool)> onNext, bool isNeedUpdate, PLSYoutubeLiveinfoData uiData)
{
	m_trySaveData = uiData;

	PLS_INFO(MODULE_PlatformService, "Youtube call saveSettings");

	auto _onUpdateNext = [=](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}

		setSelectData(m_trySaveData);
		onNext(isSucceed);
	};

	if (!m_trySaveData.isNormalLive) {
		saveTheScheduleSetting(_onUpdateNext, isNeedUpdate);
		return;
	}

	if (PLS_PLATFORM_API->isPrepareLive()) {
		requestStartToInsertLiveBroadcasts(_onUpdateNext);
		return;
	}

	if (PLS_PLATFORM_API->isLiving() && !m_trySaveData._id.isEmpty() && isNeedUpdate) {
		requestUpdateVideoData(_onUpdateNext, m_trySaveData);
		return;
	}

	_onUpdateNext(true);
}

void PLSPlatformYoutube::requestCurrentSelectData(function<void(bool)> onNext, QWidget *widget)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		auto root = doc.object();
		auto items = root["items"].toArray();
		if (items.size() <= 0) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		bool isNorNoramLive = m_selectData.isNormalLive;
		//auto data = ;
		PLSYoutubeLiveinfoData currentData = PLSYoutubeLiveinfoData(items[0].toObject());
		m_selectData = currentData;
		m_selectData.isNormalLive = isNorNoramLive;
		requestCategoryID(onNext, false, widget);
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
	PLSAPIYoutube::requestCurrentSelectData(widget, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

QString PLSPlatformYoutube::getShareUrl(bool isLiving, bool isEnc)
{
	if (!m_selectData.isNormalLive || isLiving) {
		return QString(g_plsYoutubeShareUrl).arg(isEnc ? pls_masking_person_info(m_selectData._id) : m_selectData._id);
	}

	const QString &uuid = getChannelUUID();
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
	if (info.isEmpty()) {
		return "";
	}
	QString homeUrl = info.value(ChannelData::g_shareUrl, "").toString();
	return homeUrl;
}

void PLSPlatformYoutube::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(MODULE_PlatformService, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_youtube(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PlatformService, "%s %s close liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	prepareLiveCallback(value);
}

void PLSPlatformYoutube::requestCategoryID(function<void(bool)> onNext, bool isAllList, QWidget *widget)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();
		auto items = root["items"].toArray();
		if (items.size() <= 0) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			onNext(false);
			return;
		}
		if (isAllList) {
			for (int i = 0; i < items.size(); i++) {
				auto originData = items[i].toObject();
				auto itemID = originData["id"].toString();
				for (auto &scheduleData : m_vecSchedules) {
					if (scheduleData._id == itemID) {
						scheduleData.categoryID = originData["snippet"].toObject()["categoryId"].toString();
					}
					scheduleData.setStatusAndSnippetData(originData);
				}
			}
		} else {
			auto itemFirst = items[0].toObject();
			auto snippet = itemFirst["snippet"].toObject();
			m_selectData.categoryID = snippet["categoryId"].toString();
			m_selectData.setStatusAndSnippetData(itemFirst);
			setSelectData(m_selectData);
		}
		onNext(true);
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
	PLSAPIYoutube::requestCategoryID(nullptr != widget ? static_cast<QObject *>(widget) : static_cast<QObject *>(this), _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh, isAllList,
					 m_iContext);
}

void PLSPlatformYoutube::requestCategoryList(function<void(bool)> onNext, QWidget *widget)
{
	if (!m_vecCategorys.empty()) {
		emit onGetCategorys();
		if (nullptr != onNext) {
			onNext(true);
		}
		return;
	}
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		m_vecCategorys.clear();
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();
			auto items = root["items"].toArray();

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

			if (m_vecCategorys.size() <= 0) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			}
			emit onGetCategorys();
			if (nullptr != onNext) {
				onNext(m_vecCategorys.size() > 0);
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
	PLSAPIYoutube::requestCategoryList(widget, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestScheduleList(function<void(bool)> onNext, QWidget *widget)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}

		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			PLSYoutubeLiveinfoData selectData = getTempSelectData();
			bool isContainSelectData = false;

			m_vecSchedules.clear();
			auto root = doc.object();
			auto items = root["items"].toArray();
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with itemCount:%i", items.count());
			QString boundStreamIDNils = "";
			for (int i = 0; i < items.size(); i++) {
				auto data = items[i].toObject();

				if (data["snippet"].toObject()["isDefaultBroadcast"].toBool()) {
					continue;
				}
				PLSYoutubeLiveinfoData scheduleDta = PLSYoutubeLiveinfoData(data);

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

			if (isContainSelectData == false && !selectData.isNormalLive) {
				//if the remote not found selected schedule, so add it.
				m_vecSchedules.push_back(selectData);
			}

			requestCategoryID(onNext, true, widget);
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
	PLSAPIYoutube::requestScheduleList(widget, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh, ++m_iContext);
}

void PLSPlatformYoutube::requestStartToInsertLiveBroadcasts(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();
		m_trySaveData._id = root["id"].toString();
		requestStartToInsertLiveStreams(onNext);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data, PLSYoutubeApiType::StartLive));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveBroadcastsInsert(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestStartToInsertLiveStreams(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();
		m_trySaveData.boundStreamId = root["id"].toString();
		auto info = root["cdn"].toObject()["ingestionInfo"].toObject();
		m_trySaveData.streamKey = info["streamName"].toString();
		m_trySaveData.streamUrl = info["ingestionAddress"].toString();
		m_trySaveData.streamAPIData = root;

		requestStartToBindTwo(onNext);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data, PLSYoutubeApiType::StartLive));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveStreamsInsert(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestStartToBindTwo(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		if (m_trySaveData.isNormalLive) {
			requestUpdateVideoData(onNext, m_trySaveData);
		} else {
			if (nullptr != onNext) {
				onNext(true);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data, PLSYoutubeApiType::StartLive));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveBroadcastsBind(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestLiveStreamKey(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");

			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();
		auto items = root["items"].toArray();
		auto itermCount = items.size();
		if (itermCount <= 0) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
		}
		QString currentStreamStatus = "active";
		for (int i = 0; i < itermCount; i++) {
			auto item = items[i].toObject();
			for (auto &scheduleData : m_vecSchedules) {
				auto info = item["cdn"].toObject()["ingestionInfo"].toObject();
				if (item["id"].toString() == scheduleData.boundStreamId) {
					scheduleData.streamKey = info["streamName"].toString();
					scheduleData.streamUrl = info["ingestionAddress"].toString();
					scheduleData.streamAPIData = item;
				}
				if (item["id"].toString() == m_trySaveData.boundStreamId) {
					currentStreamStatus = item["status"].toObject()["streamStatus"].toString();
					m_trySaveData.streamKey = info["streamName"].toString();
					m_trySaveData.streamUrl = info["ingestionAddress"].toString();
					m_trySaveData.streamAPIData = item;
				}
			}
		}

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "streamStatus=%s", currentStreamStatus.toStdString().c_str());
		bool isCanLived = !(currentStreamStatus == "active" || currentStreamStatus == "error");
		if (isCanLived) {
			setIsSubChannelStartApiCall(true);
		} else {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_Live_Invalid);
		}
		if (nullptr != onNext) {
			onNext(isCanLived);
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
	PLSAPIYoutube::requestLiveStreamKey(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestUpdateVideoData(function<void(bool)> onNext, PLSYoutubeLiveinfoData data)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			auto root = doc.object();

			if (nullptr != onNext) {
				onNext(true);
			}
		} else {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_UPDATE);

			if (nullptr != onNext) {
				onNext(false);
			}
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		setupApiFailedWithCode(getApiResult(code, error, data, PLSYoutubeApiType::Update));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestUpdateVideoData(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh, data);
}

void PLSPlatformYoutube::requestStopLive(function<void()> onNext)
{
	PLS_PLATFORM_YOUTUBE->setIsSubChannelStartApiCall(false);
	PLSAPIYoutube::requestStopLive(this, nullptr);
	if (nullptr != onNext) {
		onNext();
	}
}

void PLSPlatformYoutube::requestLiveBroadcastStatus()
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);

		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
		}
		auto root = doc.object();
		if (root["items"].toArray().size() <= 0) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, item count is zero");
			return;
		}
		auto itemFirst = root["items"].toArray().first().toObject();

		auto lifeCycleStatus = itemFirst["status"].toObject()["lifeCycleStatus"].toString();
		if (lifeCycleStatus == "complete" || lifeCycleStatus == "revoked") {
			m_statusTimer->stop();
			if (LiveStatus::LiveStarted <= PLS_PLATFORM_API->getLiveStatus() && PLS_PLATFORM_API->getLiveStatus() < LiveStatus::LiveStoped) {
				PLS_LIVE_ABORT_INFO(MODULE_PlatformService, "live abort because live platform stop", "FinishedBy youtube because toStopBroadcast with status:%s",
						    lifeCycleStatus.toStdString().c_str());
				PLSCHANNELS_API->toStopBroadcast();
			}
			PLSAlertView::warning(getAlertParent(), QTStr("Alert.Title"), tr("LiveInfo.live.error.stoped.byRemote").arg(getChannelName()));
		}
	};

	auto _onFail = [=](QNetworkReply *, int, QByteArray, QNetworkReply::NetworkError, void *) {};
	PLSAPIYoutube::requestLiveBroadcastStatus(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::NotRefresh);
}

void PLSPlatformYoutube::checkDuplicateStreamKey(function<void(bool)> onNext)
{
	auto _onStremNext = [=](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}

		bool isContainSameStream = false;

		for (const PLSYoutubeLiveinfoData &scheduleData : getScheduleDatas()) {
			if (m_trySaveData._id == scheduleData._id) {
				continue;
			}
			if (m_trySaveData.streamKey == scheduleData.streamKey && m_trySaveData.streamUrl == scheduleData.streamUrl) {
				isContainSameStream = true;
				break;
			}
		}

		if (isContainSameStream) {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__, "boundStreamID: %s", m_trySaveData.boundStreamId.toUtf8().constData());
			requestStartToInsertLiveStreams(onNext);
			return;
		}
		onNext(true);
	};

	requestLiveStreamKey(_onStremNext);
}

void PLSPlatformYoutube::saveTheScheduleSetting(function<void(bool)> onNext, bool isNeedUpdate)
{
	auto _onDuplicateNext = [=](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}
		if (PLS_PLATFORM_API->isPrepareLive()) {
			checkDuplicateStreamKey(onNext);
		} else {
			onNext(isSucceed);
		}
	};

	auto _onLatencyNext = [=](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}

		if (!m_trySaveData.isNormalLive && !PLS_PLATFORM_API->isLiving() && isNeedUpdateLatency(m_trySaveData)) {
			//#7687 because the live broadcast api may get the old value of kids, the newest kids value may not query succeed when video api updated.
			QTimer::singleShot(1000, this, [=] {
				requestLiveBroadcastsUpdate(_onDuplicateNext); //change latency
			});
		} else {
			_onDuplicateNext(isSucceed);
		}
	};

	if (isNeedUpdate) {
		requestUpdateVideoData(_onLatencyNext, m_trySaveData);
	} else {
		_onDuplicateNext(true);
	}
}

bool PLSPlatformYoutube::isNeedUpdateLatency(const PLSYoutubeLiveinfoData &tryData)
{
	bool latencyChange = false;
	for (auto &scheduleData : m_vecSchedules) {
		if (scheduleData._id.compare(tryData._id) == 0) {
			latencyChange = scheduleData.latency != tryData.latency;
		}
	}
	return latencyChange;
}

void PLSPlatformYoutube::requestStatisticsInfo()
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)

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
		Q_UNUSED(data)
		Q_UNUSED(error)

		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (getApiResult(code, error, data) == PLSPlatformApiResult::PAR_TOKEN_EXPIRED) {
			m_statusTimer->stop();
			PLSCHANNELS_API->setChannelStatus(getChannelUUID(), ChannelData::Expired);
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Token.Expired").arg(getChannelName()));
		}
	};
	PLSAPIYoutube::requestVideoStatus(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestLiveBroadcastsUpdate(function<void(bool)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_UPDATE);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		PLSYoutubeLatency _remoteLatency = PLSYoutubeLatency::Low;
		PLSAPIYoutube::getLatency(doc["contentDetails"].toObject(), _remoteLatency);
		if (m_trySaveData.latency != _remoteLatency) {
			QJsonObject cdnObject = QJsonObject();
			PLSAPIYoutube::setLatency(cdnObject, PLS_PLATFORM_YOUTUBE->getTrySaveDataData().latency);
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, latency not change succeed, want to %s, but is %s", cdnObject["latencyPreference"].toString().toUtf8().constData(),
				  doc["contentDetails"].toObject()["latencyPreference"].toString().toUtf8().constData());
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_LATENCY_CHANGED_FAILED);

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		m_trySaveData.contentDetails = doc["contentDetails"].toObject();
		m_trySaveData.latency = _remoteLatency;

		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data, PLSYoutubeApiType::Update));

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIYoutube::requestLiveBroadcastsUpdate(this, _onSucceed, _onFail, PLSAPIYoutube::RefreshType::CheckRefresh);
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
	PLSAPIYoutube::refreshYoutubeTokenBeforeRequest(PLSAPIYoutube::RefreshType::ForceRefresh, nullptr, this, _onSucceed, _onFail);
}

PLSPlatformApiResult PLSPlatformYoutube::getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, PLSYoutubeApiType apiType)
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	}

	if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		switch (code) {
		case 400: {
			auto doc = QJsonDocument::fromJson(data);
			if (!doc.isObject()) {
				result = PLSPlatformApiResult::PAR_API_FAILED;
				break;
			}
			auto root = doc.object();
			auto errorMsg = root["error"].toString();
			if (errorMsg.contains("invalid_grant", Qt::CaseInsensitive)) {
				result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			} else {
				result = PLSPlatformApiResult::PAR_API_FAILED;
			}

			auto errorArrs = root["error"].toObject()["errors"].toArray();
			QString errorReason = QString();
			if (errorArrs.size() > 0) {
				errorReason = errorArrs[0].toObject()["reason"].toString();
			}
			if (errorReason == "invalidLatencyPreferenceOptions") {
				result = PLS_PLATFORM_API->isPrepareLive() ? PLSPlatformApiResult::YOUTUBE_API_ERROR_LATENCY_TRANSITION : PLSPlatformApiResult::PAR_API_ERROR_UPDATE;
			}
		} break;
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
			auto errorArrs = root["error"].toObject()["errors"].toArray();
			QString errorReason = QString();
			if (errorArrs.size() > 0) {
				errorReason = errorArrs[0].toObject()["reason"].toString();
			}

			if (errorMsg.startsWith("The user is not enabled for live streaming.")) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_NO_CHANNEL;
			} else if (errorMsg.startsWith("Redundant transition")) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_REDUNDANT_TRANSITION;
			} else if (errorMsg.startsWith("Invalid transition")) {
				result = PLSPlatformApiResult::YOUTUBE_API_ERROR_INVALID_TRANSITION;
			} else if (errorMsg.startsWith("Live broadcast not found") ||
				   errorMsg.startsWith(
					   "The video that you are trying to update cannot be found. Check the value of the <code>id</code> field in the request body to ensure that it is correct.")) {
				result = PLSPlatformApiResult::PAR_API_ERROR_LIVE_BROADCAST_NOT_FOUND;
			} else if (errorMsg.startsWith("The user is blocked from live streaming.")) {
				result = PLSPlatformApiResult::PAR_API_ERROR_StartLive_User_Blocked;
			} else if (errorReason == "madeForKidsModificationNotAllowed") {
				result = PLSPlatformApiResult::PAR_API_ERROR_KIDS_READONLY;
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

	if (apiType == PLSYoutubeApiType::StartLive && result == PLSPlatformApiResult::PAR_API_FAILED) {
		result = PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other;
	} else if (apiType == PLSYoutubeApiType::Update && result == PLSPlatformApiResult::PAR_API_FAILED) {
		result = PLSPlatformApiResult::PAR_API_ERROR_UPDATE;
	}

	return result;
}

void PLSPlatformYoutube::setupApiFailedWithCode(PLSPlatformApiResult result)
{
	setIsShownAlert(true);
	auto alertParent = getAlertParent();

	switch (result) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED: {
		emit toShowLoading(true);
		PLSAlertView::Button button = PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Expired").arg(getChannelName()));
		emit closeDialogByExpired();
		if (button == PLSAlertView::Button::Ok) {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		emit toShowLoading(false);
	} break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_NO_CHANNEL:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Youtube_No_Channel_Alert"));
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_REDUNDANT_TRANSITION:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("broadcast.no.longer.valid"));
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_INVALID_TRANSITION:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_LIVE_BROADCAST_NOT_FOUND:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Broadcast.Error.Delete"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_Live_Invalid: {
		const auto channelName = getInitData().value(ChannelData::g_platformName).toString();
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.remote.have.lived").arg(channelName));
	} break;
	case PLSPlatformApiResult::PAR_API_ERROR_StartLive_User_Blocked:
		if (PLS_PLATFORM_API->getActivePlatforms().size() > 1) {
			PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.User.blocked.MultiChannel").arg(getChannelName()));
		} else {
			PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.User.blocked.SingleChannel").arg(getChannelName()));
		}
		break;
	case PLSPlatformApiResult::YOUTUBE_API_ERROR_LATENCY_TRANSITION:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.latency.ultra.low.start.1080.failed"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_UPDATE:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Error.Failed").arg(getChannelName()));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other:
		if (PLS_PLATFORM_API->getActivePlatforms().size() > 1) {
			PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Create.Failed.MultiChannel").arg(getChannelName()));
		} else {
			PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Create.Failed.SingleChannel").arg(getChannelName()));
		}
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_KIDS_READONLY:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), tr("LiveInfo.latency.will.call.api.failed").arg(getChannelName()));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_LATENCY_CHANGED_FAILED: {
		QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Close, tr("Close")}, {PLSAlertView::Button::Open, tr("Live.Check.youtube.gotoPage")}};
		auto btn = PLSAlertView::warning(getAlertParent(), QTStr("Alert.Title"), tr("Live.Check.LiveInfo.latency.change.failed").arg(getChannelName()), buttons);
		if (btn == PLSAlertView::Button::Open) {
			auto url = g_plsYoutubeStudioManagerUrl.arg(m_trySaveData._id);
			QDesktopServices::openUrl(QUrl(url));
		}
	} break;
	default:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	}
}

void PLSPlatformYoutube::onLiveEnded()
{
	setIsSubChannelStartApiCall(false);
	auto _onNext = [=]() {
		createNewNormalData();
		setSelectData(m_noramlData);
		setTempSelectID(m_noramlData._id);
		liveEndedCallback();
	};

	if (m_statusTimer && m_statusTimer->isActive()) {
		m_statusTimer->stop();
	}

	requestStopLive(_onNext);
}

void PLSPlatformYoutube::onAllPrepareLive(bool value)
{
	if (!value && getSelectData().isNormalLive && !getSelectData()._id.isEmpty()) {
		m_noramlData._id = m_noramlData.boundStreamId = m_noramlData.streamKey = m_noramlData.streamUrl = "";
		setSelectData(m_noramlData);
	}

	if (!value && getIsSubChannelStartApiCall()) {
		// cancel or fail
		requestStopLive(nullptr);
	}

	PLSPlatformBase::onAllPrepareLive(value);
}

void PLSPlatformYoutube::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}
	__super::setIsScheduleLive(!getSelectData().isNormalLive);
	const QString &uuid = getChannelUUID();
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(true));
	PLSCHANNELS_API->channelModified(uuid);

	QString resolutionKey = PLSServerStreamHandler::instance()->getOutputResolution();
	PLS_INFO(MODULE_PlatformService, "youtube start with resolutionKey:%s latency:%d", resolutionKey.toUtf8().constData(), static_cast<int>(getSelectData().latency));

	auto resList = resolutionKey.split("x");
	if (resList.size() >= 2) {
		bool isMoreThan1080 = qMin(resList[0].toInt(), resList[1].toInt()) > 1080;
		if (getSelectData().latency == PLSYoutubeLatency::UltraLow && isMoreThan1080) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("LiveInfo.latency.ultra.low.start.1080.toast"));
		}
	}

	if (PLS_PLATFORM_API->isPrismLive()) {
		//when start live will force to update youtube token. because prism server will update token each hour.
		PLS_INFO(MODULE_PlatformService, "%s %s requestRefrshAccessToken", PrepareInfoPrefix, __FUNCTION__);
		PLS_PLATFORM_PRSIM->requestRefrshAccessToken(this, nullptr);
		return;
	}

	if (m_statusTimer && !m_statusTimer->isActive()) {
		m_statusTimer->start();
	}
}

void PLSPlatformYoutube::createNewNormalData()
{
	m_noramlData = PLSYoutubeLiveinfoData();
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	if (info.isEmpty()) {
		return;
	}
	m_noramlData.description = s_description_default_add;
	auto channelName = info.value(ChannelData::g_nickName, "").toString();
	m_noramlData.title = tr("LiveInfo.live.title.suffix").arg(channelName);
	m_noramlData.privacyStatus = getPrivacyEnglishDatas()[0];
	m_noramlData.categoryID = kDefaultCategoryID;
	auto iLatency = info.value(ChannelData::g_youtube_latency, static_cast<int>(PLSYoutubeLatency::Low)).toInt();
	m_noramlData.latency = static_cast<PLSYoutubeLatency>(iLatency);
}

QJsonObject PLSPlatformYoutube::getLiveStartParams()
{
	QJsonObject platform(__super::getLiveStartParams());

	platform["refreshToken"] = getChannelRefreshToken();
	auto &data = getSelectData();
	platform["broadcastId"] = data._id;
	platform["privacyStatus"] = data.privacyStatus.toUpper();
	platform["scheduled"] = !data.isNormalLive;
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	return platform;
}

QJsonObject PLSPlatformYoutube::getWebChatParams()
{
	QJsonObject platform;
	platform["name"] = getNameForLiveStart();
	platform["isPrivate"] = isPrivateStatus();
	return platform;
}

QString PLSPlatformYoutube::getShareUrl()
{
	return getShareUrl(PLS_PLATFORM_API->isLiving());
}
QString PLSPlatformYoutube::getShareUrlEnc()
{
	return getShareUrl(PLS_PLATFORM_API->isLiving(), true);
}

bool PLSPlatformYoutube::isPrivateStatus()
{
	const auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	QString category_en = info.value(ChannelData::g_displayLine2, "").toString();
	if (category_en.isEmpty()) {
		category_en = info.value(ChannelData::g_catogry, "").toString();
	}
	bool isPrivate = s_youtube_private_en == category_en;
	return isPrivate;
}

void PLSPlatformYoutube::showAutoStartFalseAlertIfNeeded()
{
	struct YoutubeStartShowData {
		QString showStr;
		bool isMutiLive = false;
		bool isContainChannel = false;
		bool isContainRtmp = false;

		QString redirectUrl;
		QString redirectKey = QTStr("LiveInfo.live.Check.AutoStart.pressKey").arg(YOUTUBE);
	};

	auto _getShowData = [=](YoutubeStartShowData &model) {
		QString channelStr = YOUTUBE;

		auto platformActived = PLS_PLATFORM_ACTIVIED;

		model.isMutiLive = platformActived.size() > 1;
		PLSPlatformYoutube *yotubeItem = nullptr;

		for (auto item : platformActived) {
			if (YOUTUBE != item->getChannelName()) {
				continue;
			}
			if (PLSServiceType::ST_RTMP == item->getServiceType()) {
				model.isContainRtmp = true;
				continue;
			}
			if (PLSServiceType::ST_YOUTUBE == item->getServiceType()) {
				yotubeItem = reinterpret_cast<PLSPlatformYoutube *>(item);
				model.isContainChannel = true;
				continue;
			}
		}

		if (model.isContainRtmp && !model.isContainChannel) {
			//only youtube rtmp
			model.showStr = QTStr("LiveInfo.live.Check.AutoStart.Rrmp").arg(channelStr);
			return;
		}

		if (yotubeItem == nullptr) {
			return;
		}

		bool isAutoStart = yotubeItem->getSelectData().enableAutoStart;
		if (isAutoStart == true) {
			if (model.isContainRtmp) {
				//channel not need start.
				model.showStr = QTStr("LiveInfo.live.Check.AutoStart.Rrmp").arg(channelStr);
			}
			return;
		}

		model.redirectUrl = g_plsYoutubeStudioManagerUrl.arg(yotubeItem->getSelectData()._id);

		if (isAutoStart == false && !model.isContainRtmp) {
			//only channel need start by hand;
			model.showStr = QTStr("LiveInfo.live.Check.AutoStart.Channel").arg(channelStr);
			return;
		}

		//both need start by hand;
		model.showStr = QTStr("LiveInfo.live.Check.AutoStart.Both.ChannelAndRtmp").arg(channelStr);
	};

	auto model = YoutubeStartShowData();

	_getShowData(model);

	if (model.showStr.isEmpty()) {
		return;
	}

	if (PLS_PLATFORM_API->getUuidOnStarted().size() <= 1) {
		PLSAlertView::Button ret = PLSAlertView::Button::Ok;

		QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Ok, tr("OK")}};

		if (!model.redirectUrl.isEmpty()) {
			buttons.insert(PLSAlertView::Button::Open, tr("Live.Check.youtube.gotoPage"));
		}

		ret = PLSAlertView::warning(nullptr, QTStr("Alert.Title"), model.showStr, buttons);

		if (ret == PLSAlertView::Button::Open) {
			QDesktopServices::openUrl(QUrl(model.redirectUrl));
		}
		return;
	}

	if (model.redirectUrl.isEmpty()) {
		model.redirectKey = "";
	} else {
		int index = model.showStr.indexOf(model.redirectKey);
		model.showStr.replace(index, model.redirectKey.length(), model.redirectUrl);
	}
	pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, model.showStr, model.redirectUrl, model.redirectKey);
}

PLSYoutubeLiveinfoData::PLSYoutubeLiveinfoData() {}

PLSYoutubeLiveinfoData::PLSYoutubeLiveinfoData(const QJsonObject &data)
{
	auto snippet = data["snippet"].toObject();
	auto contentDetails = data["contentDetails"].toObject();

	this->isNormalLive = false;
	this->boundStreamId = contentDetails["boundStreamId"].toString();
	this->_id = data["id"].toString();
	this->title = snippet["title"].toString();
	this->description = snippet["description"].toString();
	if (snippet.contains("scheduledStartTime")) {
		this->startTimeOrigin = snippet["scheduledStartTime"].toString();
	} else {
		this->startTimeOrigin = snippet["publishedAt"].toString();
	}
	if (this->startTimeOrigin == "1970-01-01T00:00:00Z") {
		this->startTimeOrigin = "";
	}
	int timeStamp = PLSDateFormate::timeStringToStamp(this->startTimeOrigin);
	this->startTimeShort = PLSDateFormate::timeStampToShortString(timeStamp);
	this->startTimeUTC = PLSDateFormate::timeStampToUTCString(timeStamp);
	this->privacyStatus = data["status"].toObject()["privacyStatus"].toString();
	this->liveChatId = snippet["liveChatId"].toString();

	this->iskidsUserSelect = true;
	this->isForKids = data["status"].toObject()["madeForKids"].toBool();
	this->enableAutoStart = contentDetails["enableAutoStart"].toBool();
	this->enableAutoStop = contentDetails["enableAutoStop"].toBool();
	PLSAPIYoutube::getLatency(contentDetails, this->latency);
	this->contentDetails = contentDetails;
	this->isCaptions = contentDetails["closedCaptionsType"].toString() != s_closed_captions_type;
}

void PLSYoutubeLiveinfoData::setStatusAndSnippetData(const QJsonObject &data)
{
	QStringList ignoreList{"uploadStatus", "privacyStatus", "madeForKids", "selfDeclaredMadeForKids"};
	QJsonObject tempData = data["status"].toObject();
	for (const auto &item : ignoreList) {
		if (tempData.contains(item)) {
			tempData.remove(item);
		}
	}
	this->statusData = tempData;

	QJsonObject tempSnippetData = data["snippet"].toObject();
	this->snippetData["defaultLanguage"] = tempSnippetData["defaultLanguage"].toString();
	if (this->snippetData["defaultLanguage"].toString().length() == 0) {
		this->snippetData.remove("defaultLanguage"); //if not remove it, the api will failed.!!
	}
	this->snippetData["tags"] = tempSnippetData["tags"].toArray();
}
