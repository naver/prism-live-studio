/*
* @file		PlatformYoutube.h
* @brief	All youtube relevant api is implemented in this file
* @author	ren.jinbo@navercorp.com
* @date		2020-xx-xx
*/

#pragma once

#include "..\PLSPlatformBase.hpp"
#include <qobject.h>
#include <vector>
#include "pls-app.hpp"
#include <functional>
#include <QDateTime>

using namespace std;

struct PLSYoutubeCategory {
	QString _id;
	QString title;
};

struct PLSScheduleData {
	QString boundStreamId;
	QString _id;
	QString title;
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
	bool enableMonitorStream;
	bool isNormalLive; //true is live now , false is scheduled live.
};

class PLSPlatformYoutube : public PLSPlatformBase {
	Q_OBJECT

public:
	PLSPlatformYoutube();

	PLSServiceType getServiceType() const override;

	const QString getSelectID();

	const vector<QString> getPrivacyDatas();
	const vector<QString> getPrivacyEnglishDatas();
	const vector<PLSYoutubeCategory> getCategoryDatas();
	const vector<PLSScheduleData> getScheduleDatas();
	const PLSScheduleData getNomalLiveDatas();

	const PLSScheduleData getTempSelectDatas();
	const PLSScheduleData getSelectDatas();

	bool isModifiedWithNewData(QString title, QString description, int categotyIndex, int privacyIndex, PLSScheduleData *uiData);

	void saveSettings(function<void(bool)> onNext, bool isNeedUpdate, PLSScheduleData uiData);

	void requestUserInfo(function<void(bool)> onNext, bool isStartLive);
	void requestVideo(function<void(bool)> onNext, bool isSchedule, QWidget *widget);
	void requestCategory(function<void(bool)> onNext);
	void requestSchedule(function<void(bool)> onNext, QWidget *widget);
	void requestStopLive(function<void()> onNext);

	void PLSPlatformYoutube::refreshYoutubeTokenBeforeRequest(
		bool forceRefresh, function<QNetworkReply *()> originNetworkReplay, const QObject *originReceiver,
		function<void(QNetworkReply *networkReplay, int code, QByteArray data, void *context)> originOnSucceed = nullptr,
		function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context)> originOnFailed = nullptr,
		function<void(QNetworkReply *networkReplay, int code, QByteArray data, QNetworkReply::NetworkError error, void *context)> originOnFinished = nullptr,
		void *const originContext = nullptr);

	PLSPlatformYoutube &setTempSchedule(bool value)
	{
		m_bTempSchedule = value;
		return *this;
	}
	const bool getIsTempSchedule() const { return m_bTempSchedule; }

	PLSPlatformYoutube &setTempSelectID(const QString value)
	{
		m_bTempSelectID = value;
		return *this;
	}
	const QString getTempSelectID() const { return m_bTempSelectID; }

signals:
	void onGetTitleDescription();
	void onGetCategorys();
	void selectIDChanged();
	void closeDialogByExpired();

private:
	int m_idxCategory;
	int m_idxPublic;
	vector<QString> m_vecLocalPrivacy;
	vector<QString> m_vecEnglishPrivacy;
	vector<PLSYoutubeCategory> m_vecCategorys;
	vector<PLSScheduleData> m_vecSchedules;

	//normal live data
	PLSScheduleData m_noramlData;

	//finish selected data when click ok;
	PLSScheduleData m_selectData;

	//temp data
	bool m_bTempSchedule;
	QString m_bTempSelectID;

	QTimer *m_statusTimer;

	int m_iContext = 0;

private:
	QString getShareUrl() override;
	void onPrepareLive(bool value) override;
	void requestLiveStreamKey(function<void(bool)> onNext);
	void requestUpdateData(function<void(bool)> onNext, PLSScheduleData data);
	void requestDisabelMonitor(function<void(bool)> onNext);
	void openScheduleStatus(function<void(bool)> onNext);
	void requestOpenScheduleStatus(function<void(bool)> onNext);
	void requestStatisticsInfo();
	void forceToRefreshToken(function<void(bool)> onNext);
	/**
 *  @param 1527840000
 *  @return  02/07/2020 11:40AM UTC+9
 */
	QString timeStampToUTCString(long timeStamp);
	/**
 *  @param 1527840000
 *  @return  02/07/2020 11:40
 */
	QString timeStampToShortString(long timeStamp);

	/**
 *  @param 2020-02-29T07:00:00.000Z
 *  @return 1527840000
 */
	long timeStringToStamp(QString time);

	PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data);
	void showApiRefreshError(PLSPlatformApiResult value);
	void showApiUpdateError(PLSPlatformApiResult value);

	void onLiveStopped() override;
	void onAlLiveStarted(bool) override;

	void setSelectData(PLSScheduleData data);
	void setupApiFailedWithCode(PLSPlatformApiResult result);
};
