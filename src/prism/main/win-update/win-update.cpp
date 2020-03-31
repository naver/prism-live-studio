#include "remote-text.hpp"
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "network-access-manager.hpp"
#include "ui-config.h"

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

#include "main-view.hpp"
#include "update-view.hpp"
#include <QtNetwork/QHttpPart>
#include <QtNetwork/QHttpMultiPart>
#include "pls-net-url.hpp"

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

void restartApp()
{
	QString filePath = QApplication::applicationFilePath();

	QObject::connect(qApp, &QApplication::destroyed, [filePath]() {
		std::wstring filePathW = filePath.toStdWString();

		SHELLEXECUTEINFO sei = {};
		sei.cbSize = sizeof(sei);
		sei.lpFile = filePathW.c_str();
		sei.nShow = SW_SHOWNORMAL;
		if (ShellExecuteEx(&sei)) {
			PLS_INFO(APP_MODULE, "successfully to call ShellExecuteEx() for restart PRISMLiveStudio.exe");
		} else {
			PLS_ERROR(APP_MODULE, "failed to call ShellExecuteEx() for restart PRISMLiveStudio.exe ERROR: %lu", GetLastError());
		}
	});
}
