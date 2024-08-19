#include "PLSPlatformNCB2B.h"
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
#include "common/PLSAPICommon.h"
#include "ChannelCommonFunctions.h"
#include "PLSAPINCB2B.h"
#include "PLSChannelDataAPI.h"
#include "PLSDateFormate.h"
#include "PLSScheLiveNotice.h"
#include "PLSAlertView.h"

#include "frontend-api.h"
#include "log/log.h"
#include "PLSMainView.hpp"

#include "pls-net-url.hpp"
#include "window-basic-main.hpp"
#include "PLSResCommonFuns.h"
#include "login-user-info.hpp"
#include "PLSNCB2BError.h"

const QString s_default_scope_value("PUBLIC");

using namespace std;

const PLSAPICommon::privacyVec &PLSPlatformNCB2B::getPrivayList()
{
	static PLSAPICommon::privacyVec s_privayList = {
		{s_default_scope_value, QObject::tr("youtube.privacy.public")}, //
		{QString("LIMITED"), QObject::tr("b2b.privacy.unlisted")},      //
		{QString("PRIVATE"), QObject::tr("youtube.privacy.private")}    //
	};
	return s_privayList;
}

PLSPlatformNCB2B::PLSPlatformNCB2B()
{
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

		if (dataType == ChannelData::ChannelType && platformName == NCB2B) {
			reInitLiveInfo();
		}
	});
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::channelRefreshEnd, this,
		[this](const QString &platformName) {
			if (platformName == NCB2B) {
				reInitLiveInfo();
			}
		},
		Qt::QueuedConnection);

	connect(
		PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this,
		[this](bool, bool apiStarted) {
			if (!apiStarted) {
				return;
			}

			auto uuids = PLS_PLATFORM_API->getUuidOnStarted();
			if (std::find(uuids.cbegin(), uuids.cend(), getChannelUUID()) != uuids.cend()) {
				PLS_INFO(MODULE_PLATFORM_NCB2B, "NCB2B get liveEnded signals to reinit liveinfo data");
				reInitLiveInfo();
			}
		},
		Qt::DirectConnection);
}

PLSServiceType PLSPlatformNCB2B::getServiceType() const
{
	return PLSServiceType::ST_NCB2B;
}

void PLSPlatformNCB2B::saveSettings(const function<void(bool)> &onNext, const PLSNCB2BLiveinfoData &uiData, const QObject *receiver)
{
	PLS_INFO(MODULE_PLATFORM_NCB2B, __FUNCTION__);
	auto _onUpdateNext = [this, onNext](bool isSucceed) {
		if (isSucceed) {
			setSelectData(getTempSelectData());
		}
		onNext(isSucceed);
	};

	if (PLS_PLATFORM_API->isPrepareLive() && uiData.isNormalLive) {
		requestCreateLive(receiver, uiData, _onUpdateNext);
		return;
	}

	bool isNeedUpdate = uiData.isNeedUpdate(getTempSelectData());
	if (isNeedUpdate && !uiData._id.isEmpty()) {
		updateLiveinfo(receiver, uiData, _onUpdateNext);
		return;
	}
	getTempSelectDataRef() = uiData;
	_onUpdateNext(true);
}

bool PLSPlatformNCB2B::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}

QString PLSPlatformNCB2B::getShareUrl(bool isEnc) const
{
	QString url = m_selectData.liveLink;
	if (url.isEmpty()) {
		const QString &uuid = getChannelUUID();
		const auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
		if (!info.isEmpty()) {
			url = info.value(ChannelData::g_shareUrl, "").toString();
		}
	}
	return isEnc ? pls_masking_person_info(url) : url;
}

QString PLSPlatformNCB2B::getShareUrl()
{
	return getShareUrl(false);
}

QString PLSPlatformNCB2B::getShareUrlEnc()
{
	return getShareUrl(true);
}

QString PLSPlatformNCB2B::getChannelToken() const
{
	return PLSLoginUserInfo::getInstance()->getNCPPlatformToken();
}

