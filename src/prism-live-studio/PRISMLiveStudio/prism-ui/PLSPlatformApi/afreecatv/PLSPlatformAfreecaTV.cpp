#include "PLSPlatformAfreecaTV.h"
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
#include "PLSAPIAfreecaTV.h"
#include "PLSChannelDataAPI.h"
#include "PLSDateFormate.h"
#include "PLSScheLiveNotice.h"
#include "PLSAlertView.h"

#include "frontend-api.h"
#include "log/log.h"
#include "PLSMainView.hpp"

#include "pls-net-url.hpp"
#include "window-basic-main.hpp"

using namespace std;

PLSPlatformAfreecaTV::PLSPlatformAfreecaTV()
{
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_platformName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

		if (dataType == ChannelData::ChannelType && platformName == AFREECATV) {
			reInitLiveInfo(true);
		}
	});
}

PLSServiceType PLSPlatformAfreecaTV::getServiceType() const
{
	return PLSServiceType::ST_AFREECATV;
}

const PLSAfreecaTVLiveinfoData &PLSPlatformAfreecaTV::getSelectData() const
{
	return m_selectData;
}

void PLSPlatformAfreecaTV::setSelectData(const PLSAfreecaTVLiveinfoData &data)
{
	m_selectData = data;
}

void PLSPlatformAfreecaTV::saveSettings(const function<void(bool)> &onNext, const QString &title)
{
	PLS_INFO(MODULE_PlatformService, __FUNCTION__);

	auto _onNext = [this, title, onNext](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}
		m_selectData.frmTitle = title;
		onNext(isSucceed);
	};

	auto _onCheckNext = [_onNext](bool isSucceed) { _onNext(isSucceed); };

	if (PLS_PLATFORM_API->isPrepareLive()) {
		setIsChatDisabled(false);
	}

	if (title != m_selectData.frmTitle) {
		updateLiveinfo(_onCheckNext, this, title);
	} else {
		_onCheckNext(true);
	}
}

bool PLSPlatformAfreecaTV::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}

QString PLSPlatformAfreecaTV::getShareUrl(const QString &id, bool isLiveUrl, bool /*isEnc*/) const
{
	if (isLiveUrl) {
		return QString(g_plsAfreecaTVShareUrl_living).arg(id);
	}

	return QString(g_plsAfreecaTVShareUrl_beforeLive).arg(id);
}

QString PLSPlatformAfreecaTV::getShareUrl()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();

	return getShareUrl(user_id, PLS_PLATFORM_API->isLiving());
}

QString PLSPlatformAfreecaTV::getShareUrlEnc()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	return getShareUrl(pls_masking_person_info(user_id), PLS_PLATFORM_API->isLiving(), true);
}

void PLSPlatformAfreecaTV::reInitLiveInfo(bool isReset)
{
	if (!isReset) {
		return;
	}
	setSelectData(m_selectData);
};

void PLSPlatformAfreecaTV::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

	PLS_INFO(MODULE_PlatformService, "onPrepareLive show liveinfo");
	value = pls_exec_live_Info_afreecatv(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PlatformService, "onPrepareLive liveinfo closed with:%d", value);

	setStreamKey(m_selectData.frmStreamKey.toStdString());
	setStreamServer(m_selectData.frmServerUrl.toStdString());
	PLSPlatformBase::setTitle(m_selectData.frmTitle.toStdString());
	prepareLiveCallback(value);
}

