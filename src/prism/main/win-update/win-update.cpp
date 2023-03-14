#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <string>
#include <mutex>
#include <Windows.h>
#include <util/windows/WinHandle.hpp>
#include <util/windows/win-version.h>
#include <util/util.hpp>
#include <jansson.h>
#include <blake2.h>
#include <time.h>
#include <strsafe.h>
#include <winhttp.h>
#include <shellapi.h>
#include <log.h>
#include <log/module_names.h>
#include <frontend-api.h>
#include <QtNetwork/QHttpPart>
#include <QtNetwork/QHttpMultiPart>
#include "remote-text.hpp"
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "network-access-manager.hpp"
#include "ui-config.h"
#include "main-view.hpp"
#include "update-view.hpp"
#include "pls-net-url.hpp"
#include "window-basic-main.hpp"
#include "PLSAction.h"
#include "log/log.h"
#include "pls/pls-util.hpp"

#define VERSION_COMPARE_COUNT 3

#ifdef Q_OS_WIN64
#define PLATFORM_TYPE QStringLiteral("WIN64")
#else
#define PLATFORM_TYPE QStringLiteral("WIN86")
#endif

#define HEADER_USER_AGENT_KEY QStringLiteral("User-Agent")
#define HEADER_PRISM_LANGUAGE QStringLiteral("Accept-Language")
#define HEADER_PRISM_GCC QStringLiteral("X-prism-cc")
#define HEADER_PRISM_USERCODE QStringLiteral("X-prism-usercode")
#define HEADER_PRISM_OS QStringLiteral("X-prism-os")
#define HEADER_PRISM_IP QStringLiteral("X-prism-ip")
#define HEADER_PRISM_DEVICE QStringLiteral("X-prism-device")
#define HEADER_PRISM_APPVERSION QStringLiteral("X-prism-appversion")
#define PARAM_PLATFORM_TYPE QStringLiteral("platformType")
#define PARAM_VERSION QStringLiteral("version")
#define PARAM_REPLY_EMAIL_ADDRESS QStringLiteral("replyEmailAddress")
#define PARAM_REPLY_QUESTION QStringLiteral("question")
#define PARAM_REPLY_ATTACHED_FILES QStringLiteral("attachedFiles")
#define HEADER_MINE_APPLICATION QStringLiteral("application/octet-stream")

int GetConfigPath(char *path, size_t size, const char *name);

static QString getUpdateInfoUrl(const QMap<QString, QString> &updateInfoUrlList)
{
	if (!strcmp(App()->GetLocale(), "ko-KR")) {
		return updateInfoUrlList.value(QStringLiteral("kr"));
	} else {
		return updateInfoUrlList.value(QStringLiteral("en"));
	}
}
static QString getFileName(const QString &fileUrl)
{
	if (int pos = fileUrl.lastIndexOf('/'); pos >= 0) {
		return fileUrl.mid(pos + 1);
	}
	return fileUrl;
}

