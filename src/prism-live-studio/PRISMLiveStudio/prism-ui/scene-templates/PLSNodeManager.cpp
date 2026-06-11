#include "PLSNodeManager.h"
#include "libutils-api.h"
#include "liblog.h"
#include "pls-common-define.hpp"
#include "pls/pls-obs-api.h"
#include "window-basic-main.hpp"
#include "libresource.h"
#include "pls/pls-source.h"

#include <QMetaEnum>
#include <QDir>
#include <QFontDatabase>
using namespace common;

static const char *nodeMgrModuleName = "PLSNodeManager";

void CopyFileWorker::doCopyFile(const QString &src, const QString &dest)
{
	QString error;
	if (!pls_copy_file(src, dest, &error)) {
		PLS_DEBUG(nodeMgrModuleName, "copy %s to %s failed : %s.", src.toStdString().c_str(), dest.toStdString().c_str(), error.toStdString().c_str());
	}
	emit copyFinished();
}

void CopyFileWorker::doCopyDir(const QString &src, const QString &dest)
{
	QString error;
	if (pls_copy_dir(src, dest)) {
		pls_remove_dir(src);
	} else {
		PLS_DEBUG(nodeMgrModuleName, "copy %s to %s failed : %s.", src.toStdString().c_str(), dest.toStdString().c_str(), error.toStdString().c_str());
	}

	emit copyFinished();
}

void CopyFileWorker::doZip(const QString &exportDir, const QString &curName)
{
	QString exportPath = exportDir + curName;
	QString zipPath = exportPath + OVERLAY_FILE;
	if (QFile::exists(zipPath)) {
		QFile::remove(zipPath);
	}

	QString error;
	bool res = pls::rsm::zip(exportPath, zipPath, &error);
	if (!res) {
		PLS_WARN(nodeMgrModuleName, "zip failed : %s.", error.toStdString().c_str());
	}

	QDir tempDir(exportPath);
	tempDir.removeRecursively();
	emit zipFinished(res);
}

PLSNodeManager *PLSNodeManager::instance()
{
	static PLSNodeManager instance;
	return &instance;
}

PLSNodeManager::~PLSNodeManager()
{
	pls_delete_thread(copyFileThread);
	deleteNode();
}

QList<QString> PLSNodeManager::getScenesNames(const SceneTemplateItem &data)
{
	QList<QString> scenesNames;
	if (data.resourcePath().isEmpty()) {
		return scenesNames;
	}

	auto configJson = "config.json";
	auto configPath = pls_find_subdir_contains_spec_file(data.resourcePath(), configJson);
	if (configPath) {
		auto file = QDir(configPath.value()).filePath(configJson);
		if (!QFile::exists(file)) {
			PLS_WARN(nodeMgrModuleName, "config.json was not existed.");
			return scenesNames;
		}

		QJsonObject rootObject;
		pls_read_json(rootObject, file);
		QJsonArray items = rootObject.value("scenes").toObject().value("items").toArray();
		for (auto item : items) {
			QString sceneName = item.toObject().value("name").toString();
			if (sceneName.isEmpty()) {
				continue;
			}
			scenesNames.push_back(sceneName);
		}
	}

	return scenesNames;
}