void PLSPlatformNCB2B::liveInfoisShowing()
{
	if (getSelectData().isNormalLive && getSelectData().title.isEmpty()) {
		m_noramlData = PLSNCB2BLiveinfoData(getChannelUUID());
	}
	setTempSelectID(getSelectData()._id);
	m_tempNoramlData = m_noramlData;
}

void PLSPlatformNCB2B::reInitLiveInfo()
{
	m_bTempSelectID = "";
	m_noramlData = PLSNCB2BLiveinfoData(getChannelUUID());
	setSelectData(m_noramlData);
	m_tempNoramlData = m_noramlData;
}

void PLSPlatformNCB2B::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(MODULE_PLATFORM_NCB2B, "onPrepareLive show liveinfo");
	value = pls_exec_live_Info_bcb2b(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_NCB2B, "onPrepareLive liveinfo closed with:%d", value);

	PLSPlatformBase::setStreamKey(m_selectData.streamKey.toStdString());
	PLSPlatformBase::setStreamServer(m_selectData.streamUrl.toStdString());
	PLSPlatformBase::setTitle(m_selectData.title.toStdString());
	prepareLiveCallback(value);
}

void PLSPlatformNCB2B::requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const
{
	auto _onSucceed = [this, srcInfo, finishedCall](QByteArray data) {
		PLS_INFO(MODULE_PLATFORM_NCB2B, "requestChannelInfo succeed");
		dealRequestChannelInfoSucceed(srcInfo, data, finishedCall);
	};

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLSPlatformApiResult result = getApiResult(code, error, data);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, code:%1").arg(code);
		info[ChannelData::g_channelStatus] = result == PLSPlatformApiResult::PAR_TOKEN_EXPIRED ? ChannelData::ChannelStatus::Expired : ChannelData::ChannelStatus::Error;

		if (result == PLSPlatformApiResult::PAR_NETWORK_ERROR) {
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::NetWorkNoStable;
		} else if (result == PLSPlatformApiResult::PAR_TOKEN_EXPIRED) {
			info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::PlatformExpired;
		} else {
			int subCode = 0;
			QString errMsg;
			QString errException;
			PLSAPINCB2B::getErrCode(data, subCode, errMsg, errException);
			QString specialStr = PLSNCB2BError::getAlertString(subCode, PLSNCB2BError::ErrorModule::dashBord);

			if (specialStr.isEmpty()) {
				info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::UnknownError;
			} else {
				info[ChannelData::g_errorString] = specialStr;
				info[ChannelData::g_errorType] = ChannelData::NetWorkErrorType::NCB2BError;
			}
		}
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "requestChannelInfo failed with b2b2 callback: g_channelStatus:%i, g_errorType:%i, g_errorString:%s", info[ChannelData::g_channelStatus].toInt(),
			  info[ChannelData::g_errorType].toInt(), info[ChannelData::g_errorString].toString().toUtf8().constData());
		finishedCall(QList<QVariantMap>{info});
	};
	PLSAPINCB2B::requestChannelList(this, const_cast<PLSPlatformNCB2B *>(this), _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformNCB2B::dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall) const
{
	QJsonArray channelList;
	QString errMsg;
	QVariantMap info = srcInfo;
	if (!PLSAPICommon::getErrorCallBack(data, channelList, errMsg, "channels")) {
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "requestChannelInfo failed, %s", errMsg.toUtf8().constData());
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, %1").arg(errMsg);
		finishedCall(QList<QVariantMap>{info});
		return;
	}

	QList<QVariantMap> infos;
	for (int i = 0; i < channelList.size(); i++) {
		auto channel = channelList[i].toObject();
		auto channelID = channel["channelId"].toString();
		if (!channel["channelEnabled"].toBool(true)) {
			PLS_INFO(MODULE_PLATFORM_NCB2B, "requestChannelInfo channel not enable, id: %s", channelID.toUtf8().constData());
			continue;
		}

		QVariantMap subInfo;
		subInfo[ChannelData::g_shareUrl] = channel["channelLink"].toString();

		subInfo[ChannelData::g_channelName] = srcInfo[ChannelData::g_channelName].toString();
		subInfo[ChannelData::g_subChannelId] = channelID;
		auto channelUrl = channel["channelThumbnailUrl"].toString();
		subInfo[ChannelData::g_userProfileImg] = channelUrl;
		subInfo[ChannelData::g_nickName] = channel["channelName"].toString();

		subInfo[ChannelData::g_catogry] = s_default_scope_value;

		subInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;

		subInfo[ChannelData::g_viewers] = "0";
		subInfo[ChannelData::g_viewersPix] = ChannelData::g_defaultViewerIcon;

		subInfo[ChannelData::g_likes] = "0";
		subInfo[ChannelData::g_likesPix] = ChannelData::g_defaultLikeIcon;
		subInfo[ChannelData::g_userIconCachePath] = "";

		subInfo[ChannelData::g_comments] = "0";
		subInfo[ChannelData::g_showEndShare] = true;

		if (!channelUrl.isEmpty()) {
			auto path = PLSAPICommon::getMd5ImagePath(channelUrl);
			if (QFile(path).exists()) {
				subInfo[ChannelData::g_userIconCachePath] = path;
			}
		}
		infos.append(subInfo);
	}
	if (infos.isEmpty()) {
		errMsg = "all channel is disable";
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "requestChannelInfo failed, %s", errMsg.toUtf8().constData());
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, %1").arg(errMsg);
		finishedCall(QList<QVariantMap>{info});
		return;
	}
	finishedCall(infos);
	pls_async_call_mt(this, [this]() { PLSAPICommon::downloadChannelImageAsync(NCB2B); });
}

