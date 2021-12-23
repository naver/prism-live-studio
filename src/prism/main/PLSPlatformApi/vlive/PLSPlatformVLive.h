#pragma once
#include <qobject.h>
#include <vector>
#include "..\PLSPlatformBase.hpp"
#include "PLSChannelDataHandler.h"
#include "PLSHttpApi/PLSHttpHelper.h"
#include "pls-app.hpp"
#include "qmap.h"

using namespace std;

struct PLSImageStatic {
	static PLSImageStatic *instance()
	{
		static PLSImageStatic _instance;
		return &_instance;
	};
	QMap<QString, QString> profileUrlMap;
};

struct PLSVliveStatic {
	static PLSVliveStatic *instance();
	QString gcc = "KR";
	QString lang = "ko";
	QString locale = "ko_KR";
	bool isLoaded = false;

	QMap<QString, QString> countries;
	QString convertToFullName(const QString &shortName);
	QMap<QString, bool> showNoticeMap;
};

struct PLSVLiveBoardData {
public:
	PLSVLiveBoardData(){};
	PLSVLiveBoardData(const QJsonObject &data, const QString &groupTitle = "", bool isGroup = false);
	bool isGroup = false;
	int boardId = 0;
	QString title;
	QString groupTitle;
	QString boardType;
	bool payRequired = false;
	bool expose = true;
	QString channelCode;
	QJsonArray includedCountries;
	QJsonArray excludedCountries;
	QString readAllowedLabel; //this is get by detail api.
};

struct PLSVLiveProfileData {
public:
	PLSVLiveProfileData(){};
	PLSVLiveProfileData(const QJsonObject &data);
	QString memberId;
	QString channelCode;
	QString nickname;
	bool joined = false;
	QString profileImageUrl;
	QString memberType;
	QString officialProfileType;
	bool hasMultiProfiles = false;
	QString officialName;
	QVariantMap getMapData() const;

	bool operator<(const PLSVLiveProfileData &right) const;
};
struct PLSVLiveLiveinfoData {
public:
	PLSVLiveLiveinfoData();
	PLSVLiveLiveinfoData(const QJsonObject &data);

	QString boundStreamId;
	QString _id;
	QString startVideoSeq;
	QString title;
	//QString startTimeOrigin;
	long startTimeStamp = 0;
	QString startTimeUTC;
	QString startTimeShort; //show in popup button right label

	//QString endTimeOrigin;
	long endTimeStamp = 0;
	QString streamKey;
	QString streamUrl;

	//QString thumLocalPath;
	QString thumRemoteUrl;
	QPixmap pixMap;

	QString status;

	QString channelSeq;
	QString pitureUrl;
	bool isNormalLive = true; //true is live now , false is scheduled live.
	bool isPlus = false;
	bool isNewLive = true; //vlive true:a new schedule    False：which is broadcasted, to continue live

	PLSVLiveBoardData board;
	PLSVLiveProfileData profile;
};

class PLSPlatformVLive : public PLSPlatformBase {
	Q_OBJECT

public:
	PLSPlatformVLive();
	~PLSPlatformVLive();
	PLSServiceType getServiceType() const override;

	void liveInfoisShowing();
	void reInitLiveInfo(bool isReset, bool isNeedKeepProfile = false);

	const QString getSelectID();
	const vector<PLSVLiveLiveinfoData> &getScheduleDatas();
	const PLSVLiveLiveinfoData getNomalLiveData();

	const PLSVLiveLiveinfoData &getTempSelectData();
	const PLSVLiveLiveinfoData getSelectData();

	PLSVLiveLiveinfoData &getTempNormalData() { return m_tempNoramlData; };

	const vector<PLSVLiveBoardData> &getBoardDatas() { return m_vecBoards; };
	const vector<PLSVLiveProfileData> &getProfileDatas() { return m_vecProfiles; };

	const PLSVLiveProfileData &getTempProfileData() { return m_bTempSelectProfile; };
	void setTempProfileData(PLSVLiveProfileData value) { m_bTempSelectProfile = value; }
	static PLSVLiveProfileData getChannelProfile(const QVariantMap &map);

