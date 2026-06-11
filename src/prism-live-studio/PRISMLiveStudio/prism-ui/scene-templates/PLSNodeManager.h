#pragma once
#include <QString>
#include <QJsonObject>

#include "PLSOverlayNode.h"
#include "PLSSceneTemplateModel.h"

const static QString OVERLAY_FILE = ".overlay";
static const char *FROM_SCENE_TEMPLATE = "fromSceneTemplate";

// this is for keyname in scene collection. as discuss, its name should not include "paid"
static const char *SCENE_PAID_KEY_NAME = "flagInnerAttribute";

class PLSNodeManager;
class CopyFileWorker : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;

public slots:
	void doCopyFile(const QString &src, const QString &dest);
	void doCopyDir(const QString &src, const QString &dest);
	void doZip(const QString &exportDir, const QString &curName);
signals:
	void copyFinished();
	void zipFinished(bool);
};

struct SourceUpgradeDefaultInfo {
	int width = 0;
	int height = 0;
	QString id;
};

class PLSNodeManager : public QObject {
	Q_OBJECT

public:
	enum class NodeType {
		RootNode = 0,
		ScenesNode,
		SlotsNode,
		GroupNode,
		GameCaptureNode,
		MacScreenCaptureNode,
		WindowCaptureNode,
		DisplayCaptureNode,
		DisplayCapturePartNode,
		AudioInputCaptureNode,
		AudioOutputCaptureNode,
		AudioOutputCaptureNodeV2,
		AppAudioCaptureNode,
		Spout2CaptureNode,
		ImageNode,
		ImageSlideShowNode,
		TextNode,
		WebcamNode,
		MacosAvcaptureNode,
		MacosAvcaptureFastNode,
		VideoNode,
		VlcVideoNode,
		SceneItemNode,
		SceneSourceNode,
		WidgetNode,
		TransitionNode,
		PrismChatNode,
		PrismChatTemplateNode,
		VirtualTemplateNode,
		MusicPlaylistNode,
		ColorNode,
		ClockWidgetNode,
		TextTemplateNode,
		ViewerCountNode,
		AudioVisualizerNode,
		PrismLensNode,
		PrismMobileNode,
		PrismStickerNode,
		PrismGiphyNode,
		UndefinedNode
	};
	Q_ENUM(NodeType)

	enum class SceneNodeType { Item, Folder };
	Q_ENUM(SceneNodeType)

	using NodeMap = QMap<NodeType, PLSBaseNode *>;
	using SceneItemMap = QMap<QString, QVector<QJsonObject>>;
	using SceneItemTypeMap = QMap<QString, QJsonObject>;
	using SceneItemUuidMap = QMap<int64_t, QString>;
	using SceneUuidMap = QMap<QString, QString>;

	static PLSNodeManager *instance();
	~PLSNodeManager();

	// read from config.json
	QList<QString> getScenesNames(const SceneTemplateItem &data);

	NodeErrorType loadConfig(const QString &templateName, const QString &path, bool isPaidSceneTemplate, QString &outputPath);

	NodeErrorType loadNodeInfo(const QString &nodeType, const QJsonObject &content, QJsonObject &output);
	bool exportLoadInfo(obs_data_t *settings, obs_data_t *priSettings, NodeType nodeType, QJsonObject &output, void *param = nullptr);

	void clear();
	QString getConfigDataPath();

	NodeType getNodeTypeById(const QString &sourceId);

	NodeType nodeTypeKeyToValue(const QString &nodeType);
	QString nodeTypeValueToKey(NodeType nodeType);

	SceneNodeType sceneNodeTypeKeyToValue(const QString &nodeType);
	QString sceneNodeTypeValueToKey(SceneNodeType nodeType);

	void addSceneItemsInfo(const QString &sceneItemId, const QJsonObject &obejct);
	bool getSceneItemIsGroup(const QString &sceneItemId);
	bool getSceneItemInGroup(const QString &sceneItemId);
	void setSceneItemInGroup(const QString &sceneItemId, bool inGroup);
	QJsonObject getSceneItemsInfo(const QString &sceneItemId);

	QString getSceneItemUuidInfo(const int64_t &sceneItemId);
	void setSceneItemUuidInfo(SceneItemUuidMap uuidMap);

	QString getSceneUuidInfo(const QString &sceneName);
	void setSceneUuidInfo(SceneUuidMap uuidMap);

	void setTemplatesPath(const QString &templatesPath);
	QString getTemplatesPath();

	void doCopy(const QString &src, const QString &dest);
	bool doCopyFinished();
	void setExportDir(const QString &outputDir);
	void setExportName(const QString &zipName);
	void doZip(const QString &zipName);
	QString getExportPath();

	void addFont(const QString &fontPath);

	bool checkSourceHasUpgrade(const QString &id);
	SourceUpgradeDefaultInfo getSourceUpgradeDefaultInfo(const QString &id);
	QString getSourceUpgradeId(const QString &id);

private:
	void registerNodeParser();
	void registerPrismSource();
	void registerObsDefaultSource();
	PLSNodeManager();
	void createCopyFileThread();
	void addNode(NodeType type, PLSBaseNode *baseNode);
	void deleteNode();

	void initSourceUpgradeInfo();
	void updateSourcePath(bool isPaidSceneTemplate, const QString &originalPath);
signals:
	void zipFinished(bool);

private:
	NodeMap nodeMap;
	QMap<QString, NodeType> idMap;
	SceneUuidMap sceneUuidMap;
	SceneItemTypeMap sceneItemsMap;
	SceneItemUuidMap sceneItemUuidMap;
	QString templatesPath;
	QString exportDir;
	QString exportZipName;
	int copyTimes = 0;

	CopyFileWorker *copyFileWorker = nullptr;
	QThread *copyFileThread = nullptr;
	bool isCopyFinished = true;

	QMap<QString, QString> sourceUpdateMap;
	QMap<QString, SourceUpgradeDefaultInfo> sourceUpgradeDefaultInfo;
};
#define PLSNodeManagerPtr PLSNodeManager::instance()
using SNodeType = PLSNodeManager::NodeType;
