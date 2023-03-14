#include "PLSPlatformVLive.h"
#include <QObject>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <PLSChannelDataHandler.h>
#include <QDebug>
#include <QTimer>
#include <cassert>
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "PLSAPIVLive.h"
#include "PLSChannelDataAPI.h"
#include "PLSDateFormate.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSScheLiveNotice.h"
#include "alert-view.hpp"
#include "frontend-api.h"
#include "prism/PLSPlatformPrism.h"
#include "log.h"
#include "log/log.h"
#include "main-view.hpp"
#include "pls-app.hpp"
#include "pls-net-url.hpp"
#include "window-basic-main.hpp"
#include <QUuid>

const static QString vliveBasic = "BASIC";
const static QString vlivePreminm = "PREMIUM";
const static QString boardTypeStar = "STAR";

PLSPlatformVLive::PLSPlatformVLive() : m_bTempSchedule(false)
{
	setSingleChannel(false);
	m_statusTimer = new QTimer(this);
	connect(m_statusTimer, &QTimer::timeout, this, &PLSPlatformVLive::requestStatisticsInfo);

	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [=](const QVariantMap &info) {
		if (this->getChannelUUID() == info.value(ChannelData::g_channelUUID).toString()) {
			//reInitLiveInfo(true);
			PLSImageStatic::instance()->profileUrlMap.clear();
		}
	});
	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveEndPageShowComplected, this, [=](bool isRecord) {
		if (!isRecord && !getIsRehearsal() && isActive()) {
			reInitLiveInfo(true, true);
		}
	});
}

PLSPlatformVLive::~PLSPlatformVLive() {}

PLSServiceType PLSPlatformVLive::getServiceType() const
{
	return PLSServiceType::ST_VLIVE;
}

const QString PLSPlatformVLive::getSelectID()
{
	return m_selectData._id;
}

const vector<PLSVLiveLiveinfoData> &PLSPlatformVLive::getScheduleDatas()
{
	return m_vecSchedules;
}

const PLSVLiveLiveinfoData PLSPlatformVLive::getNomalLiveData()
{
	return m_noramlData;
}

const PLSVLiveLiveinfoData &PLSPlatformVLive::getTempSelectData()
{
	const PLSVLiveLiveinfoData &data = getTempSelectDataRef();
	return data;
}

const PLSVLiveLiveinfoData PLSPlatformVLive::getSelectData()
{
	return m_selectData;
}

PLSVLiveProfileData PLSPlatformVLive::getChannelProfile(const QVariantMap &map)
{
	auto profileMap = map.value(ChannelData::g_vliveProfileData).toMap();
	auto profileJson = QJsonObject(QJsonDocument::fromJson(QJsonDocument::fromVariant(QVariant(profileMap)).toJson()).object());
	auto profileData = PLSVLiveProfileData(profileJson);
	return profileData;
}

void PLSPlatformVLive::setSelectDataByID(const QString &scheID)
{
	for (auto &data : m_vecSchedules) {
		if (data._id == scheID) {
			setSelectData(data);
		}
	}
}

void PLSPlatformVLive::removeExpiredSchedule(const vector<QString> &ids)
{
	QString tempID = getTempSelectData()._id;
	QString selectID = getSelectData()._id;
	for (auto &id : ids) {
		for (int i = 0; i < m_vecSchedules.size(); i++) {
			auto &item = m_vecSchedules[i];
			if (item._id == id && tempID != id && selectID != id) {
				m_vecSchedules.erase(m_vecSchedules.begin() + i);
				break;
			}
		}
	}
}

void PLSPlatformVLive::setSelectData(PLSVLiveLiveinfoData data, bool isNeedKeepProfile)
{
	for (const auto &profile : m_vecProfiles) {
		if (data.profile.memberId == profile.memberId) {
			data.profile = profile;
			break;
		}
	}
	m_bTempSchedule = !data.isNormalLive;
	m_selectData = data;
	setTempSelectID(data._id);
	if (!data.isNormalLive) {
		for (auto &scheduleData : m_vecSchedules) {
			if (scheduleData._id.compare(data._id) == 0) {
				scheduleData = data;
			}
		}
	}
	__super::setTitle(m_selectData.title.toStdString());

	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	PLSPlatformVLive::changeChannelDataWhenProfileSelect(latInfo, data.profile, this);
	if (!data.profile.nickname.isEmpty()) {
		latInfo[ChannelData::g_vliveProfileData] = data.profile.getMapData();
	} else if (!isNeedKeepProfile) {
		latInfo[ChannelData::g_vliveProfileData] = {};
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, true);
}

bool PLSPlatformVLive::isModifiedWithNewData(QString title, const QPixmap &localPixmap, PLSVLiveLiveinfoData *uiData)
{
	bool isModified = false;
	PLSVLiveLiveinfoData sData = getTempSelectData();
	if (sData.isNormalLive) {
		sData = getNomalLiveData();
	}

	if (title.compare(sData.title) != 0) {
		isModified = true;
	} else if (sData.isNormalLive) {
		if (localPixmap != sData.pixMap) {
			isModified = true;
		}
	}

	if (isModified == false) {
		if (sData.board.boardId != getTempBoardData().boardId || sData.profile.memberId != getTempProfileData().memberId) {
			isModified = true;
		}
	}

	if (uiData != NULL) {
		//*uiData = sData;
		uiData->title = title;

		if (localPixmap != sData.pixMap) {
			uiData->pixMap = localPixmap;
			uiData->thumRemoteUrl = "";
		}
		uiData->board = getTempBoardData();
		uiData->profile = getTempProfileData();
	}

	return isModified;
}

