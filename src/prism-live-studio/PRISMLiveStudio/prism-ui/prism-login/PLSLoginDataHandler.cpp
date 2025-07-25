#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif
#include "PLSLoginDataHandler.h"

PLSLoginDataHandler *PLSLoginDataHandler::instance()
{
	static PLSLoginDataHandler p;
	return &p;
}

QVariantMap PLSLoginDataHandler::getRequestApiDefaultHeader(bool hasGcc) const
{
	return QVariantMap();
}

QMap<QString, QString> PLSLoginDataHandler::getBrowserDefaultHeader() const
{
	return QMap<QString, QString>();
}

void PLSLoginDataHandler::getAppInitDataFromRemote(const std::function<void()> &callback) {}

bool PLSLoginDataHandler::getPrismUserInfoFromRemote(const QList<QNetworkCookie> &cookies, const QString &requestUrl, pls::http::Method httpMethod)
{
	return false;
}

void PLSLoginDataHandler::startDownloadNewPackage(const downloadProgressCallback &callback, const QString &installFileUrl, const QString &gcc) {}

void PLSLoginDataHandler::stopDownloadNewPackage() {}

void PLSLoginDataHandler::refreshPrismToken() {}

AppUpdateResult PLSLoginDataHandler::getUpdateResult() const
{
	return AppUpdateResult();
}

PLSErrorHandler::RetData PLSLoginDataHandler::getAppUdateApiRetData() const
{
	return PLSErrorHandler::RetData();
}

QString PLSLoginDataHandler::getInstallFileUrl() const
{
	return QString();
}

bool PLSLoginDataHandler::isForcePrismAppUpdate() const
{
	return false;
}

QString PLSLoginDataHandler::getUpdateVersion() const
{
	return QString();
}

QString PLSLoginDataHandler::getUpdateInfoUrl() const
{
	return QString();
}

QString PLSLoginDataHandler::getSnsCallbackUrl(const QString &snsName) const
{
	return QString();
}

void PLSLoginDataHandler::getPrismThumbnail(const std::function<void()> &callback) {}

QPixmap PLSLoginDataHandler::getCurrentThumbnail() const
{
	return QPixmap();
}

bool PLSLoginDataHandler::isNeedLogin() const
{
	return false;
}

bool PLSLoginDataHandler::isTokenVaild() const
{
	return false;
}

bool PLSLoginDataHandler::isExistUserInfo() const
{
	return false;
}

QString PLSLoginDataHandler::getInstallPackagePath() const
{
	return QString();
}

void PLSLoginDataHandler::getGoogleCookie(const QString &token, const std::function<void(bool ok, const QJsonObject &)> &callback) {}

void PLSLoginDataHandler::pls_google_user_info(const std::function<void(bool ok, const QJsonObject &)> &callback, const QString &redirect_uri, const QString &code) {}

void PLSLoginDataHandler::savePrismUserInfo(const QJsonObject &userInfo, const QVariant &neo_sesCookies) {}

void PLSLoginDataHandler::getNCPServiceId(const QString &serviceName, const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback) {}

void PLSLoginDataHandler::getNCPAuthUrl(const std::function<void(const QString &)> &callback, const std::function<void(int, int)> &failedCallback) {}

bool PLSLoginDataHandler::getNCPAccessToken(const QString &url)
{
	return false;
}

void PLSLoginDataHandler::setLoginName(const QString &loginName) {}

QString PLSLoginDataHandler::getLoginName() const
{
	return QString();
}

const QJsonObject &PLSLoginDataHandler::getNCB2BServiceConnfigRes() const
{
	// TODO: �ڴ˴����� return ���
	return QJsonObject();
}

QString PLSLoginDataHandler::getNCB2BServiceLogo() const
{
	return QString();
}

QString PLSLoginDataHandler::getNCB2BServiceNBLogo() const
{
	return QString();
}

QString PLSLoginDataHandler::getNCB2BServiceColorLogo() const
{
	return QString();
}

