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
#include "PLSErrorHandler.h"

using namespace std;

PLSPlatformAfreecaTV::PLSPlatformAfreecaTV()
{
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
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
	PLS_INFO(MODULE_PLATFORM_AFREECATV, __FUNCTION__);

	bool isNeedUpdate = title != m_selectData.title;

	auto _onNext = [this, title, onNext](bool isSucceed) {
		if (!isSucceed) {
			onNext(isSucceed);
			return;
		}
		m_selectData.title = title;
		onNext(isSucceed);
	};

	auto _onUpdateNext = [_onNext, title, this, isNeedUpdate](bool isSucceed) {
		if (!isSucceed) {
			_onNext(isSucceed);
			return;
		}

		if (isNeedUpdate) {
			//update must get password api first.
			updateLiveinfo(_onNext, this, title);
		} else {
			_onNext(true);
		}
	};

	if (PLS_PLATFORM_API->isPrepareLive()) {
		setIsChatDisabled(false);
		requestStreamKeyAndPassword(_onUpdateNext, this);
		return;
	}
	if (isNeedUpdate) {
		//update need get password
		requestStreamKeyAndPassword(_onUpdateNext, this);
		return;
	}

	// not need update and not get password
	_onNext(true);
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

	PLS_INFO(MODULE_PLATFORM_AFREECATV, "onPrepareLive show liveinfo");
	value = pls_exec_live_Info_afreecatv(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PLATFORM_AFREECATV, "onPrepareLive liveinfo closed with:%d", value);

	setStreamKey(m_selectData.stremKey.toStdString());
	setStreamServer(m_selectData.rtmp_server.toStdString());
	PLSPlatformBase::setTitle(m_selectData.title.toStdString());
	prepareLiveCallback(value);
}

void PLSPlatformAfreecaTV::requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const
{
	auto _onSucceed = [this, srcInfo, finishedCall](QByteArray data) {
		PLS_INFO(MODULE_PLATFORM_AFREECATV, "requestChannelInfo succeed");
		dealRequestChannelInfoSucceed(srcInfo, data, finishedCall);
	};

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestChannelInfo failed");
		PLSErrorHandler::ExtraData exData;
		exData.urlEn = g_plsAfreecaTVChannelInfo;
		exData.errPhase = PLSErrPhaseLogin;
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, AFREECATV, "", exData);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, code:%1").arg(code);
		info[ChannelData::g_channelStatus] = retData.prismCode == PLSErrorHandler::CHANNEL_AFREECATV_LOGIN_EXPIRED ? ChannelData::ChannelStatus::Expired : ChannelData::ChannelStatus::Error;
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;
		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIAfreecaTV::requestUsersInfoAndChannel(this, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall) const
{
	QVariantMap info = srcInfo;
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "dealRequestChannelInfoSucceed failed, doc is not object");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		finishedCall(QList<QVariantMap>{info});
		return;
	}
	auto root = doc.object();
	QString userID = root["user_id"].toString();
	if (userID.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "dealRequestChannelInfoSucceed failed, userID.isEmpty");
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

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestUserNickName failed");
		PLSErrorHandler::ExtraData exData;
		exData.urlEn = g_plsAfreecaTVUserNick.arg("userNick");
		exData.errPhase = PLSErrPhaseLogin;
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, AFREECATV, "", exData);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = retData.prismCode == PLSErrorHandler::CHANNEL_AFREECATV_LOGIN_EXPIRED ? ChannelData::ChannelStatus::Expired : ChannelData::ChannelStatus::Error;
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;
		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIAfreecaTV::requestUsersNickName(srcInfo[ChannelData::g_subChannelId].toString(), PLS_PLATFORM_AFREECATV, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealUserNickNameSucceed(const QVariantMap &srcInfo, const QByteArray &data, QList<QVariantMap> &dstInfos) const
{
	PLS_INFO(MODULE_PLATFORM_AFREECATV, "dealUserNickNameSucceed succeed");
	QVariantMap info = srcInfo;
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "dealUserNickNameSucceed failed, doc is not object");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		dstInfos.append(info);
		return;
	}
	QString userNickName = doc.object()["station"].toObject()["user_nick"].toString();
	if (userNickName.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "dealUserNickNameSucceed failed, userNickName isEmpty");
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		dstInfos.append(info);
		return;
	}
	info[ChannelData::g_userName] = userNickName;
	info[ChannelData::g_nickName] = userNickName;
	info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;
	dstInfos.append(info);
}