void PLSPlatformVLive::saveSettings(function<void(bool)> onNext, PLSVLiveLiveinfoData uiData)
{

	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	auto _onNext = [=](bool isSucceed, const QString &iamgeUrl) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}

		PLSVLiveLiveinfoData savedData = uiData;
		savedData.thumRemoteUrl = iamgeUrl;
		if (!getIsTempSchedule()) {
			m_noramlData = savedData;
		}

		setSelectData(savedData);

		QJsonObject neoJson;
		neoJson["isSchedule"] = BOOL2STR(!m_selectData.isNormalLive);
		neoJson["scheduleID"] = pls_masking_user_id_info(m_selectData._id);
		neoJson["profileID"] = pls_masking_user_id_info(m_selectData.profile.memberId);
		neoJson["profileName"] = pls_masking_person_info(m_selectData.profile.nickname);
		neoJson["boardID"] = pls_masking_user_id_info(QString::number(m_selectData.board.boardId));
		neoJson["channelUuid"] = getChannelUUID();
		neoJson["channelID"] = pls_masking_user_id_info(m_selectData.channelSeq);

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ " with save data: %s", QJsonDocument(neoJson).toJson().constData());

		QJsonObject neoKRJson;
		neoKRJson["isSchedule"] = BOOL2STR(!m_selectData.isNormalLive);
		neoKRJson["scheduleID"] = m_selectData._id;
		neoKRJson["profileID"] = m_selectData.profile.memberId;
		neoKRJson["profileName"] = m_selectData.profile.nickname;
		neoKRJson["boardID"] = QString::number(m_selectData.board.boardId);
		neoKRJson["channelUuid"] = getChannelUUID();
		neoKRJson["channelID"] = m_selectData.channelSeq;
		PLS_INFO_KR(MODULE_PlatformService, __FUNCTION__ " with save data: %s", QJsonDocument(neoKRJson).toJson().constData());

		if (!PLS_PLATFORM_API->isPrepareLive()) {
			onNext(isSucceed);
			return;
		}

		auto _onAPINext = [=](bool value) {
			const QString &uuid = getChannelUUID();
			QString _shareUrl = value ? getShareUrl() : QString();
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrl, _shareUrl);
			PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, _shareUrl);

			auto latInfo = PLSCHANNELS_API->getChannelInfo(uuid);
			if (PLSAPIVLive::isVliveFanship()) {
				latInfo.remove(ChannelData::g_viewers);
			} else {
				latInfo.insert(ChannelData::g_viewers, 0);
			}
			PLSCHANNELS_API->setChannelInfos(latInfo, false);
			onNext(value);
		};
		requestStartLive(_onAPINext);
	};

	if (!uiData.isNormalLive) {
		if (PLS_PLATFORM_API->isLiving()) {
			_onNext(true, uiData.thumRemoteUrl);
		} else {
			requestScheduleList([=](bool isOk) { _onNext(isOk, uiData.thumRemoteUrl); }, this, true, uiData._id);
		}
		return;
	}

	if (uiData.pixMap.isNull()) {
		_onNext(true, uiData.thumRemoteUrl);
		return;
	}

	if (uiData.pixMap == getTempSelectData().pixMap) {
		_onNext(true, uiData.thumRemoteUrl);
		return;
	}

	requestUploadImage(uiData.pixMap, _onNext);
}

void PLSPlatformVLive::setThumPixmap(const QPixmap &pixMap)
{
	getTempSelectDataRef().pixMap = pixMap;
}

void PLSPlatformVLive::setThumRemoteUrl(const QString &remoteUrl)
{
	getTempSelectDataRef().thumRemoteUrl = remoteUrl;
}

const QString PLSPlatformVLive::getDefaultTitle(const QString &preStr)
{
	QString pre = preStr;
	if (!pre.isEmpty()) {
		pre = tr("LiveInfo.live.title.suffix").arg(pre);
	}
	return pre;
}

void PLSPlatformVLive::onShowScheLiveNoticeIfNeeded(const QString &channelID)
{
	static bool _isRequesting = false;
	if (PLSVliveStatic::instance()->showNoticeMap[channelID] == true || _isRequesting == true) {
		return;
	}
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	auto profileData = PLSPlatformVLive::getChannelProfile(info);
	if (profileData.memberId.trimmed() == "") {
		return;
	}

	_isRequesting = true;
	auto _onNext = [=](bool value) {
		_isRequesting = false;
		if (!value || getScheduleDatas().size() == 0) {
			return;
		}
		const PLSVLiveLiveinfoData *liveInfo = nullptr;
		long old10Min = PLSDateFormate::getNowTimeStamp() + 60 * 10;
		long nowTime = PLSDateFormate::getNowTimeStamp();
		vector<PLSVLiveLiveinfoData> tempDatas = getScheduleDatas();
		auto descendOrder = [&](const PLSVLiveLiveinfoData &left, const PLSVLiveLiveinfoData &right) { return left.startTimeStamp < right.startTimeStamp; };
		sort(tempDatas.begin(), tempDatas.end(), descendOrder);
		for (auto &sli : tempDatas) {
			if (sli.isNewLive == false) {
				continue;
			}
			if (old10Min > sli.startTimeStamp && sli.startTimeStamp > nowTime) {
				liveInfo = &sli;
				break;
			}
		}

		if (liveInfo && PLSVliveStatic::instance()->showNoticeMap[channelID] == false) {
			PLSVliveStatic::instance()->showNoticeMap[channelID] = true;
			showScheLiveNotice(*liveInfo);
		}
	};
	requestScheduleList(_onNext, this, false);
}

QString PLSPlatformVLive::getShareUrl()
{
	return QString(CHANNEL_VLIVE_SHARE).arg(m_selectData.startVideoSeq);
}

QString PLSPlatformVLive::getShareUrlEnc()
{
	return QString(CHANNEL_VLIVE_SHARE).arg(pls_masking_person_info(m_selectData.startVideoSeq));
}

void PLSPlatformVLive::liveInfoisShowing()
{
	auto _checkContainScheduleData = [=](const QString &schedID) {
		bool containData = false;
		for (const auto &info : m_vecSchedules) {
			if (info._id == schedID) {
				containData = true;
				break;
			}
		}
		return containData;
	};

	auto selectData = getSelectData();
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	auto profileData = PLSPlatformVLive::getChannelProfile(info);
	selectData.profile = profileData;

	if (selectData.isNormalLive && selectData.channelSeq > 0) {
		m_noramlData = selectData;
	} else {
		m_noramlData = PLSVLiveLiveinfoData();
		m_noramlData.profile = profileData;
		m_noramlData.title = getDefaultTitle(profileData.nickname);
		m_noramlData.channelSeq = info.value(ChannelData::g_subChannelId, "").toString();
	}

	if (!selectData.isNormalLive && !_checkContainScheduleData(selectData._id)) {
		m_vecSchedules.push_back(selectData);
	}

	m_tempNoramlData = m_noramlData;
	setTempSelectID(selectData._id);
	setTempSchedule(!selectData.isNormalLive);
	setTempBoardData(selectData.board);
	setTempProfileData(selectData.profile);
}