QString PLSLoginDataHandler::getNCB2BServiceWhiteLogo() const
{
	return QString();
}

QString PLSLoginDataHandler::getNCB2BServiceWatermark() const
{
	return QString();
}

QString PLSLoginDataHandler::getNCB2BServiceOutro() const
{
	return QString();
}

void PLSLoginDataHandler::initCustomChannelObj() {}

QJsonObject &PLSLoginDataHandler::getCustomChannelObj()
{
	return m_ncpAccessTokenObj;
}

const QJsonObject &PLSLoginDataHandler::getTwitchServiceList() const
{
	return m_ncpAccessTokenObj;
}

QList<QPair<QString, QString>> PLSLoginDataHandler::getTwitchServer() const
{
	return QList<QPair<QString, QString>>();
}

QString PLSLoginDataHandler::getVersionFromFileUrl(const QString &updateUrl)
{
	return QString();
}

void PLSLoginDataHandler::downloadNCB2BServiceRes(bool bRetry) {}

QString PLSLoginDataHandler::getNCB2BLogoUrl()
{
	return QString();
}

void PLSLoginDataHandler::reDownloadWaterMark() {}

bool PLSLoginDataHandler::isNeedShowB2BServiceAlert()
{
	return false;
}

void PLSLoginDataHandler::getNCB2BServiceResFromRemote(const std::function<void(const QJsonObject &data)> &successCallback,
						       const std::function<void(const QJsonObject &data, const PLSErrorHandler::RetData &retData)> &failCallback, QObject *receiver)
{
}

QImage PLSLoginDataHandler::scaleAndCrop(const QImage &original, const QSize &originTargetSize)
{
	return QImage();
}

PLSLoginDataHandler::PLSLoginDataHandler(QObject *parent) : QObject(parent) {}

QString PLSLoginDataHandler::getUpdateInfoUrl(const QJsonObject &updateInfoUrlList)
{
	return QString();
}

QString PLSLoginDataHandler::getFileNameFromUlr(const QString &fileUrl)
{
	return QString();
}

bool PLSLoginDataHandler::saveThumbnail(const QPixmap &pixmap, const QString &filePath) const
{
	return false;
}

QPixmap PLSLoginDataHandler::loadThumbnail(const QString &filePath) const
{
	return QPixmap();
}

void PLSLoginDataHandler::initApiSuccessHandle(const QJsonDocument &doc) {}

void PLSLoginDataHandler::updateApiSuccessHandle(const QJsonDocument &doc) {}

void PLSLoginDataHandler::getCookieSuccessHandle(const std::function<void(bool ok, const QJsonObject &)> &callback, const pls::http::Reply &reply, const QJsonDocument &doc) {}

void PLSLoginDataHandler::updateDownloadFailed() const {}

void PLSLoginDataHandler::getUserInfoFromOldVersion(const QString &filePath) {}

QByteArray PLSLoginDataHandler::getActionLogInfo(const QString &event1, const QString &event2, const QString &event3, const QString &target) const
{
	return QByteArray();
}

void PLSLoginDataHandler::showTermOfView(const QString &url, const QByteArray &body, const QList<QNetworkCookie> &cookies, bool &isSuccess, QEventLoop &eventLoop) {}

void PLSLoginDataHandler::requestPrivacy(const QString &url, const QByteArray &body, const QVariant &cookies, bool &isSuccess, QEventLoop &eventLoop) {}

QString PLSLoginDataHandler::getLocalGpopData(const QString &appLocalGpopPath, QJsonDocument &doc, int &version)
{
	return QString();
}

void PLSLoginDataHandler::handleB2BServiceLogowithBG() {}

void PLSLoginDataHandler::handleB2BServiceLogoNBG() {}

void PLSLoginDataHandler::handleB2BServiceBigLogo() {}

void PLSLoginDataHandler::handleB2BServiceBigLogowithColor() {}