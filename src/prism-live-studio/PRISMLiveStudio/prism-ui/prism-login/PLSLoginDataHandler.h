#ifndef PLSLAUNCHERDATAHANDLER_H
#define PLSLAUNCHERDATAHANDLER_H

#include <QObject>
#include "PLSCommonConst.h"
#include "PLSCommonFunc.h"
#include "cancel.hpp"
#include "PLSErrorHandler.h"
#include "libresource.h"

#define PLSLOGINDATAHANDLER PLSLoginDataHandler::instance()

enum class PLSUpdateDownloadState {
	PLSUpdateDownloadSuccess = 1, //
	PLSUpdateDownloadFailed,
	PLSUpdateDownloadProcess,
	PLSUpdateDownloadCancel
};

using downloadProgressCallback = std::function<void(qint64, qint64, PLSUpdateDownloadState)>;
using refreshPrismTokenCallback = std::function<void()>;

struct PLSStartData;

enum class AppUpdateResult {
	AppHasUpdate = 1,
	AppNoUpdate,
	AppFailed,
	AppHMacExceedTimeLimit,
};

struct PLSAppUpdateResult {
	bool m_isForceUpdate = false;
	AppUpdateResult m_updateResult = AppUpdateResult::AppFailed;
	QString m_newPrismVersion;
	QString m_AppInstallFileUrl;
	QString m_updateInfoUrl;
};

class PLSLoginDataHandler : public QObject {
	Q_OBJECT
public:
	static PLSLoginDataHandler *instance();
	QVariantMap getRequestApiDefaultHeader(bool hasGcc = true) const;
	QMap<QString, QString> getBrowserDefaultHeader() const;

	void getAppInitDataFromRemote(const std::function<void()> &callback); //gpop,init
	bool getPrismUserInfoFromRemote(const QList<QNetworkCookie> &cookies, const QString &requestUrl, pls::http::Method httpMethod, const QString &loginName, qint32 recentKind);
	void startDownloadNewPackage(const downloadProgressCallback &callback, const QString &installFileUrl, const QString &gcc);
	void stopDownloadNewPackage();
	void refreshPrismToken(const std::function<void(bool)> &callback = nullptr);

	const QString &prismSession() const { return m_prismSession; }
	void prismSession(const QString &session) { m_prismSession = session; }

	AppUpdateResult getUpdateResult();
	QString getInstallFileUrl();
	bool isForcePrismAppUpdate();
	QString getUpdateVersion();
	QString getUpdateInfoUrl();

	QString getSnsCallbackUrl(const QString &snsName) const;
	void getPrismThumbnail(const std::function<void()> &callback);
	QPixmap getCurrentThumbnail() const;
	std::optional<pls::http::Request> getNCPThumbnail();
	std::optional<pls::http::Request> getUserThumbnail(const QString &url = {}, int redirectCount = 3);
	std::optional<pls::http::Request> getServiceTermsHTML(bool isSync = false);
	QString getServiceTermsHtml();
	static bool isFacebookThumbnailHost(const QString &host);
	bool isNeedLogin() const;
	bool isTokenVaild() const;
	bool isExistUserInfo() const;
	QString getInstallPackagePath() const;

	void getGoogleCookie(const QString &token, const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &loginName, qint32 recentKind);
	template<typename Callback> void google_regeist_handler(const QVariant &reply, const QString &token, const Callback &callback, const QString &loginName, qint32 recentKind);
	void pls_google_user_info(const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &redirect_uri, const QString &code, const QString &loginName, qint32 recentKind);
	void savePrismUserInfo(const QJsonObject &userInfo, const QVariant &neo_sesCookies, bool isNewUser, const QString &loginName, qint32 recentKind);
	void resetLoginResultCommitState();
	bool hasCommittedLoginResult() const { return m_loginResultCommitted; }

	void getNCPServiceId(const QString &serviceName, const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback,
			     const QString &loginName = QString());
	void getNCPAuthUrl(const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback, const QString &loginName = QString());
	bool getNCPAccessToken(const QString &url, const QString &loginName = QString());
	const QJsonObject &getNCB2BServiceConnfigRes() const;
	QString getNCB2BServiceLogo() const;
	QString getNCB2BServiceNBLogo() const;
	QString getNCB2BServiceColorLogo() const;
	QString getNCB2BServiceWhiteLogo() const;
	QString getNCB2BServiceWatermark() const;
	QString getNCB2BServiceOutro() const;
	void initCustomChannelObj();
	QJsonObject &getCustomChannelObj();

	QString getVersionFromFileUrl(const QString &updateUrl);

