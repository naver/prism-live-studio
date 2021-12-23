#include "PLSLibraryResHandler.h"
#include <pls-app.hpp>

#define RESOURCEURL QStringLiteral("resourceUrl")
#define ITEMS QStringLiteral("items")
#define ITEMID QStringLiteral("itemId")
#define OUTROPATH "PRISMLiveStudio/user/library_Policy_PC/Policy_Outro_PC_v2.6.0.json"
#define Outro_ResourceTimeoutMs QStringLiteral("timeoutMs")
#define WATERMARKPATH "PRISMLiveStudio/user/library_Policy_PC/Policy_Watermark_PC_2.6.0.json"
#define LIBRARY_ZIP_NAME "library_Policy_PC.zip"
#define SENSETIME_ZIP_NAME "library_SenseTime_PC.zip"

PLSLibraryResHandler::PLSLibraryResHandler(const QStringList &resUrls, QObject *parent) : PLSResourceHandler(resUrls, parent) {}

PLSLibraryResHandler::~PLSLibraryResHandler() {}

void PLSLibraryResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All: {
		PLS_INFO(MAINFILTER_MODULE, "Start download all library resource");
		if (!PLSJsonDataHandler::saveJsonFile(data, m_relativeResPath + "../library.json")) {
			PLS_WARN(MAINFILTER_MODULE, "save to json library.json failed");
		}

		QVariantList dataList;
		if (PLSJsonDataHandler::getValuesFromByteArray(data, ITEMS, dataList)) {
			for (const auto &map : dataList) {
				QVariantMap indexMap = map.toMap();
				m_libraryResUrls.insert(indexMap.value(ITEMID).toString(), indexMap.value(RESOURCEURL).toString());
			}
		}
		getLibraryResProcess();
	} break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSLibraryResHandler::getLibraryResProcess()
{
	//upcompcess policy and sensetime
	auto _onSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void * /*context*/) {
		if (networkReplay)
			PLS_INFO(MAINFILTER_MODULE, "Get library resource successfully");
		PLSResourceHandler::saveFile(m_relativeResPath + "../", LIBRARY_ZIP_NAME, data);
		PLSResourceHandler::unCompress(m_relativeResPath + "../", LIBRARY_ZIP_NAME, true);

		getWatermarkOutroRes();
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		PLS_WARN(MAINFILTER_MODULE, "get Policy file failed, url =  %s; ", networkReplay->url().toString().toUtf8().constData());
		ResFileInfo info;
		info.fileName = LIBRARY_ZIP_NAME;
		info.resUrl = networkReplay->url().toString();
		info.savePath = m_relativeResPath + "../";
		reDownloadResources(info, [=](const ResFileInfo & /*fileInfo*/) { getWatermarkOutroRes(); });
	};
	if (m_libraryResUrls.find(LIBRARY_POLICY_PC_ID) != m_libraryResUrls.end()) {
		PLSNetworkReplyBuilder builder(m_libraryResUrls.value(LIBRARY_POLICY_PC_ID));
		auto reply = builder.get(m_manager);
		appendReply(reply);
		PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
	}
}

void PLSLibraryResHandler::getWatermarkOutroRes()
{
	auto _onSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void * /*context*/) {
		PLS_INFO(MAINFILTER_MODULE, "Get watermark and outro resource successfully");
		PLSResourceHandler::saveFile(m_relativeResPath, getPolicyFileNameByURL(networkReplay->url().toString()), data);
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		auto url = networkReplay->url().toString();
		ResFileInfo info;
		info.fileName = getPolicyFileNameByURL(url);
		info.resUrl = url;
		info.savePath = m_relativeResPath;

		PLS_WARN(MAINFILTER_MODULE, "get %s file failed, url =  %s; ", info.fileName.toUtf8().constData(), url.toUtf8().constData());
		reDownloadResources(info, nullptr);
	};

	QByteArray outroArray;
	PLSJsonDataHandler::getJsonArrayFromFile(outroArray, pls_get_user_path(OUTROPATH));

	QByteArray watermarkArray;
	PLSJsonDataHandler::getJsonArrayFromFile(watermarkArray, pls_get_user_path(WATERMARKPATH));
	//request watermark
	QVariantList resoureces;
	if (PLSJsonDataHandler::getValuesFromByteArray(watermarkArray, "resource", resoureces)) {
		for (auto resource : resoureces) {
			auto url = resource.value<QJsonObject>().value("resourceUrl").toString();
			if (!url.isEmpty()) {
				PLSNetworkReplyBuilder builder(url);
				auto reply = builder.get(m_manager);
				appendReply(reply);
				PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
			}
		}
	}
	//request outro
	if (outroArray.size() > 0) {
		QStringList urls;
		QJsonObject itemObject = QJsonDocument::fromJson(outroArray).object();
		for (auto it = itemObject.begin(); it != itemObject.end(); it++) {
			urls.append(it.value().toObject().value("outroUrl").toObject().value("kr").toObject().value("resource_land").toString());
			urls.append(it.value().toObject().value("outroUrl").toObject().value("kr").toObject().value("resource_portrait").toString());
			urls.append(it.value().toObject().value("outroUrl").toObject().value("en").toObject().value("resource_land").toString());
			urls.append(it.value().toObject().value("outroUrl").toObject().value("en").toObject().value("resource_portrait").toString());
		}

		for (auto url : urls) {
			if (!url.isEmpty()) {
				PLSNetworkReplyBuilder builder(url);
				auto reply = builder.get(m_manager);
				appendReply(reply);
				PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
			}
		}
	}
}

QString PLSLibraryResHandler::getPolicyFileNameByURL(const QString &url)
{
	QStringList strList = url.split("/");
	return strList.last();
}