void PLSPlatformAfreecaTV::requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const
{
	auto _onSucceed = [this, srcInfo, finishedCall](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestChannelInfo succeed");
		dealRequestChannelInfoSucceed(srcInfo, data, finishedCall);
	};

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "requestChannelInfo failed");
		PLSPlatformApiResult result = getApiResult(code, error);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, code:%1").arg(code);
		info[ChannelData::g_channelStatus] = result == PLSPlatformApiResult::PAR_TOKEN_EXPIRED ? ChannelData::ChannelStatus::Expired : ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIAfreecaTV::requestUsersInfoAndChannel(this, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall) const
{
	QVariantMap info = srcInfo;
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealRequestChannelInfoSucceed failed, doc is not object");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		finishedCall(QList<QVariantMap>{info});
		return;
	}
	auto root = doc.object();
	QString userID = root["user_id"].toString();
	if (userID.isEmpty()) {
		PLS_ERROR(MODULE_PlatformService, "dealRequestChannelInfoSucceed failed, userID.isEmpty");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, code:userID empty");
		finishedCall(QList<QVariantMap>{info});
		return;
	}

	info[ChannelData::g_subChannelId] = userID;
	info[ChannelData::g_userName] = root["nickname"].toString();
	info[ChannelData::g_nickName] = root["nickname"].toString();

	QString iconUrl = root["profile_image"].toString();
	if (!iconUrl.startsWith("http")) {
		iconUrl = "https:" + iconUrl;
	}
	if (iconUrl.contains("?dummy")) {
		iconUrl = iconUrl.split("?dummy").first();
	}
	info[ChannelData::g_userProfileImg] = iconUrl;
	info[ChannelData::g_shareUrl] = getShareUrl(userID, false);
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
	info[ChannelData::g_viewers] = "0";
	info[ChannelData::g_totalViewers] = "0";
	info[ChannelData::g_viewersPix] = ChannelData::g_naverTvViewersIcon;

	PLSAPICommon::downloadImageAsync(this, iconUrl, [this, info, finishedCall](bool ok, const QString &imagePath) {
		QVariantMap tInfo = info;
		if (ok && !imagePath.isEmpty()) {
			tInfo[ChannelData::g_userIconCachePath] = imagePath;
		}
		//the requestChannelInfo nick name is maybe incorrect when nickname changed in web page, so call requestUserNickName this method to get nickname again.
		requestUserNickName(tInfo, finishedCall);
	});
}

void PLSPlatformAfreecaTV::requestUserNickName(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const
{
	auto _onSucceed = [this, srcInfo, finishedCall](QByteArray data) {
		QList<QVariantMap> infos;
		dealUserNickNameSucceed(srcInfo, data, infos);
		finishedCall(infos);
	};

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "requestUserNickName failed");
		PLSPlatformApiResult result = getApiResult(code, error);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = result == PLSPlatformApiResult::PAR_TOKEN_EXPIRED ? ChannelData::ChannelStatus::Expired : ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIAfreecaTV::requestUsersNickName(srcInfo[ChannelData::g_subChannelId].toString(), PLS_PLATFORM_AFREECATV, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealUserNickNameSucceed(const QVariantMap &srcInfo, const QByteArray &data, QList<QVariantMap> &dstInfos) const
{
	PLS_INFO(MODULE_PlatformService, "dealUserNickNameSucceed succeed");
	QVariantMap info = srcInfo;
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PlatformService, "dealUserNickNameSucceed failed, doc is not object");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		dstInfos.append(info);
		return;
	}
	QString userNickName = doc.object()["data"].toObject()["user_nick"].toString();
	if (userNickName.isEmpty()) {
		PLS_ERROR(MODULE_PlatformService, "dealUserNickNameSucceed failed, userNickName isEmpty");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		dstInfos.append(info);
		return;
	}
	info[ChannelData::g_userName] = userNickName;
	info[ChannelData::g_nickName] = userNickName;
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
	dstInfos.append(info);
}

