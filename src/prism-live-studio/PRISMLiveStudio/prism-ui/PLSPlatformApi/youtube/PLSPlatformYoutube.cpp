#include "PLSPlatformYoutube.h"
#include <QObject>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <quuid.h>

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
#include "PLSPlatformApi/prism/PLSPlatformPrism.h"
#include "PLSAlertView.h"
#include "frontend-api.h"
#include "prism/PLSPlatformPrism.h"
#include "liblog.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
#include "window-basic-main.hpp"
#include "../common/PLSAPICommon.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelsVirualAPI.h"
#include "pls-gpop-data.hpp"
#include <qvariant.h>
#include <qmap.h>
#include "pls/pls-dual-output.h"

using namespace std;

const QString kDefaultCategoryID = "22"; //People & Blogs

const QString s_latencyNormal = "normal";
const QString s_latencyLow = "low";
const QString s_latencyUltraLow = "ultraLow";
const QString s_closed_captions_type = "closedCaptionsDisabled";
const QString s_youtube_private_en = "Private";
const QString s_description_default_add = "This stream is created with #PRISMLiveStudio";
const QString s_youtube_thum_level = "medium";

const QString s_youtube_remote_error = "remote_empty";

const static double s_timer_interval = 5.0;

const static int s_max_ignore_nodata_count = 2;
const static QString s_youtube_default_status_value = "default_status";

static QString _getStartLiveFailedKey()
{
	return PLS_PLATFORM_API->getActivePlatforms().size() > 1 ? PLSErrCustomKey_StartLiveFailed_Multi : PLSErrCustomKey_StartLiveFailed_Single;
}

struct YoutubeStartShowData {

	YoutubeStartShowData();

	QString showStr;
	bool isMutiLive = false;
	bool isContainChannel = false;
	bool isContainRtmp = false;
	bool isErrMsg = false;

	QString redirectUrl;
	QString redirectKey = QTStr("LiveInfo.live.Check.AutoStart.pressKey").arg(YOUTUBE);
};

PLSPlatformYoutube::PLSPlatformYoutube()
{
	m_vecLocalPrivacy.clear();
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.public"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.unlisted"));
	m_vecLocalPrivacy.push_back(QObject::tr("youtube.privacy.private"));

	m_vecEnglishPrivacy.clear();
	m_vecEnglishPrivacy.push_back(QString("Public"));
	m_vecEnglishPrivacy.push_back(QString("Unlisted"));
	m_vecEnglishPrivacy.push_back(s_youtube_private_en);

	m_statusTimer = pls_new<QTimer>(this);
	m_statusTimer->setInterval((int)s_timer_interval * 1000);
	auto requestHealthStatusInterval = (int)ceil(PLSGpopData::instance()->getYoutubeHealthStatusInterval() / s_timer_interval);

	connect(m_statusTimer, &QTimer::timeout, this, [this, requestHealthStatusInterval]() {
		auto isCheckHealth = m_requestStatusCount % requestHealthStatusInterval == 0;
		auto isCheckLive = m_requestStatusCount % 3 == 0;
		m_requestStatusCount += 1;

		requestLiveStreamStatus(isCheckHealth);

		if (getIsRehearsal() || m_isRehearsalToLived) {
			requestStatisticsInfo();
			if (isCheckLive) {
				//This api request is not frequent, so every 15 seconds
				requestLiveBroadcastStatus();
			}
		}
	});

	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

		if (dataType == ChannelData::ChannelType && platformName == YOUTUBE) {
			reInitLiveInfo();
		}
	});

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this,
		[this](bool, bool apiStarted) {
			m_isStopping = false;

			if (!apiStarted) {
				return;
			}
			auto uuids = PLS_PLATFORM_API->getUuidOnStarted();
			for (const auto &uid : uuids) {
				if (uid == getChannelUUID()) {
					PLS_INFO(MODULE_PlatformService, "Youtube get liveEnded signals to reinit liveinfo data");
					if (m_isRehearsal) {
						resetLiveInfoAfterRehearsal();
					} else {
						reInitLiveInfo();
					}
					break;
				}
			}
		},
		Qt::DirectConnection);
}

PLSServiceType PLSPlatformYoutube::getServiceType() const
{
	return PLSServiceType::ST_YOUTUBE;
}

void PLSPlatformYoutube::liveInfoIsShowing()
{

	if (getSelectData().isNormalLive && getSelectData().title.isEmpty()) {
		createNewNormalData();
	}
	setTempSelectID(getSelectData()._id);
	m_tempNormalData = m_normalData;

	if (!m_selectData._id.isEmpty() && m_thumMaps.contains(m_selectData._id)) {
		m_selectData.pixMap = m_thumMaps[m_selectData._id].pix;
		getTempSelectDataRef().pixMap = m_thumMaps[m_selectData._id].pix;
	}
}

void PLSPlatformYoutube::reInitLiveInfo()
{
	setIsRehearsal(false);
	m_bTempSelectID = "";
	createNewNormalData();
	setSelectData(m_normalData);
	m_tempNormalData = PLSYoutubeLiveinfoData();
}

void PLSPlatformYoutube::resetLiveInfoAfterRehearsal()
{
	if (m_selectData.isNormalLive) {

		m_normalData = PLSYoutubeLiveinfoData();
		m_normalData.title = m_selectData.title;
		m_normalData.description = m_selectData.description;

		m_normalData.privacyStatus = m_selectData.privacyStatus;
		m_normalData.categoryID = m_selectData.categoryID;
		m_normalData.latency = m_selectData.latency;
		m_normalData.pixMap = m_thumMaps[m_selectData._id].pix;
		m_normalData.isForKids = m_selectData.isForKids;
		m_normalData.iskidsUserSelect = m_selectData.iskidsUserSelect;
		m_normalData.channelID = m_selectData.channelID;

		m_bTempSelectID = "";
		setSelectData(m_normalData);
		m_tempNormalData = PLSYoutubeLiveinfoData();
	}
	setIsRehearsal(false);
}

QString PLSPlatformYoutube::getSelectID() const
{
	return m_selectData._id;
}

vector<QString> PLSPlatformYoutube::getPrivacyDatas() const
{
	return m_vecLocalPrivacy;
}
vector<QString> PLSPlatformYoutube::getPrivacyEnglishDatas() const
{
	return m_vecEnglishPrivacy;
}

const vector<PLSYoutubeCategory> &PLSPlatformYoutube::getCategoryDatas() const
{
	return m_vecCategories;
}

const vector<PLSYoutubeLiveinfoData> &PLSPlatformYoutube::getScheduleDatas() const
{
	return m_vecSchedules;
}

const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getNormalLiveData() const
{
	return m_normalData;
}
const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getTempSelectData()
{
	const PLSYoutubeLiveinfoData &data = getTempSelectDataRef();
	return data;
}

PLSYoutubeLiveinfoData &PLSPlatformYoutube::getTempSelectDataRef()
{

	for (auto &scheduleData : m_vecSchedules) {
		if (0 == getTempSelectID().compare(scheduleData._id)) {
			return scheduleData;
		}
	}
	return m_tempNormalData;
}

const PLSYoutubeLiveinfoData &PLSPlatformYoutube::getSelectData() const
{
	return m_selectData;
}

PLSYoutubeLiveinfoData::IngestionType PLSPlatformYoutube::getSettingIngestionType() const
{
	return m_ingestionType;
}

void PLSPlatformYoutube::setSelectData(PLSYoutubeLiveinfoData data)
{
	auto oldPrivacyStatus = m_selectData.privacyStatus;
	auto oldKids = m_selectData.isForKids;
	auto oldID = m_selectData._id;

	if (data._id.isEmpty() && data.thumbnailUrl.size() > 0) {
		data.thumbnailUrl = QString();
	}
	m_bTempSelectID = data._id;
	m_selectData = data;
	if (!data.isNormalLive) {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(data._id) == 0) {
				scheduleData = data;
			}
		}
	} else {
		m_normalData = data;
		m_tempNormalData = data;
	}
	const QString &uuid = getChannelUUID();

	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_broadcastID, m_selectData._id);

	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(PLS_PLATFORM_API->isLiving()));
	if (data.isNormalLive) {
		PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_youtube_latency, static_cast<int>(data.latency));
	}
	for (const auto &item : m_vecEnglishPrivacy) {
		if (m_selectData.privacyStatus.compare(item, Qt::CaseInsensitive) != 0) {
			continue;
		}
		PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_displayLine2, item);
		if (!PLS_PLATFORM_API->isGoLive()) {
			PLSCHANNELS_API->channelModified(uuid);
		} else if (PLS_PLATFORM_API->isGoLive() && PLS_PLATFORM_API->isPrepareLive()) {
			PLSCHANNELS_API->channelModified(uuid);
		}
		break;
	}
	PLSPlatformBase::setTitle(m_selectData.title.toStdString());
	PLSPlatformBase::setStreamKey(m_selectData.streamKey.toStdString());
	PLSPlatformBase::setStreamServer(m_selectData.streamUrl.toStdString());

	if (oldID != m_selectData._id) {
		emit selectIDChanged();
	}

	bool isChatNeededChange = false;
	if (oldPrivacyStatus.toLower() == s_youtube_private_en.toLower() && oldPrivacyStatus.toLower() != m_selectData.privacyStatus.toLower()) {
		isChatNeededChange = true;
	}
	if (!m_selectData.isNormalLive && oldID == m_selectData._id && oldKids != m_selectData.isForKids) {
		isChatNeededChange = true;
	}
	if (isChatNeededChange) {
		emit privateChangedToOther();
	}
}

