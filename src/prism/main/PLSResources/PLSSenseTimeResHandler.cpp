#include "PLSSenseTimeResHandler.h"
#include "pls-app.hpp"
#include "PLSResourceMgr.h"
#include <qeventloop.h>

#define ITEMS QStringLiteral("items")
#define ITEMID QStringLiteral("itemId")
#define RESOURCEURL QStringLiteral("resourceUrl")
#define SENSETIME_ZIP_NAME "library_SenseTime_PC.zip"

PLSSenseTimeResHandler::PLSSenseTimeResHandler(const QStringList &resUrls, QObject *parent, bool isUseSubThread) : PLSResourceHandler(resUrls, parent, isUseSubThread) {}
PLSSenseTimeResHandler::~PLSSenseTimeResHandler() {}

void PLSSenseTimeResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All: {
		QVariantList dataList;
		if (PLSJsonDataHandler::getValuesFromByteArray(data, ITEMS, dataList)) {
			for (const auto &map : dataList) {
				QVariantMap indexMap = map.toMap();
				m_libraryResUrls.insert(indexMap.value(ITEMID).toString(), indexMap.value(RESOURCEURL).toString());
			}
		}
		getSensetime();
	} break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSSenseTimeResHandler::checkAndDownloadSensetimeResource()
{
	auto fileName = pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + SENSETIME_NEW_VERSION_FILE_NAME;
	if (!QFile::exists(fileName)) {
		PLS_WARN(MAINFILTER_MODULE, "Sensetime resource dose not exist, redownload it.");
		PLSResourceMgr::instance()->downloadSenseTimeResources();
	}
}

void PLSSenseTimeResHandler::getSensetime()
{ //upcompcess policy and sensetime
	auto _onSuccessed = [=](QNetworkReply * /*networkReplay*/, int /*code*/, QByteArray data, void *context) {
		PLS_INFO(MAINFILTER_MODULE, "Get sense_time resource successfully");
		PLSResourceHandler::saveFile(pls_get_user_path("PRISMLiveStudio/beauty/"), SENSETIME_ZIP_NAME, data);
		PLSResourceHandler::unCompress(pls_get_user_path("PRISMLiveStudio/beauty/"), SENSETIME_ZIP_NAME, true);
		auto fileSrc = pls_get_user_path(SENSETIME_NEW_VERSION_FILE_PATH) + SENSETIME_NEW_VERSION_FILE_NAME;
		bool result = QFile::copy(fileSrc, pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + SENSETIME_NEW_VERSION_FILE_NAME);
		if (!result) {
			PLS_WARN(MAINFILTER_MODULE, "Copy license sense_license_encode.lic file failed.");
		} else {
			QDir dir(pls_get_user_path(SENSETIME_FILE_PATH));
			dir.removeRecursively();
		}
		static_cast<QEventLoop *>(context)->quit();
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void *context) {
		auto url = networkReplay->url().toString();
		PLS_WARN(MAINFILTER_MODULE, "get sensetime  file failed, url =  %s; ", url.toUtf8().constData());
		ResFileInfo info;
		info.fileName = SENSETIME_ZIP_NAME;
		info.resUrl = url;
		info.savePath = pls_get_user_path("PRISMLiveStudio/beauty/");
		reDownloadResources(info, [=](const ResFileInfo & /*fileInfo*/) { PLSResourceHandler::unCompress(pls_get_user_path("PRISMLiveStudio/beauty/"), SENSETIME_ZIP_NAME, true); });
		static_cast<QEventLoop *>(context)->quit();
	};
	if (m_libraryResUrls.find(LIBRARY_SENSETIME_PC_ID) != m_libraryResUrls.end()) {
		PLSNetworkReplyBuilder builder(m_libraryResUrls.value(LIBRARY_SENSETIME_PC_ID));
		auto reply = builder.get(m_manager);
		appendReply(reply);
		PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, &m_eventLoop, 3000);
	} else {
		m_eventLoop.quit();
	}
}