bool checkLastestVersion(QString &update_info_url)
{
	bool checkVersionResult = false;
	//init request url
	QUrl url(LASTEST_UPDATE_URL.arg(PRISM_SSL));
	QUrlQuery query;
	query.addQueryItem(PARAM_PLATFORM_TYPE, PLATFORM_TYPE);
	url.setQuery(query);

	//init request header
	QVariantMap headers;

	//eventloop request
	QEventLoop eventLoop;
	QString uri = url.toString();
	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyResultData, &eventLoop, [&](int statusCode, const QString &url, const QByteArray array) {
		if (url != uri) {
			return;
		}

		//quit event loop
		struct QuitQEventLoop {
			QEventLoop *eventLoop;
			~QuitQEventLoop() { eventLoop->quit(); }
		} quitQEventLoop = {&eventLoop};

		if (statusCode != HTTP_STATUS_OK) {
			PLS_ERROR(UPDATE_MODULE, "request LatestAppversion api failed, status code: %d", statusCode);
			return;
		}

		QJsonParseError result;
		//Parses json as a UTF-8 encoded JSON document,
		QJsonDocument doc = QJsonDocument::fromJson(array, &result);
		if (result.error != QJsonParseError::NoError) {
			PLS_ERROR(UPDATE_MODULE, "request Latest Appversion api failed, json parse failed, reason: %s", result.errorString().toUtf8().constData());
			return;
		}
		if (!doc.isObject()) {
			return;
		}

		//Returns the QJsonObject contained in the document.
		QJsonObject response = doc.object();

		//get platform string
		QString platformType = response.value(QLatin1String("platformType")).toString();
		if (platformType != PLATFORM_TYPE) {
			PLS_ERROR(UPDATE_MODULE, "Latest Appversion info error, platformType not matched, request platformType: %s, response platformType: %s", PLATFORM_TYPE.toUtf8().constData(),
				  platformType.toUtf8().constData());
			return;
		}

		//get update info url
		QJsonObject updateInfoUrlListObject = response.value(QLatin1String("updateInfoUrlList")).toObject();
		QMap<QString, QString> updateInfoUrlList;
		for (auto iter = updateInfoUrlListObject.begin(); iter != updateInfoUrlListObject.end(); ++iter) {
			updateInfoUrlList.insert(iter.key(), iter.value().toString());
		}
		update_info_url = getUpdateInfoUrl(updateInfoUrlList);
		checkVersionResult = true;
		//PLS_DEBUG(UPDATE_MODULE, "request Latest Appversion api updateInfoUrl: %s",updateInfoUrl.toUtf8().constData());
	});
	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyErrorData, &eventLoop, [&](const QString &url, const QString error) {
		Q_UNUSED(url)
		if (url != uri) {
			return;
		}
		PLS_ERROR(UPDATE_MODULE, "update failed, reason: %s", error.toUtf8().constData());
		eventLoop.quit();
	});
	PLSNetworkAccessManager::getInstance()->createHttpRequest(QNetworkAccessManager::GetOperation, uri.toUtf8().constData(), true, headers);

	eventLoop.exec();
	return checkVersionResult;
}

pls_upload_file_result_t upload_contactus_files(const QString &email, const QString &question, const QList<QFileInfo> files)
{
	PLS_DEBUG(CONTACT_US_MODULE, "request contactus send email api");
	//means the body part contains form element.
	pls_upload_file_result_t upload_result = pls_upload_file_result_t::Ok;
	QHttpMultiPart *multi_part = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	for (const QFileInfo &fileInfo : files) {
		QHttpPart file_part;
		file_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(HEADER_MINE_APPLICATION));
		file_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"" + PARAM_REPLY_ATTACHED_FILES + "\"; filename=\"" + fileInfo.fileName() + "\""));
		QFile *file = new QFile(fileInfo.filePath());
		if (!file->open(QIODevice::ReadOnly)) {
			continue;
		}
		file->setParent(multi_part);
		file_part.setBodyDevice(file);
		multi_part->append(file_part);
	}

	//init the email form data
	QHttpPart email_part;
	email_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"" + PARAM_REPLY_EMAIL_ADDRESS + "\""));
	email_part.setBody(email.toUtf8());
	multi_part->append(email_part);

	//init the question forom data
	QHttpPart question_part;
	question_part.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"" + PARAM_REPLY_QUESTION + "\""));
	question_part.setBody(question.toUtf8());
	multi_part->append(question_part);
	PLSNetworkAccessManager::getInstance()->setPostMultiPart(multi_part);

	//eventloop request
	QEventLoop eventLoop;
	QString uri = CONTACT_SEND_EMAIL_URL.arg(PRISM_SSL);
	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyResultData, &eventLoop, [&](int statusCode, const QString &url, const QByteArray array) {
		if (url != uri) {
			return;
		}

		//quit event loop
		struct QuitQEventLoop {
			QEventLoop *eventLoop;
			~QuitQEventLoop() { eventLoop->quit(); }
		} quitQEventLoop = {&eventLoop};

		if (statusCode != HTTP_STATUS_OK) {
			PLS_ERROR(UPDATE_MODULE, "request LatestAppversion api failed, status code: %d", statusCode);
			upload_result = pls_upload_file_result_t::NetworkError;
			return;
		}
	});
	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyErrorDataWithSatusCode, &eventLoop,
			 [&](int statusCode, const QString &url, const QString &body, const QString &) {
				 Q_UNUSED(url)
				 if (url != uri) {
					 return;
				 }
				 QByteArray root = body.toUtf8();
				 QVariant codeVariant;
				 PLSJsonDataHandler::getValueFromByteArray(root, "code", codeVariant);
				 int code = codeVariant.toInt();
				 upload_result = pls_upload_file_result_t::NetworkError;
				 if (code == 1001) {
					 upload_result = pls_upload_file_result_t::EmailFormatError;
				 } else if (code == 1203) {
					 upload_result = pls_upload_file_result_t::FileFormatError;
				 } else if (code == 1200) {
					 upload_result = pls_upload_file_result_t::AttachUpToMaxFile;
				 }
				 PLS_ERROR(UPDATE_MODULE, "contact send email failed, code: %d, statusCode = %d", code, statusCode);
				 eventLoop.quit();
			 });
	PLSNetworkAccessManager::getInstance()->createHttpRequest(QNetworkAccessManager::PostOperation, CONTACT_SEND_EMAIL_URL.arg(PRISM_SSL).toUtf8().constData(), true);
	eventLoop.exec();

	return upload_result;
}