NodeErrorType PLSNodeManager::loadConfig(const QString &templateName, const QString &path, bool isPaidSceneTemplate, QString &outputPath)
{
	auto file = QDir(path).filePath("config.json");
	if (!QFile::exists(file)) {
		PLS_WARN(nodeMgrModuleName, "%s/config.json was not existed.", templateName.toStdString().c_str());
		return NodeErrorType::FileNotExisted;
	}

	QJsonObject rootObject;
	if (!pls_read_json(rootObject, file)) {
		PLS_WARN(nodeMgrModuleName, "get content from %s/config.json failed.", templateName.toStdString().c_str());
		return NodeErrorType::FileContentError;
	}

	QString nodeType = rootObject["nodeType"].toString();
	auto node = PLSNodeManagerPtr->nodeTypeKeyToValue(nodeType);
	if (node != SNodeType::RootNode) {
		PLS_WARN(nodeMgrModuleName, "root node was not existed in config.json.");
		return NodeErrorType::NodeNotMatch;
	}

	addFont(path);

	PLSNodeManagerPtr->updateSourcePath(isPaidSceneTemplate, path);

	QJsonObject outputObject;
	auto errType = PLSNodeManagerPtr->loadNodeInfo(nodeType, rootObject, outputObject);
	if (errType != NodeErrorType::Ok) {
		clear();
		return errType;
	}
	outputObject["name"] = templateName;
	outputObject[SCENE_PAID_KEY_NAME] = isPaidSceneTemplate;
	outputPath = QDir(path).filePath("output.json");
	QString error;
	if (pls_write_json(outputPath, outputObject, &error)) {
		return NodeErrorType::Ok;
	}
	PLS_WARN(nodeMgrModuleName, "save json error : %s.", error.toStdString().c_str());
	return NodeErrorType::SaveFileError;
}

void PLSNodeManager::createCopyFileThread()
{
	if (copyFileWorker) {
		return;
	}
	copyFileWorker = pls_new<CopyFileWorker>();
	copyFileThread = pls_new<QThread>();
	connect(copyFileThread, &QThread::finished, [this]() { pls_delete(copyFileWorker); });
	connect(
		copyFileWorker, &CopyFileWorker::copyFinished, this,
		[this]() {
			copyTimes--;

			if (0 == copyTimes) {
				isCopyFinished = true;
				QMetaObject::invokeMethod(copyFileWorker, "doZip", Qt::QueuedConnection, Q_ARG(const QString &, exportDir), Q_ARG(const QString &, exportZipName));
			}
		},
		Qt::QueuedConnection);
	connect(copyFileWorker, &CopyFileWorker::zipFinished, this, [this](bool res) { emit zipFinished(res); }, Qt::QueuedConnection);
	copyFileWorker->moveToThread(copyFileThread);
	copyFileThread->start();
}

void PLSNodeManager::addNode(NodeType type, PLSBaseNode *baseNode)
{
	nodeMap.insert(type, baseNode);
	idMap.insert(baseNode->getSourceId(), type);
}

void PLSNodeManager::deleteNode()
{
	for (auto key : nodeMap.keys()) {
		auto node = nodeMap.value(key);
		if (node) {
			delete node;
			node = nullptr;
		}
	}
}

void PLSNodeManager::initSourceUpgradeInfo()
{
	// source default info
	sourceUpgradeDefaultInfo.insert(PRISM_CHAT_SOURCE_ID, {488, 784, PRISM_CHAT_SOURCE_ID});
	sourceUpgradeDefaultInfo.insert(PRISM_CHATV2_SOURCE_ID, {360, 650, PRISM_CHATV2_SOURCE_ID});

	// source update to
	sourceUpdateMap.insert(PRISM_CHAT_SOURCE_ID, PRISM_CHATV2_SOURCE_ID);
}

void PLSNodeManager::updateSourcePath(bool isPaidSceneTemplate, const QString &originalPath)
{
	auto trialPath = originalPath;
	trialPath.replace("\\", "/");
	PLSNodeManagerPtr->setTemplatesPath(trialPath);
}

NodeErrorType PLSNodeManager::loadNodeInfo(const QString &nodeType, const QJsonObject &content, QJsonObject &output)
{
	NodeType type = nodeTypeKeyToValue(nodeType);

	auto iter = nodeMap.find(type);
	if (iter == nodeMap.end()) {
		PLS_WARN(nodeMgrModuleName, "node type : %s has not reigstered.", nodeType.toStdString().c_str());
		return NodeErrorType::UnregisterNode;
	}

	PLSBaseNode *nodeParser = iter.value();
	if (!nodeParser) {
		PLS_WARN(nodeMgrModuleName, "node type : %s has not reigstered.", nodeType.toStdString().c_str());
		return NodeErrorType::UnregisterNode;
	}

	if (!nodeParser->checkSourceRegistered()) {
		PLS_WARN(nodeMgrModuleName, "id : %s has not found.", nodeParser->getSourceId().toStdString().c_str());
		return NodeErrorType::SourceNotRegistered;
	}

	if (nodeParser->checkHasUpdate()) {
		nodeParser->setForceUpdateSource(true);
		PLS_WARN(nodeMgrModuleName, "id : %s has updated.", nodeParser->getSourceId().toStdString().c_str());
	}

	auto res = nodeParser->load(content);
	if (NodeErrorType::Ok == res) {
		nodeParser->save(output);
	}
	nodeParser->clear();
	return res;
}

