#include "PLSPlatformChzzk.h"

#include <QUrl>

#include "PLSChannelDataHandlerFunctions.h"
#include "pls-common-define.hpp"
#include "../PLSLiveInfoDialogs.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "../channels/ChannelsDataApi/ChannelCommonFunctions.h"
#include "pls-channel-const.h"
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "PLSLiveInfoChzzk.h"
#include "PLSChzzkDataHandler.h"
#include "prism/PLSPlatformPrism.h"
#include "libhttp-client.h"
#include <QUrlQuery>
#include "PLSAPIChzzk.h"
#include <type_traits>
#include "PLSSearchCombobox.h"
#include "login-user-info.hpp"

using namespace common;
using namespace std;
static std::vector<QString> s_permissionList = {"ALL", "FOLLOWER", "MANAGER"};

int PLSPlatformChzzk::getIndexOfChatPermission(const QString &permission)
{
	const auto &vec = s_permissionList;
	auto it = std::find_if(vec.begin(), vec.end(), [&permission](const QString &s) { return s.compare(permission, Qt::CaseInsensitive) == 0; });

	if (it != vec.end()) {
		return (int)std::distance(vec.begin(), it);
	} else {
		return 0;
	}
}
QString PLSPlatformChzzk::getChatPermissionByIndex(int index)
{
	if (s_permissionList.size() <= index) {
		return s_permissionList.front();
	}
	return s_permissionList[index];
}

PLSPlatformChzzk::PLSPlatformChzzk()
{
	connect(PLS_PLATFORM_API, &PLSPlatformApi::channelRemoved, this, [this](const QVariantMap &info) {
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

		if (dataType == ChannelData::ChannelType && platformName == CHZZK) {
			reInitLiveInfo();
		}
	});
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::channelRefreshEnd, this,
		[this](const QString &platformName) {
			if (platformName == CHZZK) {
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
				PLS_INFO(MODULE_PLATFORM_CHZZK, "CHZZK get liveEnded signals to reinit liveinfo data");
				reInitLiveInfo();
			}
		},
		Qt::DirectConnection);
}

PLSServiceType PLSPlatformChzzk::getServiceType() const
{
	return PLSServiceType::ST_CHZZK;
}

void PLSPlatformChzzk::onPrepareLive(bool value)
{
	if (!value) {
		PLSPlatformBase::onPrepareLive(value);
		return;
	}
	PLS_INFO(MODULE_PlatformService, "onPrepareLive show liveinfo chzzk");
	value = pls_exec_live_Info_chzzk(getChannelUUID(), getInitData()) == QDialog::Accepted;
	PLS_INFO(MODULE_PlatformService, "onPrepareLive liveinfo chzzk closed with:%d", value);

	setStreamKey(m_selectData.streamKey.toStdString());
	setStreamServer(m_selectData.streamUrl.toStdString());
	PLSPlatformBase::setTitle(m_selectData.title.toStdString());
	prepareLiveCallback(value);
}

void PLSPlatformChzzk::liveInfoIsShowing()
{
	if (getSelectData().title.isEmpty()) {
		m_selectData = PLSChzzkLiveinfoData(getChannelUUID());
	}
	m_tmpData = m_selectData;
}

void PLSPlatformChzzk::reInitLiveInfo()
{
	m_selectData = PLSChzzkLiveinfoData(getChannelUUID());
	m_tmpData = m_selectData;
}

void PLSPlatformChzzk::requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	auto _onSucceed = [this, srcInfo, finishedCall](QByteArray data) {
		PLS_INFO(MODULE_PlatformService, "requestChannelInfo succeed");
		dealRequestChannelInfoSucceed(srcInfo, data, finishedCall);
	};

	auto _onFail = [this, srcInfo, finishedCall](int code, QByteArray data, QNetworkReply::NetworkError error) {
		PLS_ERROR(MODULE_PlatformService, "requestChannelInfo failed");

		auto extraData = getErrorExtraData("requestChannelInfo");
		extraData.errPhase = PLSErrPhaseLogin;
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), "", extraData);

		auto chStatus = ChannelData::ChannelStatus::Error;
		if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired) {
			chStatus = ChannelData::ChannelStatus::Expired;
		}
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, result:%1").arg((int)retData.prismCode);
		info[ChannelData::g_channelStatus] = chStatus;
		info[ChannelData::g_errorRetdata] = QVariant::fromValue(retData);
		info[ChannelData::g_errorString] = retData.alertMsg;

		finishedCall(QList<QVariantMap>{info});
	};

	PLSAPIChzzk::requestChannelList(this, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::NotRefresh);
}