pls_check_update_result_t checkUpdate(QString &gcc, bool &isForceUpdate, QString &version, QString &fileUrl, QString &updateInfoUrl)
{
	static QPointer<QNetworkReply> reply;
	if (reply != nullptr) {
		reply->abort();
	}

	pls_check_update_result_t check_update_result = pls_check_update_result_t::Failed;

	int ver[VERSION_COMPARE_COUNT + 1] = {PLS_RELEASE_CANDIDATE_MAJOR, PLS_RELEASE_CANDIDATE_MINOR, PLS_RELEASE_CANDIDATE_PATCH, PLS_RELEASE_CANDIDATE};
	QString verstr = QString("%1.%2.%3").arg(ver[0]).arg(ver[1]).arg(ver[2]);

	win_version_info wvi;
	get_win_ver(&wvi);

	QUrl url(UPDATE_URL.arg(PRISM_SSL));

	QUrlQuery query;
	query.addQueryItem(PARAM_PLATFORM_TYPE, PLATFORM_TYPE);
	query.addQueryItem(PARAM_VERSION, verstr);
	url.setQuery(query);

	QVariantMap headers;

	QString uri = url.toString();

	QEventLoop eventLoop;

	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyResultData, &eventLoop, [&](int statusCode, const QString &url, const QByteArray array) {
		if (url != uri) {
			return;
		}

		struct QuitQEventLoop {
			QEventLoop *eventLoop;
			~QuitQEventLoop() { eventLoop->quit(); }
		} quitQEventLoop = {&eventLoop};

		if (statusCode != HTTP_STATUS_OK) {
			PLS_INIT_ERROR(UPDATE_MODULE, "request appInit api failed, status code: %d", statusCode);
			return;
		}

		QJsonParseError result;
		QJsonDocument doc = QJsonDocument::fromJson(array, &result);
		if (result.error != QJsonParseError::NoError) {
			PLS_INIT_ERROR(UPDATE_MODULE, "request appInit api failed, json parse failed, reason: [%s][%s]", result.errorString().toUtf8().constData(), array.constData());
			return;
		}

		QJsonObject response = doc.object();
		check_update_result = pls_check_update_result_t::OkNoUpdate;
		gcc = response.value(QLatin1String("gcc")).toString();

		auto hashUsercode = response.value("hashedUserCode").toString();
		PLSLoginUserInfo::getInstance()->setUserCodeWithEncode(hashUsercode);
		pls_set_user_id(hashUsercode.toUtf8().constData(), PLS_SET_TAG_CN);
		PLS_INIT_INFO(UPDATE_MODULE, "set hasusercode to nelo userid.");

		maskingLogUserID = hashUsercode.toUtf8().constData();

		PLS_INIT_INFO(UPDATE_MODULE, "request appInit api success, gcc: %s; hashUserCode:%s", gcc.toUtf8().constData(), hashUsercode.toUtf8().constData());

		if (response.contains("error_code")) {
			PLS_INIT_ERROR(UPDATE_MODULE, "update api return failure, error info: %s", array.constData());
			return;
		}

		QJsonObject appUpdateObject = response.value(QLatin1String("appUpdate")).toObject();
		if (!response.contains("appUpdate") || appUpdateObject.isEmpty()) {
			PLS_INIT_INFO(UPDATE_MODULE, "no update available.");
			return;
		}

		check_update_result = pls_check_update_result_t::OkHasUpdate;
		isForceUpdate = appUpdateObject.value(QLatin1String("updateType")).toString() == QLatin1String("FORCE");
		version = appUpdateObject.value(QLatin1String("version")).toString();
		fileUrl = appUpdateObject.value(QLatin1String("fileUrl")).toString();
		QJsonObject updateInfoUrlListObject = appUpdateObject.value(QLatin1String("updateInfoUrlList")).toObject();
		QMap<QString, QString> updateInfoUrlList;
		for (auto iter = updateInfoUrlListObject.begin(); iter != updateInfoUrlListObject.end(); ++iter) {
			updateInfoUrlList.insert(iter.key(), iter.value().toString());
		}
		updateInfoUrl = getUpdateInfoUrl(updateInfoUrlList);
		PLS_INIT_INFO(UPDATE_MODULE, "update available, isForceUpdate: %s, version: %s, fileUrl: %s, updateInfoUrl: %s", isForceUpdate ? "true" : "false", version.toUtf8().constData(),
			      fileUrl.toUtf8().constData(), updateInfoUrl.toUtf8().constData());
	});
	QObject::connect(PLSNetworkAccessManager::getInstance(), &PLSNetworkAccessManager::replyErrorDataWithSatusCode, &eventLoop, [&](int, const QString &url, const QString &, const QString &) {
		Q_UNUSED(url)
		if (url != uri) {
			return;
		}
		eventLoop.quit();
	});
	reply = PLSNetworkAccessManager::getInstance()->createHttpRequest(QNetworkAccessManager::GetOperation, uri.toUtf8().constData(), true, headers, QVariantMap(), false);
	eventLoop.exec();
	return check_update_result;
}
bool showUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent)
{
	PLSUpdateView updateDlg(manualUpdate, isForceUpdate, version, fileUrl, updateInfoUrl, parent);
	return updateDlg.exec() == PLSUpdateView::Accepted;
}