bool PLSNodeManager::exportLoadInfo(obs_data_t *settings, obs_data_t *priSettings, NodeType nodeType, QJsonObject &output, void *param)
{
	auto iter = nodeMap.find(nodeType);
	if (iter == nodeMap.end()) {
		return false;
	}

	PLSOverlayNode *nodeParser = iter.value();
	if (!nodeParser) {
		return false;
	}
	if (nodeParser->doExport(settings, priSettings, output, param)) {
		return true;
	}
	return false;
}

void PLSNodeManager::clear()
{
	sceneItemsMap.clear();
	sceneUuidMap.clear();
	sceneItemUuidMap.clear();
}

QString PLSNodeManager::getConfigDataPath()
{
	QString dataPath = pls_get_user_path(SCENE_TEMPLATE_DIR);
	QDir dir(dataPath);
	if (!dir.exists()) {
		dir.mkpath(dataPath);
	}
	return dataPath;
}

SNodeType PLSNodeManager::getNodeTypeById(const QString &sourceId)
{
	auto iter = idMap.find(sourceId);
	if (iter != idMap.end()) {
		return iter.value();
	}
	return SNodeType::UndefinedNode;
}

SNodeType PLSNodeManager::nodeTypeKeyToValue(const QString &nodeType)
{
	return static_cast<NodeType>(QMetaEnum::fromType<NodeType>().keyToValue(nodeType.toStdString().c_str()));
}

QString PLSNodeManager::nodeTypeValueToKey(NodeType nodeType)
{
	return QMetaEnum::fromType<NodeType>().valueToKey(static_cast<int>(nodeType));
}

PLSNodeManager::SceneNodeType PLSNodeManager::sceneNodeTypeKeyToValue(const QString &nodeType)
{
	return static_cast<SceneNodeType>(QMetaEnum::fromType<SceneNodeType>().keyToValue(nodeType.toStdString().c_str()));
}

QString PLSNodeManager::sceneNodeTypeValueToKey(SceneNodeType nodeType)
{
	return QMetaEnum::fromType<SceneNodeType>().valueToKey(static_cast<int>(nodeType));
}

void PLSNodeManager::addSceneItemsInfo(const QString &sceneItemId, const QJsonObject &obejct)
{
	sceneItemsMap.insert(sceneItemId, obejct);
}

bool PLSNodeManager::getSceneItemIsGroup(const QString &sceneItemId)
{
	auto iter = sceneItemsMap.find(sceneItemId);
	if (iter != sceneItemsMap.end()) {
		QJsonObject data = iter.value();
		return data["group"].toBool();
	}
	return false;
}

bool PLSNodeManager::getSceneItemInGroup(const QString &sceneItemId)
{
	auto iter = sceneItemsMap.find(sceneItemId);
	if (iter != sceneItemsMap.end()) {
		QJsonObject data = iter.value();
		return data["inGroup"].toBool();
	}
	return false;
}

void PLSNodeManager::setSceneItemInGroup(const QString &sceneItemId, bool inGroup)
{
	auto iter = sceneItemsMap.find(sceneItemId);
	if (iter != sceneItemsMap.end()) {
		QJsonObject &data = iter.value();
		data["inGroup"] = inGroup;
	}
}

QJsonObject PLSNodeManager::getSceneItemsInfo(const QString &sceneItemId)
{
	auto iter = sceneItemsMap.find(sceneItemId);
	if (iter != sceneItemsMap.end()) {
		return iter.value();
	}
	return QJsonObject();
}

