#pragma once

#include <optional>
#include <functional>
#include <QJsonDocument>
#include <QTimer>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"

constexpr auto MODULE_PLATFORM_NAVERTV = "Platform/NaverTV";

class PLSPlatformNaverTV : public PLSPlatformBase {
	Q_OBJECT

public:
	PLSPlatformNaverTV() = default;
	~PLSPlatformNaverTV() override = default;

	PLSServiceType getServiceType() const override;
	void onPrepareLive(bool value) override;
	void onAllPrepareLive(bool value) override;
	void onLiveStarted() override;
	void onAlLiveStarted(bool value) override;
	void onLiveEnded() override;
	void onActive() override;
	QString getShareUrl() override;
	QJsonObject getWebChatParams() override;
	bool isSendChatToMqtt() const override;
	QJsonObject getLiveStartParams() override;
	void onInitDataChanged() override;
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	bool isMqttChatCanShow(const QJsonObject &) override;
	QMap<QString, QString> getDirectEndParams() override;

	struct Token {
		QString accessToken;
		QString refreshToken;
		QString tokenType;
		qint64 expiresIn = 0;

		Token() = default;
		explicit Token(const QJsonObject &);
		~Token() = default;

		Token(const Token &) = default;
		Token &operator=(const Token &) = default;
	};
	struct LiveInfo {
		bool isScheLive = false;
		bool isRehearsal = false;
		int liveNo = -1;
		int oliveId = -1;
		qint64 startDate = 0;
		qint64 endDate = 0;
		QString title;
		QString status;
		QString channelId;
		QString categoryCode;
		QString openYn;
		QString notice;
		QString thumbnailImageUrl;
		QString thumbnailImagePath;

		LiveInfo() = default;
		explicit LiveInfo(const QJsonObject &);
		~LiveInfo() = default;

		LiveInfo(const LiveInfo &) = default;
		LiveInfo &operator=(const LiveInfo &) = default;

		bool needUploadThumbnail() const;
	};
	struct Live {
		bool manager = false;
		int oliveId = -1;
		qint64 likeCount = 0;
		qint64 watchCount = 0;
		qint64 commentCount = 0;
		QString ticketId;
		QString objectId;
		QString templateId;
		QString groupId;
		QString idNo;
		QString pcLiveUrl;
		QString mobileLiveUrl;

		Live() = default;
		~Live() = default;
	};
	struct Url {
		QUrl url;
		QString maskingUrl;
		bool allowAbort = true;

		explicit Url(const QUrl &url_) : url(url_) {}
		explicit Url(const QUrl &url_, bool allowAbort_) : url(url_), allowAbort(allowAbort_) {}
		explicit Url(const QUrl &url_, const QString &maskingUrl_) : url(url_), maskingUrl(maskingUrl_) {}
		explicit Url(const QUrl &url_, const QString &maskingUrl_, bool allowAbort_) : url(url_), maskingUrl(maskingUrl_), allowAbort(allowAbort_) {}
		~Url() = default;
	};
	enum class ApiId { LiveOpen, Other };

	using CodeCallback = std::function<void(bool ok, QNetworkReply::NetworkError error, const QString &code)>;
	using TokenCallback = std::function<void(bool ok, QNetworkReply::NetworkError error, const QString &loginFailed)>;
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
	using LiveStatusCallback = std::function<void(bool ok, qint64 watchCount, qint64 likeCount, qint64 commentCount, bool closed)>;
	using CommentOptionsCallback = std::function<void(bool ok, int code)>;
	using UploadImageCallback = std::function<void(bool ok, const QString &imageUrl)>;
	using UpdateLiveInfoCallback = std::function<void(bool ok, int code)>;
	using ChannelEmblemCallback = std::function<void(const QList<QPair<bool, QString>> &images)>;

	// error code
	static constexpr int ERROR_AUTHEXCEPTION = 10001;
	static constexpr int ERROR_LIVENOTFOUND = 10002;
	static constexpr int ERROR_PERMITEXCEPTION = 10003;
	static constexpr int ERROR_LIVESTATUSEXCEPTION = 10006;
	static constexpr int ERROR_START30MINEXCEPTION = 10007;
	static constexpr int ERROR_ALREADYONAIR = 10008;
	static constexpr int ERROR_PAIDSPONSORSHIPINFO = 10010;
	static constexpr int ERROR_NETWORK_ERROR = -10001;
	static constexpr int ERROR_OTHERS = -10000;

	// request api
	void getChannelEmblem(const QList<QString> &urls, const QObject *receiver, const ChannelEmblemCallback &callback) const;

	void getAuth(const QString &channelName, const QVariantMap &srcInfo, const UpdateCallback &finishedCall, bool isFirstUpdate);
	void processGetChannelListResult(const QString &channelName, const QVariantMap &srcInfo, const UpdateCallback &finishedCall, const QJsonDocument &json);

	void getCode(const CodeCallback &callback, const QString &url, bool redirectUrl);
	void getToken(const TokenCallback &callback);
	void getToken(const QString &code, const TokenCallback &callback);
	void processGetTokenOk(bool ok, QNetworkReply::NetworkError error, const QByteArray &body, const TokenCallback &callback);
	void getAuth2(const QString &url, const char *log, const Auth2Callback &callback, const QVariantMap &urlQueries = QVariantMap()) const;
	void processGetAuth2Fail(const QNetworkReply *reply, const QByteArray &data, QNetworkReply::NetworkError error, const char *log, const Auth2Callback &callback) const;
	void getUserInfo(const UserInfoCallback &callback);