void PLSPlatformNCB2B::updateLiveinfo(const QObject *reciever, const PLSNCB2BLiveinfoData &uiData, const function<void(bool)> &onNext)
{
	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		setupApiFailedWithCode(getApiResult(code, error, data, PLSAPICommon::PLSApiType::Update), data);
		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPINCB2B::requestUpdateLive(
		reciever, uiData, this,
		[this, onNext](QByteArray data) {
			bool isOk = dealCurrentLiveSucceed(data, "updateLiveinfo");
			if (!isOk) {
				setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_UPDATE, {});
			}
			if (onNext) {
				onNext(isOk);
			}
		},
		_onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformNCB2B::requestScheduleList(const std::function<void(bool)> &onNext, const QObject *widget)
{
	auto tmpContext = (++m_iContext);
	auto _onSucceed = [this, onNext, widget, tmpContext](QByteArray data) {
		if (tmpContext != m_iContext) {
			return;
		}
		dealScheduleListSucceed(data, onNext, widget);
	};

	auto _onFail = [this, onNext, tmpContext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		if (tmpContext != m_iContext) {
			return;
		}
		setupApiFailedWithCode(getApiResult(code, error, data), data);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPINCB2B::requestScheduleList(widget, subChannelID(), this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformNCB2B::dealScheduleListSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *widget)
{

	QJsonArray items;
	QString errMsg;
	if (!PLSAPICommon::getErrorCallBack(data, items, errMsg, "lives")) {
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "dealScheduleListSucceed failed, %s", errMsg.toUtf8().constData());
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	PLSNCB2BLiveinfoData selectData = getTempSelectData();
	bool isContainSelectData = false;

	m_vecSchedules.clear();
	PLS_INFO(MODULE_PLATFORM_NCB2B, "dealScheduleListSucceed succeed with itemCount:%i", items.count());

	for (int i = 0; i < items.size(); i++) {
		auto dataItem = items[i].toObject();
		auto scheduleDta = PLSNCB2BLiveinfoData(dataItem);
		m_vecSchedules.push_back(scheduleDta);

		if (scheduleDta._id == selectData._id) {
			isContainSelectData = true;
		}
	}

	if (isContainSelectData == false && !selectData.isNormalLive) {
		//if the remote not found selected schedule, so add it.
		m_vecSchedules.push_back(selectData);
	}

	if (nullptr != onNext) {
		onNext(true);
	}
}

void PLSPlatformNCB2B::requestCreateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, const function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		bool isOk = dealCurrentLiveSucceed(data, "requestCreateLive");
		if (!isOk) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other, {});
		}
		if (onNext) {
			onNext(isOk);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		setupApiFailedWithCode(getApiResult(code, error, data, PLSAPICommon::PLSApiType::StartLive), data);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPINCB2B::requestCreateLive(receiver, data, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

bool PLSPlatformNCB2B::dealCurrentLiveSucceed(const QByteArray &data, const QString &preLog)
{
	QJsonObject item;
	QString errMsg;
	if (!PLSAPICommon::getErrorCallBack(data, item, errMsg, "live")) {
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "%s failed, %s", preLog.toUtf8().constData(), errMsg.toUtf8().constData());
		return false;
	}
	PLSNCB2BLiveinfoData &liveData = getTempSelectDataRef();
	auto scheduleDta = PLSNCB2BLiveinfoData(item);
	scheduleDta.isNormalLive = liveData.isNormalLive;
	liveData = scheduleDta;
	return true;
}

void PLSPlatformNCB2B::requestCurrentSelectData(const function<void(bool)> &onNext, const QWidget *widget)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		bool isOk = dealCurrentLiveSucceed(data, "requestCurrentSelectData");
		if (!isOk) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED, {});
		}
		if (onNext) {
			onNext(isOk);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		setupApiFailedWithCode(getApiResult(code, error, data), data);

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPINCB2B::requestGetLiveInfo(widget, getTempSelectID(), this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

PLSPlatformApiResult PLSPlatformNCB2B::getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, PLSAPICommon::PLSApiType apiType) const
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	}
	result = PLSPlatformApiResult::PAR_API_FAILED;
	if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		if (code == 401) {
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
		}
	}

	if (result == PLSPlatformApiResult::PAR_API_FAILED) {
		switch (apiType) {
		case PLSAPICommon::PLSApiType::Normal:
			break;
		case PLSAPICommon::PLSApiType::StartLive:
			result = PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other;
			break;
		case PLSAPICommon::PLSApiType::Update:
			result = PLSPlatformApiResult::PAR_API_ERROR_UPDATE;
			break;
		default:
			break;
		}
	}

	return result;
}