void PLSPlatformVLive::reInitLiveInfo(bool isReset, bool isNeedKeepProfile)
{
	if (!isReset) {
		return;
	}
	isStopedByRemote = false;
	m_isRehearsal = false;
	m_bTempSelectID = "";
	m_bTempSchedule = false;
	m_selectData = PLSVLiveLiveinfoData();
	m_noramlData = PLSVLiveLiveinfoData();
	m_tempNoramlData = PLSVLiveLiveinfoData();

	if (isNeedKeepProfile) {
		auto channelInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
		m_selectData.profile = PLSPlatformVLive::getChannelProfile(channelInfo);
	}
	setSelectData(m_selectData, isNeedKeepProfile);
};

void PLSPlatformVLive::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(MODULE_PlatformService, "%s %s show liveinfo value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	value = pls_exec_live_Info_vlive(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_NAVERTV, "%s %s liveinfo closed value(%s)", PrepareInfoPrefix, __FUNCTION__, BOOL2STR(value));
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	auto latInfo = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (getIsRehearsal()) {
		latInfo.remove(ChannelData::g_viewers);
		latInfo.remove(ChannelData::g_likes);
	} else {
		if (PLSAPIVLive::isVliveFanship()) {
			latInfo.remove(ChannelData::g_viewers);
			latInfo.insert(ChannelData::g_likes, "0");
		} else {
			latInfo.insert(ChannelData::g_viewers, "0");
			latInfo.insert(ChannelData::g_likes, "0");
		}
	}
	PLSCHANNELS_API->setChannelInfos(latInfo, true);

	if (getIsRehearsal()) {
		PLSCHANNELS_API->rehearsalBegin();
	}
	prepareLiveCallback(value);
}

void PLSPlatformVLive::initVliveGcc(function<void()> onNext)
{
	auto _finish = [=](QNetworkReply *networkReplay, int, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		PLSVliveStatic::instance()->isLoaded = true;

		PLSPlatformApiResult apiResult = PLSPlatformApiResult::PAR_SUCCEED;
		int responseCode = 0;
		auto doc = QJsonDocument::fromJson(data);
		if (doc.isObject()) {
			responseCode = doc["code"].toInt();
		}
		if (responseCode > 0 && responseCode != 1000) {
			apiResult = PLSPlatformApiResult::PAR_API_FAILED;
		} else if (error != QNetworkReply::NetworkError::NoError) {
			apiResult = PLSPlatformApiResult::PAR_API_FAILED;
		} else if (!doc.isObject()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			apiResult = PLSPlatformApiResult::PAR_API_FAILED;
		}

		if (apiResult != PLSPlatformApiResult::PAR_SUCCEED) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed");
			onNext();
			return;
		}

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");

		auto root = doc.object();

		auto result = root["result"].toObject();
		QString gcc = result["gcc"].toString();

		if (gcc.isEmpty()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, gcc is empty");
			onNext();
			return;
		}
		QString lang = "";
		QString locale = "";
		auto localLists = result["localeLabelList"].toArray();
		for (int i = 0; i < localLists.size(); i++) {
			auto data = localLists[i].toObject();
			if (data["country"].toString().compare(gcc, Qt::CaseInsensitive) == 0) {
				lang = data["language"].toString();
				locale = data["locale"].toString();
				break;
			}
		}
		if (!gcc.isEmpty() && !lang.isEmpty()) {
			PLSVliveStatic::instance()->gcc = gcc;
			PLSVliveStatic::instance()->lang = lang;
			PLSVliveStatic::instance()->locale = locale;
		}
		onNext();
	};

	PLSAPIVLive::vliveRequestGccAndLanguage(this, _finish);
}

void PLSPlatformVLive::getCountryCodes(function<void()> onNext)
{
	auto _finish = [=](QNetworkReply *networkReplay, int, QByteArray data, QNetworkReply::NetworkError, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		auto doc = QJsonDocument::fromJson(data);
		auto root = doc.object()["result"].toArray();
		auto nameKey = IS_KR() ? "name" : "nameEn";

		for (int i = 0; i < root.size(); i++) {
			auto data = root[i].toObject();
			PLSVliveStatic::instance()->countries.insert(data["countryCode"].toString(), data[nameKey].toString());
		}

		if (PLSVliveStatic::instance()->countries.isEmpty()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
		} else {
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		}

		onNext();
	};

	PLSAPIVLive::vliveRequestCountryCodes(this, _finish);
}