bool PLSPlatformYoutube::isModifiedWithNewData(int categoryIndex, int privacyIndex, bool isKidSelect, bool isNotKidSelect, PLSYoutubeLiveinfoData *uiData)
{
	bool isModified = false;
	PLSYoutubeLiveinfoData sData = getTempSelectData();
	if (sData.isNormalLive) {
		sData = getNormalLiveData();
	}
	if (getPrivacyEnglishDatas().size() <= privacyIndex || privacyIndex < 0) {
		isModified = false;
	} else if (getCategoryDatas().size() <= categoryIndex || categoryIndex < 0) {
		isModified = false;
	} else if (uiData->title.compare(sData.title) != 0) {
		isModified = true;
	} else if (uiData->description.compare(sData.description) != 0) {
		isModified = true;
	} else if (getPrivacyEnglishDatas()[privacyIndex].compare(sData.privacyStatus, Qt::CaseInsensitive) != 0) {
		isModified = true;
	} else if (getCategoryDatas()[categoryIndex]._id.compare(sData.categoryID) != 0) {
		isModified = true;
	} else if (sData.latency != uiData->latency) {
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
	//the pixmap not need judge is modified
	if (isModified && uiData != nullptr) {
		sData.title = uiData->title;
		sData.description = uiData->description;
		sData.categoryID = getCategoryDatas()[categoryIndex]._id;
		sData.privacyStatus = getPrivacyEnglishDatas()[privacyIndex];
		sData.latency = uiData->latency;
		sData.iskidsUserSelect = true;
		sData.isForKids = isKidSelect;
		sData.pixMap = uiData->pixMap;
		*uiData = sData;
	}

	return isModified;
}

void PLSPlatformYoutube::saveSettings(const function<void(bool)> &onNext, bool isNeedUpdate, const PLSYoutubeLiveinfoData &uiData, const QObject *receiver)
{
	m_isUploadedImage = false;
	m_failedValues = UpdateValue::None;
	m_trySaveData = uiData;
	if (PLS_PLATFORM_API->isPrepareLive()) {
		m_isCallTested = false;
		m_isRehearsalToLived = false;
		m_rehearsalSavedData = m_trySaveData.startData;

		if (getIsRehearsal()) {
			m_trySaveData.startData.enableAutoStart = m_trySaveData.startData.enableAutoStop = false;
			m_trySaveData.startData.enableMonitorStream = true;
		}
	}

	PLS_INFO(MODULE_PlatformService, "Youtube call saveSettings");

	if (PLS_PLATFORM_API->isPrepareLive()) {
		updateSettingIngestionType();
		PLS_INFO(MODULE_PlatformService, "youtube set ingestionType:%s", QVariant::fromValue(m_ingestionType).toString().toUtf8().constData());
	}

	auto _onSaveCompleteNext = [this, onNext](bool isSucceed) {
		if (isSucceed) {

			setSelectData(m_trySaveData);
			emit receiveVideoId(m_selectData.isNormalLive, m_selectData._id);
			if (m_isUploadedImage) {
				//ignore the remote thum, use the current upload image
				m_thumMaps[m_selectData._id].pix = m_selectData.pixMap;
				downloadThumImage(nullptr, m_selectData.thumbnailUrl, this, true);
			} else if (PLS_PLATFORM_API->isPrepareLive() && getIsRehearsal() && m_trySaveData.isNormalLive) {
				//need download the default pix
				downloadThumImage(nullptr, m_selectData.thumbnailUrl, this, false);
			}
		}
		onNext(isSucceed);
	};

	auto _onUpdateNext = [this, _onSaveCompleteNext, receiver](bool isSucceed) {
		if (!isSucceed) {
			_onSaveCompleteNext(isSucceed);
			return;
		}
		bool isNormalStart = m_trySaveData.isNormalLive && PLS_PLATFORM_API->isPrepareLive();
		bool isThumChanged = m_trySaveData.pixMap.toImage() != getTempSelectData().pixMap.toImage();
		if (!m_trySaveData._id.isEmpty() && !m_trySaveData.pixMap.isNull() && (isNormalStart || isThumChanged)) {
			m_isUploadedImage = true;
			requestUploadImage(m_trySaveData.pixMap, _onSaveCompleteNext, receiver);
			return;
		}
		_onSaveCompleteNext(isSucceed);
	};

	//schedue
	//normal live change data when living
	if (!m_trySaveData.isNormalLive || (PLS_PLATFORM_API->isLiving() && !m_trySaveData._id.isEmpty())) {
		saveTheScheduleSetting(_onUpdateNext, isNeedUpdate, receiver);
		return;
	}

	//normal live start live
	if (PLS_PLATFORM_API->isPrepareLive()) {
		requestStartToInsertLiveBroadcasts(_onUpdateNext, receiver);
		return;
	}

	if (PLS_PLATFORM_API->isLiving() && !m_trySaveData._id.isEmpty() && isNeedUpdate) {
		requestUpdateVideoData(_onUpdateNext, m_trySaveData, receiver);
		return;
	}

	_onUpdateNext(true);
}

void PLSPlatformYoutube::requestUploadImage(const QPixmap &pixmap, const function<void(bool)> &onNext, const QObject *receiver)
{

	QString path = QString("%1/%2.png").arg(getTmpCacheDir()).arg(QUuid::createUuid().toString());
	QFile file(path);
	file.open(QIODevice::WriteOnly);
	pixmap.save(&file, "PNG");
	file.close();

	PLS_INFO(MODULE_PlatformService, "PLSPlatformYoutube::requestUploadImage start");

	auto _dealNext = [onNext, this](bool isOK) {
		if (PLS_PLATFORM_API->isPrepareLive() && !isOK) {
			m_failedValues |= UpdateValue::Thum;
			isOK = true;
		}
		pls_invoke_safe(onNext, isOK);
	};

	auto _onFail = [this, _dealNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		if (PLS_PLATFORM_API->isPrepareLive()) {
			PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_UploadImageFailed, getErrorExtraData(YT_API_UploadImage));
		} else {
			showAlert({code, error, data}, PLSErrCustomKey_UploadImageFailed, YT_API_UploadImage);
		}
		_dealNext(false);
	};

	PLSAPIYoutube::uploadImage(
		receiver, path,
		[this, path, _dealNext, pixmap](bool isOK, const QString &) {
			if (QFile::exists(path)) {
				QFile::remove(path);
			}
			m_thumMaps[m_trySaveData._id].pix = pixmap;
			_dealNext(isOK);
		},
		_onFail);
}

void PLSPlatformYoutube::requestCurrentSelectData(const function<void(bool)> &onNext, const QWidget *widget)
{
	auto _onSucceed = [this, onNext, widget](QByteArray data) { dealCurrentSelectDataSucceed(data, onNext, widget); };

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_RequestCurrentSelectData);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestCurrentSelectData(widget, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::dealCurrentSelectDataSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QWidget *widget)
{
	PLS_INFO(MODULE_PlatformService, "PLSPlatformYoutube::dealCurrentSelectDataSucceed succeed");
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealCurrentSelectDataSucceed failed, doc is not object");

		showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_RequestCurrentSelectData, "body not json");

		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	auto root = doc.object();
	auto items = root["items"].toArray();
	if (items.size() <= 0) {
		PLS_ERROR(MODULE_PlatformService, "PLSPlatformYoutube::dealCurrentSelectDataSucceed failed, current selete schedule is maybe deleted");
		reInitLiveInfo();
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	bool isNormalLive = m_selectData.isNormalLive;
	auto pix = m_selectData.pixMap;
	auto oldPrivacyStatus = m_selectData.privacyStatus;
	auto currentData = PLSYoutubeLiveinfoData(items[0].toObject());
	m_selectData = currentData;
	m_selectData.isNormalLive = isNormalLive;
	m_selectData.pixMap = pix;
	requestCategoryID(onNext, m_selectData._id, widget);
	if (oldPrivacyStatus.toLower() == s_youtube_private_en.toLower() && oldPrivacyStatus.toLower() != m_selectData.privacyStatus.toLower()) {
		emit privateChangedToOther();
	}
}

