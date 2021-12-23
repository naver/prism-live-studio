#include "PLSColorResHandler.h"

PLSColorResHandler::PLSColorResHandler(const QStringList &resUrls, QObject *parent) : PLSResourceHandler(resUrls, parent) {}

PLSColorResHandler::~PLSColorResHandler() {}

void PLSColorResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All:
		onGetColorResourcesSuccess(data);
		break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSColorResHandler::onGetColorResourcesSuccess(const QByteArray &array)
{
	// save json file to color_filter path
	if (!PLSJsonDataHandler::saveJsonFile(array, m_relativeResPath + COLOR_FILTER_JSON_FILE)) {
		PLS_ERROR(MAINFILTER_MODULE, "save to json %s failed", COLOR_FILTER_JSON_FILE);
	}

	int index = 1;
	QJsonObject objectJson = QJsonDocument::fromJson(array).object();
	const QJsonArray &jsonArray = objectJson.value(HTTP_ITEMS).toArray();
	for (auto item : jsonArray) {
		QString name = item[HTTP_ITEM_ID].toString();
		QString shortTitle = item[SHORT_TITLE].toString();
		QString number{};
		if (index < COLOR_FILTER_ORDER_NUMBER) {
			number = number.append("0").append(QString::number(index++));
		} else {
			number = number.append(QString::number(index++));
		}

		QString totalName = number + "_" + name + "_" + shortTitle;
		m_resourceUrls.insert(item[HTTP_RESOURCE_URL].toString(), totalName);
		m_thumbnailUrls.insert(item[COLOR_THUMBNAILURL].toString(), totalName);
	}
	downloadThumbnailRequest();
}

bool PLSColorResHandler::colorFilterDirIsExisted(const QString &path)
{
	QDir dir(pls_get_color_filter_dir_path());
	if (!dir.exists()) {
		bool ok = dir.mkpath(pls_get_color_filter_dir_path());
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir color_filter failed.");
			return false;
		}
	}

	if (dir.exists(path)) {
		return true;
	}

	bool ok = dir.mkdir(path);
	if (!ok) {
		PLS_ERROR(MAINFILTER_MODULE, "mkdir color_filter failed.");
		return false;
	}

	return true;
}

void PLSColorResHandler::downloadThumbnailRequest()
{
	auto _onResSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void * /*context*/) {
		QString name = m_resourceUrls.value(networkReplay->url().toString()).toString();
		SaveToLocalThumbnailImage(data, m_relativeResPath + name.append(COLOR_FILTER_IMAGE_FORMAT_PNG));
	};
	auto _onResFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		auto name = m_resourceUrls.value(networkReplay->url().toString()).toString();
		PLS_WARN(MAINFILTER_MODULE, "get Color resource file failed, name =  %s; ", name.toUtf8().constData());
		ResFileInfo info;
		info.fileName = name.append(COLOR_FILTER_IMAGE_FORMAT_PNG);
		info.resUrl = networkReplay->url().toString();
		info.savePath = m_relativeResPath;
		reDownloadResources(info, nullptr);
	};

	for (auto key : m_resourceUrls.keys()) {
		PLSNetworkReplyBuilder builder(key);
		auto reply = builder.get(m_manager);
		appendReply(reply);
		PLSHttpHelper::connectFinished(reply, this, _onResSuccessed, _onResFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
	}

	auto _onThumbSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void * /*context*/) {
		QString name = m_thumbnailUrls.value(networkReplay->url().toString()).toString();
		SaveToLocalThumbnailImage(data, m_relativeResPath + name.append(COLOR_FILTER_THUMBNAIL).append(COLOR_FILTER_IMAGE_FORMAT_PNG));
	};

	auto _onThumbFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		auto name = m_thumbnailUrls.value(networkReplay->url().toString()).toString();
		PLS_WARN(MAINFILTER_MODULE, "get Color thumbnail file failed, name =  %s; ", name.toUtf8().constData());

		ResFileInfo info;
		info.fileName = name.append(COLOR_FILTER_THUMBNAIL).append(COLOR_FILTER_IMAGE_FORMAT_PNG);
		info.resUrl = networkReplay->url().toString();
		info.savePath = m_relativeResPath;
		reDownloadResources(info, nullptr);
	};

	for (auto key : m_thumbnailUrls.keys()) {
		PLSNetworkReplyBuilder builder(key);
		auto reply = builder.get(m_manager);
		PLSHttpHelper::connectFinished(reply, this, _onThumbSuccessed, _onThumbFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
		appendReply(reply);
	}
}
bool PLSColorResHandler::SaveToLocalThumbnailImage(const QByteArray &array, const QString &path)
{
	return QImage::fromData(array).save(path);
}