void PLSPlatformVLive::requestChannelInfo(const QVariantMap &srcInfo, UpdateCallback finishedCall)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		auto doc = QJsonDocument::fromJson(data);

		if (doc.isObject() && doc.object()["code"].toInt() == 3001) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, code == 3001");
			QVariantMap info = srcInfo;
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
			finishedCall(QList<QVariantMap>{info});
			return;
		}

		if (!doc.isArray() || doc.array().isEmpty()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not array or empty");
			QVariantMap info = srcInfo;
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
			finishedCall(QList<QVariantMap>{info});

			return;
		}

		QList<QVariantMap> infos;
		QJsonArray neloArr;
		// save sub channels
		for (int i = 0, count = doc.array().count(); i < count; ++i) {
			auto object = doc.array()[i].toObject();
			auto subChannelID = object["channelCode"].toString();
			QVariantMap info;
			info[ChannelData::g_channelToken] = srcInfo[ChannelData::g_channelToken];
			info[ChannelData::g_platformName] = VLIVE;
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
			info[ChannelData::g_subChannelId] = subChannelID;
			info[ChannelData::g_nickName] = object["channelName"].toString();
			info[ChannelData::g_vliveNormalChannelName] = info[ChannelData::g_nickName]; //backup
			info[ChannelData::g_catogry] = "";

			info[ChannelData::g_shareUrl] = QString();
			info[ChannelData::g_viewers] = "0";
			info[ChannelData::g_viewersPix] = ChannelData::g_naverTvViewersIcon;
			info[ChannelData::g_comments] = "0";
			info[ChannelData::g_commentsPix] = ChannelData::g_defaultCommentsIcon;
			info[ChannelData::g_likes] = "0";
			info[ChannelData::g_likesPix] = ChannelData::g_naverTvLikeIcon;
			info[ChannelData::g_catogry] = QString();
			QString channelUrl = object["channelProfileImage"].toString();
			info[ChannelData::g_userIconThumbnailUrl] = channelUrl;

			if (!channelUrl.isEmpty() && PLSImageStatic::instance()->profileUrlMap.contains(channelUrl) && !PLSImageStatic::instance()->profileUrlMap[channelUrl].isEmpty()) {
				info[ChannelData::g_userIconCachePath] = PLSImageStatic::instance()->profileUrlMap[channelUrl];
			} else if (auto icon = PLSAPIVLive::downloadImageSync(this, channelUrl, info[ChannelData::g_platformName].toString()); icon.first) {
				if (!icon.second.isEmpty()) {
					info[ChannelData::g_userIconCachePath] = icon.second;
					PLSImageStatic::instance()->profileUrlMap[channelUrl] = icon.second;
				}
			}

			infos.append(info);

			QJsonObject _sub;
			_sub["channelCode"] = pls_masking_user_id_info(subChannelID);
			_sub["channelName"] = pls_masking_person_info(object["channelName"].toString());
			neloArr.append(_sub);
		}
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with response: %s", QJsonDocument(neloArr).toJson().constData());
		finishedCall(infos);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		PLSPlatformApiResult result = getApiResult(code, error, data);
		QVariantMap info = srcInfo;

		switch (result) {
		case PLSPlatformApiResult::PAR_NETWORK_ERROR:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::NetWorkNoStable;
			break;
		case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Expired;
			break;
		case PLSPlatformApiResult::VLIVE_API_ERROR_NO_PROFILE:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("Live.Check.LiveInfo.channel.no.permission");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
			break;
		case PLSPlatformApiResult::PAR_API_ERROR_CHANNEL_NO_PERMISSON:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("Live.Check.LiveInfo.channel.no.avaiable.channel.705");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
			break;
		case PLSPlatformApiResult::PAR_SERVER_ERROR:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("Live.Check.message.mutilive.failed.normal");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
			break;
		case PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorString] = QTStr("Live.Check.LiveInfo.account.forbidden");
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::SpecializedError;
			break;
		default:
			info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::UnknownError;
			break;
		}

		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIVLive::vliveRequestUsersInfoAndChannel(this, _onSucceed, _onFail);
}