QString PLSPlatformYoutube::getShareUrl(bool isLiving, bool isEnc) const
{
	if (!m_selectData.isNormalLive || isLiving) {
		QString currentUrl = getIsRehearsal() ? g_plsYoutubeRehearsalUrl : g_plsYoutubeShareUrl;
		return currentUrl.arg(isEnc ? pls_masking_person_info(m_selectData._id) : m_selectData._id);
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

	if (getIsRehearsal()) {
		PLSCHANNELS_API->rehearsalBegin();
	}
	prepareLiveCallback(value);
}

void PLSPlatformYoutube::requestCategoryID(const std::function<void(bool)> &onNext, const QString &searchID, const QObject *widget, bool isShowAlert)
{
	auto _onSucceed = [this, onNext, searchID, isShowAlert](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "PLSPlatformYoutube::requestCategoryID succeed");
		dealCategoriesRequestDatas(onNext, searchID, data, isShowAlert);
	};

	auto _onFail = [this, onNext, isShowAlert](int code, QByteArray data, QNetworkReply::NetworkError error) {
		if (isShowAlert) {
			showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetVideo);
		}

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestCategoryID(widget, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh, searchID);
}

void PLSPlatformYoutube::dealCategoriesRequestDatas(const std::function<void(bool)> &onNext, const QString &searchID, const QByteArray &data, bool isShowAlert)
{
	if (!QJsonDocument::fromJson(data).isObject()) {
		PLS_ERROR(MODULE_PlatformService, "PLSPlatformYoutube::dealCategoriesRequestDatas failed, doc is not object");

		if (isShowAlert)
			showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetVideo, "body not json");
		if (nullptr != onNext)
			onNext(false);
		return;
	}
	auto items = QJsonDocument::fromJson(data).object()["items"].toArray();
	if (items.empty()) {
		PLS_ERROR(MODULE_PlatformService, "PLSPlatformYoutube::dealCategoriesRequestDatas failed, item count is zero");
		if (nullptr != onNext)
			onNext(false);
		return;
	}

	if (!searchID.isEmpty() && searchID == m_selectData._id) {
		auto itemFirst = items[0].toObject();
		auto snippet = itemFirst["snippet"].toObject();
		m_selectData.categoryID = snippet["categoryId"].toString();
		m_selectData.setStatusAndSnippetData(itemFirst);
		setSelectData(m_selectData);
	}
	for (const auto &item : items) {
		auto originData = item.toObject();
		auto itemID = originData["id"].toString();
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id == itemID) {
				scheduleData.categoryID = originData["snippet"].toObject()["categoryId"].toString();
			}
			scheduleData.setStatusAndSnippetData(originData);
		}
	}

	if (nullptr != onNext)
		onNext(true);
}

void PLSPlatformYoutube::requestCategoryList(const function<void(bool)> &onNext, const QWidget *widget)
{
	if (!m_vecCategories.empty()) {
		if (nullptr != onNext) {
			onNext(true);
		}
		return;
	}

	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "PLSPlatformYoutube::requestCategoryList succeed");
		m_vecCategories.clear();
		dealCategoriesSucceed(data);
		if (nullptr != onNext) {
			onNext(!m_vecCategories.empty());
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetCategoryList);

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIYoutube::requestCategoryList(widget, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::dealCategoriesSucceed(const QByteArray &data)
{
	auto doc = QJsonDocument::fromJson(data);
	if (doc.isObject()) {
		auto root = doc.object();
		auto items = root["items"].toArray();

		for (int i = 0; i < items.size(); i++) {
			auto dataItem = items[i].toObject();
			bool isShow = dataItem["snippet"].toObject()["assignable"].toBool();
			if (!isShow) {
				continue;
			}
			auto category = PLSYoutubeCategory();
			category.title = dataItem["snippet"].toObject()["title"].toString();
			category._id = dataItem["id"].toString();
			m_vecCategories.push_back(category);
		}

		if (m_vecCategories.size() <= 0) {
			PLS_ERROR(MODULE_PlatformService, "PLSPlatformYoutube::dealCategoriesSucceed failed, item count is zero");
		}

	} else {
		PLS_ERROR(MODULE_PlatformService, "PLSPlatformYoutube::dealCategoriesSucceed failed, doc is not object");

		showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetCategoryList, "body not json");
	}
}

void PLSPlatformYoutube::updateScheduleListAndSort()
{
	if (m_selectData.isNormalLive || m_selectData._id.isEmpty()) {
		return;
	}

	for (auto iter = m_vecSchedules.begin(); iter != m_vecSchedules.end(); iter++) {
		if (iter->_id == m_selectData._id) {
			m_vecSchedules.erase(iter);
			break;
		}
	}
	m_vecSchedules.push_back(m_selectData);
	PLSAPICommon::sortScheduleListsByCustom(m_vecSchedules);
}

void PLSPlatformYoutube::requestScheduleListByGuidePage(const std::function<void(bool)> &onNext, const QObject *widget)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
		m_vecGuideSchedules.clear();
		dealScheduleListGuidePageSucceed(data, onNext);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
		m_vecGuideSchedules.clear();
		auto retData =
			PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_LoadLiveInfoFailed, getErrorExtraData(YT_API_RequestScheduleList + " by guide page"));
		mySharedData().m_lastError = createScheduleGetError(getChannelName(), retData);
		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLS_ERROR(MODULE_PlatformService, "start to request schedule list by guide page");
	PLSAPIYoutube::requestScheduleList(widget, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::dealScheduleListGuidePageSucceed(const QByteArray &data, const std::function<void(bool)> &onNext)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealScheduleListGuidePageSucceed failed, doc is not object");
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	auto root = doc.object();
	auto items = root["items"].toArray();
	PLS_INFO(MODULE_PlatformService, "dealScheduleListGuidePageSucceed succeed with itemCount:%i", items.count());
	QString boundStreamIDNils = "";
	for (int i = 0; i < items.size(); i++) {
		auto dataItem = items[i].toObject();
		if (dataItem["snippet"].toObject()["isDefaultBroadcast"].toBool()) {
			continue;
		}
		auto scheduleData = PLSYoutubeLiveinfoData(dataItem);

		if (scheduleData.boundStreamId.isEmpty()) {
			//if boundStreamId is nil, then call get streamkey and url will failed.
			boundStreamIDNils.append(scheduleData.title).append(",   ");
		} else {
			m_vecGuideSchedules.push_back(scheduleData);
		}
	}

	if (!boundStreamIDNils.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "dealScheduleListGuidePageSucceed succeed with ignore titles:%s", boundStreamIDNils.toStdString().c_str());
	}

	if (nullptr != onNext) {
		onNext(true);
	}
}
void PLSPlatformYoutube::requestScheduleList(const std::function<void(bool)> &onNext, const QObject *widget, bool isShowAlert)
{
	auto tmpContext = (++m_iContext);
	auto _onSucceed = [this, onNext, widget, tmpContext, isShowAlert](QByteArray data) {
		if (tmpContext != m_iContext) {
			return;
		}

		dealScheduleListSucceed(data, onNext, widget, isShowAlert);
	};

	auto _onFail = [this, onNext, tmpContext, isShowAlert](int code, QByteArray data, QNetworkReply::NetworkError error) {
		if (tmpContext != m_iContext) {
			return;
		}
		if (isShowAlert) {
			showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_RequestScheduleList);
		}

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestScheduleList(widget, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::dealScheduleListSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *widget, bool isShowAlert)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealScheduleListSucceed failed, doc is not object");
		if (isShowAlert) {
			showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_RequestScheduleList, "body not json");
		}
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	PLSYoutubeLiveinfoData selectData = getTempSelectData();
	bool isContainSelectData = false;

	m_vecSchedules.clear();
	auto root = doc.object();
	auto items = root["items"].toArray();
	PLS_INFO(MODULE_PlatformService, "dealScheduleListSucceed succeed with itemCount:%i", items.count());
	QString boundStreamIDNils = "";
	for (int i = 0; i < items.size(); i++) {
		auto dataItem = items[i].toObject();

		if (dataItem["snippet"].toObject()["isDefaultBroadcast"].toBool()) {
			continue;
		}
		auto scheduleData = PLSYoutubeLiveinfoData(dataItem);

		if (scheduleData.boundStreamId.isEmpty()) {
			//if boundStreamId is nil, then call get streamkey and url will failed.
			boundStreamIDNils.append(scheduleData.title).append(",   ");
		} else {
			m_vecSchedules.push_back(scheduleData);
		}

		if (scheduleData._id == selectData._id) {
			isContainSelectData = true;
		}

		if (m_thumMaps.contains(selectData._id)) {
			selectData.pixMap = m_thumMaps[selectData._id].pix;
		}
	}

	if (!boundStreamIDNils.isEmpty()) {
		PLS_INFO(MODULE_PlatformService, "dealScheduleListSucceed succeed with ignore titles:%s", boundStreamIDNils.toStdString().c_str());
	}

	if (isContainSelectData == false && !selectData.isNormalLive) {
		//if the remote not found selected schedule, so add it.
		m_vecSchedules.push_back(selectData);
	}
	PLSAPICommon::sortScheduleListsByCustom(m_vecSchedules);
	if (nullptr != onNext) {
		onNext(true);
	}
}