void PLSPlatformChzzk::dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall)
{
	QJsonArray channelList;
	QString errMsg;
	QVariantMap info = srcInfo;
	if (!PLSAPICommon::getErrorCallBack(data, channelList, errMsg, "channels")) {
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "requestChannelInfo failed, %s", errMsg.toUtf8().constData());
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
			PLS_INFO(MODULE_PLATFORM_CHZZK, "requestChannelInfo channel not enable, id: %s", channelID.toUtf8().constData());
			continue;
		}

		QVariantMap subInfo;
		subInfo[ChannelData::g_channelCookie] = srcInfo[ChannelData::g_channelCookie];
		subInfo[ChannelData::g_channelName] = srcInfo[ChannelData::g_channelName].toString();

		subInfo[ChannelData::g_shareUrl] = channel["channelLink"].toString();

		subInfo[ChannelData::g_subChannelId] = channelID;
		auto channelUrl = channel["channelThumbnailUrl"].toString();
		subInfo[ChannelData::g_userProfileImg] = channelUrl;
		subInfo[ChannelData::g_nickName] = channel["channelName"].toString();

		subInfo[ChannelData::g_catogry] = channel["defaultLiveCategory"].toObject()["name"].toString();

		subInfo[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Valid;

		subInfo[ChannelData::g_viewers] = "0";
		subInfo[ChannelData::g_viewersPix] = ChannelData::g_defaultViewerIcon;
		subInfo[ChannelData::g_userIconCachePath] = "";
		subInfo[ChannelData::g_chzzkExtraData] = channel["extraFields"].toObject();

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
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "requestChannelInfo failed, %s", errMsg.toUtf8().constData());
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::EmptyChannel;
		info[ChannelData::g_channelSreLoginFailed] = QString("Get Channel List Failed, %1").arg(errMsg);
		finishedCall(QList<QVariantMap>{info});
		return;
	}
	finishedCall(infos);
	pls_async_call_mt([]() { PLSAPICommon::downloadChannelImageAsync(CHZZK); });
}

void PLSPlatformChzzk::saveSettings(const function<void(bool)> &onNext, const PLSChzzkLiveinfoData &uiData, const QString &imagePath, const QObject *receiver, bool isClickedDeleteBtn)
{
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	auto cid = channelData.value(ChannelData::g_subChannelId, "").toString();

	m_tmpData = uiData;
	PLS_INFO(MODULE_PLATFORM_CHZZK, __FUNCTION__);

	auto _onUpdateNext = [this, onNext, uiData](bool isSucceed) {
		if (isSucceed) {
			setSelectData(m_tmpData);
		}
		onNext(isSucceed);
	};

	auto _onUpdateImageNext = [this, uiData, receiver, imagePath, _onUpdateNext, isClickedDeleteBtn](bool isSucceed) {
		if (!isSucceed) {
			_onUpdateNext(false);
			return;
		}
		bool isNeedUploadImage = uiData.isNeedUploadImage(imagePath);
		bool isNeedDeleteImage = uiData.isNeedDeleteImage(imagePath, isClickedDeleteBtn);

		if (isNeedUploadImage) {
			requestUploadImage(receiver, uiData, imagePath, _onUpdateNext);
		} else if (isNeedDeleteImage) {
			requestDeleteImage(receiver, uiData, _onUpdateNext);
		} else {
			_onUpdateNext(isSucceed);
		}
	};

	if (PLS_PLATFORM_API->isPrepareLive()) {
		requestCreateLive(receiver, m_tmpData, _onUpdateImageNext);
		return;
	}
	bool isNeedUpdate = m_tmpData.isNeedUpdate(m_selectData);
	if (isNeedUpdate) {
		requestUpdateInfo(receiver, m_tmpData, true, _onUpdateImageNext);
	} else {
		_onUpdateImageNext(true);
	}
}

void PLSPlatformChzzk::requestCreateLive(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		bool isOk = dealCurrentLiveSucceed(data, "requestCreateLive", true);
		if (!isOk) {
			showAlertByCustName(PLSErrCustomKey_StartLiveFailed_Single, "requestCreateLive");
		}
		if (onNext) {
			onNext(isOk);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_StartLiveFailed_Single, "requestCreateLive");

		if (nullptr != onNext) {
			onNext(false);
		}
	};
	PLSAPIChzzk::requestCreateLive(receiver, liveData, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

bool PLSPlatformChzzk::dealCurrentLiveSucceed(const QByteArray &data, const QString &preLog, bool isSaveTmp)
{
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isObject()) {
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "%s failed, doc is not object", preLog.toUtf8().constData());
		return false;
	}
	bool isChannel = false;
	auto root = doc.object();
	QJsonObject obj = root["live"].toObject();
	if (obj.isEmpty()) {
		isChannel = true;
		obj = root["channel"].toObject();
	}
	if (obj.isEmpty()) {
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "%s failed, data is empty", preLog.toUtf8().constData());
		return false;
	}

	PLSChzzkLiveinfoData liveData = isSaveTmp ? m_tmpData : m_selectData;
	liveData.setupData(obj, isChannel);
	if (isSaveTmp) {
		m_tmpData = liveData;
	} else {
		setSelectData(liveData);
	}
	return true;
}