void PLSPlatformAfreecaTV::requestDashborad(const function<void(bool)> &onNext, const QObject *reciever)
{
	auto _onSucceed = [this, onNext, reciever](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestDashborad succeed");

		dealRequestDashborad(data, onNext, reciever);
	};

	auto _onFail = [this, onNext](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "requestDashborad failed");

		setupApiFailedWithCode(getApiResult(code, error));
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIAfreecaTV::requestDashboradData(reciever, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealRequestDashborad(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *reciever)
{
	PLSAfreecaTVLiveinfoData livinfoData;
	if (data.isEmpty()) {
		setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	PLSAPIAfreecaTV::readDataByRegu(livinfoData, data);

	if (livinfoData.frmTitle.isEmpty()) {
		if (data.startsWith("<script>location.href='https://login.afreecatv.com/afreeca/login.php")) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_TOKEN_EXPIRED);
		} else {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
		}

		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	m_selectData = livinfoData;
	requestCategories(onNext, reciever);
}

void PLSPlatformAfreecaTV::requestCategories(const function<void(bool)> &onNext, const QObject *reciever)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestCategories succeed");

		if (data.isEmpty()) {
			setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		PLSAPIAfreecaTV::parseCategory(data);
		m_selectData.frmCategoryStr = PLSAPIAfreecaTV::getSelectCategoryString(m_selectData.frmCategoryID);
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "requestCategories failed");

		setupApiFailedWithCode(getApiResult(code, error));
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	if (getCategories().empty()) {
		PLSAPIAfreecaTV::requestCategoryList(reciever, _onSucceed, _onFail);
	} else {
		m_selectData.frmCategoryStr = PLSAPIAfreecaTV::getSelectCategoryString(m_selectData.frmCategoryID);
		if (nullptr != onNext) {
			onNext(true);
		}
	}
}

void PLSPlatformAfreecaTV::updateLiveinfo(const function<void(bool)> &onNext, const QObject *reciever, const QString &title)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "updateLiveinfo succeed");

		dealUpdateLiveinfoSucceed(data, onNext);
	};

	auto _onFail = [this, onNext](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "updateLiveinfo failed");

		showApiUpdateError(getApiResult(code, error));
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIAfreecaTV::updateLiveInfo(reciever, title, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::checkCanBroadcast(const std::function<void(bool)> &onNext, const QObject *reciever)
{

	auto _onSucceed = [onNext, this](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "checkCanBroadcast succeed");

		bool isSingleChannel = PLSCHANNELS_API->currentSelectedCount() == 1;
		int oId = 0;
		auto isOnline = PLSAPIAfreecaTV::getIsRemoteOnline(data, oId);
		PLS_INFO(MODULE_PlatformService, "the afreecatv is remote online:%i oid:%d", isOnline, oId);

		if (!isSingleChannel && isOnline) {
			setAllowPushStream(false);
		}

		if (isOnline) {
			pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("LiveInfo.failed.by.other.pc.isOnline").arg(AFREECATV));
		}

		if (nullptr != onNext) {
			onNext(isSingleChannel ? !isOnline : true);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "checkCanBroadcast failed");

		showApiUpdateError(getApiResult(code, error));
		if (nullptr != onNext) {
			onNext(false);
		}
	};
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	PLSAPIAfreecaTV::requestCheckIsOnline(user_id, reciever, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealUpdateLiveinfoSucceed(const QByteArray &data, const std::function<void(bool)> &onNext)
{
	if (data.isEmpty() || !QJsonDocument::fromJson(data).isObject()) {
		showApiUpdateError(PLSPlatformApiResult::PAR_API_FAILED);
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	auto channel = doc["channel"].toObject();
	if (channel["result"].toInt() == -1) {
		if (channel["remsg"].toString() == "You must log in.(406)") {
			showApiUpdateError(PLSPlatformApiResult::PAR_TOKEN_EXPIRED);
		} else {
			showApiUpdateError(PLSPlatformApiResult::PAR_API_FAILED);
		}
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	if (nullptr != onNext) {
		onNext(true);
	}
}

PLSPlatformApiResult PLSPlatformAfreecaTV::getApiResult(int code, QNetworkReply::NetworkError error) const
{
	auto result = PLSPlatformApiResult::PAR_SUCCEED;

	if (QNetworkReply::NoError == error) {
		return result;
	}

	if (QNetworkReply::UnknownNetworkError >= error) {
		result = PLSPlatformApiResult::PAR_NETWORK_ERROR;
	} else {
		switch (code) {
		case 401:
		case 515:
			result = PLSPlatformApiResult::PAR_TOKEN_EXPIRED;
			break;
		case 403:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		default:
			result = PLSPlatformApiResult::PAR_API_FAILED;
			break;
		}
	}
	return result;
}

void PLSPlatformAfreecaTV::setupApiFailedWithCode(PLSPlatformApiResult result)
{
	showApiRefreshError(result);
}

void PLSPlatformAfreecaTV::showApiRefreshError(PLSPlatformApiResult value)
{
	auto alertParent = getAlertParent();

	switch (value) {
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		PLSAlertView::warning(alertParent, QTStr("Alert.Title"), QTStr("login.check.note.network"));
		break;
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
		showTokenExpiredAlert(alertParent);
		break;
	case PLSPlatformApiResult::PAR_API_ERROR_StartLive_Other:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.start.other").arg(AFREECATV));
	default:
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Failed"));
		break;
	}
}

void PLSPlatformAfreecaTV::showTokenExpiredAlert(QWidget *alertParent)
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto channelName = info.value(ChannelData::g_platformName).toString();
	PLSAlertView::Button button = pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("Live.Check.LiveInfo.Refresh.Expired").arg(channelName));
	if (button == PLSAlertView::Button::Ok) {
		emit closeDialogByExpired();
		PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
	}
}

void PLSPlatformAfreecaTV::showApiUpdateError(PLSPlatformApiResult value)
{
	QString failedReason = "unknown";

	auto alertParent = getAlertParent();

	switch (value) {
	case PLSPlatformApiResult::PAR_TOKEN_EXPIRED:
		failedReason = "token expired";
		showTokenExpiredAlert(alertParent);
		break;
	case PLSPlatformApiResult::PAR_NETWORK_ERROR:
		failedReason = "network";
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.update.failed"));
		break;
	default:
		failedReason = "unknown";
		pls_alert_error_message(alertParent, QTStr("Alert.Title"), QTStr("LiveInfo.live.error.update.failed"));
		break;
	}

	QString failedErr = QString("updateAPI-%2-code:%3").arg(failedReason).arg((int)value);
	setFailedErr(failedErr);
}

void PLSPlatformAfreecaTV::onLiveEnded()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, getShareUrl(user_id, false));
	liveEndedCallback();
}

