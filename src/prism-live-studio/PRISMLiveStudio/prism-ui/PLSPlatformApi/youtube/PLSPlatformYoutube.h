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
#include "PLSPlatformBase.hpp"

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

enum class PLSYoutubeLatency { Low = 0, Normal = 1, UltraLow = 2 };

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
public:
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
	QJsonObject contentDetails;
	QJsonObject streamAPIData;
	QJsonObject statusData{};  //get in the video api
	QJsonObject snippetData{}; //get in the video api

	PLSYoutubeLatency latency = PLSYoutubeLatency::Low;

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
	enum class IngestionType { Auto = 0, Rtmps = 1, Hls = 2 };

	PLSPlatformYoutube();

	PLSServiceType getServiceType() const override;

	static void showAutoStartFalseAlertIfNeeded();

	void liveInfoisShowing();
	void reInitLiveInfo();
	void resetLiveInfoAfterRehearsal();
	QString getSelectID() const;

	std::vector<QString> getPrivacyDatas() const;
	std::vector<QString> getPrivacyEnglishDatas() const;
	const std::vector<PLSYoutubeCategory> &getCategoryDatas() const;
	const std::vector<PLSYoutubeLiveinfoData> &getScheduleDatas() const;
	const PLSYoutubeLiveinfoData &getNomalLiveData() const;

	const PLSYoutubeLiveinfoData &getTempSelectData();
	PLSYoutubeLiveinfoData &getTempSelectDataRef();
	const PLSYoutubeLiveinfoData &getSelectData() const;
	PLSPlatformYoutube::IngestionType getSettingIngestionType() const;

	void updateScheduleListAndSort();

	bool isModifiedWithNewData(int categotyIndex, int privacyIndex, bool isKidSelect, bool isNotKidSelect, PLSYoutubeLiveinfoData *uiData);

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

	PLSYoutubeLiveinfoData &getTempNormalData() { return m_tempNoramlData; };
	PLSYoutubeLiveinfoData &getTrySaveDataData() { return m_trySaveData; };

	bool isSendChatToMqtt() const override { return true; }
	bool isShownAlert() const { return m_isShownAlert; }
	void setIsShownAlert(bool isShownAlert) { m_isShownAlert = isShownAlert; }

	QJsonObject getLiveStartParams() override;
	QJsonObject getWebChatParams() override;

	QString getShareUrl() override;
	QString getShareUrlEnc() override;

	bool isPrivateStatus() const;
	bool isKidsLiving() const;
	void setupApiFailedWithCode(PLSPlatformApiResult result);
	void downloadThumImage(const std::function<void()> &onNext, const QString &url, const QObject *reciver, bool notShowThisPix = false);

	void dealDownloadImageCallBack(bool ok, const QString &imagePath, bool notShowThisPix);

	//the self stopped, but the other platform is still living
	bool isSelfStopping() const { return m_isStopping; }

	bool getIsRehearsal() const { return m_isRehearsal; }
	void setIsRehearsal(bool value);

	const QString &getFailedErr() const { return m_startFailedStr; }
	void setFailedErr(const QString &failedStr) { m_startFailedStr = failedStr; }
	void setlastRequestAPI(const QString &apiName) { m_lastRequestAPI = apiName; }

public slots:
	void refreshTokenSucceed() const;
	void updateScheduleList() override;

protected:
	void convertScheduleListToMapList() override;

signals:
	void onGetTitleDescription();
	void selectIDChanged();
	void privateChangedToOther();
	void kidsChangedToOther();
	void closeDialogByExpired();
	void toShowLoading(bool isShowLoading);
	void receiveLiveStop();

private:
	int m_idxCategory = 0;
	int m_idxPublic = 0;
	std::vector<QString> m_vecLocalPrivacy;
	std::vector<QString> m_vecEnglishPrivacy;
	std::vector<PLSYoutubeCategory> m_vecCategorys;
	std::vector<PLSYoutubeLiveinfoData> m_vecSchedules;
	std::vector<PLSYoutubeLiveinfoData> m_vecGuideSchedules;

	//normal live data
	PLSYoutubeLiveinfoData m_noramlData;

	PLSYoutubeLiveinfoData m_tempNoramlData;
	//finish selected data when click ok
	PLSYoutubeLiveinfoData m_selectData;

	//temp data
	QString m_bTempSelectID;

	PLSYoutubeLiveinfoData m_trySaveData;

	QTimer *m_statusTimer;

	qint64 m_iContext = 0;
	bool m_isShownAlert = false;

	QMap<QString, PLSYoutubeThum> m_thumMaps;

	bool m_isUploadedImage = false;

	//the self stopped, but the other platform is still living
	bool m_isStopping{false};

	QString m_healthStatus{};
	int m_ignoreNoDataCount = 2;
	int m_requestStatusCount = 0;
	PLSPlatformYoutube::IngestionType m_ingestionType = PLSPlatformYoutube::IngestionType::Auto;

	QString getShareUrl(bool isLiving, bool isEnc = false) const;
	void onPrepareLive(bool value) override;
	void onAllPrepareLive(bool value) override;
	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;
	void createNewNormalData();

	void requestLiveStreamKey(const std::function<void(bool)> &onNext, const QObject *receiver);
	void dealStreamKeySucceed(const QJsonDocument &doc, const std::function<void(bool)> &onNext);
	void requestUpdateVideoData(const std::function<void(bool)> &onNext, const PLSYoutubeLiveinfoData &infoData, const QObject *receiver);
	void requestStatisticsInfo() const;
	void requestLiveBroadcastsUpdate(const PLSYoutubeStart &startData, const std::function<void(bool)> &onNext);

	void dealLiveBroadcastsUpdateSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);

	void forceToRefreshToken(const std::function<void(bool)> &onNext);
	PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, PLSYoutubeApiType apiType = PLSYoutubeApiType::Normal) const;

	PLSPlatformApiResult dealFailedCase400(const QString &errorReason) const;
	PLSPlatformApiResult dealFailedCase403(const QString &errorReason) const;
	PLSPlatformApiResult dealFailedCase404(const QString &errorReason) const;

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

	bool isValidDownloadUrl(const QString &url);

	void rehearsalSwitchToLive();

	//is live status to ready and is called start test api.
	bool m_isCallTested = false;
	std::atomic_bool m_isRehearsal = false;
	PLSYoutubeStart m_rehearsalSaveedData;
	bool m_isIgnoreAlert = false;
	std::atomic_bool m_isRehearsalToLived = false;

	void updateNewStreamStatus(const QString &status);

	void updateDashboardStatisticsUI() const;
	void updateSettingIngestionType();

	QString getStreamUrlFromJson(const QJsonObject &obj);
	QString m_startFailedStr{};
	QString m_lastRequestAPI{};
};