void PLSPlatformYoutube::requestStartToInsertLiveBroadcasts(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	auto _onSucceed = [this, onNext, receiver](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestStartToInsertLiveBroadcasts succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, "requestStartToInsertLiveBroadcasts failed, doc is not object");
			showAlertByCustName(_getStartLiveFailedKey(), YT_API_PostLiveBroadcasts, "body not json");

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();
		m_trySaveData._id = root["id"].toString();
		m_trySaveData.updateOriginThumUrl(root["snippet"].toObject());
		m_trySaveData.livebroadcastAPIData = root;

		requestStartToInsertLiveStreams(onNext, receiver);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, _getStartLiveFailedKey(), YT_API_PostLiveBroadcasts);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveBroadcastsInsert(receiver, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestStartToInsertLiveStreams(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	auto _onSucceed = [this, onNext, receiver](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestStartToInsertLiveStreams succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, "requestStartToInsertLiveStreams failed, doc is not object");
			showAlertByCustName(_getStartLiveFailedKey(), YT_API_PostLiveStreams, "body not json");

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.object();

		m_trySaveData.boundStreamId = root["id"].toString();
		auto info = root["cdn"].toObject()["ingestionInfo"].toObject();
		m_trySaveData.streamKey = info["streamName"].toString();
		m_trySaveData.streamUrl = getStreamUrlFromJson(info);
		m_trySaveData.streamAPIData = root;

		requestStartToBindTwo(onNext, receiver);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, _getStartLiveFailedKey(), YT_API_PostLiveStreams);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveStreamsInsert(receiver, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestStartToBindTwo(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	auto _onSucceed = [this, onNext, receiver](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestStartToBindTwo succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, "requestStartToBindTwo failed, doc is not object");
			showAlertByCustName(_getStartLiveFailedKey(), YT_API_PostLiveBroadcastsBind, "body not json");

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		if (m_trySaveData.isNormalLive) {
			requestUpdateVideoData(onNext, m_trySaveData, receiver, UpdateValue::Category);
		} else {
			if (nullptr != onNext) {
				onNext(true);
			}
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, _getStartLiveFailedKey(), YT_API_PostLiveBroadcastsBind);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIYoutube::requestLiveBroadcastsBindOrUnbind(receiver, m_trySaveData, true, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestUnBindStream(const std::function<void(bool)> &onNext, const QObject *receiver)
{

	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestUnBindStream succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			if (nullptr != onNext) {
				onNext(true);
			}
			return;
		}
		PLS_ERROR(MODULE_PlatformService, "requestUnBindStream failed, doc is not object");
		showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_PostLiveBroadcastsBind, "body not json");
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_PostLiveBroadcastsBind);

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIYoutube::requestLiveBroadcastsBindOrUnbind(receiver, m_trySaveData, false, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestStartTest()
{
	auto _onSucceed = [](QByteArray) { PLS_INFO(MODULE_PlatformService, "youtube requestStartTest succeed"); };

	auto _onFail = [this](int code, QByteArray data, QNetworkReply::NetworkError error) {
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_StartRehearsalFailed, getErrorExtraData(YT_API_RequestTestLive));
		if (retData.prismCode == PLSErrorHandler::CHANNEL_YOUTUBE_FORBIDDEN_403_REDUNDANTTRANSITION || retData.prismCode == PLSErrorHandler::CHANNEL_YOUTUBE_FORBIDDEN_403_INVALIDTRANSITION) {
			PLS_INFO(MODULE_PlatformService, "youtube requestStartTest failed but the rehearsal can continue. the prismCode is : %i", static_cast<int>(retData.prismCode));
			return;
		}

		PLS_INFO(MODULE_PlatformService, "youtube requestStartTest failed");
		if (showAlertPreAction()) {
			PLSErrorHandler::directShowAlert(retData, nullptr);
			showAlertPostAction(retData);
		}
	};
	PLSAPIYoutube::requestTestLive(this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::requestLiveStreamKey(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestLiveStreamKey succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, "requestLiveStreamKey failed, doc is not object");

			showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetLiveStream, "body not json");

			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		dealStreamKeySucceed(doc, onNext);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_GetLiveStream);

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	QStringList ids;
	ids << m_trySaveData.boundStreamId;
	PLSAPIYoutube::requestLiveStream(ids, receiver, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh, "snippet,cdn,contentDetails,status", YT_API_GetLiveStream.toUtf8());
}

void PLSPlatformYoutube::dealStreamKeySucceed(const QJsonDocument &doc, const std::function<void(bool)> &onNext)
{
	auto root = doc.object();
	auto items = root["items"].toArray();
	auto itermCount = items.size();
	if (itermCount <= 0) {
		PLS_ERROR(MODULE_PlatformService, "dealStreamKeySucceed failed, item count is zero");
	}
	QString currentStreamStatus = "active";
	QString currentIngestionType;
	for (int i = 0; i < itermCount; i++) {
		auto item = items[i].toObject();
		for (auto &scheduleData : m_vecSchedules) {
			auto info = item["cdn"].toObject()["ingestionInfo"].toObject();
			if (item["id"].toString() == scheduleData.boundStreamId) {
				scheduleData.streamKey = info["streamName"].toString();
				scheduleData.streamUrl = getStreamUrlFromJson(info);
				scheduleData.streamAPIData = item;
			}
			if (item["id"].toString() == m_trySaveData.boundStreamId) {
				currentIngestionType = item["cdn"].toObject()["ingestionType"].toString();
				currentStreamStatus = item["status"].toObject()["streamStatus"].toString();
				m_trySaveData.streamKey = info["streamName"].toString();
				m_trySaveData.streamUrl = getStreamUrlFromJson(info);
				m_trySaveData.streamAPIData = item;
			}
		}
	}

	PLS_INFO(MODULE_PlatformService, "dealStreamKeySucceed streamStatus=%s \t ingestionType:%s", currentStreamStatus.toStdString().c_str(), currentIngestionType.toUtf8().constData());
	bool isSupportType = currentIngestionType.toLower() == "rtmp" || currentIngestionType.toLower() == "hls";
	bool isCanLived = !(currentStreamStatus == "active" || currentStreamStatus == "error");

	if (isCanLived && isSupportType) {
		setIsSubChannelStartApiCall(true);
	} else {
		auto prismCode = isSupportType ? PLSErrorHandler::CHANNEL_YOUTUBE_CUSTOM_REMOTEINVALID : PLSErrorHandler::CHANNEL_YOUTUBE_CUSTOM_BROADCASTTYPENOTSUPPORT;
		showAlertByPrismCode(prismCode, _getStartLiveFailedKey(), YT_API_GetLiveStream, (isSupportType ? "remote status invalid" : "ingestion type not support"));
	}
	if (nullptr != onNext) {
		onNext(isCanLived && isSupportType);
	}
}

/**
 when live id got. both category and kids update by this.
 when not live id. only category is update by this.
 */
void PLSPlatformYoutube::requestUpdateVideoData(const std::function<void(bool)> &onNext, const PLSYoutubeLiveinfoData &infoData, const QObject *receiver, UpdateValues requestValues)
{
	auto _dealNext = [onNext, this, requestValues](bool isOK) {
		if (PLS_PLATFORM_API->isPrepareLive() && !isOK) {
			m_failedValues |= requestValues;
			isOK = true;
		}
		pls_invoke_safe(onNext, isOK);
	};

	auto _onSucceed = [this, _dealNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestUpdateVideoData succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			_dealNext(true);
		} else {
			PLS_ERROR(MODULE_PlatformService, "requestUpdateVideoData failed, doc is not object");
			if (PLS_PLATFORM_API->isPrepareLive()) {
				PLSErrorHandler::getAlertStringByCustomErrName(PLSErrCustomKey_UpdateLiveInfoFailed, getChannelName(), getErrorExtraData(YT_API_PutVideo));
			} else {
				showAlertByCustName(PLSErrCustomKey_UpdateLiveInfoFailed, YT_API_PutVideo, "body not json");
			}
			_dealNext(false);
		}
	};

	auto _onFail = [this, _dealNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		if (PLS_PLATFORM_API->isPrepareLive()) {
			PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_UpdateLiveInfoFailed, getErrorExtraData(YT_API_PutVideo));
		} else {
			showAlert({code, error, data}, PLSErrCustomKey_UpdateLiveInfoFailed, YT_API_PutVideo);
		}
		_dealNext(false);
	};
	PLSAPIYoutube::requestUpdateVideoData(receiver, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh, infoData);
}

void PLSPlatformYoutube::requestStopLive(const std::function<void()> &onNext)
{
	PLS_PLATFORM_YOUTUBE->setIsSubChannelStartApiCall(false);
	if (!m_isRehearsal) {
		PLSAPIYoutube::requestStopLive(this, nullptr);
		onNext();
		return;
	}
	if (m_selectData.isNormalLive) {
		PLSAPIYoutube::requestStopLive(this, nullptr);
		onNext();
		return;
	}

	PLS_INFO(MODULE_PlatformService, "start to unbind and crate new live stream");

	m_trySaveData = m_selectData;
	auto deleteID = m_trySaveData.boundStreamId;

	m_isIgnoreAlert = true;
	auto _onStopNext = [this, onNext, deleteID](bool) {
		m_isIgnoreAlert = false;
		onNext();
	};

	auto _onPrepareNext = [this, deleteID, _onStopNext](bool succeed) {
		if (!succeed) {
			_onStopNext(succeed);
			return;
		}
		PLSAPIYoutube::requestDeleteStream(this, deleteID, nullptr, nullptr, PLSAPICommon::RefreshType::NotRefresh);
		if (m_trySaveData.startData == m_rehearsalSavedData) {
			_onStopNext(succeed);
			return;
		}
		requestLiveBroadcastsUpdate(m_rehearsalSavedData, _onStopNext);
	};

	requestStartToInsertLiveStreams(_onPrepareNext, this);
}

