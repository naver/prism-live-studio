#include "PLSBeautyResHandler.h"
#include "../PLSBeautyFilterView.h"
#include <qfile.h>
#include <set>
#include "platform.hpp"

PLSBeautyResHandler::PLSBeautyResHandler(const QStringList &resUrls, QObject *parent) : PLSResourceHandler(resUrls, parent) {}

PLSBeautyResHandler::~PLSBeautyResHandler() {}

void PLSBeautyResHandler::doWorks(const QByteArray &data, DownLoadResourceMode downMode, const QString &resPath)
{
	switch (downMode) {
	case DownLoadResourceMode::All:
		PLS_INFO(MAINFILTER_MODULE, "Start download beauty resource");
		SaveBeautyJsonFile(data);
		onGetBeautyResourcesSuccess(data);
		break;
	case DownLoadResourceMode::Part:
		saveResource(data, resPath);
		break;
	default:
		break;
	}
}

void PLSBeautyResHandler::onGetBeautyResourcesSuccess(const QByteArray &array)
{
	auto _onSuccessed = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, void * /*context*/) {
		PLS_INFO(MAINFILTER_MODULE, "Get beauty resource successfully");
		auto url = networkReplay->url().toString();

		QString zipName = url.mid(url.lastIndexOf('/') + 1);
		PLSResourceHandler::saveFile(m_relativeResPath, zipName, data);

		auto id = m_beautyPersetUrls.key(url);
		HandleBeautyPresetData(zipName, m_relativeResPath, id, m_beautyPersetUrls.keys().indexOf(id));
		PLSBasic::Get()->OnBeautySourceDownloadFinished();
	};

	auto _onFail = [=](QNetworkReply *networkReplay, int /*code*/, QByteArray data, QNetworkReply::NetworkError /*error*/, void * /*context*/) {
		PLS_ERROR(MAINFILTER_MODULE, "Get beauty preset URL %s failed: %s", networkReplay->url().toString().toUtf8().data(), data.data());
		auto url = networkReplay->url().toString();
		ResFileInfo info;
		info.fileName = url.mid(url.lastIndexOf('/') + 1);
		info.savePath = m_relativeResPath;
		info.resUrl = url;
		reDownloadResources(info, [=](const ResFileInfo &fileInfo) {
			auto id = m_beautyPersetUrls.key(fileInfo.resUrl);
			HandleBeautyPresetData(fileInfo.fileName, m_relativeResPath, id, m_beautyPersetUrls.keys().indexOf(id));
		});
	};

	QJsonObject objectJson = QJsonDocument::fromJson(array).object();
	const QJsonArray &jsonArray = objectJson.value(HTTP_ITEMS).toArray();
	for (auto item : jsonArray) {
		QString title = item[HTTP_ITEM_ID].toString();
		QString resourceUrl = item[HTTP_RESOURCE_URL].toString();
		m_beautyPersetUrls.insert(title, resourceUrl);

		PLSNetworkReplyBuilder builder(resourceUrl);
		auto reply = builder.get(m_manager);
		PLSHttpHelper::connectFinished(reply, this, _onSuccessed, _onFail, nullptr, nullptr, PRISM_NET_DOWNLOAD_TIMEOUT);
		appendReply(reply);
	}
}

void renameBeautyPreset(const QString &path, const int itemIndex);
void renameBeautyCustomDir(const QString &path, const int itemIndex);

void PLSBeautyResHandler::HandleBeautyPresetData(const QString &zipName, const QString &desPath, const QString &id, int itemIndex)
{
	if (!PLSResourceHandler::unCompress(desPath, zipName, true)) {
		PLS_WARN(MAINFILTER_MODULE, "Zip file: %s decompressed error, delete it.", qPrintable(zipName));
		auto srcPath = QString("%1%2").arg(desPath).arg(zipName);
		QFile::remove(srcPath);
		return;
	}

	QString beautyTmpPath = desPath + CONFIG_BEAUTY_TMP_PATH;
	QDir dir(beautyTmpPath);
	if (!dir.exists()) {
		bool ok = dir.mkpath(beautyTmpPath);
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir beauty tmp failed.");
			return;
		}
	}
	QString fileName = zipName.mid(0, zipName.lastIndexOf('.'));
	QString srcFileName = pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + fileName + "/" + fileName + ".json";
	QString destFileName = beautyTmpPath + id + ".json";
	QFile::copy(srcFileName, destFileName); // copy json

	// copy preset/custom image
	srcFileName = pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + fileName + CONFIGS_BEATURY_PRESET_IMAGE_PATH;
	destFileName = pls_get_user_path(CONFIGS_BEATURY_DEFAULT_IMAGE_PATH) + id + "/";

	PLSResourceHandler::moveDirectoryToDest(srcFileName, destFileName, true);

	renameBeautyPreset(beautyTmpPath, itemIndex);

	PLSResourceHandler::moveDirectoryToDest(beautyTmpPath, pls_get_user_path(CONFIGS_BEATURY_USER_PATH), true);

	renameBeautyCustomDir(pls_get_user_path(CONFIGS_BEATURY_DEFAULT_IMAGE_PATH), itemIndex);

	//del uzip files
	QDir uzipDir(pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + fileName);
	uzipDir.removeRecursively();
	PLS_INFO(MAINFILTER_MODULE, "TRACK BEAUTY Item %s uncompressed beauty files finished", id.toUtf8().data());
}

