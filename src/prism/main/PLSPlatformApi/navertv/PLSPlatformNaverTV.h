#pragma once

#include <functional>
#include <QJsonDocument>
#include <QTimer>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"

#define MODULE_PLATFORM_NAVERTV "Platform/NaverTV"

class PLSPlatformNaverTV : public PLSPlatformBase {
	Q_OBJECT
public:
	PLSPlatformNaverTV();
	virtual ~PLSPlatformNaverTV();

public:
	virtual PLSServiceType getServiceType() const;
	virtual void onPrepareLive(bool value);
	virtual void onAllPrepareLive(bool value);
	virtual void onLiveStarted(bool value);
	virtual void onAlLiveStarted(bool value);
	virtual void onLiveEnded();
	virtual void onActive();
	virtual QString getShareUrl();
	virtual QJsonObject getWebChatParams();
	virtual bool isSendChatToMqtt() const;
	virtual QJsonObject getLiveStartParams();
	virtual void onInitDataChanged();
	virtual void onMqttStatus(PLSPlatformMqttStatus status);
	virtual bool isMqttChatCanShow(const QJsonObject &);
	QMap<QString, QString> getDirectEndParams() override;

public:
	struct Token {
		QString accessToken;
		QString refreshToken;
		QString tokenType;
		qint64 expiresIn = 0;

		Token();
		Token(const QJsonObject &);
		~Token();

		Token(const Token &);
		Token &operator=(const Token &);
	};
	struct LiveInfo {
		bool isScheLive;
		bool isRehearsal;
		int liveNo;
		int oliveId;
		qint64 startDate;
		qint64 endDate;
		QString title;
		QString status;
		QString channelId;
		QString categoryCode;
		QString openYn;
		QString notice;
		QString thumbnailImageUrl;
		QString thumbnailImagePath;

		LiveInfo();
		LiveInfo(const QJsonObject &);
		~LiveInfo();

		LiveInfo(const LiveInfo &);
		LiveInfo &operator=(const LiveInfo &);

		bool needUploadThumbnail() const;
	};
	struct Live {
		bool manager;
		// bool liveStarted;
		// bool allLiveStarted;
		int oliveId;
		qint64 likeCount;
		qint64 watchCount;
		qint64 commentCount;
		QString ticketId;
		QString objectId;
		QString templateId;
		QString groupId;
		QString idNo;
		QString pcLiveUrl;
		QString mobileLiveUrl;

		Live();
		~Live();
	};
	struct Url {
		QUrl url;
		QString maskingUrl;

		Url(const QUrl &url_) : url(url_), maskingUrl() {}
		Url(const QUrl &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		~Url() {}
	};
	enum class ApiId { LiveOpen, Other };

	using CodeCallback = std::function<void(bool ok, QNetworkReply::NetworkError error, const QString &code)>;
	using TokenCallback = std::function<void(bool ok, QNetworkReply::NetworkError error)>;
	using Auth2Callback = std::function<void(bool ok, QNetworkReply::NetworkError error, const QByteArray &body)>;
	using UserInfoCallback = std::function<void(bool ok)>;
	using LiveInfosCallback = std::function<void(bool ok, const QList<LiveInfo> &scheLiveInfos)>;
	using LiveInfosCallbackEx = std::function<void(bool ok, int code, const QList<LiveInfo> &scheLiveInfos)>;
	using CheckScheLiveCallback = std::function<void(bool ok, bool valid)>;
	using StreamInfoCallback = std::function<void(bool ok, bool publishing, int code)>;
	using CheckStreamPublishingCallback = std::function<void(bool ok, bool publishing, int code)>;
	using QuickStartCallback = std::function<void(bool ok, int code)>;
	using LiveOpenCallback = std::function<void(bool ok, int code)>;
	using LiveCloseCallback = std::function<void(bool ok)>;
	using LiveModifyCallback = std::function<void(bool ok)>;
	using LiveStatusCallback = std::function<void(bool ok, int watchCount, int likeCount, int commentCount, bool closed)>;
	using CommentOptionsCallback = std::function<void(bool ok, int code)>;
	using UploadImageCallback = std::function<void(bool ok, const QString &imageUrl)>;
	using UpdateLiveInfoCallback = std::function<void(bool ok, int code)>;
	using ReceiverIsValid = std::function<bool(QObject *)>;

	// error code
	static const int ERROR_AUTHEXCEPTION = 10001;
	static const int ERROR_LIVENOTFOUND = 10002;
	static const int ERROR_PERMITEXCEPTION = 10003;
	static const int ERROR_LIVESTATUSEXCEPTION = 10006;
	static const int ERROR_START30MINEXCEPTION = 10007;
	static const int ERROR_ALREADYONAIR = 10008;
	static const int ERROR_PAIDSPONSORSHIPINFO = 10010;
	static const int ERROR_OTHERS = -10000;
	static const int ERROR_NETWORK_ERROR = -10001;

	// request api
	QPair<bool, QString> getChannelEmblemSync(const QString &url);
	QList<QPair<bool, QString>> getChannelEmblemSync(const QList<QString> &urls);

	void getAuth(const QString &channelName, const QVariantMap &srcInfo, UpdateCallback finishedCall, bool isFirstUpdate);