void PLSPlatformYoutube::requestLiveBroadcastStatus()
{
	auto _onSucceed = [this](QByteArray data) {
		if (!QJsonDocument::fromJson(data).isObject() || QJsonDocument::fromJson(data).object()["items"].toArray().size() <= 0) {
			PLS_ERROR(MODULE_PlatformService, "requestLiveBroadcastStatus failed, doc is not object or item count is zero");
			checkLiveStatus(s_youtube_remote_error);
			return;
		}
		auto root = QJsonDocument::fromJson(data).object();
		auto itemFirst = root["items"].toArray().first().toObject();
		auto lifeCycleStatus = itemFirst["status"].toObject()["lifeCycleStatus"].toString();
		auto webPrivacyStatus = itemFirst["status"].toObject()["privacyStatus"].toString();
		if (webPrivacyStatus.toLower() != m_selectData.privacyStatus.toLower()) {
			if (m_selectData.privacyStatus.toLower() == s_youtube_private_en.toLower()) {
				emit privateChangedToOther();
			}
			m_selectData.privacyStatus = webPrivacyStatus;
		}
		checkLiveStatus(lifeCycleStatus);
	};
	PLSAPIYoutube::requestLiveBroadcastStatus(this, _onSucceed, nullptr, PLSAPICommon::RefreshType::NotRefresh);
}

void PLSPlatformYoutube::requestLiveStreamStatus(bool isToCheckHealth)
{
	if (!isToCheckHealth) {
		if (!getIsRehearsal() || m_isCallTested) {
			return;
		}
	}
	QString logFrom = YT_API_GetLiveStream + " in loop";
	auto _onSucceed = [this, isToCheckHealth](QByteArray data) { dealLiveStreamStatusSucceed(data, isToCheckHealth); };
	auto _onFail = [this, logFrom](int code, QByteArray data, QNetworkReply::NetworkError error) {
		auto extraData = getErrorExtraData(logFrom);
		extraData.printLog = false;
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_LoadLiveInfoFailed, extraData);
		if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
			PLSErrorHandler::printLog(retData);
			bool isAlreadyExpired = PLSCHANNELS_API->getChannelStatus(getChannelUUID()) == ChannelData::Expired;
			m_statusTimer->stop();
			PLSCHANNELS_API->setChannelStatus(getChannelUUID(), ChannelData::Expired);
			if (!getIsRehearsal() && !isAlreadyExpired) {
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Token.Expired").arg(getChannelName()));
			}
		}
	};
	PLSAPIYoutube::requestLiveStream({m_selectData.boundStreamId}, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh, "status", logFrom.toUtf8());
}

void PLSPlatformYoutube::dealLiveStreamStatusSucceed(const QByteArray &data, bool isToCheckHealth)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject() || doc.object()["items"].toArray().size() <= 0) {
		PLS_ERROR(MODULE_PlatformService, "dealLiveStreamStatusSucceed failed, item count is zero or data is not object");
		return;
	}
	auto items = doc.object()["items"].toArray();

	QString statusLog;
	if (isToCheckHealth) {
		auto firstItem = items.first().toObject();
		auto currentStatus = firstItem["status"].toObject()["healthStatus"].toObject()["status"].toString();
		statusLog = QString("\t with status: %1").arg(currentStatus);
		updateNewStreamStatus(currentStatus);
	}

	PLS_INFO(MODULE_PlatformService, "dealLiveStreamStatusSucceed succeed.%s", statusLog.toUtf8().constData());

	if (!getIsRehearsal() || m_isCallTested) {
		return;
	}

	for (int i = 0; i < items.size(); i++) {
		auto item = items[i].toObject();
		auto currentStreamStatus = item["status"].toObject()["streamStatus"].toString();
		if (item["id"].toString() == m_selectData.boundStreamId && currentStreamStatus == "active") {
			if (!m_isCallTested) {
				PLS_INFO(MODULE_PlatformService, "dealLiveStreamStatusSucceed ready go");
				m_isCallTested = true;
				requestStartTest();
			}
		}
	}
}

void PLSPlatformYoutube::checkLiveStatus(const QString &lifeCycleStatus)
{
	if (getIsRehearsal() && (lifeCycleStatus == "liveStarting" || lifeCycleStatus == "live")) {
		rehearsalSwitchToLive();
	}

	bool LiveEnd = lifeCycleStatus == "complete" || lifeCycleStatus == "revoked" || lifeCycleStatus == s_youtube_remote_error;
	if (!LiveEnd) {
		return;
	}

	m_statusTimer->stop();
	if (LiveStatus::LiveStarted <= PLS_PLATFORM_API->getLiveStatus() && PLS_PLATFORM_API->getLiveStatus() < LiveStatus::LiveStoped) {
		if (pls_is_dual_output_on()) {
			PLS_PLATFORM_API->doMqttRequestBroadcastEnd(this, isHorizontalOutput() ? DualOutputType::Horizontal : DualOutputType::Vertical);
		} else {
			PLS_PLATFORM_API->doMqttRequestBroadcastEnd(this, DualOutputType::All);
		}
	}
}

bool PLSPlatformYoutube::getIsContainSameStream() const
{
	bool isContainSameStream = false;
	for (const PLSYoutubeLiveinfoData &scheduleData : getScheduleDatas()) {
		if (m_trySaveData._id == scheduleData._id) {
			continue;
		}

		if (m_trySaveData.boundStreamId == scheduleData.boundStreamId) {
			isContainSameStream = true;
			break;
		}
	}
	return isContainSameStream;
}

void PLSPlatformYoutube::checkDuplicateStreamKey(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	auto _onCheckIngestionTypeNext = [this, onNext, receiver](bool isSucceed) {
		if (!isSucceed) {
			if (onNext) {
				onNext(isSucceed);
			}
			return;
		}
		checkIngestionTypeNext(onNext, receiver);
	};

	requestLiveStreamKey(_onCheckIngestionTypeNext, receiver);
}

void PLSPlatformYoutube::checkIngestionTypeNext(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	//check duplicate id
	if (getIsContainSameStream()) {
		PLS_INFO(MODULE_PlatformService, "check have duplicate boundStreamID: %s", pls_masking_person_info(m_trySaveData.boundStreamId).toUtf8().constData());
		requestStartToInsertLiveStreams(onNext, receiver);
		return;
	}

	PLSYoutubeLiveinfoData::IngestionType remoteType = PLSYoutubeLiveinfoData::IngestionType::Rtmps;
	bool isSupportType = false;
	if (m_trySaveData.streamUrl.startsWith("http")) {
		remoteType = PLSYoutubeLiveinfoData::IngestionType::Hls;
		isSupportType = true;
	} else if (m_trySaveData.streamUrl.startsWith("rtmp")) {
		remoteType = PLSYoutubeLiveinfoData::IngestionType::Rtmps;
		isSupportType = true;
	}

	//the type is same with local, or is webrtc and other not support type(will show PAR_API_ERROR_TYPE_NOT_SUPPORT errmsg in other place)
	if (m_ingestionType == remoteType || !isSupportType) {
		if (onNext) {
			onNext(true);
		}
		return;
	}
	//must recreate to switch rtmp and hls live.
	PLS_INFO(MODULE_PlatformService, "ingestionType local is %s, is not support by local setting, so crate a new liveStream", QVariant::fromValue(remoteType).toString().toUtf8().constData());
	requestStartToInsertLiveStreams(onNext, receiver);
}

void PLSPlatformYoutube::saveTheScheduleSetting(const std::function<void(bool)> &onNext, bool isNeedUpdate, const QObject *receiver)
{
	auto _onDuplicateNext = [this, onNext, receiver](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}
		if (PLS_PLATFORM_API->isPrepareLive()) {
			checkDuplicateStreamKey(onNext, receiver);
		} else {
			onNext(isSucceed);
		}
	};

	auto _onLatencyNext = [this, _onDuplicateNext, receiver, isNeedUpdate](bool isSucceed) {
		if (!isSucceed) {
			_onDuplicateNext(isSucceed);
			return;
		}
		bool isReharsalUpdate = getIsRehearsal() && PLS_PLATFORM_API->isPrepareLive() && !m_rehearsalSavedData.isCanRehearsal(); //set rehearsal auto start, auto stop, monitor
		if (isReharsalUpdate || isNeedCallUpdateBroadcastAPIWhenSchedule(m_trySaveData)) {
			//#7687 because the live broadcast api may get the old value of kids, the newest kids value may not query succeed when video api updated.
			QTimer::singleShot(1000, receiver, [this, _onDuplicateNext] {
				PLS_INFO(MODULE_PlatformService, "PLSPlatformYoutube::saveTheScheduleSetting singleShot to request live update");
				requestLiveBroadcastsUpdate(m_trySaveData.startData, _onDuplicateNext);
			});
		} else {
			_onDuplicateNext(isSucceed);
		}
	};

	if (UpdateValues fv; isNeedCallUpdateVideoAPIWhenSchedule(m_trySaveData, fv)) {
		requestUpdateVideoData(_onLatencyNext, m_trySaveData, receiver, fv);
	} else {
		_onLatencyNext(true);
	}
}