void PLSPlatformVLive::requestScheduleList(function<void(bool)> onNext, QObject *widget, bool isNeedShowErrAlert, const QString &checkID)
{
	static qint64 m_iContext = 0;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (isNeedShowErrAlert && context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		auto doc = QJsonDocument::fromJson(data);

		if (!doc.isArray()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not array or empty");
			if (isNeedShowErrAlert) {
				setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			}
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.array();

		PLSVLiveLiveinfoData selectData = getTempSelectData();
		bool isContainSelectData = false;
		bool isContainCheckedItem = false;
		m_vecSchedules.clear();
		QJsonArray neloArr;
		for (int i = 0; i < root.size(); i++) {
			auto data = root[i].toObject();
			PLSVLiveLiveinfoData scheduleDta = PLSVLiveLiveinfoData(data);

			if (isNeedShowErrAlert && !getTempProfileData().memberId.isEmpty()) {
				if (scheduleDta.profile.memberId != getTempProfileData().memberId) {
					continue;
				}
			}

			if (scheduleDta.isNewLive) {
				long ago10Min = PLSDateFormate::getNowTimeStamp() - 10 * 60;
				long early3Day = PLSDateFormate::getNowTimeStamp() + 3 * 60 * 60 * 24;
				if (scheduleDta.startTimeStamp < ago10Min || early3Day < scheduleDta.startTimeStamp) {
					continue;
				}
			}
			if (scheduleDta._id == selectData._id) {
				isContainSelectData = true;
			}
			if (scheduleDta._id == checkID) {
				isContainCheckedItem = true;
			}
			m_vecSchedules.push_back(scheduleDta);

			QJsonObject _sub;
			_sub["videoSeq"] = pls_masking_user_id_info(scheduleDta.startVideoSeq);
			_sub["title"] = pls_masking_person_info(scheduleDta.title);
			_sub["isNewLive"] = BOOL2STR(scheduleDta.isNewLive);
			_sub["onAirStartAt"] = QString::number(scheduleDta.startTimeStamp);
			_sub["willEndAt"] = QString::number(scheduleDta.endTimeStamp);
			neloArr.append(_sub);
		}

		if (checkID.isEmpty()) {
			isContainCheckedItem = true;
		}
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with real itemCount:%i, response:%s", root.count(), QJsonDocument(neloArr).toJson().constData());

		if (isContainSelectData == false && !selectData.isNormalLive) {
			//if the remote not found selected schedule, so add it.
			m_vecSchedules.push_back(selectData);
		}

		if (!isContainCheckedItem) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_LIVE_BROADCAST_NOT_FOUND);
		}
		if (nullptr != onNext) {
			onNext(!isContainCheckedItem ? false : true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (isNeedShowErrAlert && context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		if (isNeedShowErrAlert) {
			setupApiFailedWithCode(getApiResult(code, error, data));
		}

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIVLive::vliveRequestScheduleList(this, widget, _onSucceed, _onFail, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformVLive::requestUpdateScheduleList(function<void(bool isSucceed, bool isDelete)> onNext, QObject *widget)
{
	static qint64 m_iContext = 0;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		bool isDeleted = true;

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);

		if (!doc.isArray()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not array or empty");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			if (nullptr != onNext) {
				onNext(true, isDeleted);
			}
			return;
		}
		auto root = doc.array();

		PLSVLiveLiveinfoData selectData = getTempSelectData();

		m_vecSchedules.clear();

		for (int i = 0; i < root.size(); i++) {
			auto data = root[i].toObject();
			PLSVLiveLiveinfoData scheduleDta = PLSVLiveLiveinfoData(data);

			if (!getTempProfileData().memberId.isEmpty()) {
				if (scheduleDta.profile.memberId != getTempProfileData().memberId) {
					continue;
				}
			}

			if (scheduleDta.isNewLive) {
				long ago10Min = PLSDateFormate::getNowTimeStamp() - 10 * 60;
				long early3Day = PLSDateFormate::getNowTimeStamp() + 3 * 60 * 60 * 24;
				if (scheduleDta.startTimeStamp < ago10Min || early3Day < scheduleDta.startTimeStamp) {
					continue;
				}
			}
			if (scheduleDta._id == selectData._id) {
				isDeleted = false;
			}
			m_vecSchedules.push_back(scheduleDta);
		}

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with itemCount:%i", root.count());
		if (isDeleted) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_Live_Invalid);
		}
		if (nullptr != onNext) {
			onNext(true, isDeleted);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		auto type = getApiResult(code, error, data);
		if (type != PLSPlatformApiResult::PAR_TOKEN_EXPIRED && type != PLSPlatformApiResult::PAR_NETWORK_ERROR) {
			type = PLSPlatformApiResult::PAR_API_ERROR_SCHEDULE_API_FAILED;
		}
		setupApiFailedWithCode(type);

		if (nullptr != onNext) {
			onNext(false, false);
		}
	};
	PLSAPIVLive::vliveRequestScheduleList(this, widget, _onSucceed, _onFail, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformVLive::downloadThumImage(function<void()> onNext, QWidget *reciver)
{
	static qint64 m_iContext = 0;
	auto _callBack = [=](bool ok, const QString &imagePath, void *const context) {
		if (context != reinterpret_cast<void *>(m_iContext)) {
			if (QFile::exists(imagePath)) {
				QFile::remove(imagePath);
			}
			return;
		}
		if (ok) {
			setThumPixmap(QPixmap(imagePath));
			onNext();
		}
		if (QFile::exists(imagePath)) {
			QFile::remove(imagePath);
		}
	};
	PLSAPIVLive::downloadImageAsync(reciver, getTempSelectData().thumRemoteUrl, _callBack, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformVLive::requestBoardList(function<void(bool)> onNext, QObject *widget)
{

	auto _isEmptyGroup = [=](const QJsonArray &boards) {
		for (int j = 0; j < boards.size(); j++) {
			auto board = boards[j].toObject();
			if (board["boardType"].toString().compare(boardTypeStar, Qt::CaseInsensitive) == 0) {
				return false;
			}
		}
		return true;
	};

	static qint64 m_iContext = 0;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		//PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isArray()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.array();
		m_vecBoards.clear();
		for (int i = 0; i < root.size(); i++) {
			auto group = root[i].toObject();
			auto boards = group["boards"].toArray();
			if (_isEmptyGroup(boards)) {
				continue;
			}

			PLSVLiveBoardData boardData = PLSVLiveBoardData(group, "", true);
			m_vecBoards.push_back(boardData);

			vector<PLSVLiveBoardData> boardDatas;
			for (int j = 0; j < boards.size(); j++) {
				auto board = boards[j].toObject();
				PLSVLiveBoardData boardData = PLSVLiveBoardData(board, "", false);
				if (boardData.boardType.compare(boardTypeStar, Qt::CaseInsensitive) == 0) {
					m_vecBoards.push_back(boardData);
				}
			}
		}
		QJsonArray neloArr;
		for (auto &data : m_vecBoards) {
			QJsonObject _sub;
			_sub["groupTitle"] = pls_masking_person_info(data.groupTitle);
			_sub["title"] = pls_masking_person_info(data.title);
			_sub["boardId"] = pls_masking_user_id_info(QString::number(data.boardId));
			neloArr.append(_sub);
		}
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with response: %s", QJsonDocument(neloArr).toJson().constData());

		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIVLive::vliveRequestBoardList(this, widget, _onSucceed, _onFail, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformVLive::requestBoardDetail(function<void(bool)> onNext, QObject *widget)
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

		QString allowed = doc.object()["readAllowedLabel"].toString();
		getTempSelectDataRef().board.readAllowedLabel = allowed;
		m_bTempSelectBoard.readAllowedLabel = allowed;
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIVLive::vliveRequestBoardDetail(this, widget, _onSucceed, _onFail);
}

void PLSPlatformVLive::requestProfileList(function<void(bool)> onNext, QObject *widget)
{
	static qint64 m_iContext = 0;
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		//PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed");
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isArray()) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, doc is not object");
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		auto root = doc.array();
		m_vecProfiles.clear();
		QJsonArray neloArr;
		for (int i = 0; i < root.size(); i++) {
			auto item = root[i].toObject();
			PLSVLiveProfileData data = PLSVLiveProfileData(item);
			m_vecProfiles.push_back(data);
			QJsonObject _sub;
			_sub["memberId"] = pls_masking_user_id_info(data.memberId);
			_sub["nickname"] = pls_masking_name_info(data.nickname);
			neloArr.append(_sub);
		}
		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with response: %s", QJsonDocument(neloArr).toJson().constData());

		sort(m_vecProfiles.begin(), m_vecProfiles.end());
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (context != reinterpret_cast<void *>(m_iContext)) {
			return;
		}
		setupApiFailedWithCode(getApiResult(code, error, data));

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIVLive::vliveRequestProfileList(getChannelUUID(), widget, _onSucceed, _onFail, reinterpret_cast<void *>(++m_iContext));
}

void PLSPlatformVLive::requestUploadImage(const QPixmap &pixmap, function<void(bool, const QString &)> onNext)
{

	QString path = QString("%1\\%2.png").arg(getTmpCacheDir()).arg(QUuid::createUuid().toString());
	QFile file(path);
	file.open(QIODevice::WriteOnly);
	pixmap.save(&file, "PNG");

	PLS_INFO(MODULE_PlatformService, __FUNCTION__ " start");
	PLSAPIVLive::uploadImage(this, path, [=](PLSPlatformApiResult result, const QString &imageUrl) {
		if (QFile::exists(path)) {
			QFile::remove(path);
		}
		if (result != PLSPlatformApiResult::PAR_SUCCEED) {
			setupApiFailedWithCode(result);
		}
		if (nullptr != onNext) {
			onNext(result == PLSPlatformApiResult::PAR_SUCCEED, imageUrl);
		}
	});
}

void PLSPlatformVLive::requestStatisticsInfo()
{
	auto _finish = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)

		int responseCode = 0;

		auto doc = QJsonDocument::fromJson(data);
		bool isLiveEnd = false;
		QString errorAlert;

		if (doc.isObject()) {
			responseCode = doc["code"].toInt();
			isLiveEnd = doc.object()["status"].toString() == "ENDED" || doc.object()["status"].toString() == "CANCELED";
		}
		if (isLiveEnd || responseCode == 9100 || responseCode == 9102 || responseCode == 9115) {
			errorAlert = tr("LiveInfo.live.error.stoped.byRemote").arg(getNameForChannelType());
		}

		if (!errorAlert.isEmpty()) {
			if (isStopedByRemote) {
				return;
			}
			isStopedByRemote = true;
			PLS_INFO(MODULE_PlatformService, __FUNCTION__ " stop by remote errorCode, errorCode:%d, responseCode:%d", error, responseCode);

			if (LiveStatus::LiveStarted <= PLS_PLATFORM_API->getLiveStatus() && PLS_PLATFORM_API->getLiveStatus() < LiveStatus::LiveStoped) {
				PLS_LIVE_ABORT_INFO(MODULE_PlatformService, "live abort because live platform stop", "Finished because vlive stop by remote errorCode, errorCode:%d, responseCode:%d",
						    error, responseCode);
				PLSCHANNELS_API->toStopBroadcast();
			}
			PLSAlertView::warning(getAlertParent(), QTStr("Alert.Title"), errorAlert);
			return;
		}

		auto root = doc.object();
		auto likeCount = QString::number(root["likeCount"].toInt());
		auto watchCount = QString::number(root["playCount"].toInt());
		auto commentCount = QString::number(root["commentCount"].toInt());

		const auto &mSourceData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
		if (mSourceData.contains(ChannelData::g_viewers)) {
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_viewers, watchCount);
		}
		if (mSourceData.contains(ChannelData::g_likes)) {
			PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_likes, likeCount);
		}
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_comments, commentCount);
	};
	PLSAPIVLive::vliveRequestStatistics(this, _finish);
}