void PLSPlatformChzzk::requestUploadImage(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const QString &imagePath, const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext, imagePath](QByteArray data) {
		QString thumbnailUrl;
		QString errMsg;
		if (!PLSAPICommon::getErrorCallBack(data, thumbnailUrl, errMsg, "thumbnailUrl")) {
			PLS_ERROR(MODULE_PLATFORM_CHZZK, "chzzk requestUploadImage failed, %s", "", errMsg.toUtf8().constData());
			showAlertByCustName(PLSErrCustomKey_UploadImageFailed, "requestUploadImage");
			if (nullptr != onNext) {
				onNext(false);
			}
			return;
		}
		m_tmpData.thumbnailUrl = thumbnailUrl;
		if (nullptr != onNext) {
			onNext(true);
		}
		auto mapedLocalPath = PLSAPICommon::getMd5ImagePath(thumbnailUrl);
		if (QFile(mapedLocalPath).exists()) {
			QFile::remove(mapedLocalPath);
		}
		QFile::copy(imagePath, mapedLocalPath);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_UploadImageFailed, "requestUploadImage");

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIChzzk::uploadImage(receiver, liveData, imagePath, this, _onSucceed, _onFail);
}

void PLSPlatformChzzk::requestDeleteImage(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		m_tmpData.thumbnailUrl = "";
		m_selectData.thumbnailUrl = "";
		if (nullptr != onNext) {
			onNext(true);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_UpdateLiveInfoFailed, "requestDeleteImage");

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIChzzk::deleteImage(receiver, liveData, this, _onSucceed, _onFail);
}

void PLSPlatformChzzk::requestLiveInfo(const QObject *receiver, const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		bool isOk = dealCurrentLiveSucceed(data, "requestLiveInfo", false);
		if (!isOk) {
			showAlertByCustName(PLSErrCustomKey_LoadLiveInfoFailed, "requestLiveInfo");
		}
		if (onNext) {
			onNext(isOk);
		}
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_LoadLiveInfoFailed, "requestLiveInfo");

		if (nullptr != onNext) {
			onNext(false);
		}
	};

	PLSAPIChzzk::requestChannelOrLiveInfo(receiver, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

void PLSPlatformChzzk::requestUpdateInfo(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, bool isChannel, const std::function<void(bool)> &onNext)
{
	auto _onSucceed = [this, onNext](QByteArray data) {
		bool isOk = dealCurrentLiveSucceed(data, "requestUpdateInfo", true);
		if (!isOk) {
			showAlertByCustName(PLSErrCustomKey_UpdateLiveInfoFailed, "requestUpdateInfo");
		}
		pls_invoke_safe(onNext, isOk);
	};

	auto _onFail = [this, onNext](int code, QByteArray data, QNetworkReply::NetworkError error) {
		showAlert({code, error, data}, PLSErrCustomKey_UpdateLiveInfoFailed, "requestUpdateInfo");
		pls_invoke_safe(onNext, false);
	};
	PLSAPIChzzk::requestUpdateChannelOrLiveInfo(receiver, liveData, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::CheckRefresh);
}

QString PLSPlatformChzzk::subChannelID() const
{
	const auto &info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	return info.value(ChannelData::g_subChannelId, "").toString();
}
void PLSPlatformChzzk::requestSearchCategory(const QWidget *receiver, const QString &query)
{
	auto _onSucceed = [this, query](QByteArray data) {
		QJsonArray categories;
		QString errMsg;
		if (!PLSAPICommon::getErrorCallBack(data, categories, errMsg, "categories")) {
			PLS_INFO(MODULE_PLATFORM_CHZZK, "requestSearchCategory failed, %s", errMsg.toUtf8().constData());
		}
		std::vector<PLSSearchData> datas;
		for (int i = 0; i < categories.size(); i++) {
			PLSSearchData data(categories[i].toObject());
			datas.emplace_back(data);
		}
		emit onGetCategory(datas, query);
	};

	auto _onFail = [this, query](int code, QByteArray data, QNetworkReply::NetworkError error) {
		auto retData = PLSErrorHandler::getAlertString({code, error, data}, getChannelName(), PLSErrCustomKey_UpdateLiveInfoFailed, getErrorExtraData("requestSearchCategory"));
		QString emptyShowMsg = retData.prismCode == PLSErrorHandler::COMMON_NETWORK_ERROR ? retData.alertMsg : QString();
		emit onGetCategory({}, query, emptyShowMsg);
	};
	PLSAPIChzzk::requestSearchCategory(receiver, query, this, _onSucceed, _onFail, PLSAPICommon::RefreshType::NotRefresh);
}

QString PLSPlatformChzzk::getShareUrl(bool isEnc) const
{
	auto uid = isEnc ? pls_masking_person_info(subChannelID()) : subChannelID();
	return QString("%1/live/%2").arg(g_plsChzzkApiHost).arg(uid);
}

QString PLSPlatformChzzk::getShareUrl()
{
	return getShareUrl(false);
}

QString PLSPlatformChzzk::getShareUrlEnc()
{
	return getShareUrl(true);
}

bool PLSPlatformChzzk::onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject)
{
	return true;
}