	void downloadNCB2BServiceRes(bool bRetry = false);
	QString getNCB2BLogoUrl();
	void reDownloadWaterMark();
	bool isNeedShowB2BServiceAlert();
	void getNCB2BServiceResFromRemote(const std::function<void(const QJsonObject &data)> &successCallback,
					  const std::function<void(const QJsonObject &data, const PLSErrorHandler::RetData &retData)> &failCallback, QObject *receiver);
	QImage scaleAndCrop(const QImage &original, const QSize &originTargetSize);
	void setSnsCode(const QString &snsCode) { m_snsCode = snsCode; }
	QUrl getSnsAuthUrl(const QString &url, const QString &clientId, const QString &secret, const QString &redirectUri, const QString &scope, const QString &auth_type,
			   const QString &loginName = QString());
	bool getSNSAccessToken(const QString &url, const QString &clientId, const QString &secret, const QString redirectUri, const QString &code, const QString &loginName = QString());
	bool isSupportAutoChannelLogins();
signals:
	void updateNCB2BIcon();
	void updatePrismLogo();

private:
	explicit PLSLoginDataHandler(QObject *parent = nullptr);
	~PLSLoginDataHandler() override = default;
	static QString getUpdateInfoUrl(const QJsonObject &updateInfoUrlList);
	static QString getFileNameFromUlr(const QString &fileUrl);
	bool saveThumbnail(const QPixmap &pixmap, const QString &filePath) const;
	QPixmap loadThumbnail(const QString &filePath) const;
	void initApiSuccessHandle(const QJsonDocument &doc);
	void updateApiSuccessHandle(const QJsonDocument &doc);
	void getCookieSuccessHandle(const std::function<void(bool ok, const QJsonObject &)> &callback, const QVariant &cookie, int statusCode, const QString &token, const QJsonObject &docObj,
				    const QString &loginName, qint32 recentKind);
	void updateDownloadFailed() const;
	void getUserInfoFromOldVersion(const QString &filePath);
	QByteArray getActionLogInfo(const QString &event1, const QString &event2, const QString &event3, const QString &target) const;
	void showTermOfView(const QString &url, const QJsonObject &body, const QList<QNetworkCookie> &cookies, bool &isSuccess, QEventLoop &eventLoop, const QString &loginName, qint32 recentKind);
	void requestPrivacy(const QString &url, const QJsonObject &body, const QVariant &cookies, bool &isSuccess, QEventLoop &eventLoop, const QString &loginName, qint32 recentKind);
	QString getLocalGpopData(const QString &appLocalGpopPath, QJsonDocument &doc, int &version);
	void handleB2BServiceLogowithBG();
	void handleB2BServiceLogoNBG();
	void handleB2BServiceBigLogo();
	void handleB2BServiceBigLogowithColor();
	void getAppleIDAgreementParams(QJsonObject &agreementParams, const QJsonObject &data);
	QJsonObject getSNSLoginParams(const QString &loginName);
	void requestUpdateNoticeDetail(const QString &appVersion, bool forTargetVersion);
	void applyUpdateNoticeDetail(const QString &detailUrl, bool forTargetVersion, const QString &error = QString());
	void finalizePendingUpdateResultIfReady();

	struct PendingUpdateState {
		PLSAppUpdateResult result;
		QString startupNoticeDetailUrl;
		QString targetNoticeDetailUrl;
		bool updateApiFinished = false;
		bool waitingForTargetNotice = false;
		bool startupNoticeFinished = false;
		bool targetNoticeFinished = false;
		bool hardFailed = false;
	};

	pls::AsyncResult<PLSAppUpdateResult> m_appUpdataResult;
	pls::AsyncResult<QString> m_serviceTermsHtml;
	PendingUpdateState m_pendingUpdateState;
	bool m_stopDownloadInstall = false;
	bool m_isPrismTokenValid = true;
	QString m_localeFilePath;
	QString m_prismSession;
	bool m_isExistUserInfo = false;
	PLSCancel m_plsCancel;
	pls::http::Request downloadPackageRequest;
	QString m_ncpServiceId;
	QJsonObject m_snsAccessTokenObj;
	QString m_serviceName;
	QString m_NCB2BAuthUrl;
	QJsonObject m_NCB2BServiceConfigObj;

	QJsonObject m_serviceResLocalObj;
	QJsonObject m_twitchServiceListObj;
	std::list<pls::rsm::UrlAndHowSave> m_urlAndHowSaves;

	bool m_isNeedShowB2BDisableAlert = false;
	bool m_loginResultCommitted = false;
	QString m_snsCode;
	QString m_committedLoginPlatform;
};
#endif // PLSLAUNCHERDATAHANDLER_H