void PLSPlatformVLive::requestStartLive(function<void(bool)> onNext)
{
	auto _finish = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		auto doc = QJsonDocument::fromJson(data);
		PLSPlatformApiResult apiResult = getApiResult(code, error, data, true);

		QString pushUrl;
		if (apiResult == PLSPlatformApiResult::PAR_SUCCEED && doc.isObject()) {
			pushUrl = doc["publishUrl"].toString();
			if (pushUrl.isEmpty()) {
				PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed, pushUrl is empty");
				apiResult = PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other;
			}
		}

		if (apiResult != PLSPlatformApiResult::PAR_SUCCEED) {
			PLS_ERROR(MODULE_PlatformService, __FUNCTION__ " failed");
			setupApiFailedWithCode(apiResult);
			if (nullptr != onNext) {
				onNext(false);
				return;
			}
		}

		QString videoSeq;
		if (getIsRehearsal()) {
			videoSeq = QString::number(doc["rehearsalSeq"].toInt());
		} else {
			videoSeq = QString::number(doc["videoSeq"].toInt());
		}

		PLS_INFO(MODULE_PlatformService, __FUNCTION__ "succeed with videoSeq: %s", pls_masking_person_info(videoSeq).toUtf8().constData());

		m_selectData.startVideoSeq = videoSeq;

		QStringList list = pushUrl.split("/");
		QString streamKey = list.last();
		list.removeLast();
		QString streamUrl = list.join("/");

		m_selectData.streamKey = streamKey;
		m_selectData.streamUrl = streamUrl;

		setStreamKey(m_selectData.streamKey.toStdString());
		setStreamServer(m_selectData.streamUrl.toStdString());
		setIsSubChannelStartApiCall(true);
		if (getIsRehearsal() || (!m_selectData.isNormalLive && m_selectData.isNewLive)) {
			if (nullptr != onNext) {
				onNext(true);
			}
			return;
		}
		requestStartLiveToPostBoard(onNext);
	};

	PLSAPIVLive::vliveRequestStartLive(getChannelUUID(), this, _finish);
}

void PLSPlatformVLive::requestStartLiveToPostBoard(function<void(bool)> onNext)
{
	auto _finish = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)

		if (code == 201 && error == QNetworkReply::NoError) {
			if (nullptr != onNext) {
				onNext(true);
			}
			return;
		}
		PLSPlatformApiResult apiResult = getApiResult(code, error, data, true);

		if (getIsSubChannelStartApiCall()) {
			requestStopLive(nullptr);
		}
		setupApiFailedWithCode(apiResult);
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIVLive::vliveRequestStartLiveToPostBoard(getChannelUUID(), this, _finish);
}

void PLSPlatformVLive::requestStopLive(function<void(void)> onNext)
{
	auto _onSucceed = [=](QNetworkReply *networkReplay, int code, QByteArray data, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		if (nullptr != onNext) {
			onNext();
		}
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context) {
		Q_UNUSED(networkReplay)
		Q_UNUSED(context)
		Q_UNUSED(code)
		Q_UNUSED(error)
		PLS_ERROR(MODULE_PlatformService, __FUNCTION__ "failed");
		if (nullptr != onNext) {
			onNext();
		}
	};
	setIsSubChannelStartApiCall(false);
	PLSAPIVLive::vliveRequestStopLive(this, this, _onSucceed, _onFail);
}