bool PLSPlatformYoutube::isNeedCallUpdateBroadcastAPIWhenSchedule(const PLSYoutubeLiveinfoData &tryData) const
{

	auto getIsNeedCall = [](const PLSYoutubeLiveinfoData &lv, const PLSYoutubeLiveinfoData &rv) -> bool {
		if (lv.title != rv.title) {
			return true;
		}
		if (lv.privacyStatus.toLower() != rv.privacyStatus.toLower()) {
			return true;
		}
		if (lv.description != rv.description) {
			return true;
		}
		if (lv.latency != rv.latency) {
			return true;
		}
		return false;
	};

	if (tryData.isNormalLive) {
		return getIsNeedCall(m_selectData, tryData);
	} else {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(tryData._id) == 0) {
				return getIsNeedCall(scheduleData, tryData);
			}
		}
	}
	return false;
}

bool PLSPlatformYoutube::isNeedCallUpdateVideoAPIWhenSchedule(const PLSYoutubeLiveinfoData &tryData, UpdateValues &requestValues) const
{
	requestValues = UpdateValue::None;
	auto getIsNeedCall = [&requestValues](const PLSYoutubeLiveinfoData &lv, const PLSYoutubeLiveinfoData &rv) -> bool {
		if (lv.categoryID != rv.categoryID) {
			requestValues |= UpdateValue::Category;
		}
		if (lv.isForKids != rv.isForKids) {
			requestValues |= UpdateValue::Kids;
		}
		return requestValues != UpdateValue::None;
	};

	if (tryData.isNormalLive) {
		return getIsNeedCall(m_selectData, tryData);
	} else {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(tryData._id) == 0) {
				return getIsNeedCall(scheduleData, tryData);
			}
		}
	}
	return false;
}

bool PLSPlatformYoutube::isValidDownloadUrl(const QString &url)
{
	if (url.isEmpty()) {
		return false; //not need call onNext
	}
	if (getTempSelectData()._id.isEmpty() && url.size() > 0) {
		return false;
	}
	return true;
}

void PLSPlatformYoutube::updateNewStreamStatus(const QString &status)
{
	if (m_healthStatus == status) {
		return;
	}

	if (m_healthStatus == s_youtube_default_status_value && status == "noData") {
		if (m_ignoreNoDataCount > 0) {
			PLS_INFO(MODULE_PlatformService, "the youtube health status ignore nodata, current count: %d", m_ignoreNoDataCount);
			--m_ignoreNoDataCount;
			return;
		}
	}

	auto tmpStatus = m_healthStatus;
	m_healthStatus = status;

	QString showStr;

	if (status == "noData") {
		showStr = tr("LiveInfo.live.health.status.nodata");
	} else if (status == "bad") {
		showStr = tr("LiveInfo.live.health.status.bad");
	} else if (status == "ok") {
		showStr = tr("LiveInfo.live.health.status.ok");
	} else if (status == "good") {
		showStr = tr("LiveInfo.live.health.status.good");
	} else {
		PLS_ERROR(MODULE_PlatformService, "the youtube health status is error key: %s", status.toUtf8().constData());
		return;
	}

	PLS_INFO(MODULE_PlatformService, "the youtube health status is from %s switch to %s", tmpStatus.toUtf8().constData(), status.toUtf8().constData());
	pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, showStr);
}

void PLSPlatformYoutube::updateDashboardStatisticsUI() const
{
	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (getIsRehearsal()) {
		latInfo.remove(ChannelData::g_viewers);
		latInfo.remove(ChannelData::g_likes);
	} else {
		latInfo.insert(ChannelData::g_viewers, 0);
		latInfo.insert(ChannelData::g_likes, 0);
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, false);
}

void PLSPlatformYoutube::updateSettingIngestionType()
{
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		return;
	}
	m_ingestionType = PLSYoutubeLiveinfoData::IngestionType::Rtmps;

	if (PLS_PLATFORM_API->getActivePlatforms().size() > 1) {
		PLS_INFO(MODULE_PlatformService, "preLive to check ingestionType, for set rtmp, because is multi broadcast");
		return;
	}

	const obs_service_t *service_obj = obs_frontend_get_streaming_service();
	if (!service_obj) {
		return;
	}

	OBSDataAutoRelease settings = obs_service_get_settings(service_obj);
	const char *service = obs_data_get_string(settings, "service");
	if (pls_is_equal(service, "YouTube - HLS")) {
		m_ingestionType = PLSYoutubeLiveinfoData::IngestionType::Hls;
	}
	PLS_INFO(MODULE_PlatformService, "preLive to check ingestionType, service:%s \tgenerate type:%s", service, QVariant::fromValue(m_ingestionType).toString().toUtf8().constData());
}

QString PLSPlatformYoutube::getStreamUrlFromJson(const QJsonObject &obj)
{
	if (obj.contains("ingestionAddress")) {
		return obj["ingestionAddress"].toString();
	}
	if (obj.contains("rtmpsIngestionAddress")) {
		return obj["rtmpsIngestionAddress"].toString();
	}
	if (obj.contains("rtmpIngestionAddress")) {
		return obj["rtmpIngestionAddress"].toString();
	}
	return {};
}

void PLSPlatformYoutube::rehearsalSwitchToLive()
{
	m_isRehearsalToLived = true;
	setIsRehearsal(false);
	PLS_INFO(MODULE_PlatformService, "the rehearsal mode. but the youtube remote status is switch to live");

	const QString &uuid = getChannelUUID();
	updateDashboardStatisticsUI();
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(true));
	PLSCHANNELS_API->setRehearsal(false);

	if (pls_get_app_exiting()) {
		return;
	}

	PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE);
	QMetaObject::invokeMethod(this, []() { PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("LiveInfo.rehearasl.switchto.live")); }, Qt::QueuedConnection);
}

void PLSPlatformYoutube::requestStatisticsInfo() const
{
	if (getIsRehearsal()) {
		return;
	}

	auto _onSucceed = [this](QByteArray data) {
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
			PLS_ERROR(MODULE_PlatformService, "requestStatisticsInfo failed, doc is not object");
		}
	};

	PLSAPIYoutube::requestVideoStatus(this, _onSucceed, nullptr, PLSAPICommon::RefreshType::NotRefresh);
}

void PLSPlatformYoutube::requestLiveBroadcastsUpdate(const PLSYoutubeStart &startData, const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) { dealLiveBroadcastsUpdateSucceed(data, onNext); };

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_UpdateLiveInfoFailed, YT_API_PutLiveBroadcasts);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLS_INFO(MODULE_PlatformService, "youtube start update live broadcasts");
	PLSAPIYoutube::requestLiveBroadcastsUpdate(this, startData, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformYoutube::dealLiveBroadcastsUpdateSucceed(const QByteArray &data, const std::function<void(bool)> &onNext)
{
	PLS_INFO(MODULE_PlatformService, "dealLiveBroadcastsUpdateSucceed succeed");
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealLiveBroadcastsUpdateSucceed failed, doc is not object");
		showAlertByCustName(PLSErrCustomKey_UpdateLiveInfoFailed, YT_API_PutLiveBroadcasts, "body not json");

		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	PLSYoutubeLiveinfoData::Latency _remoteLatency = PLSYoutubeLiveinfoData::Latency::Low;
	auto detail = doc["contentDetails"].toObject();
	m_trySaveData.startData.enableAutoStart = detail["enableAutoStart"].toBool();
	m_trySaveData.startData.enableAutoStop = detail["enableAutoStop"].toBool();
	m_trySaveData.startData.enableMonitorStream = detail["monitorStream"].toObject()["enableMonitorStream"].toBool();

	PLSAPIYoutube::getLatency(detail, _remoteLatency);
	if (m_trySaveData.latency != _remoteLatency) {
		auto cdnObject = QJsonObject();
		PLSAPIYoutube::setLatency(cdnObject, PLS_PLATFORM_YOUTUBE->getTrySaveDataData().latency);
		PLS_ERROR(MODULE_PlatformService, "dealLiveBroadcastsUpdateSucceed failed, latency not change succeed, want to %s, but is %s",
			  cdnObject["latencyPreference"].toString().toUtf8().constData(), doc["contentDetails"].toObject()["latencyPreference"].toString().toUtf8().constData());
		showAlertByPrismCode(PLSErrorHandler::CHANNEL_YOUTUBE_CUSTOM_LATENCYCHANGEFAILED, PLSErrCustomKey_UpdateLiveInfoFailed, YT_API_PutLiveBroadcasts, "latency change failed");

		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	m_trySaveData.livebroadcastAPIData = doc.object();
	m_trySaveData.latency = _remoteLatency;
	if (nullptr != onNext) {
		onNext(true);
	}
}

void PLSPlatformYoutube::forceToRefreshToken(const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [onNext](QByteArray) {
		PLS_INFO(MODULE_PlatformService, "forceToRefreshToken succeed");
		if (onNext) {
			onNext(true);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, YT_API_RefreshYoutubeToken);
		if (onNext) {
			onNext(false);
		}
	};

	PLS_INFO(MODULE_PlatformService, "forceToRefreshToken start");
	PLSAPIYoutube::refreshYoutubeTokenBeforeRequest(PLSAPICommon::RefreshType::ForceRefresh, nullptr, this, _onSucceed, _onFail);
}

static QString getYoutubeErrString(const QByteArray &data)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		return {};
	}
	auto root = doc.object();
	auto errorMsgTop = root["error"].toString();
	auto errorMsg = root["error"].toObject()["message"].toString();
	auto errorArrs = root["error"].toObject()["errors"].toArray();

	QString errorReason;
	if (!errorArrs.isEmpty()) {
		errorReason = errorArrs[0].toObject()["reason"].toString();
	}
	if (errorReason.isEmpty()) {
		errorReason = errorMsg;
	}
	if (errorReason.isEmpty()) {
		errorReason = errorMsgTop;
	}
	return errorReason;
}

