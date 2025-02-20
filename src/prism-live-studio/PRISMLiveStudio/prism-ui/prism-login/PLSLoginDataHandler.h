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

class PLSLoginDataHandler : public QObject {
	Q_OBJECT
public:
	static PLSLoginDataHandler *instance();
	QVariantMap getRequestApiDefaultHeader(bool hasGcc = true) const;
	QMap<QString, QString> getBrowserDefaultHeader() const;

	void getAppInitDataFromRemote(const std::function<void()> &callback); //gpop,init
	bool getPrismUserInfoFromRemote(const QList<QNetworkCookie> &cookies, const QString &requestUrl, pls::http::Method httpMethod);
	void startDownloadNewPackage(const downloadProgressCallback &callback, const QString &installFileUrl, const QString &gcc);
	void stopDownloadNewPackage();
	void refreshPrismToken();

	const QString &prismSession() const { return m_prismSession; }
	void prismSession(const QString &session) { m_prismSession = session; }

	AppUpdateResult getUpdateResult() const;
	PLSErrorHandler::RetData getAppUdateApiRetData() const;
	QString getInstallFileUrl() const;
	bool isForcePrismAppUpdate() const;
	QString getUpdateVersion() const;
	QString getUpdateInfoUrl() const;

	QString getSnsCallbackUrl(const QString &snsName) const;
	void getPrismThumbnail(const std::function<void()> &callback);
	QPixmap getCurrentThumbnail() const;

	bool isNeedLogin() const;
	bool isTokenVaild() const;
	bool isExistUserInfo() const;
	QString getInstallPackagePath() const;

	void getGoogleCookie(const QString &token, const std::function<void(bool ok, const QJsonObject &)> &callback);
	template<typename Callback> void google_regeist_handler(const QVariant &reply, const QString &token, const Callback &callback);
	void pls_google_user_info(const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &redirect_uri, const QString &code);
	void savePrismUserInfo(const QJsonObject &userInfo, const QVariant &neo_sesCookies);

	void getNCPServiceId(const QString &serviceName, const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback);
	void getNCPAuthUrl(const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback);
	bool getNCPAccessToken(const QString &url);
	void setLoginName(const QString &loginName);
	QString getLoginName() const;
	const QJsonObject &getNCB2BServiceConnfigRes() const;
	QString getNCB2BServiceLogo() const;
	QString getNCB2BServiceNBLogo() const;
	QString getNCB2BServiceColorLogo() const;
	QString getNCB2BServiceWhiteLogo() const;
	QString getNCB2BServiceWatermark() const;
	QString getNCB2BServiceOutro() const;
	void initCustomChannelObj();
	QJsonObject &getCustomChannelObj();

	const QJsonObject &getTwitchServiceList() const;
	QList<QPair<QString, QString>> getTwitchServer() const;
	QString getVersionFromFileUrl(const QString &updateUrl);

	void downloadNCB2BServiceRes(bool bRetry = false);
	QString getNCB2BLogoUrl();
	void reDownloadWaterMark();
	bool isNeedShowB2BServiceAlert();
	void getNCB2BServiceResFromRemote(const std::function<void(const QJsonObject &data)> &successCallback,
					  const std::function<void(const QJsonObject &data, const PLSErrorHandler::RetData &retData)> &failCallback, QObject *receiver);
	QImage scaleAndCrop(const QImage &original, const QSize &originTargetSize);
signals:
	void updateNCB2BIcon();

private:
	explicit PLSLoginDataHandler(QObject *parent = nullptr);
	~PLSLoginDataHandler() override = default;
	static QString getUpdateInfoUrl(const QJsonObject &updateInfoUrlList);
	static QString getFileNameFromUlr(const QString &fileUrl);
	bool saveThumbnail(const QPixmap &pixmap, const QString &filePath) const;
	QPixmap loadThumbnail(const QString &filePath) const;
	void initApiSuccessHandle(const QJsonDocument &doc);
	void updateApiSuccessHandle(const QJsonDocument &doc);
	void getCookieSuccessHandle(const std::function<void(bool ok, const QJsonObject &)> &callback, const pls::http::Reply &reply, const QJsonDocument &doc);
	void updateDownloadFailed() const;
	void getUserInfoFromOldVersion(const QString &filePath);
	QByteArray getActionLogInfo(const QString &event1, const QString &event2, const QString &event3, const QString &target) const;
	void showTermOfView(const QString &url, const QByteArray &body, const QList<QNetworkCookie> &cookies, bool &isSuccess, QEventLoop &eventLoop);
	void requestPrivacy(const QString &url, const QByteArray &body, const QVariant &cookies, bool &isSuccess, QEventLoop &eventLoop);
	QString getLocalGpopData(const QString &appLocalGpopPath, QJsonDocument &doc, int &version);
	void handleB2BServiceLogowithBG();
	void handleB2BServiceLogoNBG();
	void handleB2BServiceBigLogo();
	void handleB2BServiceBigLogowithColor();

	PLSErrorHandler::RetData m_retData;
	bool m_isForceUpdate = false;
	AppUpdateResult m_updateResult = AppUpdateResult::AppFailed;
	QString m_newPrismVersion;
	QString m_AppInstallFileUrl;
	QString m_updateInfoUrl;
	bool m_stopDownloadInstall = false;
	bool m_isPrismTokenValid = true;
	QString m_localeFilePath;
	QString m_prismSession;
	bool m_isExistUserInfo = false;
	PLSCancel m_plsCancel;
	pls::http::Request downloadPackageRequest;
	QString m_ncpServiceId;
	QJsonObject m_ncpAccessTokenObj;
	QString m_serviceName;
	QString m_loginName;
	QString m_NCB2BAuthUrl;
	QJsonObject m_NCB2BServiceConfigObj;

	QJsonObject m_serviceResLocalObj;
	QJsonObject m_twitchServiceListObj;
	std::list<pls::rsm::UrlAndHowSave> m_urlAndHowSaves;

	bool m_isNeedShowB2BDisableAlert = false;
};
#endif // PLSLAUNCHERDATAHANDLER_H

template<typename Callback> inline void PLSLoginDataHandler::google_regeist_handler(const QVariant &reply, const QString &token, const Callback &callback) {}