void renameBeautyPreset(const QString &path, const int itemIndex)
{
	QDir sourceDir(path);

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (auto &fileInfo : fileInfoList) {
		if (fileInfo.fileName() == "." || fileInfo.fileName() == ".." || fileInfo.isDir())
			continue;

		if (fileInfo.fileName().endsWith(".json")) {
			QString filename = path + QString::number(itemIndex + 1) + ".json";
			QFile::rename(fileInfo.filePath(), filename);
		}
	}
}
void renameBeautyCustomDir(const QString &path, const int itemIndex)
{
	QDir sourceDir(path);

	QVariantMap renameList;
	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (auto &fileInfo : fileInfoList) {
		if (fileInfo.fileName() == "." || fileInfo.fileName() == ".." || !fileInfo.isDir())
			continue;

		if (fileInfo.fileName().startsWith(CONFIGS_BEATURY_CUSTOM_IMAGE_PATH)) {
			continue;
		}
		QString target = path + CONFIGS_BEATURY_CUSTOM_IMAGE_PATH + QString::number(itemIndex + 1);
		renameList.insert(path + fileInfo.fileName(), target);
	}

	QDir dir;
	for (auto iter = renameList.begin(); iter != renameList.end(); ++iter) {
		if (dir.exists(iter.value().toString())) {
			dir.setPath(iter.value().toString());
			dir.removeRecursively();
		}
		dir.rename(iter.key(), iter.value().toString());
	}
}

using FilterType = std::set<QString>;
bool MoveDirectoryToDest(const QString &srcDir, const QString &destDir, bool isRemove = false, const FilterType &filter = FilterType())
{
	QDir sourceDir(srcDir);
	QDir targetDir(destDir);

	if (!targetDir.exists()) {
		bool ok = targetDir.mkpath(destDir);
		if (!ok) {
			PLS_ERROR(MAINFILTER_MODULE, "mkdir %s failed.", GetFileName(destDir.toStdString().c_str()).c_str());
			return false;
		}
	}

	QFileInfoList fileInfoList = sourceDir.entryInfoList();
	for (auto &fileInfo : fileInfoList) {

		if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
			continue;

		if (fileInfo.isDir()) {
			MoveDirectoryToDest(fileInfo.filePath(), destDir + fileInfo.fileName().append("/"), isRemove, filter);
			continue;
		}

		// Only copy the `filter` type file.
		// If filter is empty, copy all files.
		if (!filter.empty() && filter.find(fileInfo.suffix()) == filter.end())
			continue;

		if (targetDir.exists(fileInfo.fileName())) {
			targetDir.remove(fileInfo.fileName());
		}

		if (!QFile::copy(fileInfo.filePath(), targetDir.filePath(fileInfo.fileName()))) {
			continue;
		}
	}
	if (isRemove) {
		sourceDir.removeRecursively();
	}
	return true;
}

bool PLSBeautyResHandler::CopyBeautyRequiredFile()
{
	return MoveDirectoryToDest(CONFIG_BEAUTY_PATH, pls_get_user_path(CONFIGS_BEATURY_USER_PATH), false, {"cso", "model"});
}

bool PLSBeautyResHandler::CopyBeautyConfigFile()
{
	bool ok = MoveDirectoryToDest(CONFIG_BEAUTY_IMAGE_PATH, pls_get_user_path(CONFIGS_BEATURY_USER_PATH), false) &&
		  MoveDirectoryToDest(CONFIG_BEAUTY_PATH, pls_get_user_path(CONFIGS_BEATURY_USER_PATH), false, {"json"});
	return ok;
}

bool PLSBeautyResHandler::SaveBeautyJsonFile(const QByteArray &data)
{
	if (!PLSJsonDataHandler::saveJsonFile(data, pls_get_user_path(CONFIGS_BEATURY_USER_PATH) + CONFIGS_BEATURY_JSON_FILE)) {
		PLS_WARN(MAINFILTER_MODULE, "save to json beauty.json failed");
		return false;
	}
	return true;
}