QString PLSNodeManager::getSceneItemUuidInfo(const int64_t &sceneItemId)
{
	auto iter = sceneItemUuidMap.find(sceneItemId);
	if (iter != sceneItemUuidMap.end()) {
		return iter.value();
	}
	return QString();
}

void PLSNodeManager::setSceneItemUuidInfo(SceneItemUuidMap uuidMap)
{
	sceneItemUuidMap.swap(uuidMap);
}

QString PLSNodeManager::getSceneUuidInfo(const QString &sceneName)
{
	auto iter = sceneUuidMap.find(sceneName);
	if (iter != sceneUuidMap.end()) {
		return iter.value();
	}
	return QString();
}

void PLSNodeManager::setSceneUuidInfo(SceneUuidMap uuidMap)
{
	sceneUuidMap.swap(uuidMap);
}

void PLSNodeManager::setTemplatesPath(const QString &templatesPath_)
{
	templatesPath = templatesPath_;
}

QString PLSNodeManager::getTemplatesPath()
{
	return templatesPath + "/";
}

void PLSNodeManager::doCopy(const QString &src, const QString &dest)
{
	QFileInfo info(src);
	bool isFile = info.isFile();
	bool isDir = info.isDir();
	if (!isFile && !isDir) {
		return;
	}

	isCopyFinished = false;
	copyTimes++;
	if (isFile) {
		QMetaObject::invokeMethod(copyFileWorker, "doCopyFile", Qt::QueuedConnection, Q_ARG(const QString &, src), Q_ARG(const QString &, dest));
	} else {
		QMetaObject::invokeMethod(copyFileWorker, "doCopyDir", Qt::QueuedConnection, Q_ARG(const QString &, src), Q_ARG(const QString &, dest));
	}
}

bool PLSNodeManager::doCopyFinished()
{
	return isCopyFinished;
}

void PLSNodeManager::setExportDir(const QString &outputDir)
{
	exportDir = outputDir;
}

void PLSNodeManager::setExportName(const QString &zipName)
{
	exportZipName = zipName;
}

void PLSNodeManager::doZip(const QString &zipName)
{
	QMetaObject::invokeMethod(copyFileWorker, "doZip", Qt::QueuedConnection, Q_ARG(const QString &, exportDir), Q_ARG(const QString &, zipName));
}

QString PLSNodeManager::getExportPath()
{
	return QDir(exportDir).filePath(exportZipName) + "/";
}

void PLSNodeManager::addFont(const QString &fontPath)
{
	auto customFontPath = pls_get_user_path(QString(SCENE_TEMPLATE_DIR).append("custom_font/"));
	pls_enum_dir(
		fontPath,
		[&customFontPath](const QString &reldir, const QFileInfo &file) {
			if (file.isFile()) {
				pls_copy_file(file.absoluteFilePath(), customFontPath + file.fileName());
			}
			return true;
		},
		[](const QString &reldir, const QFileInfo &file) {
			if (file.isDir()) {
				return false;
			}
			auto suffix = file.completeSuffix().toLower();
			if (0 == suffix.compare("ttf") || 0 == suffix.compare("otf") || 0 == suffix.compare("ttc")) {
				return false;
			}
			return true;
		});

	pls_add_custom_font(customFontPath);
}

bool PLSNodeManager::checkSourceHasUpgrade(const QString &id)
{
	auto iter = sourceUpdateMap.find(id);
	if (iter == sourceUpdateMap.end()) {
		return false;
	}
	return (nullptr != obs_source_get_display_name(iter.value().toStdString().c_str()));
}

SourceUpgradeDefaultInfo PLSNodeManager::getSourceUpgradeDefaultInfo(const QString &id)
{
	auto iter = sourceUpgradeDefaultInfo.find(id);
	if (iter != sourceUpgradeDefaultInfo.end()) {
		return iter.value();
	}
	return SourceUpgradeDefaultInfo();
}

