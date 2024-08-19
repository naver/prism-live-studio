#pragma once
#include <QString>
#include <QJsonObject>

#include "PLSOverlayNode.h"
#include "PLSSceneTemplateModel.h"

const static QString OVERLAY_FILE = ".overlay";

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

	QString loadConfig(const QString &templateName, const QString &path);

	bool loadNodeInfo(const QString &nodeType, const QJsonObject &content, QJsonObject &output);
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

private:
	void registerNodeParser();
	void registerPrismSource();
	void registerObsDefaultSource();
	PLSNodeManager();
	void createCopyFileThread();
	void addNode(NodeType type, PLSBaseNode *baseNode);
	void deleteNode();
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
};
#define PLSNodeManagerPtr PLSNodeManager::instance()
using SNodeType = PLSNodeManager::NodeType;
