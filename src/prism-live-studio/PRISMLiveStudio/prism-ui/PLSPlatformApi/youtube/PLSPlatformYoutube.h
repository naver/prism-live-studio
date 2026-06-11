/*
* @file		PlatformYoutube.h
* @brief	All youtube relevant api is implemented in this file
* @author	ren.jinbo@navercorp.com
* @date		2020-xx-xx
*/

#pragma once

#include <qobject.h>
#include <QDateTime>
#include <functional>
#include <vector>
#include <mutex>
#include "PLSPlatformBase.hpp"
#include "PLSErrorHandler.h"

extern const QString kDefaultCategoryID;
extern const QString s_latencyLow;
extern const QString s_latencyNormal;
extern const QString s_latencyUltraLow;
extern const QString s_closed_captions_type;
extern const QString s_youtube_private_en;
extern const QString s_description_default_add;

struct PLSYoutubeCategory {
	QString _id;
	QString title;
};

struct PLSYoutubeStart {
	bool enableAutoStart = true;
	bool enableAutoStop = true;
	bool enableMonitorStream = false;
	inline bool operator==(const PLSYoutubeStart &rhs) const
	{
		return enableAutoStart == rhs.enableAutoStart && enableAutoStop == rhs.enableAutoStop && enableMonitorStream == rhs.enableMonitorStream;
	}
	inline bool operator!=(const PLSYoutubeStart &rhs) const { return !(*this == rhs); }

	inline bool isCanRehearsal() const { return !enableAutoStart && !enableAutoStop && enableMonitorStream; }
};

struct PLSYoutubeLiveinfoData {
	Q_GADGET
public:
	enum class Latency { Low = 0, Normal = 1, UltraLow = 2 };
	Q_ENUM(Latency)

	enum class IngestionType { Rtmps = 1, Hls = 2 };
	Q_ENUM(IngestionType)

	PLSYoutubeLiveinfoData() = default;
	explicit PLSYoutubeLiveinfoData(const QJsonObject &data);
	PLSYoutubeLiveinfoData(PLSYoutubeLiveinfoData const &) = default;
	PLSYoutubeLiveinfoData(PLSYoutubeLiveinfoData &&) noexcept = default;
	PLSYoutubeLiveinfoData &operator=(PLSYoutubeLiveinfoData const &) = default;
	PLSYoutubeLiveinfoData &operator=(PLSYoutubeLiveinfoData &&) noexcept = default;

	void setStatusAndSnippetData(const QJsonObject &data);
	void updateOriginThumUrl(const QJsonObject &snippet);

	QString boundStreamId;
	QString _id;
	QString title = "";
	QString description;
	QString startTimeOrigin;
	QString startTimeUTC;
	QString startTimeShort; //show in popup button right label
	QString liveChatId;
	QString privacyStatus;
	QString categoryID;
	QString channelID;
	QString streamKey;
	QString streamUrl;
	QString ingestionType;
	long timeStamp = 0;

	bool isNormalLive = true; //true is live now , false is scheduled live.
	bool isForKids = false;
	bool iskidsUserSelect = false;
	bool isCaptions = false;

	PLSYoutubeStart startData;

	//schedule parameter
	QJsonObject livebroadcastAPIData;
	QJsonObject streamAPIData;
	QJsonObject statusData{};  //get in the video api
	QJsonObject snippetData{}; //get in the video api

	Latency latency = Latency::Normal;

	QString thumbnailUrl{};
	QPixmap pixMap;
};

struct PLSYoutubeThum {
	QPixmap pix;
	QString hashs;
};

class PLSPlatformYoutube : public PLSPlatformBase {
	Q_OBJECT

public:
	enum class PLSYoutubeApiType { Normal = 0, StartLive = 1, Update = 2, Rehearsal = 3 };
	enum UpdateValue { None = 0x00, Thum = 0x01, Category = 0x01 << 1, Kids = 0x01 << 2 };
	Q_DECLARE_FLAGS(UpdateValues, UpdateValue)

	PLSPlatformYoutube();

	PLSServiceType getServiceType() const override;

	static void showAutoStartFalseAlertIfNeeded();

	void liveInfoIsShowing();
	void reInitLiveInfo();
	void resetLiveInfoAfterRehearsal();
	QString getSelectID() const;