//only this youtube is finished, not mean all live is finished. so the live data should not reset in hear, need reset in PLSPlatformApi::liveEnded signal.
void PLSPlatformYoutube::onLiveEnded()
{
	m_isStopping = true;
	emit receiveLiveStop();

	setIsSubChannelStartApiCall(false);

	if (m_statusTimer && m_statusTimer->isActive()) {
		m_statusTimer->stop();
	}

	requestStopLive([this]() { liveEndedCallback(); });
}

void PLSPlatformYoutube::onAllPrepareLive(bool value)
{
	if (!value && getSelectData().isNormalLive && !getSelectData()._id.isEmpty()) {
		m_normalData._id = m_normalData.boundStreamId = m_normalData.streamKey = m_normalData.streamUrl = "";
		setSelectData(m_normalData);
	}

	if (!value && getIsSubChannelStartApiCall()) {
		requestStopLive([this, value]() { PLSPlatformBase::onAllPrepareLive(value); });
		return;
	}

	updateDashboardStatisticsUI();

	PLSPlatformBase::onAllPrepareLive(value);
}

void PLSPlatformYoutube::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}
	PLSPlatformBase::setIsScheduleLive(!getSelectData().isNormalLive);
	const QString &uuid = getChannelUUID();
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(true));
	PLSCHANNELS_API->channelModified(uuid);
	m_healthStatus = s_youtube_default_status_value;
	m_requestStatusCount = 0;
	m_ignoreNoDataCount = s_max_ignore_nodata_count;

	QString resolutionKey = PLSServerStreamHandler::instance()->getOutputResolution(pls_is_dual_output_on() && PLS_PLATFORM_YOUTUBE->isVerticalOutput());
	PLS_INFO(MODULE_PlatformService, "youtube start with resolutionKey:%s latency:%s", resolutionKey.toUtf8().constData(),
		 QVariant::fromValue(getSelectData().latency).toString().toUtf8().constData());

	auto resList = resolutionKey.split("x");
	if (resList.size() >= 2) {
		bool isMoreThan1080 = qMin(resList[0].toInt(), resList[1].toInt()) > 1080;
		if (getSelectData().latency == PLSYoutubeLiveinfoData::Latency::UltraLow && isMoreThan1080) {
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("LiveInfo.latency.ultra.low.start.1080.toast"));
		}
	}

	showFailedAPIToast();

	if (m_statusTimer && !m_statusTimer->isActive()) {
		m_statusTimer->start();
	}

	if (PLS_PLATFORM_API->isPrismLive(this)) {
		//when start live will force to update youtube token. because prism server will update token each hour.
		PLS_INFO(MODULE_PlatformService, "%s %s requestRefreshAccessToken", PrepareInfoPrefix, __FUNCTION__);
		return;
	}
}
void PLSPlatformYoutube::showFailedAPIToast() const
{
	if (m_failedValues == UpdateValue::None) {
		PLS_INFO(MODULE_PlatformService, "youtube start live and not show api failed toast.");
		return;
	}
	bool constainCategory = (m_failedValues & UpdateValue::Category) != 0;
	bool constainThum = (m_failedValues & UpdateValue::Thum) != 0;
	bool constainKids = (m_failedValues & UpdateValue::Kids) != 0;

	PLS_INFO(MODULE_PlatformService, "youtube start live and show api failed toast. category:%s thum:%s kids:%s", pls_bool_2_string(constainCategory), pls_bool_2_string(constainThum),
		 pls_bool_2_string(constainKids));

	QString toastStr;
	if (constainCategory && constainThum && constainKids) {
		toastStr = tr("YouTube.Start.Continue.Category.Thum.Kids");
	} else if (constainCategory && constainThum) {
		toastStr = tr("YouTube.Start.Continue.Category.Thum");
	} else if (constainCategory && constainKids) {
		toastStr = tr("YouTube.Start.Continue.Category.Kids");
	} else if (constainThum && constainKids) {
		toastStr = tr("YouTube.Start.Continue.Thum.Kids");
	} else if (constainCategory) {
		toastStr = tr("YouTube.Start.Continue.Category");
	} else if (constainThum) {
		toastStr = tr("YouTube.Start.Continue.Thum");
	} else if (constainKids) {
		toastStr = tr("YouTube.Start.Continue.Kids");
	}

	if (!toastStr.isEmpty()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, toastStr);
	}
}

void PLSPlatformYoutube::createNewNormalData()
{
	m_normalData = PLSYoutubeLiveinfoData();
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	if (info.isEmpty()) {
		return;
	}
	m_normalData.description = s_description_default_add;
	auto channelName = info.value(ChannelData::g_nickName, "").toString();
	m_normalData.title = tr("LiveInfo.live.title.suffix").arg(channelName);
	m_normalData.privacyStatus = getPrivacyEnglishDatas()[0];
	m_normalData.categoryID = kDefaultCategoryID;
	auto iLatency = info.value(ChannelData::g_youtube_latency, static_cast<int>(PLSYoutubeLiveinfoData::Latency::Normal)).toInt();
	m_normalData.latency = static_cast<PLSYoutubeLiveinfoData::Latency>(iLatency);
}

QJsonObject PLSPlatformYoutube::getLiveStartParams()
{
	QJsonObject platform(PLSPlatformBase::getLiveStartParams());
	platform["isRehearsal"] = getIsRehearsal();

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
	platform["isKids"] = getSelectData().isForKids;
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

bool PLSPlatformYoutube::isPrivateStatus() const
{
	const auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	QString category_en = info.value(ChannelData::g_displayLine2, "").toString();
	if (category_en.isEmpty()) {
		category_en = info.value(ChannelData::g_catogry, "").toString();
	}
	bool isPrivate = s_youtube_private_en == category_en;
	return isPrivate;
}

bool PLSPlatformYoutube::isKidsLiving() const
{
	return getSelectData().isForKids;
}

void PLSPlatformYoutube::showAutoStartFalseAlertIfNeeded()
{
	//init in this method
	YoutubeStartShowData model;

	if (model.showStr.isEmpty()) {
		return;
	}

	if (PLS_PLATFORM_API->getUuidOnStarted().size() <= 1) {
		PLSAlertView::Button ret = PLSAlertView::Button::Ok;

		QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Ok, tr("OK")}};

		if (!model.redirectUrl.isEmpty()) {
			buttons.insert(PLSAlertView::Button::Open, tr("Live.Check.youtube.gotoPage"));
		}

		if (!model.isErrMsg) {
			ret = PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"), model.showStr, buttons);
		} else {
			ret = pls_alert_error_message(pls_get_main_view(), QTStr("Alert.Title"), model.showStr, buttons);
		}

		if (ret == PLSAlertView::Button::Open) {
			QDesktopServices::openUrl(QUrl(model.redirectUrl));
		}
		return;
	}

	if (model.redirectUrl.isEmpty()) {
		model.redirectKey = "";
	} else {
		auto index = model.showStr.indexOf(model.redirectKey);
		model.showStr.replace(index, model.redirectKey.length(), model.redirectUrl);
	}
	pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, model.showStr, model.redirectUrl, model.redirectKey);
}

QByteArray getFileHashValue(const QString &imageFilePath)
{
	QFile imageFile(imageFilePath);
	imageFile.open(QFile::ReadOnly);
	QByteArray imageData = imageFile.readAll();
	imageFile.close();

	QByteArray md5Str = QCryptographicHash::hash(imageData, QCryptographicHash::Md5).toHex();
	return md5Str;
}