void PLSPlatformChzzk::onAllPrepareLive(bool isOk)
{
	PLSPlatformBase::onAllPrepareLive(isOk);
}
void PLSPlatformChzzk::setSelectData(const PLSChzzkLiveinfoData &data)
{
	auto oldID = m_selectData._id;
	m_selectData = data;

	const QString &uuid = getChannelUUID();
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_shareUrlTemp, getShareUrl(false));

	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_displayLine2, data.categoryData.name);
	PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_catogry, data.categoryData.name);

	if (!PLS_PLATFORM_API->isGoLive()) {
		PLSCHANNELS_API->channelModified(uuid);
	} else if (PLS_PLATFORM_API->isGoLive() && PLS_PLATFORM_API->isPrepareLive()) {
		PLSCHANNELS_API->channelModified(uuid);
	}
}

void PLSPlatformChzzk::onLiveEnded()
{
	auto info = PLSCHANNELS_API->getChannelInfo(getChannelUUID());
	auto user_id = info.value(ChannelData::g_subChannelId).toString();
	liveEndedCallback();
}

void PLSPlatformChzzk::onAlLiveStarted(bool) {}

QJsonObject PLSPlatformChzzk::getLiveStartParams()
{
	QJsonObject platform;
	platform["cookie"] = getChannelCookie();
	const auto &channelData = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());
	platform["simulcastChannel"] = channelData.value(ChannelData::g_nickName, "").toString();
	platform["channelId"] = channelData.value(ChannelData::g_subChannelId, "").toString();
	platform["partnerLiveId"] = m_selectData._id;
	platform["liveLink"] = getShareUrl();
	return platform;
}

QJsonObject PLSPlatformChzzk::getMqttChatParams()
{
	QJsonObject platform;
	platform.insert("cookie", getChannelCookie());
	platform["accessToken"] = QString(PLSLoginUserInfo::getInstance()->getPrismCookie());
	return platform;
}
QJsonObject PLSPlatformChzzk::getWebChatParams()
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(getChannelUUID());

	QJsonObject obj;
	obj["sub_name"] = info.value(ChannelData::g_channelName).toString();
	obj["name"] = "CHZZK";
	obj["userId"] = info.value(ChannelData::g_subChannelId, "").toString();
	obj["access_token"] = QString(PLSLoginUserInfo::getInstance()->getPrismCookie());
	obj["x-prism-access-token"] = getChannelCookie();
	obj["simulcast_seq"] = getChannelLiveSeq();
	obj["global_uri"] = PRISM_SSL;
	obj["hmac_salt"] = PLS_PC_HMAC_KEY;

	return obj;
}
QString PLSPlatformChzzk::getServiceLiveLink()
{
	return getShareUrl();
}
QString PLSPlatformChzzk::getChannelToken() const
{
	return PLSLoginUserInfo::getInstance()->getPrismCookie();
}