QString PLSNodeManager::getSourceUpgradeId(const QString &id)
{
	auto iter = sourceUpdateMap.find(id);
	if (iter != sourceUpdateMap.end()) {
		return iter.value();
	}
	return QString();
}

void PLSNodeManager::registerNodeParser()
{
	// Base Node
	PLSRootNode *root = pls_new<PLSRootNode>();
	addNode(NodeType::RootNode, root);

	PLSSlotsNode *slotsParser = pls_new<PLSSlotsNode>();
	addNode(NodeType::SlotsNode, slotsParser);

	PLSSceneItemNode *sceneItem = pls_new<PLSSceneItemNode>();
	addNode(NodeType::SceneItemNode, sceneItem);
	//
	// OBS Source Node
	registerObsDefaultSource();

	// Prism Source Node
	registerPrismSource();

	//Transition Source Node
	PLSTransitionNode *transition = pls_new<PLSTransitionNode>("");
	addNode(NodeType::TransitionNode, transition);
}

void PLSNodeManager::registerPrismSource()
{
	PLSBaseNode *prismLens = pls_new<PLSBaseNode>(PRISM_LENS_SOURCE_ID);
	addNode(NodeType::PrismLensNode, prismLens);

	PLSBaseNode *prismMobile = pls_new<PLSBaseNode>(PRISM_LENS_MOBILE_SOURCE_ID);
	addNode(NodeType::PrismMobileNode, prismMobile);

	PLSBaseNode *textTemplate = pls_new<PLSBaseNode>(PRISM_TEXT_TEMPLATE_ID);
	addNode(NodeType::TextTemplateNode, textTemplate);

	PLSBaseNode *chat = pls_new<PLSBaseNode>(PRISM_CHAT_SOURCE_ID);
	addNode(NodeType::PrismChatNode, chat);

	PLSBaseNode *chatTemplate = pls_new<PLSBaseNode>(PRISM_CHATV2_SOURCE_ID);
	addNode(NodeType::PrismChatTemplateNode, chatTemplate);

	PLSBaseNode *viewer = pls_new<PLSBaseNode>(PRISM_VIEWER_COUNT_SOURCE_ID);
	addNode(NodeType::ViewerCountNode, viewer);

	PLSPrismStickerNode *prismSticker = pls_new<PLSPrismStickerNode>(PRISM_STICKER_SOURCE_ID);
	addNode(NodeType::PrismStickerNode, prismSticker);

	PLSPrismGiphyNode *prismGiphy = pls_new<PLSPrismGiphyNode>(PRISM_GIPHY_STICKER_SOURCE_ID);
	addNode(NodeType::PrismGiphyNode, prismGiphy);

	PLSMusicPlaylistNode *musicPlaylist = pls_new<PLSMusicPlaylistNode>(BGM_SOURCE_ID);
	addNode(NodeType::MusicPlaylistNode, musicPlaylist);

	PLSBaseNode *visual = pls_new<PLSBaseNode>(PRISM_SPECTRALIZER_SOURCE_ID);
	addNode(NodeType::AudioVisualizerNode, visual);

	PLSBaseNode *virtualTemplate = pls_new<PLSBaseNode>(PRISM_BACKGROUND_TEMPLATE_SOURCE_ID);
	addNode(NodeType::VirtualTemplateNode, virtualTemplate);

	PLSBaseNode *clock = pls_new<PLSBaseNode>(PRISM_TIMER_SOURCE_ID);
	addNode(NodeType::ClockWidgetNode, clock);
}