void PLSPlatformNCB2B::setupApiFailedWithCode(PLSPlatformApiResult result, const QByteArray &errData)
{
	setIsShownAlert(true);
	auto alertParent = getAlertParent();
	PLSAlertView::Button button;

	if (m_startFailedStr.isEmpty() && !m_lastRequestAPI.isEmpty()) {
		m_startFailedStr = QString("%1-common-code:%2").arg(m_lastRequestAPI).arg((int)result);
	}

	if (!errData.isEmpty()) {
		int subCode = 0;
		QString errMsg;
		QString errException;
		PLSAPINCB2B::getErrCode(errData, subCode, errMsg, errException);
		if (PLSNCB2BError::ncb2bErrorHandler(subCode, PLSNCB2BError::Living)) {
			return;
		}
	}

	switch (result) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED: {
		emit toShowLoading(true);
		auto expiredStr = PLS_PLATFORM_API->isPrepareLive() ? QTStr("Ncb2b.Token.Expired") : QTStr("Live.Check.LiveInfo.Refresh.Expired"); //#5053 ux
		button = pls_alert_error_message(alertParent, QTStr("Alert.Title"), expiredStr.arg(getChannelName()));
		emit closeDialogByExpired();
		if (button == PLSAlertView::Button::Ok) {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		emit toShowLoading(false);
	} break;
	case PLSPlatformApiResult::PAR_API_ERROR_UPDATE:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Update.Error.Failed").arg(getChannelName()));
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Create.Failed.SingleChannel").arg(getChannelName()));
		break;
	default:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	}
}

void PLSPlatformNCB2B::onLiveEnded()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	liveEndedCallback();
}

void PLSPlatformNCB2B::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}
}