	void getCode(CodeCallback callback, const QString &url, bool redirectUrl);
	void getToken(TokenCallback callback);
	void getToken(const QString &code, TokenCallback callback);
	void getAuth2(const QString &url, const char *log, Auth2Callback callback, const QVariantMap &urlQueries = QVariantMap());
	void getUserInfo(UserInfoCallback callback);

	void getScheLives(LiveInfosCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr, bool popupNeedShow = true, bool popupGenericError = true);
	void getScheLives(LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr, bool popupNeedShow = true, bool popupGenericError = true);
	void getScheLives(int before, int after, LiveInfosCallback callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr, bool popupNeedShow = true, bool popupGenericError = true);
	void getScheLives(int before, int after, LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr, bool popupNeedShow = true, bool popupGenericError = true);
	void getScheLives(const QString &fromDate, const QString &toDate, LiveInfosCallbackEx callback, QObject *receiver, ReceiverIsValid receiverIsValid = nullptr, bool popupNeedShow = true,
			  bool popupGenericError = true);
	void checkScheLive(int oliveId, CheckScheLiveCallback callback);

	void getStreamInfo(StreamInfoCallback callback, const char *log = "Naver TV get stream key", bool popupGenericError = true);
	void checkStreamPublishing(CheckStreamPublishingCallback callback);

	void immediateStart(QuickStartCallback callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false, bool popupGenericError = true);
	void scheduleStart(LiveOpenCallback callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false);
	void scheduleStartWithCheck(LiveOpenCallback callback, bool isShareOnFacebook, bool isShareOnTwitter);

	void quickStart(QuickStartCallback callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false, bool popupGenericError = true);

	void liveOpen(LiveOpenCallback callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false);
	void liveClose(LiveCloseCallback callback);
	void liveModify(const QString &title, const QString &thumbnailImageUrl, LiveModifyCallback callback);
	void liveStatus(LiveStatusCallback callback);

	void getCommentOptions(CommentOptionsCallback callback);

	void uploadImage(const QString &imageFilePath, UploadImageCallback callback);

public:
	bool isPrimary() const;
	QString getSubChannelId() const;
	QString getSubChannelName() const;
	int getLiveId() const;
	bool isRehearsal() const;
	bool isScheduleLive() const;
	// bool isLiveStarted() const;
	// bool isAllLiveStarted() const;
	bool isKnownError(int code) const;

	const Token *getToken() const;

	LiveInfo *getLiveInfo() const;
	LiveInfo *getNewLiveInfo() const;
	void clearLiveInfo();

	Live *getLive() const;
	void clearLive();

	const LiveInfo *getSelectedLiveInfo() const;
	const LiveInfo *getScheLiveInfo(int scheLiveId) const;
	const LiveInfo *getImmediateLiveInfo() const;

	const QList<LiveInfo> &getScheLiveInfoList() const;
	bool isLiveInfoModified(int oliveId) const;
	bool isLiveInfoModified(int oliveId, const QString &title, const QString &thumbnailImagePath) const;
	void updateLiveInfo(const QList<LiveInfo> &scheLiveInfos, int oliveId, bool isRehearsal, const QString &title, const QString &thumbnailImagePath, UpdateLiveInfoCallback callback);
	void initImmediateLiveInfoTitle() const;

	void showScheLiveNotice(const LiveInfo &liveInfo);

signals:
	void closeDialogByExpired();
	void apiRequestFailed(bool tokenExpired);

private slots:
	void onCheckStatus();
	void onShowScheLiveNotice();

private:
	QString getChannelInfo(const QString &key, const QString &defaultValue = QString()) const;
	void getJson(const Url &url, const char *log, std::function<void(const QJsonDocument &)> ok, std::function<void(bool expired, int code)> fail, QObject *receiver = nullptr,
		     ReceiverIsValid receiverIsValid = nullptr, ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(), bool expiredNotify = true, bool popupNeedShow = true,
		     bool popupGenericError = true);
	void postJson(const Url &url, const QJsonObject &json, const char *log, std::function<void(const QJsonDocument &)> ok, std::function<void(bool expired, int code)> fail,
		      QObject *receiver = nullptr, ReceiverIsValid receiverIsValid = nullptr, ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(), bool expiredNotify = true,
		      bool popupNeedShow = true, bool popupGenericError = true);
	void processFailed(const char *log, const QJsonDocument &respJson, std::function<void(bool expired, int code)> fail, QObject *receiver, ReceiverIsValid receiverIsValid,
			   QNetworkReply::NetworkError networkError, int statusCode, bool expiredNotify, bool popupNeedShow, bool popupGenericError, ApiId apiId);
	int processError(bool &expired, QObject *receiver, ReceiverIsValid receiverIsValid, QNetworkReply::NetworkError networkError, int statusCode, int code, bool expiredNotify, bool popupNeedShow,
			 bool popupGenericError, ApiId apiId);
	void tokenExpired(bool expiredNotify, bool popupNeedShow);

private:
	bool primary = false;

	Token token;

	QString nickName;
	QString headImageUrl;

	int selectedScheLiveInfoId = -1;
	mutable LiveInfo immediateLive;
	QList<LiveInfo> scheLiveList;

	bool isScheLiveNoticeShown = false;
	bool isLiveEndedByMqtt = false;
	mutable LiveInfo *liveInfo = nullptr;
	mutable Live *live = nullptr;
	int checkStreamPublishingRetryTimes = 0;
	QTimer *checkStreamPublishingTimer = nullptr;
	QTimer *checkStatusTimer = nullptr;
};
