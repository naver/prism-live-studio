#pragma once

#include <functional>
#include <QJsonDocument>
#include <QTimer>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSScheduleComboxMenu.h"

#define MODULE_PLATFORM_NAVER_SHOPPING_LIVE "Platform/NaverShoppingLIVE"

enum class NaverShoppingAccountType { NaverShoppingSelective = 1, NaverShoppingSmartStore };

class PLSPlatformNaverShoppingLIVE : public PLSPlatformBase {
	Q_OBJECT
public:
	PLSPlatformNaverShoppingLIVE();
	virtual ~PLSPlatformNaverShoppingLIVE();

public:
	virtual PLSServiceType getServiceType() const;
	virtual void onPrepareLive(bool value);
	virtual void onAlLiveStarted(bool) override;
	virtual void onAllPrepareLive(bool) override;
	virtual void onLiveEnded();
	virtual void onActive();
	virtual QString getShareUrl();
	virtual QJsonObject getWebChatParams();
	virtual bool isSendChatToMqtt() const;
	virtual QJsonObject getLiveStartParams();
	virtual void onInitDataChanged();
	QString getServiceLiveLink() override;

public:
	void getUserInfo(const QString channelName, const QVariantMap srcInfo, UpdateCallback finishedCall, bool isFirstUpdate);

public:
	bool isPrimary() const;
	QString getSubChannelId() const;
	QString getSubChannelName() const;
	bool isRehearsal() const;
	bool isPlanningLive() const;
	NaverShoppingAccountType getAccountType();
	void clearLiveInfo();
	QString getAccessToken() const;
	void setAccessToken(const QString &accessToken);
	const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &getUserInfo();
	const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &getLivingInfo();
	const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &getPrepareLiveInfo();
	const PLSNaverShoppingLIVEAPI::ScheduleInfo getSelectedScheduleInfo(const QString &item_id);
	const QList<PLSNaverShoppingLIVEAPI::LiveCategory> &getCategoryList();
	const QStringList &getFirstCategoryTitleList();
	const QStringList getSecondCategoryTitleList(const QString &title);
	const QString getFirstCategoryTitle(const QString &id);
	const PLSNaverShoppingLIVEAPI::LiveCategory getFirstLiveCategory(const QString &title);
	void setUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo);
	void setLivingInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo);
	void setPrepareInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo);
	void setStreamUrlAndStreamKey();
	void createLiving(std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> callback, QObject *receiver,
			  PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr);
	void getLivingInfo(bool livePolling, std::function<void(PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo)> callback, QObject *receiver,
			   PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr);
	void getCategoryList(PLSNaverShoppingLIVEAPI::GetCategoryListCallback callback, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr);
	void updateLiving(std::function<void(PLSAPINaverShoppingType apiType)> callback, const QString &id, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr);
	void getScheduleList(PLSNaverShoppingLIVEAPI::GetScheduleListCallback callback, QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr);
	bool isClickConfirmUseTerm();
	void loginFinishedPopupAlert();
	bool checkNaverShoppingTermOfAgree(bool isGolive = false);
	void checkNaverShoopingNotes(bool isGolive = false);
	bool checkSupportResolution(bool isGolive = false);
	bool isPopupResolution(bool isGolive = false);
	bool isPortraitSupportResolution();
	bool showResolutionAlertView(const QString &message);
	void checkPushNotification(function<void()> onNext);
	void setShareLink(const QString &sharelink);
	bool getScalePixmapPath(QString &scaleImagePath, const QString &originPath);
	void setScalePixmapPath(const QString &scaleImagePath, const QString &originPath);
	void addScaleImageThread(const QString &originPath);
	bool isAddScaleImageThread(const QString &originPath);
	QString getScaleImagePath(const QString &originPath);
	void handleCommonApiType(PLSAPINaverShoppingType apiType, const ApiPropertyMap &apiPropertyMap = ApiPropertyMap());
	const QString &getSoftwareUUid();
	void getCategoryName(QString &firstCategoryName, QString &secondCategoryName, const QString &displayName, QString &parentId);

public slots:
	void onCheckStatus();

signals:
	void showLiveinfoLoading();
	void closeDialogByExpired();
	void hiddenLiveinfoLoading();

private slots:
	void onShowScheLiveNotice();
	void endTimeCountdownCalculate();

private:
	QString getOutputResolution();
	QList<QPair<bool, QString>> getChannelImagesSync(const QList<QString> &urls);
	void loadUserInfo(PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo);
	void saveUserInfo(const PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo &userInfo);
	void showScheLiveNotice(const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo);
	void requestNotifyApi(function<void()> onNext);

private:
	bool primary = false;
	PLSNaverShoppingLIVEAPI::NaverShoppingUserInfo m_userInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo m_livingInfo;
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo m_prepareLiveInfo;
	QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> m_scheduleList;
	QList<PLSNaverShoppingLIVEAPI::LiveCategory> m_categoryList;
	QStringList m_firstCategoryTitleList;
	bool m_isMustStop{false};
	QMap<QString, QString> scaleImagePixmapCache;
	QReadWriteLock downloadImagePixmapCacheRWLock{QReadWriteLock::Recursive};
	QStringList m_imagePaths;
	bool networkErrorPopupShowing{false};
	QTimer *m_endCountdownTimer;
	PLSNShoppingLIVEEndTime m_endTimeData;
	bool isScheLiveNoticeShown{false};
	QTimer *checkStatusTimer = nullptr;
	bool m_callCreateLiveSuccess = false;
	bool m_naverShoppingTermAndNotesChecked = false;
	QString m_softwareUUid;
};