	void getScheLives(const LiveInfosCallback &callback, const QObject *receiver, bool popupNeedShow = true, bool popupGenericError = true, bool expiredNotify = true);
	void getScheLives(const LiveInfosCallbackEx &callback, const QObject *receiver, bool popupNeedShow = true, bool popupGenericError = true, bool expiredNotify = true);
	void getScheLives(int before, int after, const LiveInfosCallback &callback, const QObject *receiver, bool popupNeedShow = true, bool popupGenericError = true, bool expiredNotify = true);
	void getScheLives(int before, int after, const LiveInfosCallbackEx &callback, const QObject *receiver, bool popupNeedShow = true, bool popupGenericError = true, bool expiredNotify = true);
	void getScheLives(const QString &fromDate, const QString &toDate, const LiveInfosCallbackEx &callback, const QObject *receiver, bool popupNeedShow = true, bool popupGenericError = true,
			  bool expiredNotify = true);
	void checkScheLive(int oliveId, const CheckScheLiveCallback &callback);

	void getStreamInfo(const StreamInfoCallback &callback, const char *log = "Naver TV get stream key", bool popupGenericError = true);
	void checkStreamPublishing(const CheckStreamPublishingCallback &callback);

	void immediateStart(const QuickStartCallback &callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false, bool popupGenericError = true);
	void scheduleStart(const LiveOpenCallback &callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false);
	void scheduleStartWithCheck(const LiveOpenCallback &callback, bool isShareOnFacebook, bool isShareOnTwitter);

	void quickStart(const QuickStartCallback &callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false, bool popupGenericError = true);

	void liveOpen(const LiveOpenCallback &callback, bool isShareOnFacebook = false, bool isShareOnTwitter = false);
	void liveClose(const LiveCloseCallback &callback);
	void liveModify(const QString &title, const QString &thumbnailImageUrl, const LiveModifyCallback &callback);
	void liveStatus(const LiveStatusCallback &callback);

	void getCommentOptions(const CommentOptionsCallback &callback);

	void uploadImage(const QString &imageFilePath, const UploadImageCallback &callback) const;

	bool isPrimary() const;
	QString getSubChannelId() const;
	QString getSubChannelName() const;
	int getLiveId() const;
	bool isRehearsal() const;
	bool isKnownError(int code) const;

	const Token *getToken() const;

	LiveInfo *getLiveInfo();
	LiveInfo *getNewLiveInfo();
	void clearLiveInfo();

	Live *getLive() const;
	void clearLive();

	const LiveInfo *getSelectedLiveInfo() const;
	const LiveInfo *getScheLiveInfo(int scheLiveId) const;
	const LiveInfo *getImmediateLiveInfo() const;

	const QList<LiveInfo> &getScheLiveInfoList() const;
	bool isLiveInfoModified(int oliveId);
	bool isLiveInfoModified(int oliveId, const QString &title, const QString &thumbnailImagePath);
	void updateLiveInfo(const QList<LiveInfo> &scheLiveInfos, int oliveId, bool isRehearsal, const QString &title, const QString &thumbnailImagePath, const UpdateLiveInfoCallback &callback);
	void initImmediateLiveInfoTitle() const;

	void showScheLiveNotice(const LiveInfo &liveInfo);

	void updateScheduleList() override;

signals:
	void closeDialogByExpired();
	void apiRequestFailed(bool tokenExpired);

protected:
	void convertScheduleListToMapList() override;

private slots:
	void onCheckStatus();
	void onShowScheLiveNotice();

private:
	QString getChannelInfo(const QString &key, const QString &defaultValue = QString()) const;
	void getJson(const Url &url, const char *log, const std::function<void(const QJsonDocument &)> &ok, const std::function<void(bool expired, int code)> &fail, const QObject *receiver = nullptr,
		     ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(), bool expiredNotify = true, bool popupNeedShow = true, bool popupGenericError = true,
		     bool onlyFailLog = false);
	void getJson(const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok, const std::function<void(bool expired, int code)> &fail,
		     const QObject *receiver = nullptr, ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(), bool expiredNotify = true, bool popupNeedShow = true,
		     bool popupGenericError = true);
	void postJson(const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok, const std::function<void(bool expired, int code)> &fail,
		      const QObject *receiver = nullptr, ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(), bool expiredNotify = true, bool popupNeedShow = true,
		      bool popupGenericError = true);
	void getOrPostJson(pls::http::Method method, const Url &url, const QJsonObject &json, const char *log, const std::function<void(const QJsonDocument &)> &ok,
			   const std::function<void(bool expired, int code)> &fail, const QObject *receiver = nullptr, ApiId apiId = ApiId::Other, const QVariantMap &headers = QVariantMap(),
			   bool expiredNotify = true, bool popupNeedShow = true, bool popupGenericError = true);
	void processFailed(const char *log, const QJsonDocument &respJson, const std::function<void(bool expired, int code)> &fail, const QObject *receiver, QNetworkReply::NetworkError networkError,
			   int statusCode, bool expiredNotify, bool popupNeedShow, bool popupGenericError, ApiId apiId);
	int processError(bool &expired, const QObject *receiver, QNetworkReply::NetworkError networkError, int statusCode, int code, bool expiredNotify, bool popupNeedShow, bool popupGenericError,
			 ApiId apiId);
	void tokenExpired(bool expiredNotify, bool popupNeedShow);

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