PLSPlatformApiResult PLSPlatformVLive::getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, bool isStartApi)
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	}

	if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		result = PLSPlatformApiResult::PAR_API_FAILED;

		auto doc = QJsonDocument::fromJson(data);
		QString responseCode = QString();
		if (doc.isObject()) {
			responseCode = doc["error_code"].toString().isEmpty() ? doc["errorCode"].toString() : doc["error_code"].toString();
		}

		switch (code) {
		case 401:
			if (responseCode == "common_704") {
				result = PLSPlatformApiResult::VLIVE_API_ERROR_NO_PROFILE;
				break;
			}
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			break;
		case 403: {
			//https://wiki.navercorp.com/pages/viewpage.action?pageId=520278326
			if (responseCode == "common_704") {
				result = PLSPlatformApiResult::VLIVE_API_ERROR_NO_PROFILE;
			} else if (responseCode == "common_705") {
				result = PLSPlatformApiResult::PAR_API_ERROR_CHANNEL_NO_PERMISSON;
			}
		} break;
		case 500:
			if (responseCode == "VIDEO_500") {
				result = PLSPlatformApiResult::PAR_SERVER_ERROR;
				break;
			}
			break;
		default:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		}

		if (code != 401) {
			if (responseCode == "common_403") {
				result = PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN;
			} else if (responseCode == "common_500") {
				result = PLSPlatformApiResult::PAR_SERVER_ERROR;
			}
		}
	}
	if (isStartApi && result == PLSPlatformApiResult::PAR_API_FAILED) {
		result = PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other;
	}
	return result;
}

void PLSPlatformVLive::setupApiFailedWithCode(PLSPlatformApiResult result)
{
	auto alertParent = getAlertParent();

	switch (result) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED: {
		emit toShowLoading(true);
		auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
		auto channelName = info.value(ChannelData::g_platformName).toString();
		PLSAlertView::Button button = PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Expired").arg(channelName));
		emit closeDialogByExpired();
		if (button == PLSAlertView::Button::Ok) {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		emit toShowLoading(false);
	} break;
	case PLSPlatformApiResult::PAR_API_ERROR_Upload_Image:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.set_photo_error"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.start.other").arg(getChannelName()));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_LIVE_BROADCAST_NOT_FOUND:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Broadcast.Error.Delete"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_CUSTOM:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), m_showCustomMsg);
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_CHANNEL_NO_PERMISSON:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.channel.no.avaiable.channel.705"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_CONNECT_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.connection.error.video.500").arg(getChannelName()));
		break;
	case PLSPlatformApiResult::VLIVE_API_ERROR_NO_PROFILE:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.channel.no.permission"));
		if (!PLS_PLATFORM_API->isLiving()) {
			reInitLiveInfo(true, false);
			liveInfoisShowing();
			emit profileIsInvalid();
		}
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_FORBIDDEN:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.account.forbidden"));
		break;
	case PLSPlatformApiResult::PAR_SERVER_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.message.mutilive.failed.normal"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_Live_Invalid:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("broadcast.invalid.schedule"));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_SCHEDULE_API_FAILED:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("common.message.error"));
		break;
	default:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	}
}

void PLSPlatformVLive::onActive()
{
	PLSPlatformBase::onActive();
	QMetaObject::invokeMethod(
		this, [=]() { this->onShowScheLiveNoticeIfNeeded(getChannelUUID()); }, Qt::QueuedConnection);
}

void PLSPlatformVLive::onLiveEnded()
{
	if (m_statusTimer && m_statusTimer->isActive()) {
		m_statusTimer->stop();
	}

	if (PLS_PLATFORM_API->isPrismLive() || !isActive()) {
		liveEndedCallback();
		return;
	}

	auto _onNext = [=]() {
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, QString());
		PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrlTemp, QString());
		liveEndedCallback();
	};

	requestStopLive(nullptr);
	_onNext();
}

void PLSPlatformVLive::onAlLiveStarted(bool value)
{
	isStopedByRemote = false;

	if (!value || PLS_PLATFORM_API->isPrismLive() || !isActive()) {
		return;
	}
	__super::setIsScheduleLive(!getSelectData().isNormalLive);
	if (getIsRehearsal()) {
		return;
	}

	if (m_statusTimer && !m_statusTimer->isActive()) {
		m_statusTimer->start(5000);
	}
}

QJsonObject PLSPlatformVLive::getWebChatParams()
{
	assert(getSelectData().startVideoSeq.size() > 0);

	const static QString s_strTicket = "globalv";
	const static QString s_templateId = "default";
	const static QString s_pool = "cbox4";
	const static QString s_strNEOIDConsumerKey = "EvbbxIFhoa3Z2SkNwe0K";

	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	QString allToken = this->getChannelToken();
	QJsonObject platform;
	platform["name"] = "VLIVE";
	platform["objectId"] = getSelectData().startVideoSeq;
	platform["gcc"] = PLSVliveStatic::instance()->gcc;
	platform["lang"] = PLSVliveStatic::instance()->lang;
	platform["ticket"] = s_strTicket;
	platform["templateId"] = s_templateId;
	platform["objectUrl"] = g_plsVliveObjectUrl;
	auto snsCdList = allToken.split(";snsCd=");
	if (snsCdList.size() > 1) {
		platform["snsCode"] = snsCdList[1];
	}
	platform["pool"] = s_pool;
	platform["userName"] = getSelectData().profile.nickname;
	platform["userProfileImage"] = info.value(ChannelData::g_profileThumbnailUrl, "").toString();
	platform["isVliveFanship"] = !PLSAPIVLive::isVliveFanship(); //when show chat fanship is true, not show fanshiop icon is false.
	platform["version"] = 1;
	platform["GLOBAL_URI"] = pls_get_gpop_connection().ssl;
	platform["HMAC_SALT"] = PLS_VLIVE_HMAC_KEY;
	platform["consumerKey"] = s_strNEOIDConsumerKey;
	platform["isRehearsal"] = getIsRehearsal();
	platform["profileId"] = getSelectData().profile.memberId;
	QString neoAll = allToken.split(";snsToken=").first();
	auto neoStringList = neoAll.split("NEO_SES=");
	if (neoStringList.size() > 1) {
		platform["Authorization"] = QString("Bearer %1").arg(neoStringList[1]).remove("\"");
	}
	return platform;
}