QJsonObject PLSPlatformNCB2B::getLiveStartParams()
{
	QJsonObject platform;
	platform["cookie"] = getChannelCookie();
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["channelId"] = channelData.value(ChannelData::g_subChannelId, "").toString();
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	platform["serviceId"] = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	platform["accessToken"] = PLSLoginUserInfo::getInstance()->getNCPPlatformToken();
	platform["ncpLiveId"] = m_selectData._id;
	return platform;
}

QJsonObject PLSPlatformNCB2B::getMqttChatParams()
{
	QJsonObject platform;
	platform.insert("cookie", getChannelCookie());
	platform["serviceId"] = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	platform["accessToken"] = PLSLoginUserInfo::getInstance()->getNCPPlatformToken();
	return platform;
}

QJsonObject PLSPlatformNCB2B::getWebChatParams()
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());

	QJsonObject obj;
	obj["sub_name"] = info.value(ChannelData::g_channelName).toString();
	obj["name"] = NCP_LIVE_START_NAME;
	obj["userId"] = info.value(ChannelData::g_subChannelId, "").toString();
	obj["access_token"] = QString(PLSLoginUserInfo::getInstance()->getPrismCookie());
	obj["service_id"] = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
	obj["simulcast_seq"] = getChannelLiveSeq();
	obj["global_uri"] = PRISM_SSL;
	obj["hmac_salt"] = PLS_PC_HMAC_KEY;

	return obj;
}
QString PLSPlatformNCB2B::getServiceLiveLink()
{
	return getShareUrl();
}

const PLSNCB2BLiveinfoData &PLSPlatformNCB2B::getSelectData() const
{
	return m_selectData;
}

void PLSPlatformNCB2B::setSelectData(const PLSNCB2BLiveinfoData &data)
{
	auto oldID = m_selectData._id;
	m_selectData = data;
	m_bTempSelectID = data._id;

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
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_broadcastID, m_selectData._id);
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(false));

	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_displayLine2, data.scope);
	if (!PLS_PLATFORM_API->isGoLive()) {
		PLSCHANNELS_API->channelModified(uuid);
	} else if (PLS_PLATFORM_API->isGoLive() && PLS_PLATFORM_API->isPrepareLive()) {
		PLSCHANNELS_API->channelModified(uuid);
	}

	if (oldID != m_selectData._id) {
		emit selectIDChanged();
	}
}

void PLSPlatformNCB2B::updateScheduleList()
{
	auto next = [this](bool) { emit scheduleListUpdateFinished(); };
	this->requestScheduleListByGuidePage(next, this);
}

void PLSPlatformNCB2B::convertScheduleListToMapList()
{
	std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
	auto uuid = this->getChannelUUID();
	auto tmpSrc = m_vecGuideSchedules;
	QVariantList tmpResult;
	auto channelName = getChannelName();
	auto convertData = [uuid, channelName](const PLSNCB2BLiveinfoData &data) {
		QVariantMap mapData;
		mapData.insert(ChannelData::g_timeStamp, QVariant::fromValue(data.timeStamp));
		mapData.insert(ChannelData::g_nickName, data.title);
		mapData.insert(ChannelData::g_channelUUID, uuid);
		mapData.insert(ChannelData::g_channelName, channelName);
		return QVariant::fromValue(mapData);
	};
	std::transform(tmpSrc.cbegin(), tmpSrc.cend(), std::back_inserter(tmpResult), convertData);

	mySharedData().m_scheduleList = tmpResult;
}