	const PLSVLiveBoardData &getTempBoardData() { return m_bTempSelectBoard; };
	void setTempBoardData(PLSVLiveBoardData value) { m_bTempSelectBoard = value; }

	void setSelectDataByID(const QString &scheID);

	void removeExpiredSchedule(const vector<QString> &ids);

	void initVliveGcc(function<void()> onNext);
	void getCountryCodes(function<void()> onNext);
	void requestChannelInfo(const QVariantMap &srcInfo, UpdateCallback finishedCall);

	void requestScheduleList(function<void(bool)> onNext, QObject *widget, bool isNeedShowErrAlert = true, const QString &checkID = QString());
	void requestUpdateScheduleList(function<void(bool isSucceed, bool isDelete)> onNext, QObject *widget);
	void downloadThumImage(function<void()> onNext, QWidget *reciver);

	void requestProfileList(function<void(bool)> onNext, QObject *widget);
	void requestBoardList(function<void(bool)> onNext, QObject *widget);
	void requestBoardDetail(function<void(bool)> onNext, QObject *widget);

	bool isModifiedWithNewData(QString title, const QPixmap &localPixmap, PLSVLiveLiveinfoData *uiData);

	void saveSettings(function<void(bool)> onNext, PLSVLiveLiveinfoData uiData);

	void setThumPixmap(const QPixmap &pixMap);
	void setThumRemoteUrl(const QString &remoteUrl);

	PLSPlatformVLive &setTempSchedule(bool value)
	{
		m_bTempSchedule = value;
		return *this;
	}
	const bool getIsTempSchedule() const { return m_bTempSchedule; }
	const bool getIsRehearsal() const { return m_isRehearsal; }
	void setIsRehearsal(bool value) { m_isRehearsal = value; }

	PLSPlatformVLive &setTempSelectID(const QString value)
	{
		m_bTempSelectID = value;
		return *this;
	}
	const QString getTempSelectID() const { return m_bTempSelectID; }
	const QString getDefaultTitle(const QString &preStr);

	bool isSendChatToMqtt() const override { return true; }
	QString getServiceLiveLink() override;
	static void changeChannelDataWhenProfileSelect(QVariantMap &info, const PLSVLiveProfileData &profileData, const QObject *receiver);

	void onShowScheLiveNoticeIfNeeded(const QString &channelID);
signals:
	void closeDialogByExpired();
	void toShowLoading(bool isShowLoading);
	void profileIsInvalid();

private:
	vector<PLSVLiveLiveinfoData> m_vecSchedules;
	vector<PLSVLiveBoardData> m_vecBoards;
	vector<PLSVLiveProfileData> m_vecProfiles;

	//normal live data
	PLSVLiveLiveinfoData m_noramlData;
	PLSVLiveLiveinfoData m_tempNoramlData;

	//finish selected data when click ok;
	PLSVLiveLiveinfoData m_selectData;

	//temp data
	bool m_bTempSchedule = false;
	QString m_bTempSelectID;

	PLSVLiveProfileData m_bTempSelectProfile;
	PLSVLiveBoardData m_bTempSelectBoard;

	QTimer *m_statusTimer;
	bool m_isHaveShownScheduleNotice = false;
	bool m_isRehearsal = false;
	bool isStopedByRemote = false;
	QString m_showCustomMsg{};

private:
	QString getShareUrl() override;
	QString getShareUrlEnc() override;
	void onPrepareLive(bool value) override;
	void requestUploadImage(const QPixmap &pixmap, function<void(bool, const QString &)> onNext);
	void requestStatisticsInfo();
	void requestStartLive(function<void(bool)> onNext);
	void requestStartLiveToPostBoard(function<void(bool)> onNext);
	void requestStopLive(function<void(void)> onNext);

	PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error, QByteArray data, bool isStartApi = false);
	void setupApiFailedWithCode(PLSPlatformApiResult result);

	void onActive() override;
	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;
	QJsonObject getWebChatParams() override;

	void setSelectData(PLSVLiveLiveinfoData data, bool isNeedKeepProfile = true);

	PLSVLiveLiveinfoData &getTempSelectDataRef();

	void showScheLiveNotice(const PLSVLiveLiveinfoData &liveInfo);
};