//dowload
bool downloadUpdate(QString &localFilePath, const QString &fileUrl, PLSCancel &cancel, const std::function<void(qint64 download_bytes, qint64 total_bytes)> &progress)
{
	PLS_INFO(UPDATE_MODULE, "downloadUpdate request start");
	QEventLoop eventLoop;
	QNetworkAccessManager manager;

	QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(fileUrl)));

	char updateTmpDir[512];
	if (GetConfigPath(updateTmpDir, sizeof(updateTmpDir), "PRISMLiveStudio/updates/") <= 0) {
		PLS_ERROR(UPDATE_MODULE, "get update template directory failed");
		return false;
	}

	QString updateFilePath = updateTmpDir;
	updateFilePath += getFileName(fileUrl);

	PLS_INFO(UPDATE_MODULE, "downloadUpdate file start");
	if (QFile::exists(updateFilePath)) {
		QFile::remove(updateFilePath);
	}

	QFile updateFile(updateFilePath);
	if (!updateFile.open(QFile::WriteOnly)) {
		PLS_ERROR(UPDATE_MODULE, "create update template file failed");
		QString error = updateFile.errorString();
		return false;
	}

	qint64 tmpBytesReceived = 0;
	qint64 tmpTytesTotal = 0;

	bool ok = false;
	QObject::connect(reply, &QNetworkReply::downloadProgress, &eventLoop, [&](qint64 bytesReceived, qint64 bytesTotal) {
		PLS_DEBUG(UPDATE_MODULE, "download update progress, download: %lld, total: %lld", bytesReceived, bytesTotal);
		tmpBytesReceived = bytesReceived;
		tmpTytesTotal = bytesTotal;
		progress(bytesReceived, bytesTotal);
	});
	QObject::connect(reply, &QNetworkReply::readyRead, &eventLoop, [&]() { updateFile.write(reply->readAll()); });
	QObject::connect(reply, &QNetworkReply::finished, &eventLoop, [&]() {
		if (tmpBytesReceived == tmpTytesTotal && tmpTytesTotal != 0) {
			PLS_INFO(UPDATE_MODULE, "download update finished");
			ok = true;
			reply->deleteLater();
			eventLoop.quit();
		}
	});
	void (QNetworkReply::*error)(QNetworkReply::NetworkError) = &QNetworkReply::error;
	QObject::connect(reply, error, &eventLoop, [&](QNetworkReply::NetworkError ne) {
		if (ne != QNetworkReply::NetworkError::NoError) {
			PLS_INFO(UPDATE_MODULE, "download update failed, reason: %s", reply->errorString().toUtf8().constData());
			ok = false;
			reply->deleteLater();
			eventLoop.quit();
		}
	});
	QObject::connect(&cancel, &PLSCancel::cancelSignal, &eventLoop, [&]() {
		PLS_ERROR(UPDATE_MODULE, "download update failed, user cancel");
		reply->abort();
	});

	eventLoop.exec();

	updateFile.close();

	if (ok && !cancel) {
		localFilePath = updateFilePath;
		PLS_INFO(UPDATE_MODULE, "download update success");
		return true;
	}
	PLS_INFO(UPDATE_MODULE, "downloadUpdate request return false: %d", ok);
	updateFile.remove();
	return false;
}
bool installUpdate(const QString &filePath)
{
	std::wstring filePathW = filePath.toStdWString();

	SHELLEXECUTEINFO sei = {};
	sei.cbSize = sizeof(sei);
	sei.lpFile = filePathW.c_str();
	sei.nShow = SW_SHOWNORMAL;
	bool isSuccess = false;
	if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
		if (ShellExecuteEx(&sei)) {
			PLS_INFO(UPDATE_MODULE, "successfully to call ShellExecuteEx() for new packet");
			isSuccess = true;
		} else {
			PLS_ERROR(UPDATE_MODULE, "failed to call ShellExecuteEx() for new packet ERROR: %lu", GetLastError());
		}
		CoUninitialize();
	} else {
		PLS_ERROR(APP_MODULE, "failed to call CoInitializeEx() for new packet ERROR");
	}
	return isSuccess;
}
void restartApp()
{
	QString filePath = QApplication::applicationFilePath();

	QObject::connect(qApp, &QApplication::destroyed, [filePath]() {
		std::wstring filePathW = filePath.toStdWString();
		PLS_INFO("App Restart", "Prism Live Studio restarting now...");
		SHELLEXECUTEINFO sei = {};
		sei.cbSize = sizeof(sei);
		sei.lpFile = filePathW.c_str();
		sei.nShow = SW_SHOWNORMAL;
		if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
			if (ShellExecuteEx(&sei)) {
				PLS_INFO(APP_MODULE, "successfully to call ShellExecuteEx() for restart PRISMLiveStudio.exe");
			} else {
				PLS_ERROR(APP_MODULE, "failed to call ShellExecuteEx() for restart PRISMLiveStudio.exe ERROR: %lu", GetLastError());
			}
			CoUninitialize();
		} else {
			PLS_ERROR(APP_MODULE, "failed to call CoInitializeEx() for restart PRISMLiveStudio.exe ERROR");
		}
	});
}
