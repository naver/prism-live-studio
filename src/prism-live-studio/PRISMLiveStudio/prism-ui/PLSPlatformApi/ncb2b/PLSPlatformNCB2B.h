#pragma once
#include <qobject.h>
#include <vector>
#include <tuple>
#include "../PLSPlatformBase.hpp"
#include "PLSChannelDataHandler.h"
#include "PLSAPICommon.h"
#include "PLSErrorHandler.h"

struct PLSNCB2BLiveinfoData {
	PLSNCB2BLiveinfoData() = default;
	explicit PLSNCB2BLiveinfoData(const QString &channelUUID);
	explicit PLSNCB2BLiveinfoData(const QJsonObject &data);
	PLSNCB2BLiveinfoData(PLSNCB2BLiveinfoData const &) = default;
	PLSNCB2BLiveinfoData(PLSNCB2BLiveinfoData &&) noexcept = default;
	PLSNCB2BLiveinfoData &operator=(PLSNCB2BLiveinfoData const &) = default;
	PLSNCB2BLiveinfoData &operator=(PLSNCB2BLiveinfoData &&) noexcept = default;

	bool isNeedUpdate(const PLSNCB2BLiveinfoData &r) const;

	QString _id;
	QString title = "";
	QString description;
	QString startTimeOrigin;
	QString startTimeUTC;
	QString startTimeShort; //show in popup button right label
	long timeStamp = 0;

	QString status; //[RESERVED, ON_AIR, END]
	QString scope;
	QString streamKey;
	QString streamUrl;
	QString liveLink;

	bool isNormalLive = true; //true is live now , false is scheduled live.
};

class PLSPlatformNCB2B : public PLSPlatformBase {
	Q_OBJECT

public:
	static const PLSAPICommon::privacyVec &getPrivacyList();

	PLSPlatformNCB2B();
	~PLSPlatformNCB2B() = default;
	PLSServiceType getServiceType() const override;
	void liveInfoIsShowing();
	void reInitLiveInfo();
	void requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall);
	void dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall) const;

	void saveSettings(const std::function<void(bool)> &onNext, const PLSNCB2BLiveinfoData &uiData, const QObject *receiver);
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	bool isSendChatToMqtt() const override { return true; }
	QJsonObject getLiveStartParams() override;
	QJsonObject getMqttChatParams() override;
	QJsonObject getWebChatParams() override;

	QString getServiceLiveLink() override;
	QString getShareUrl() override;
	QString getShareUrlEnc() override;
	QString getChannelToken() const override;

	const QString &getFailedErr() const { return m_startFailedStr; }
	void setFailedErr(const QString &failedStr) { m_startFailedStr = failedStr; }

	void requestScheduleList(const std::function<void(bool)> &onNext, const QObject *widget);
	void dealScheduleListSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *widget);

	void requestCreateLive(const QObject *receiver, const PLSNCB2BLiveinfoData &data, const std::function<void(bool)> &onNext);
	bool dealCurrentLiveSucceed(const QByteArray &data, const QString &preLog);
	void requestCurrentSelectData(const std::function<void(bool)> &onNext, const QWidget *widget);

	const std::vector<PLSNCB2BLiveinfoData> &getScheduleDatas() const;
	const PLSNCB2BLiveinfoData &getNormalLiveData() const;
	const PLSNCB2BLiveinfoData &getTempSelectData();
	PLSNCB2BLiveinfoData &getTempSelectDataRef();
	const PLSNCB2BLiveinfoData &getSelectData() const;
	PLSPlatformNCB2B &setTempSelectID(const QString value)
	{
		m_bTempSelectID = value;
		return *this;
	}
	QString getTempSelectID() const { return m_bTempSelectID; }
	PLSNCB2BLiveinfoData &getTempNormalData() { return m_tempNormalData; };

	void updateScheduleListAndSort();

	QString subChannelID() const;

	//error handle
	PLSErrorHandler::ExtraData getErrorExtraData(const QString &urlEn, const QString &urlKr = {});
	void showAlert(const PLSErrorHandler::NetworkData &netData, const QString &customErrName, const QString &logFrom);
	void showAlertByCustName(const QString &customErrName, const QString &logFrom);
	void showAlertByPrismCode(PLSErrorHandler::ErrCode prismCode, const QString &customErrName, const QString &logFrom);
	void showAlertPostAction(const PLSErrorHandler::RetData &retData);

public slots:
	void updateScheduleList() override;

protected:
	void convertScheduleListToMapList() override;

signals:
	void closeDialogByExpired();
	void toShowLoading(bool isShowLoading);
	void selectIDChanged();

private:
	QString m_startFailedStr{};
	qint64 m_iContext{0};

	std::vector<PLSNCB2BLiveinfoData> m_vecSchedules;
	std::vector<PLSNCB2BLiveinfoData> m_vecGuideSchedules;

	//normal live data
	PLSNCB2BLiveinfoData m_normalData;
	PLSNCB2BLiveinfoData m_tempNormalData;
	//finish selected data when click ok
	PLSNCB2BLiveinfoData m_selectData;
	//temp data
	QString m_bTempSelectID;
	mutable std::mutex m_channelScheduleMutex;

	QString getShareUrl(bool isEnc) const;
	void onPrepareLive(bool value) override;
	void showApiRefreshError(PLSPlatformApiResult value);
	void showTokenExpiredAlert(QWidget *alertParent);
	void showApiUpdateError(PLSPlatformApiResult value);

	void updateLiveinfo(const QObject *receiver, const PLSNCB2BLiveinfoData &uiData, const std::function<void(bool)> &onNext);

	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;

	void setSelectData(const PLSNCB2BLiveinfoData &data);

	void requestScheduleListByGuidePage(const std::function<void(bool)> &onNext, const QObject *widget);
	void dealScheduleListGuidePageSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);
};