void PLSNodeManager::registerObsDefaultSource()
{
	PLSCameraNode *webcam = pls_new<PLSCameraNode>(OBS_DSHOW_SOURCE_ID);
	addNode(NodeType::WebcamNode, webcam);

	PLSBaseNode *macosAvcapture = pls_new<PLSBaseNode>("macos-avcapture");
	addNode(NodeType::MacosAvcaptureNode, macosAvcapture);

	PLSBaseNode *macosAvcaptureFast = pls_new<PLSBaseNode>("macos-avcapture-fast");
	addNode(NodeType::MacosAvcaptureFastNode, macosAvcaptureFast);

	PLSBaseNode *audioInput = pls_new<PLSBaseNode>(AUDIO_INPUT_SOURCE_ID);
	addNode(NodeType::AudioInputCaptureNode, audioInput);

	PLSBaseNode *audioOutput = pls_new<PLSBaseNode>(AUDIO_OUTPUT_SOURCE_ID);
	addNode(NodeType::AudioOutputCaptureNode, audioOutput);

	PLSBaseNode *audioOutputV2 = pls_new<PLSBaseNode>(AUDIO_OUTPUT_SOURCE_ID_V2);
	addNode(NodeType::AudioOutputCaptureNodeV2, audioOutputV2);

	PLSBaseNode *appAudioCapture = pls_new<PLSBaseNode>(OBS_APP_AUDIO_CAPTURE_ID);
	addNode(NodeType::AppAudioCaptureNode, appAudioCapture);

	PLSGameCaptureNode *gameCapture = pls_new<PLSGameCaptureNode>(GAME_SOURCE_ID);
	addNode(NodeType::GameCaptureNode, gameCapture);

	PLSBaseNode *display = pls_new<PLSBaseNode>(PRISM_MONITOR_SOURCE_ID);
	addNode(NodeType::DisplayCaptureNode, display);

	PLSBaseNode *region = pls_new<PLSBaseNode>(PRISM_REGION_SOURCE_ID);
	addNode(NodeType::DisplayCapturePartNode, region);

	PLSBaseNode *macScreenCapture = pls_new<PLSBaseNode>(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID);
	addNode(NodeType::MacScreenCaptureNode, macScreenCapture);

	PLSBaseNode *window = pls_new<PLSBaseNode>(WINDOW_SOURCE_ID);
	addNode(NodeType::WindowCaptureNode, window);

	PLSBaseNode *spout2Capture = pls_new<PLSBaseNode>(OBS_INPUT_SPOUT_CAPTURE_ID);
	addNode(NodeType::Spout2CaptureNode, spout2Capture);

	PLSBrowserNode *browser = pls_new<PLSBrowserNode>(BROWSER_SOURCE_ID);
	addNode(NodeType::WidgetNode, browser);

	PLSMediaNode *video = pls_new<PLSMediaNode>(MEDIA_SOURCE_ID);
	addNode(NodeType::VideoNode, video);

	PLSImageNode *image = pls_new<PLSImageNode>(IMAGE_SOURCE_ID);
	addNode(NodeType::ImageNode, image);

	PLSImageSlideShowNode *imageSlider = pls_new<PLSImageSlideShowNode>(SLIDESHOW_SOURCE_ID);
	addNode(NodeType::ImageSlideShowNode, imageSlider);

	PLSBaseNode *color = pls_new<PLSBaseNode>(COLOR_SOURCE_ID_V3);
	addNode(NodeType::ColorNode, color);

	PLSTextNode *text = pls_new<PLSTextNode>(GDIP_TEXT_SOURCE_ID_V2);
	addNode(NodeType::TextNode, text);

#if defined(Q_OS_WINDOWS)
	addNode(NodeType::TextNode, pls_new<PLSTextNode>("text_gdiplus_v3"));
#endif // Q_OS_WINDOWS

	PLSScenesNode *scenes = pls_new<PLSScenesNode>(SCENE_SOURCE_ID);
	addNode(NodeType::ScenesNode, scenes);

	PLSSceneSourceNode *scene = pls_new<PLSSceneSourceNode>(SCENE_SOURCE_ID);
	addNode(NodeType::SceneSourceNode, scene);

	PLSVlcVideoNode *vlcVideo = pls_new<PLSVlcVideoNode>(VLC_SOURCE_ID);
	addNode(NodeType::VlcVideoNode, vlcVideo);

	PLSGroupNode *group = pls_new<PLSGroupNode>(GROUP_SOURCE_ID);
	addNode(NodeType::GroupNode, group);
}

PLSNodeManager::PLSNodeManager()
{
	initSourceUpgradeInfo();
	registerNodeParser();
	createCopyFileThread();
}