void PLSPlatformNCB2B::requestScheduleListByGuidePage(const std::function<void(bool)> &onNext, const QObject *widget)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
		m_vecGuideSchedules.clear();
		dealScheduleListGuidePageSucceed(data, onNext);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		std::lock_guard<std::mutex> locker(m_channelScheduleMutex);
		m_vecGuideSchedules.clear();
		QString errorType;
		auto errorRet = getApiResult(code, error, data);
		if (errorRet == PLSPlatformApiResult::PAR_TOKEN_EXPIRED) {
			errorType = "PlatformExpired";
			mySharedData().m_lastError = createScheduleGetError(getChannelName(), channel_data::NetWorkErrorType::PlatformExpired);
		} else if (errorRet == PLSPlatformApiResult::PAR_NETWORK_ERROR) {
			errorType = "NetWorkNoStable";
			mySharedData().m_lastError = createScheduleGetError(getChannelName(), channel_data::NetWorkErrorType::NetWorkNoStable);
		}

		PLS_ERROR(MODULE_PlatformService, "PLSPlatformNCB2B::requestScheduleListByGuidePage failed. with type:%s", errorType.toUtf8().constData());

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPINCB2B::requestScheduleList(widget, subChannelID(), this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}
void PLSPlatformNCB2B::dealScheduleListGuidePageSucceed(const QByteArray &data, const std::function<void(bool)> &onNext)
{
	QJsonArray items;
	QString errMsg;
	if (!PLSAPICommon::getErrorCallBack(data, items, errMsg, "lives")) {
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "dealScheduleListGuidePageSucceed failed, %s", errMsg.toUtf8().constData());
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	PLS_INFO(MODULE_PLATFORM_NCB2B, "dealScheduleListGuidePageSucceed succeed with itemCount:%i", items.count());
	for (int i = 0; i < items.size(); i++) {
		auto dataItem = items[i].toObject();
		auto scheduleDta = PLSNCB2BLiveinfoData(dataItem);
		m_vecGuideSchedules.push_back(scheduleDta);
	}
	if (nullptr != onNext) {
		onNext(true);
	}
}

const vector<PLSNCB2BLiveinfoData> &PLSPlatformNCB2B::getScheduleDatas() const
{
	return m_vecSchedules;
}

const PLSNCB2BLiveinfoData &PLSPlatformNCB2B::getNomalLiveData() const
{
	return m_noramlData;
}
const PLSNCB2BLiveinfoData &PLSPlatformNCB2B::getTempSelectData()
{
	const PLSNCB2BLiveinfoData &data = getTempSelectDataRef();
	return data;
}

PLSNCB2BLiveinfoData &PLSPlatformNCB2B::getTempSelectDataRef()
{
	for (auto &scheuleData : m_vecSchedules) {
		if (getTempSelectID() == scheuleData._id) {
			return scheuleData;
		}
	}
	return m_tempNoramlData;
}

void PLSPlatformNCB2B::updateScheduleListAndSort()
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

QString PLSPlatformNCB2B::subChannelID() const
{
	const auto &info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	if (info.isEmpty()) {
		return "";
	}
	return info.value(ChannelData::g_subChannelId, "").toString();
}

PLSNCB2BLiveinfoData::PLSNCB2BLiveinfoData(const QString &channelUUID) : isNormalLive(true)
{
	this->_id = "";

	description = "";
	scope = s_default_scope_value;

	const auto &info = PLSCHANNELS_API->getChanelInfoRef(channelUUID);
	if (info.isEmpty()) {
		return;
	}
	auto channelName = info.value(ChannelData::g_nickName, "").toString();
	title = QObject::tr("LiveInfo.live.title.suffix").arg(channelName);
}

PLSNCB2BLiveinfoData::PLSNCB2BLiveinfoData(const QJsonObject &data) : isNormalLive(false)
{
	this->_id = data["liveId"].toString();
	this->title = data["title"].toString();
	this->description = data["description"].toString();

	this->startTimeOrigin = data["reservedAt"].toString();

	this->timeStamp = PLSDateFormate::iso8601ToStamp(this->startTimeOrigin);
	this->startTimeShort = PLSDateFormate::timeStampToShortString(this->timeStamp);
	this->startTimeUTC = PLSDateFormate::timeStampToUTCString(this->timeStamp);
	this->status = data["status"].toString();
	this->scope = data["scope"].toString();

	auto rtmpPath = data["rtmpPath"].toString();
	this->streamKey = rtmpPath.split("/").last();
	this->streamUrl = rtmpPath.replace("/" + this->streamKey, "");

	this->liveLink = data["liveLink"].toString();
}

bool PLSNCB2BLiveinfoData::isNeedUpdate(const PLSNCB2BLiveinfoData &r) const
{
	return this->title != r.title || this->description != r.description || this->scope != r.scope;
}