void PLSPlatformAfreecaTV::requestDashborad(const function<void(bool)> &onNext, const QObject *receiver, bool isForUpdate)
{
	auto _onSucceed = [this, onNext, receiver, isForUpdate](QByteArray data) {
		PLS_INFO(MODULE_PLATFORM_AFREECATV, "requestDashborad succeed");
		dealRequestDashborad(data, onNext, receiver, isForUpdate);
	};

	auto _onFail = [this, onNext, isForUpdate](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestDashborad failed");
		auto customName = isForUpdate ? PLSErrCustomKey_UpdateLiveInfoFailed : PLSErrCustomKey_LoadLiveInfoFailed;
		showApiError(g_plsAfreecaTVDashboard, "requestDashborad", customName, code, data, error);
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIAfreecaTV::requestDashboradData(receiver, _onSucceed, _onFail);
}

void PLSPlatformAfreecaTV::dealRequestDashborad(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *receiver, bool isForUpdate)
{
	QJsonObject retData;
	auto dealJson = [data, &retData]() {
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			PLS_ERROR(MODULE_PLATFORM_AFREECATV, "%s failed, doc is not object", "dealRequestDashborad");
			return false;
		}
		auto root = doc.object();
		auto resultInt = root["result"].toInt();
		if (resultInt != 1) {
			PLS_ERROR(MODULE_PLATFORM_AFREECATV, "%s failed, result is %i", "dealRequestDashborad", resultInt);
			return false;
		}
		retData = root["data"].toObject();
		if (retData.isEmpty()) {
			PLS_ERROR(MODULE_PLATFORM_AFREECATV, "%s failed, data is empty", "dealRequestDashborad");
			return false;
		}
		return true;
	};

	if (!dealJson()) {
		PLSErrorHandler::ExtraData exData;
		exData.urlEn = g_plsAfreecaTVDashboard;
		auto customName = isForUpdate ? PLSErrCustomKey_UpdateLiveInfoFailed : PLSErrCustomKey_LoadLiveInfoFailed;
		showApiError(g_plsAfreecaTVDashboard, "requestDashborad", customName, 200, data, QNetworkReply::NetworkError::NoError);
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	m_selectData.user_id = retData["user_id"].toString();
	m_selectData.categoryID = retData["category"].toString();
	m_selectData.rtmp_server = retData["rtmp_server"].toString();
	m_selectData.hashtags = retData["hashtags"].toString();
	m_selectData.broad_grade = retData["broad_grade"].toString();
	m_selectData.broad_hidden = retData["broad_hidden"].toString();
	m_selectData.broad_tune_out = retData["broad_tune_out"].toString();
	m_selectData.paid_promotion = retData["paid_promotion"].toString();

	if (!isForUpdate) {
		//is For update， can't set title, because title need update by liveinfo user set.
		m_selectData.title = retData["title"].toString();
	}
	requestCategories(onNext, receiver);
}

void PLSPlatformAfreecaTV::requestCategories(const function<void(bool)> &onNext, const QObject *receiver)
{
	PLSErrorHandler::ExtraData exData;
	exData.urlEn = g_plsAfreecaTVCategories.arg("Categories");
	auto _onSucceed = [this, onNext, exData](QByteArray data) {
		PLS_INFO(MODULE_PLATFORM_AFREECATV, "requestCategories succeed");

		if (data.isEmpty()) {
			PLSErrorHandler::showAlertByCustomErrName(PLSErrCustomKey_LoadLiveInfoFailed, AFREECATV, exData);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}

		PLSAPIAfreecaTV::parseCategory(data);
		m_selectData.categoryStr = PLSAPIAfreecaTV::getSelectCategoryString(m_selectData.categoryID);
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [this, onNext, exData](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestCategories failed");
		auto retData = PLSErrorHandler::showAlert({code, error, data}, AFREECATV, PLSErrCustomKey_LoadLiveInfoFailed, exData);
		if (retData.prismCode == PLSErrorHandler::CHANNEL_AFREECATV_API_EXPIRED && retData.clickedBtn == QDialogButtonBox::Ok) {
			emit closeDialogByExpired();
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	if (getCategories().empty()) {
		PLSAPIAfreecaTV::requestCategoryList(receiver, _onSucceed, _onFail);
	} else {
		m_selectData.categoryStr = PLSAPIAfreecaTV::getSelectCategoryString(m_selectData.categoryID);
		if (nullptr != onNext) {
			onNext(true);
		}
	}
}

void PLSPlatformAfreecaTV::updateLiveinfo(const function<void(bool)> &onNext, const QObject *receiver, const QString &title)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		PLS_INFO(MODULE_PLATFORM_AFREECATV, "updateLiveinfo succeed");
		dealUpdateLiveinfoSucceed(data, onNext);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "updateLiveinfo failed");
		showApiError(g_plsAfreecaTVUpdate, "updateAPI", PLSErrCustomKey_UpdateLiveInfoFailedNoService, code, data, error);
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	requestDashborad(
		[receiver, title, _onSucceed, _onFail, onNext](bool isOK) {
			if (!isOK) {
				if (nullptr != onNext) {
					onNext(false);
				}
				return;
			}
			PLSAPIAfreecaTV::updateLiveInfo(receiver, title, _onSucceed, _onFail);
		},
		receiver, true);
}

void PLSPlatformAfreecaTV::dealUpdateLiveinfoSucceed(const QByteArray &data, const std::function<void(bool)> &onNext)
{
	if (data.isEmpty() || !QJsonDocument::fromJson(data).isObject()) {
		showApiError(g_plsAfreecaTVUpdate, "updateAPI", PLSErrCustomKey_UpdateLiveInfoFailedNoService, 200, data, QNetworkReply::NetworkError::NoError);
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}
	auto doc = QJsonDocument::fromJson(data);
	auto toInt = doc["result"].toInt();
	if (toInt != 1) {
		showApiError(g_plsAfreecaTVUpdate, "updateAPI", PLSErrCustomKey_UpdateLiveInfoFailedNoService, 200, data, QNetworkReply::NetworkError::NoError);
		if (nullptr != onNext) {
			onNext(false);
		}
		return;
	}

	if (nullptr != onNext) {
		onNext(true);
	}
}

void PLSPlatformAfreecaTV::showApiError(const QString &url, const QString &apiName, const QString &customErrName, int code, QByteArray data, QNetworkReply::NetworkError error, bool isShowExpired)
{
	QString failedReason = "unknown";
	PLSErrorHandler::RetData retData;
	PLSErrorHandler::ExtraData exData;
	exData.urlEn = url;
	if (isShowExpired) {
		retData = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::COMMON_TOKEN_EXPIRED_ERROR, AFREECATV, customErrName, exData);
	} else {
		retData = PLSErrorHandler::showAlert({code, error, data}, AFREECATV, customErrName, exData);
	}

	if (retData.prismCode == PLSErrorHandler::CHANNEL_AFREECATV_API_EXPIRED) {
		failedReason = "token expired";
		if (retData.clickedBtn == QDialogButtonBox::Ok) {
			emit closeDialogByExpired();
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
	}
	if (retData.prismCode == PLSErrorHandler::COMMON_NETWORK_ERROR) {
		failedReason = "network";
	}
	if (retData.prismCode == PLSErrorHandler::COMMON_DEFAULT_UPDATELIVEINFOFAILED_NOSERVICE) {
		failedReason = "unknown";
	}
	QString failedErr = apiName + QString("-%1-code:%2").arg(failedReason).arg((int)retData.prismCode);
	setFailedErr(failedErr);
}

void PLSPlatformAfreecaTV::requestStreamKeyAndPassword(const std::function<void(bool)> &onNext, const QObject *receiver)
{
	PLSErrorHandler::ExtraData exData;
	exData.urlEn = g_plsAfreecaTVCategories.arg("Categories");
	auto _onSucceed = [this, onNext, exData](QByteArray data) {
		QString streamKey;
		QString pw;
		PLSAPIAfreecaTV::getStreamKeyAndPassword(data, streamKey, pw);
		m_selectData.broad_pwd_chk = !pw.isEmpty();
		m_selectData.access_code = pw;

		if (streamKey.isEmpty()) {
			bool isToeknExpired = false;
			if (data.startsWith("<script>location.href='https://login.sooplive.co.kr/afreeca/login.php") ||
			    data.startsWith("<script>location.href='http://login.sooplive.co.kr/afreeca/login.php")) {
				PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestStreamKeyAndPassword failed, data is startwith login page");
				isToeknExpired = true;
			} else {
				PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestStreamKeyAndPassword failed, streamkey is empty");
			}
			showApiError("", "requestStreamKeyAndPassword", PLSErrCustomKey_UpdateLiveInfoFailedNoService, 200, data, QNetworkReply::NetworkError::NoError,
				     isToeknExpired);
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		m_selectData.stremKey = m_selectData.user_id + "-" + streamKey;

		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [this, onNext, exData](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PLATFORM_AFREECATV, "requestStreamKeyAndPassword failed");
		showApiError("", "requestStreamKeyAndPassword", PLSErrCustomKey_UpdateLiveInfoFailedNoService, code, data, error);
		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIAfreecaTV::requestMainHtml(receiver, _onSucceed, _onFail);
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
	QString logStr = QString("%1 m_IsChatDisabled:%2 b_containFrmAccess:%3").arg(__FUNCTION__).arg(BOOL2STR(m_IsChatDisabled)).arg(BOOL2STR(m_selectData.broad_pwd_chk));
	PLS_INFO(MODULE_PLATFORM_AFREECATV, logStr.toStdString().c_str());
	return m_IsChatDisabled || m_selectData.broad_pwd_chk;
}

QString PLSPlatformAfreecaTV::getServiceLiveLink()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto shareUrl = info.value(ChannelData::g_shareUrl).toString();
	return shareUrl;
}