void PLSPlatformAfreecaTV::onAlLiveStarted(bool value)
{
	if (!value || !isActive()) {
		return;
	}

	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	PLSCHANNELS_API->setValueOfChannel(getChannelUUID(), ChannelData::g_shareUrl, getShareUrl(user_id, true));
}

QJsonObject PLSPlatformAfreecaTV::getLiveStartParams()
{
	QJsonObject platform;
	platform["cookie"] = getChannelCookie();
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["channelId"] = channelData.value(ChannelData::g_subChannelId, "").toString();
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	//platform["liveId"] = ""; //[play.afreecatv.com/ian8539/215696088] -> 215696088, but got after pushed
	return platform;
}

QJsonObject PLSPlatformAfreecaTV::getMqttChatParams()
{
	QJsonObject platform;
	platform.insert("cookie", getChannelCookie());
	return platform;
}

QJsonObject PLSPlatformAfreecaTV::getWebChatParams()
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	QJsonObject platform;
	platform["name"] = "AFREECATV";
	platform["userId"] = info.value(ChannelData::g_subChannelId, "").toString();
	platform["isPrivate"] = getIsChatDisabled();
	return platform;
}
bool PLSPlatformAfreecaTV::getIsChatDisabled() const
{
	QString logStr = QString("%1 m_IsChatDisabled:%2 b_containFrmAccess:%3").arg(__FUNCTION__).arg(BOOL2STR(m_IsChatDisabled)).arg(BOOL2STR(m_selectData.b_containFrmAccess));
	PLS_INFO(MODULE_PlatformService, logStr.toStdString().c_str());
	return m_IsChatDisabled || m_selectData.b_containFrmAccess;
}

QString PLSPlatformAfreecaTV::getServiceLiveLink()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto shareUrl = info.value(ChannelData::g_shareUrl).toString();
	return shareUrl;
}