PLSErrorHandler::ExtraData PLSPlatformChzzk::getErrorExtraData(const QString &urlEn, const QString &urlKr)
{
	PLSErrorHandler::ExtraData extraData;
	extraData.urlEn = urlEn;
	extraData.pathValueMap = {{"chzzkLiveId", m_selectData._id}};
	return extraData;
}
void PLSPlatformChzzk::showAlert(const PLSErrorHandler::NetworkData &netData, const QString &customErrName, const QString &logFrom)
{
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlert(netData, getChannelName(), customErrName, getErrorExtraData(logFrom));
	showAlertPostAction(retData);
}
void PLSPlatformChzzk::showAlertByCustName(const QString &customErrName, const QString &logFrom)
{
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlertByCustomErrName(customErrName, getChannelName(), getErrorExtraData(logFrom));
	showAlertPostAction(retData);
}
void PLSPlatformChzzk::showAlertByPrismCode(PLSErrorHandler::ErrCode prismCode, const QString &customErrName, const QString &logFrom)
{
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlertByPrismCode(prismCode, getChannelName(), customErrName, getErrorExtraData(logFrom));
	showAlertPostAction(retData);
}
void PLSPlatformChzzk::showAlertPostAction(const PLSErrorHandler::RetData &retData)
{
	setFailedErr(retData.extraData.urlEn + " " + retData.failedLogString);
	if (retData.prismCode == PLSErrorHandler::COMMON_TOKEN_EXPIRED_ERROR) {
		emit closeDialogByExpired();
		if (retData.clickedBtn == PLSAlertView::Button::Ok) {
			PLSCHANNELS_API->channelExpired(getChannelUUID(), false);
		}
	}
}

PLSChzzkLiveinfoData::PLSChzzkLiveinfoData()
{
	auto infos = PLSCHANNELS_API->getChanelInfosByPlatformName(CHZZK, ChannelData::ChannelType);
	if (infos.empty()) {
		return;
	}
	auto info = infos[0];
	_id = info[ChannelData::g_subChannelId].toString();
	categoryData.name = info[ChannelData::g_catogry].toString();
}
PLSChzzkLiveinfoData::PLSChzzkLiveinfoData(const QString &channelUUID) : isNormalLive(true)
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(channelUUID);
	if (info.isEmpty()) {
		return;
	}
	this->_id = info.value(ChannelData::g_subChannelId, "").toString();
	categoryData.name = info[ChannelData::g_catogry].toString();
}

void PLSChzzkLiveinfoData::setupData(const QJsonObject &data, bool isChannel)
{
	QJsonObject cData;
	if (isChannel) {
		this->title = data["defaultLiveTitle"].toString();
		this->description = data["channelDescription"].toString();
		cData = data["defaultLiveCategory"].toObject();
		this->thumbnailUrl = data["defaultThumbnailImageUrl"].toString();
		this->liveLink = data["channelLink"].toString();
	} else {
		this->_id = data["liveId"].toString();
		title = data["title"].toString();
		auto rtmpPath = data["rtmpPath"].toString();
		this->streamKey = rtmpPath.split("/").last();
		this->streamUrl = rtmpPath.replace("/" + this->streamKey, "");
		this->liveLink = data["liveLink"].toString();
		this->thumbnailUrl = data["liveThumbnailImageUrl"].toString();
		cData = data["liveCategory"].toObject();
	}
	categoryData = PLSSearchData(cData);
	auto extra = data["extraFields"].toObject();
	isAgeLimit = extra["adult"].toBool();
	isNeedMoney = extra["paidPromotion"].toBool();
	chatPermission = extra["chatAvailableGroup"].toString();
	clipActive = extra["clipActive"].toBool();
	extraObj = extra;
}

bool PLSChzzkLiveinfoData::isNeedUpdate(const PLSChzzkLiveinfoData &r) const
{
	if (title != r.title) {
		return true;
	}
	if (categoryData.name != r.categoryData.name) {
		return true;
	}
	if (isAgeLimit != r.isAgeLimit) {
		return true;
	}
	if (isNeedMoney != r.isNeedMoney) {
		return true;
	}
	if (chatPermission != r.chatPermission) {
		return true;
	}
	if (clipActive != r.clipActive) {
		return true;
	}
	if (categoryData.id != r.categoryData.id) {
		return true;
	}
	return false;
}
bool PLSChzzkLiveinfoData::isNeedUploadImage(const QString &localPath) const
{
	if (localPath.isEmpty()) {
		return false;
	}
	if (thumbnailUrl.isEmpty() && !localPath.isEmpty()) {
		return true;
	}

	auto oldPath = PLSAPICommon::getMd5ImagePath(thumbnailUrl);
	if (oldPath != localPath) {
		return true;
	}
	return false;
}
bool PLSChzzkLiveinfoData::isNeedDeleteImage(const QString &localPath, bool isClickedDeleteBtn) const
{
	if (localPath.isEmpty() && !thumbnailUrl.isEmpty()) {
		return isClickedDeleteBtn;
	}

	return false;
}