	std::vector<QString> getPrivacyDatas() const;
	std::vector<QString> getPrivacyEnglishDatas() const;
	const std::vector<PLSYoutubeCategory> &getCategoryDatas() const;
	const std::vector<PLSYoutubeLiveinfoData> &getScheduleDatas() const;
	const PLSYoutubeLiveinfoData &getNormalLiveData() const;

	const PLSYoutubeLiveinfoData &getTempSelectData();
	PLSYoutubeLiveinfoData &getTempSelectDataRef();
	const PLSYoutubeLiveinfoData &getSelectData() const;
	PLSYoutubeLiveinfoData::IngestionType getSettingIngestionType() const;

	void updateScheduleListAndSort();

	bool isModifiedWithNewData(int categoryIndex, int privacyIndex, bool isKidSelect, bool isNotKidSelect, PLSYoutubeLiveinfoData *uiData);

	void saveSettings(const std::function<void(bool)> &onNext, bool isNeedUpdate, const PLSYoutubeLiveinfoData &uiData, const QObject *receiver);
	void requestUploadImage(const QPixmap &pixmap, const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestCurrentSelectData(const std::function<void(bool)> &onNext, const QWidget *widget);
	void dealCurrentSelectDataSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QWidget *widget);
	void requestCategoryID(const std::function<void(bool)> &onNext, const QString &searchID, const QObject *widget, bool isShowAlert = true);
	void dealCategoriesRequestDatas(const std::function<void(bool)> &onNext, const QString &searchID, const QByteArray &data, bool isShowAlert);
	void requestCategoryList(const std::function<void(bool)> &onNext, const QWidget *widget);
	void dealCategoriesSucceed(const QByteArray &data);
	void requestScheduleListByGuidePage(const std::function<void(bool)> &onNext, const QObject *widget);
	void dealScheduleListGuidePageSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);

	void requestScheduleList(const std::function<void(bool)> &onNext, const QObject *widget, bool isShowAlert = true);
	void dealScheduleListSucceed(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *widget, bool isShowAlert);

	void requestStartToInsertLiveBroadcasts(const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestStartToInsertLiveStreams(const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestStartToBindTwo(const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestUnBindStream(const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestStartTest();

	PLSPlatformYoutube &setTempSelectID(const QString value)
	{
		m_bTempSelectID = value;
		return *this;
	}
	QString getTempSelectID() const { return m_bTempSelectID; }

	PLSYoutubeLiveinfoData &getTempNormalData() { return m_tempNormalData; };
	PLSYoutubeLiveinfoData &getTrySaveDataData() { return m_trySaveData; };

	bool isSendChatToMqtt() const override { return true; }

	QJsonObject getLiveStartParams() override;
	QJsonObject getWebChatParams() override;

	QString getShareUrl() override;
	QString getShareUrlEnc() override;

	bool isPrivateStatus() const;
	bool isKidsLiving() const;
	void downloadThumImage(const std::function<void()> &onNext, const QString &url, const QObject *receiver, bool notShowThisPix = false);

	void dealDownloadImageCallBack(bool ok, const QString &imagePath, bool notShowThisPix);

	//the self stopped, but the other platform is still living
	bool isSelfStopping() const { return m_isStopping; }

	bool getIsRehearsal() const { return m_isRehearsal; }
	void setIsRehearsal(bool value);

	const QString &getFailedErr() const { return m_startFailedStr; }
	void setFailedErr(const QString &failedStr) { m_startFailedStr = failedStr; }

	bool showAlertPreAction();
	PLSErrorHandler::ExtraData getErrorExtraData(const QString &urlEn, const QString &urlKr = {});
	void showAlert(const PLSErrorHandler::NetworkData &netData, const QString &customErrName, const QString &logFrom, const QString &errorReason = {});
	void showAlertByCustName(const QString &customErrName, const QString &logFrom, const QString &errorReason = {});
	void showAlertByPrismCode(PLSErrorHandler::ErrCode prismCode, const QString &customErrName, const QString &logFrom, const QString &errorReason = {});
	void showAlertPostAction(const PLSErrorHandler::RetData &retData, const QString &errorReason = {});

public slots:
	void refreshTokenSucceed();
	void updateScheduleList() override;

protected:
	void convertScheduleListToMapList() override;

signals:
	void onGetTitleDescription();
	void selectIDChanged();
	void privateChangedToOther();
	void closeDialogByExpired();
	void toShowLoading(bool isShowLoading);
	void receiveLiveStop();
	void receiveVideoId(bool isNewCreate, QString sVideoId);

private:
	int m_idxCategory = 0;
	int m_idxPublic = 0;
	std::vector<QString> m_vecLocalPrivacy;
	std::vector<QString> m_vecEnglishPrivacy;
	std::vector<PLSYoutubeCategory> m_vecCategories;
	std::vector<PLSYoutubeLiveinfoData> m_vecSchedules;
	std::vector<PLSYoutubeLiveinfoData> m_vecGuideSchedules;

	//normal live data
	PLSYoutubeLiveinfoData m_normalData;

	PLSYoutubeLiveinfoData m_tempNormalData;
	//finish selected data when click ok
	PLSYoutubeLiveinfoData m_selectData;

	//temp data
	QString m_bTempSelectID;

	PLSYoutubeLiveinfoData m_trySaveData;

	QTimer *m_statusTimer;

	qint64 m_iContext = 0;

	QMap<QString, PLSYoutubeThum> m_thumMaps;

	bool m_isUploadedImage = false;

	//the self stopped, but the other platform is still living
	bool m_isStopping{false};

	QString m_healthStatus{};
	int m_ignoreNoDataCount = 2;
	int m_requestStatusCount = 0;
	PLSYoutubeLiveinfoData::IngestionType m_ingestionType = PLSYoutubeLiveinfoData::IngestionType::Rtmps;

	QString getShareUrl(bool isLiving, bool isEnc = false) const;
	void onPrepareLive(bool value) override;
	void onAllPrepareLive(bool value) override;
	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;
	void createNewNormalData();

	void requestLiveStreamKey(const std::function<void(bool)> &onNext, const QObject *receiver);
	void dealStreamKeySucceed(const QJsonDocument &doc, const std::function<void(bool)> &onNext);
	void requestUpdateVideoData(const std::function<void(bool)> &onNext, const PLSYoutubeLiveinfoData &infoData, const QObject *receiver, UpdateValues requestValues = UpdateValue::None);
	void requestStatisticsInfo() const;
	void requestLiveBroadcastsUpdate(const PLSYoutubeStart &startData, const std::function<void(bool)> &onNext);

	void dealLiveBroadcastsUpdateSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);

	void forceToRefreshToken(const std::function<void(bool)> &onNext);

	void setSelectData(PLSYoutubeLiveinfoData data);
	void requestStopLive(const std::function<void()> &onNext);
	void requestLiveBroadcastStatus();
	void requestLiveStreamStatus(bool isToCheckHealth);
	void dealLiveStreamStatusSucceed(const QByteArray &data, bool isToCheckHealth);
	void checkLiveStatus(const QString &lifeCycleStatus);
	bool getIsContainSameStream() const;
	void checkDuplicateStreamKey(const std::function<void(bool)> &onNext, const QObject *receiver);
	void checkIngestionTypeNext(const std::function<void(bool)> &onNext, const QObject *receiver);

	void saveTheScheduleSetting(const std::function<void(bool)> &onNext, bool isNeedUpdate, const QObject *receiver);
	bool isNeedUpdateLatency(const PLSYoutubeLiveinfoData &tryData) const;
	bool isNeedCallUpdateBroadcastAPIWhenSchedule(const PLSYoutubeLiveinfoData &tryData) const;
	bool isNeedCallUpdateVideoAPIWhenSchedule(const PLSYoutubeLiveinfoData &tryData, UpdateValues &requestValues) const;

	bool isValidDownloadUrl(const QString &url);
	void showFailedAPIToast() const;

	void rehearsalSwitchToLive();

	//is live status to ready and is called start test api.
	bool m_isCallTested = false;
	std::atomic_bool m_isRehearsal = false;
	PLSYoutubeStart m_rehearsalSavedData;
	bool m_isIgnoreAlert = false;
	std::atomic_bool m_isRehearsalToLived = false;

	void updateNewStreamStatus(const QString &status);

	void updateDashboardStatisticsUI() const;
	void updateSettingIngestionType();

	QString getStreamUrlFromJson(const QJsonObject &obj);
	QString m_startFailedStr{};

	mutable std::mutex m_channelScheduleMutex;

	UpdateValues m_failedValues = UpdateValue::None;
};
