#pragma once

#include <functional>
#include <QJsonDocument>
#include <QTimer>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSScheduleComboxMenu.h"
#include <QReadWriteLock>

constexpr auto MODULE_PLATFORM_NAVER_SHOPPING_LIVE = "Platform/NaverShoppingLIVE";
constexpr auto RELEASE_LEVEL_REAL = "REAL";
constexpr auto RELEASE_LEVEL_REHEARSAL = "TEST";
constexpr auto LIVEINFO_GET_SCHEDULE_LIST = "LIVEINFO_GET_SCHEDULE_LIST";
constexpr auto NOTICE_GET_SCHEDULE_LIST = "NOTICE_GET_SCHEDULE_LIST";
constexpr auto LAUNCHER_GET_SCHEDULE_LIST = "LAUNCHER_GET_SCHEDULE_LIST";
constexpr auto PLANNING_LIVING = "PLANNING";

enum class NaverShoppingAccountType { NaverShoppingSelective = 1, NaverShoppingSmartStore };

class PLSPlatformNaverShoppingLIVE : public PLSPlatformBase {
	Q_OBJECT
public:
	using Product = PLSNaverShoppingLIVEAPI::ProductInfo;

	PLSPlatformNaverShoppingLIVE();
	~PLSPlatformNaverShoppingLIVE() override = default;

	PLSServiceType getServiceType() const override;
	void onPrepareLive(bool value) override;
	void onAlLiveStarted(bool) override;
	void onAllPrepareLive(bool) override;
	void onPrepareFinish() override;
	void onLiveEnded() override;
	void onActive() override;
	QString getShareUrl() override;
	QJsonObject getWebChatParams() override;
	bool isSendChatToMqtt() const override;
	QJsonObject getLiveStartParams() override;
	QString getServiceLiveLink() override;
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	void getUserInfo(const QString channelName, const QVariantMap srcInfo, const UpdateCallback &finishedCall, bool isFirstUpdate);
	QVariantMap getUserInfoFinished(const QVariantMap &srcInfo, PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo, const QString &channelName);
	QString getSubChannelId() const;
	QString getSubChannelName() const;
	bool isRehearsal() const;
	bool isPlanningLive() const;
	NaverShoppingAccountType getAccountType() const;
	void clearLiveInfo();
	QString getAccessToken() const;
	void setAccessToken(const QString &accessToken) const;
	const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &getUserInfo() const;
	const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &getLivingInfo() const;
	const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &getPrepareLiveInfo() const;
	const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &getScheduleRehearsaPrepareLiveInfo() const;
	PLSNaverShoppingLIVEAPI::ScheduleInfo getSelectedScheduleInfo(const QString &item_id) const;

	void setUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo);
	void setLivingInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo);
	void setPrepareInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo);
	void saveCurrentScheduleRehearsalPrepareInfo();
	void setStreamUrlAndStreamKey();
	void createLiving(const std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> &callback, const QObject *receiver,
			  const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void getLivingInfo(bool livePolling, const std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> &callback,
			   const QObject *receiver, const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void updateLivingRequest(const std::function<void(PLSAPINaverShoppingType apiType)> &callback, const QString &liveId, const QObject *receiver,
				 const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void updateScheduleRequest(const std::function<void(PLSAPINaverShoppingType apiType)> &callback, const QString &scheduleId, const QObject *receiver,
				   const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void downloadScheduleListImage(const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int page, int totalCount, const QObject *receiver,
				       const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void getScheduleList(const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int currentPage, uint64_t flag, const QString &type, QObject *receiver,
			     const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid = nullptr);
	void getSchduleListRequestSuccess(const uint64_t &flag, PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::GetScheduleListCallback &callback, int page, int totalCount,
					  const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int currentPage, const QString &type, QObject *receiver,
					  const PLSNaverShoppingLIVEAPI::ReceiverIsValid &receiverIsValid);
	bool isClickConfirmUseTerm() const;
	void loginFinishedPopupAlert();
	bool checkNaverShoppingTermOfAgree(bool isGolive = false) const;
	void checkNaverShoopingNotes(bool isGolive = false) const;
	void checkSupportResolution() const;
	bool checkGoLiveShoppingResolution(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo) const;
	bool isPortraitSupportResolution() const;
	bool showResolutionAlertView(const QString &message, bool errorMsg = false) const;
	void checkPushNotification(const std::function<void()> &onNext);
	void setShareLink(const QString &sharelink) const;
	bool getScalePixmapPath(QString &scaleImagePath, const QString &originPath);
	void setScalePixmapPath(const QString &scaleImagePath, const QString &originPath);
	void addScaleImageThread(const QString &originPath);
	bool isAddScaleImageThread(const QString &originPath) const;
	QString getScaleImagePath(const QString &originPath) const;
	void handleCommonApiType(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	void handleCommonAlert(PLSAPINaverShoppingType apiType, const QString &content, const ApiPropertyMap &apiPropertyMap, bool errorMessage = false) const;
	void handleNetworkError(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap);
	void handleInvalidToken(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap);
	const QString &getSoftwareUUid() const;
	void liveNoticeScheduleListSuccess(const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList);
	bool isHighResolutionSlvByPrepareInfo() const;
	void updateScheduleList() override;

public slots:
	void onCheckStatus();

protected:
	void convertScheduleListToMapList() override;

signals:
	void showLiveinfoLoading();
	void closeDialogByExpired();
	void hiddenLiveinfoLoading();

private slots:
	void onShowScheLiveNotice();

private:
	QString getOutputResolution() const;
	void loadUserInfo(PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo) const;
	void saveUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo) const;
	void showScheLiveNotice(const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo);
	void requestNotifyApi(const std::function<void()> &onNext);
	bool isModified(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &srcInfo, const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &destInfo) const;
	bool isInvalidRemoteLiving(const QString &liveStatus, const QString &displayType) const;
	void showGoLiveResolutionInvalidAlert() const;
	bool isHighResolutionSLV(const QString &scheduleId) const;

	PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo m_userInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo m_livingInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_prepareLiveInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_scheduleRehearsalprepareLiveInfo;
	QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> m_scheduleList;
	QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> m_wizardScheduleList;
	bool m_isMustStop{false};
	QMap<QString, QString> scaleImagePixmapCache;
	QReadWriteLock downloadImagePixmapCacheRWLock{QReadWriteLock::Recursive};
	QStringList m_imagePaths;
	bool networkErrorPopupShowing{false};
	bool isScheLiveNoticeShown{false};
	QTimer *checkStatusTimer = nullptr;
	bool m_callCreateLiveSuccess = false;
	bool m_naverShoppingTermAndNotesChecked = false;
	QString m_softwareUUid;
	QMap<QString, uint64_t> m_duplicateFlagMap;
	QMap<QString, QList<PLSNaverShoppingLIVEAPI::ScheduleInfo>> m_duplicateListMap;
};