PLSVLiveLiveinfoData &PLSPlatformVLive::getTempSelectDataRef()
{
	if (!m_bTempSchedule) {
		return m_tempNoramlData;
	}
	for (auto &scheuleData : m_vecSchedules) {
		if (0 == getTempSelectID().compare(scheuleData._id)) {
			return scheuleData;
			break;
		}
	}
	return m_tempNoramlData;
}

void PLSPlatformVLive::showScheLiveNotice(const PLSVLiveLiveinfoData &liveInfo)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	PLSScheLiveNotice scheLiveNotice(this, liveInfo.title, liveInfo.startTimeUTC, App()->getMainView());
	if (scheLiveNotice.exec() != PLSScheLiveNotice::Accepted) {
		return;
	}
}

PLSVLiveLiveinfoData::PLSVLiveLiveinfoData() {}

PLSVLiveLiveinfoData::PLSVLiveLiveinfoData(const QJsonObject &data)
{
	this->_id = QString::number(data["videoSeq"].toInt());
	this->title = data["title"].toString();
	this->channelSeq = data["post"].toObject()["channel"].toObject()["channelCode"].toString();
	this->thumRemoteUrl = data["thumb"].toString();
	this->isNewLive = !data["ended"].toBool();
	this->isNormalLive = false;

	long long startAt = data.value(QString("onAirStartAt")).toVariant().toLongLong();
	long long endAt = data.value(QString("willEndAt")).toVariant().toLongLong();
	if (startAt > 0) {
		startAt /= 1000;
	}
	if (endAt > 0) {
		endAt /= 1000;
	}

	this->startTimeStamp = startAt;
	this->endTimeStamp = endAt;

	this->status = data["exposeStatus"].toString();

	this->startTimeShort = PLSDateFormate::timeStampToShortString(this->startTimeStamp);
	this->startTimeUTC = PLSDateFormate::timeStampToUTCString(this->startTimeStamp);
	this->profile = PLSVLiveProfileData(data["post"].toObject()["author"].toObject());
	this->board = PLSVLiveBoardData(data["post"].toObject()["board"].toObject());
}

QString PLSPlatformVLive::getServiceLiveLink()
{
	return getShareUrl();
}

void PLSPlatformVLive::changeChannelDataWhenProfileSelect(QVariantMap &info, const PLSVLiveProfileData &profileData, const QObject *receiver)
{
	QString showUrl = info[ChannelData::g_userIconThumbnailUrl].toString();
	if (!profileData.profileImageUrl.isEmpty()) {
		showUrl = profileData.profileImageUrl;
	}

	if (!showUrl.isEmpty()) {
		if (PLSImageStatic::instance()->profileUrlMap.contains(showUrl) && !PLSImageStatic::instance()->profileUrlMap[showUrl].isEmpty()) {
			info[ChannelData::g_userIconCachePath] = PLSImageStatic::instance()->profileUrlMap[showUrl];
		} else if (auto icon = PLSAPIVLive::downloadImageSync(receiver, showUrl, info[ChannelData::g_platformName].toString()); icon.first) {
			if (!icon.second.isEmpty()) {
				info[ChannelData::g_userIconCachePath] = icon.second;
				PLSImageStatic::instance()->profileUrlMap[showUrl] = icon.second;
			}
		}
	}

	if (profileData.memberId.isEmpty()) {
		info[ChannelData::g_displayLine2] = QString();
		info[ChannelData::g_displayLine1] = info[ChannelData::g_vliveNormalChannelName].toString();
	} else {
		info[ChannelData::g_displayLine2] = info[ChannelData::g_vliveNormalChannelName].toString();
		info[ChannelData::g_displayLine1] = profileData.nickname;
	}
}

PLSVliveStatic *PLSVliveStatic::instance()
{
	static PLSVliveStatic _instance;
	return &_instance;
}

QString PLSVliveStatic::convertToFullName(const QString &shortName)
{
	auto fullName = countries.value(shortName, shortName);
	return fullName;
}

PLSVLiveBoardData::PLSVLiveBoardData(const QJsonObject &data, const QString &, bool isGroup)
{
	this->isGroup = isGroup;

	if (isGroup) {
		this->groupTitle = this->title = data["groupTitle"].toString().replace("\n", " ");
		return;
	}

	this->boardId = data["boardId"].toInt();
	this->title = data["title"].toString().replace("\n", " ");
	this->boardType = data["boardType"].toString();
	this->payRequired = data["payRequired"].toBool();
	this->expose = data["expose"].toBool();
	this->channelCode = data["channelCode"].toString();
	this->includedCountries = data["includedCountries"].toArray();
	this->excludedCountries = data["excludedCountries"].toArray();
	this->readAllowedLabel = data["readAllowedLabel"].toString();
}

PLSVLiveProfileData::PLSVLiveProfileData(const QJsonObject &data)
{
	this->memberId = data["memberId"].toString();
	this->channelCode = data["channelCode"].toString();
	this->joined = data["joined"].toBool();
	this->nickname = data["nickname"].toString();
	this->profileImageUrl = data["profileImageUrl"].toString();
	this->memberType = data["memberType"].toString();

	this->officialProfileType = data["officialProfileType"].toString();
	this->hasMultiProfiles = data["hasMultiProfiles"].toBool();
	this->officialName = data["officialName"].toString();
}

QVariantMap PLSVLiveProfileData::getMapData() const
{
	QVariantMap mapData;
	mapData["memberId"] = this->memberId;
	mapData["channelCode"] = this->channelCode;
	mapData["joined"] = this->joined;
	mapData["nickname"] = this->nickname;
	mapData["profileImageUrl"] = this->profileImageUrl;
	mapData["officialProfileType"] = this->officialProfileType;
	mapData["hasMultiProfiles"] = this->hasMultiProfiles;
	mapData["officialName"] = this->officialName;
	return mapData;
}

bool PLSVLiveProfileData::operator<(const PLSVLiveProfileData &right) const
{
	QString leftString = this->nickname;
	QString righrString = right.nickname;
	if (leftString == righrString) {
		leftString = this->memberId;
		righrString = right.memberId;
	}

	return QString::localeAwareCompare(leftString, righrString) < 0;
}