void PLSPlatformYoutube::downloadThumImage(const std::function<void()> &onNext, const QString &url, const QObject *receiver, bool notShowThisPix)
{
	if (!isValidDownloadUrl(url)) {
		return;
	}
	static qint64 s_iContext = 0;
	++s_iContext;
	auto tmpContext = s_iContext;
	auto _callBack = [this, notShowThisPix, onNext, tmpContext](bool ok, const QString &imagePath) {
		if (tmpContext != s_iContext) {

			if (QFile::exists(imagePath)) {
				QFile::remove(imagePath);
			}
			return;
		}
		dealDownloadImageCallBack(ok, imagePath, notShowThisPix);
		if (onNext) {
			onNext();
		}

		if (QFile::exists(imagePath)) {
			QFile::remove(imagePath);
		}
	};
	PLSAPICommon::downloadImageAsync(receiver, url, _callBack, true);
}
void PLSPlatformYoutube::dealDownloadImageCallBack(bool ok, const QString &imagePath, bool notShowThisPix)
{
	bool isSaved = false;
	auto scheduleID = getTempSelectData()._id;
	if (ok) {
		auto bys = getFileHashValue(imagePath);
		if (!scheduleID.isEmpty() && m_thumMaps.contains(scheduleID) && m_thumMaps[scheduleID].hashs.contains(bys)) {
			isSaved = true;
		}
		if (!isSaved) {
			const auto px = QPixmap(imagePath);
			m_thumMaps[scheduleID].hashs = m_thumMaps[scheduleID].hashs.append(", ").append(bys);
			if (!notShowThisPix) {
				m_thumMaps[scheduleID].pix = px;
				getTempSelectDataRef().pixMap = px;
			}
		}
	}
	if (getTempSelectDataRef().pixMap.isNull() && isSaved) {
		getTempSelectDataRef().pixMap = m_thumMaps[scheduleID].pix;
	}
}
void PLSPlatformYoutube::refreshTokenSucceed()
{
}

void PLSPlatformYoutube::setIsRehearsal(bool value)
{
	m_isRehearsal = value;
}

void PLSPlatformYoutube::updateScheduleList()
{
	auto next = [this](bool) { emit scheduleListUpdateFinished(); };
	this->requestScheduleListByGuidePage(next, this);
}

void PLSPlatformYoutube::convertScheduleListToMapList()
{
	std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
	auto uuid = this->getChannelUUID();
	auto tmpSrc = m_vecGuideSchedules;
	QVariantList tmpResult;
	auto convertData = [uuid](const PLSYoutubeLiveinfoData &data) {
		QVariantMap mapData;
		mapData.insert(ChannelData::g_timeStamp, QVariant::fromValue(data.timeStamp));
		mapData.insert(ChannelData::g_nickName, data.title);
		mapData.insert(ChannelData::g_channelUUID, uuid);
		mapData.insert(ChannelData::g_channelName, YOUTUBE);
		return QVariant::fromValue(mapData);
	};
	std::transform(tmpSrc.cbegin(), tmpSrc.cend(), std::back_inserter(tmpResult), convertData);

	mySharedData().m_scheduleList = tmpResult;
}

bool PLSPlatformYoutube::showAlertPreAction()
{
	if (m_isIgnoreAlert) {
		PLS_INFO(MODULE_PlatformService, "youtube ignore this alert, because m_isIgnoreAlert == true");
		return false;
	}
	return true;
}

PLSErrorHandler::ExtraData PLSPlatformYoutube::getErrorExtraData(const QString &urlEn, const QString &urlKr)
{
	PLSErrorHandler::ExtraData extraData;
	extraData.urlEn = urlEn;

	QString selectID = m_trySaveData._id;
	if (selectID.isEmpty()) {
		selectID = m_selectData._id;
	}
	extraData.pathValueMap = {{"youtubeLiveId", selectID}};
	return extraData;
}

void PLSPlatformYoutube::showAlert(const PLSErrorHandler::NetworkData &netData, const QString &customErrName, const QString &logFrom, const QString &errorReason)
{
	if (!showAlertPreAction()) {
		return;
	}
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlert(netData, getChannelName(), customErrName, getErrorExtraData(logFrom));
	showAlertPostAction(retData, errorReason);
}
void PLSPlatformYoutube::showAlertByCustName(const QString &customErrName, const QString &logFrom, const QString &errorReason)
{
	if (!showAlertPreAction()) {
		return;
	}

	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlertByCustomErrName(customErrName, getChannelName(), getErrorExtraData(logFrom));
	showAlertPostAction(retData, errorReason);
}
void PLSPlatformYoutube::showAlertByPrismCode(PLSErrorHandler::ErrCode prismCode, const QString &customErrName, const QString &logFrom, const QString &errorReason)
{
	if (!showAlertPreAction()) {
		return;
	}
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlertByPrismCode(prismCode, getChannelName(), customErrName, getErrorExtraData(logFrom));
	showAlertPostAction(retData, errorReason);
}

void PLSPlatformYoutube::showAlertPostAction(const PLSErrorHandler::RetData &retData, const QString &errorReason)
{
	setFailedErr(retData.extraData.urlEn + " " + (errorReason.isEmpty() ? retData.failedLogString : errorReason));
	if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
		emit closeDialogByExpired();
		if (retData.clickedBtn == PLSAlertView::Button::Ok) {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
	}
}

PLSYoutubeLiveinfoData::PLSYoutubeLiveinfoData(const QJsonObject &data) : isNormalLive(false)
{
	auto snippet = data["snippet"].toObject();
	auto details = data["contentDetails"].toObject();

	this->boundStreamId = details["boundStreamId"].toString();
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
	this->timeStamp = PLSDateFormate::timeStringToStamp(this->startTimeOrigin);
	this->startTimeShort = PLSDateFormate::timeStampToShortString(this->timeStamp);
	this->startTimeUTC = PLSDateFormate::timeStampToUTCString(this->timeStamp);
	this->privacyStatus = data["status"].toObject()["privacyStatus"].toString();
	this->liveChatId = snippet["liveChatId"].toString();

	this->iskidsUserSelect = true;
	this->isForKids = data["status"].toObject()["madeForKids"].toBool();
	this->startData.enableAutoStart = details["enableAutoStart"].toBool();
	this->startData.enableAutoStop = details["enableAutoStop"].toBool();
	this->startData.enableMonitorStream = details["monitorStream"].toObject()["enableMonitorStream"].toBool();
	PLSAPIYoutube::getLatency(details, this->latency);
	this->livebroadcastAPIData = data;
	this->isCaptions = details["closedCaptionsType"].toString() != s_closed_captions_type;
	updateOriginThumUrl(snippet);
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

YoutubeStartShowData::YoutubeStartShowData()
{
	QString channelStr = YOUTUBE;

	auto platformActived = PLS_PLATFORM_ACTIVIED;

	this->isMutiLive = platformActived.size() > 1;
	const PLSPlatformYoutube *youtubeItem = nullptr;

	for (auto item : platformActived) {
		if (YOUTUBE != item->getChannelName()) {
			continue;
		}
		if (PLSServiceType::ST_CUSTOM == item->getServiceType()) {
			this->isContainRtmp = true;
			continue;
		}
		if (PLSServiceType::ST_YOUTUBE == item->getServiceType()) {
			youtubeItem = dynamic_cast<PLSPlatformYoutube *>(item);
			this->isContainChannel = true;
			continue;
		}
	}

	if (this->isContainRtmp && !this->isContainChannel) {
		//only Youtube rtmp
		this->showStr = QTStr("LiveInfo.live.Check.AutoStart.Rrmp").arg(channelStr);
		this->isErrMsg = true;
		return;
	}

	if (youtubeItem == nullptr || youtubeItem->getIsRehearsal()) {
		return;
	}

	bool isAutoStart = youtubeItem->getSelectData().startData.enableAutoStart;
	if (isAutoStart == true) {
		if (this->isContainRtmp) {
			//channel not need start.
			this->showStr = QTStr("LiveInfo.live.Check.AutoStart.Rrmp").arg(channelStr);
			this->isErrMsg = true;
		}
		return;
	}

	this->redirectUrl = g_plsYoutubeStudioManagerUrl.arg(youtubeItem->getSelectData()._id);

	if (isAutoStart == false && !this->isContainRtmp) {
		//only channel need start by hand
		this->showStr = QTStr("LiveInfo.live.Check.AutoStart.Channel").arg(channelStr);
		return;
	}

	//both need start by hand
	this->showStr = QTStr("LiveInfo.live.Check.AutoStart.Both.ChannelAndRtmp").arg(channelStr);
}

void PLSYoutubeLiveinfoData::updateOriginThumUrl(const QJsonObject &snippet)
{
	if (!snippet.contains("thumbnails")) {
		return;
	}
	auto thum = snippet["thumbnails"].toObject();
	if (!thum.contains(s_youtube_thum_level)) {
		return;
	}
	this->thumbnailUrl = thum[s_youtube_thum_level].toObject()["url"].toString();
}
