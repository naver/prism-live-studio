#ifndef PLSAPIVLive_H
#define PLSAPIVLive_H

#include <PLSHttpApi\PLSHttpHelper.h>
#include <QObject>

class PLSPlatformVLive;
enum class PLSPlatformApiResult;

class PLSAPIVLive : public QObject {
	Q_OBJECT
public:
	explicit PLSAPIVLive(QObject *parent = nullptr);

	using UploadImageCallback = std::function<void(PLSPlatformApiResult result, const QString &imageUrl)>;

	static void vliveRequestGccAndLanguage(const QObject *receiver, dataErrorFunction onFinish);
	static void vliveRequestCountryCodes(const QObject *receiver, dataErrorFunction onFinish);
	static void vliveRequestUsersInfoAndChannel(const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed);
	static void vliveRequestScheduleList(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context = nullptr);
	static void vliveRequestBoardList(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context);
	static void vliveRequestBoardDetail(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed);
	static void vliveRequestProfileList(const QString &channelUuid, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed, void *const context);
	static void vliveRequestStartLive(const QString &channelUuid, const QObject *receiver, dataErrorFunction onFinish);
	static void vliveRequestStartLiveToPostBoard(const QString &channelUuid, const QObject *receiver, dataErrorFunction onFinish);
	static void vliveRequestStatistics(const QObject *receiver, dataErrorFunction onFinish);
	static void vliveRequestStopLive(PLSPlatformVLive *vlive, const QObject *receiver, dataFunction onSucceed, dataErrorFunction onFailed);
	static void uploadImage(const QObject *receiver, const QString &imageFilePath, UploadImageCallback callback);
	static bool isVliveFanship();

	static QPair<bool, QString> downloadImageSync(const QObject *receive, const QString &url, const QString &prefix = "", bool ingoreCache = false);
	static void downloadImageAsync(const QObject *receiver, const QString &imageUrl, ImageCallback callback, void *const context = nullptr, const QString &prefix = "", bool ingoreCache = false);
	static QString getLocalImageFile(const QString &imageUrl, const QString &prefix = "");

	static void maskingUrlKeys(PLSNetworkReplyBuilder &builder, QNetworkReply *reply, const QStringList &keys);

private:
	enum class PLSNetMehod {
		PUT = 0,
		POST,
		GET,
	};

	static void addCommenQuery(PLSNetworkReplyBuilder &builder);
	static void addCommenCookie(PLSNetworkReplyBuilder &builder);
	static void addMacAddress(PLSNetworkReplyBuilder &builder);
	static void addUserAgent(PLSNetworkReplyBuilder &builder);
	static void addDebugMessage(PLSNetworkReplyBuilder &builder);
	static QString getVliveHost();
	static QString getStreamName();
};

#endif // PLSAPIVLive_H
