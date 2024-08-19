#ifndef PLSPLATFORMCHZZK_H
#define PLSPLATFORMCHZZK_H

#include <functional>
#include <QJsonDocument>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include <vector>
#include "PLSAPICommon.h"
#include "PLSSearchCombobox.h"
#include <vector>
#include <QJsonObject>

constexpr auto MODULE_PLATFORM_CHZZK = "Platform/Chzzk";

struct PLSChzzkLiveinfoData {
	PLSChzzkLiveinfoData();
	explicit PLSChzzkLiveinfoData(const QString &channelUUID);

	void setupData(const QJsonObject &data, bool isChannel);

	PLSChzzkLiveinfoData(PLSChzzkLiveinfoData const &) = default;
	PLSChzzkLiveinfoData(PLSChzzkLiveinfoData &&) noexcept = default;
	PLSChzzkLiveinfoData &operator=(PLSChzzkLiveinfoData const &) = default;
	PLSChzzkLiveinfoData &operator=(PLSChzzkLiveinfoData &&) noexcept = default;

	bool isNeedUpdate(const PLSChzzkLiveinfoData &r) const;
	bool isNeedUploadImage(const QString &localPath) const;
	bool isNeedDeleteImage(const QString &localPath, bool isClickedDeleteBtn) const;

	QString _id{};
	QString title{};
	QString description{};
	QString streamKey{};
	QString streamUrl{};
	QString liveLink{};

	bool isNormalLive = true; //true is live now , false is scheduled live.
	QString thumbnailUrl{};

	PLSSearchData categoryData;
	bool isAgeLimit = false;
	bool isNeedMoney = false;
	QString chatPermisson{};
	bool clipActive = false;

	QJsonObject extraObj;
};

class PLSPlatformChzzk : public PLSPlatformBase {
	Q_OBJECT
public:
	static const std::vector<QString> &getChatPermissionList();
	static int getIndexOfChatPermission(const QString &permission);
	static QString getchatPermissionByIndex(int index);

	PLSPlatformChzzk();
	~PLSPlatformChzzk() override = default;

	PLSServiceType getServiceType() const override;
	void onPrepareLive(bool value) override;
	void liveInfoisShowing();
	void reInitLiveInfo();

	void requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall);
	void dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall);
	void saveSettings(const std::function<void(bool)> &onNext, const PLSChzzkLiveinfoData &uiData, const QString &imagePath, const QObject *receiver, bool isClickedDeleteBtn);
	QString subChannelID() const;

	PLSChzzkLiveinfoData getSelectData() { return m_selectData; }

	/*common*/
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	bool isSendChatToMqtt() const override { return true; }
	QJsonObject getLiveStartParams() override;
	QJsonObject getMqttChatParams() override;
	QJsonObject getWebChatParams() override;

	QString getServiceLiveLink() override;
	QString getShareUrl() override;
	QString getShareUrlEnc() override;
	QString getShareUrl(bool isEnc) const;
	QString getChannelToken() const override;

	const QString &getFailedErr() const { return m_startFailedStr; }
	void setFailedErr(const QString &failedStr) { m_startFailedStr = failedStr; }
	void setlastRequestAPI(const QString &apiName) { m_lastRequestAPI = apiName; }
	void setupApiFailedWithCode(PLSPlatformApiResult result, const QByteArray &errData);
	bool isShownAlert() const { return m_isShownAlert; }
	void setIsShownAlert(bool isShownAlert) { m_isShownAlert = isShownAlert; }

	/*request*/
	void requestCreateLive(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const std::function<void(bool)> &onNext);
	bool dealCurrentLiveSucceed(const QByteArray &data, const QString &preLog, bool isSaveTmp);
	void requestUploadImage(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const QString &imagePath, const std::function<void(bool)> &onNext);
	void requestDeleteImage(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, const std::function<void(bool)> &onNext);
	void requestLiveInfo(const QObject *receiver, const std::function<void(bool)> &onNext);
	void requestUpdateInfo(const QObject *receiver, const PLSChzzkLiveinfoData &liveData, bool isChannel, const std::function<void(bool)> &onNext);

signals:
	void closeDialogByExpired();
	void toShowLoading(bool isShowLoading);
	void onGetCategory(const std::vector<PLSSearchData> &datas, const QString &request, const QString &emptyShowMsg = {});
	void changeClipToNotAllow();

public slots:
	void requestSearchCategory(const QWidget *reciever, const QString &query);

private:
	void onAllPrepareLive(bool isOk) override;

	QString m_startFailedStr{};
	QString m_lastRequestAPI{};
	bool m_isShownAlert = false;
	PLSChzzkLiveinfoData m_selectData;
	PLSChzzkLiveinfoData m_tmpData;

	void setSelectData(const PLSChzzkLiveinfoData &data);

	PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, PLSAPICommon::PLSApiType apiType = PLSAPICommon::PLSApiType::Normal) const;
	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;
};

#endif // PLSBANDDATAHANDLER_H
