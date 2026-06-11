#pragma once
#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>

#include "obs-app.hpp"

enum class NodeErrorType { Ok, FileNotExisted, FileContentError, UnregisterNode, SourceHasUpdate, SourceNotRegistered, NodeNotMatch, SaveFileError, Unknown };

class PLSOverlayNode : public QObject {
	Q_OBJECT
public:
	// parse nodeType from config.json
	virtual NodeErrorType load(const QJsonObject &content) = 0;

	// save to prism scene collection format
	virtual void save(QJsonObject &output) = 0;

	// export overlay file
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr) = 0;
};

class PLSBaseNode : public PLSOverlayNode {
	Q_OBJECT
public:
	explicit PLSBaseNode(const QString &sourceId = "");
	NodeErrorType load(const QJsonObject &content);
	bool checkSourceRegistered();
	bool checkHasUpdate();
	void setForceUpdateSource(bool update);
	void save(QJsonObject &output);
	void clear();
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

	QString getSourceId();

protected:
	QJsonObject outputObject;
	QString sourceId;
	bool forceUpdateSource = false;
};

class PLSRootNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &rootObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
	void clear();

private:
	NodeErrorType loadScenes(const QJsonObject &scenesObject);
	NodeErrorType loadTransitions(const QJsonObject &transitionsObject);

private:
	QString schemaVersion;
	QString nodeType;
	QJsonObject scenesOutputObject;
	QJsonObject transitionsOutputObject;
	QJsonObject chatTemplateOutputObject;
};

class PLSScenesNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &scenesObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
	void clear();

	struct SceneInfo {
		QString name;
		QString sceneId;
		QJsonObject scenesInfo;
	};

private:
	QString schemaVersion;
	QVector<SceneInfo> sceneInfoVec;
};

class PLSSceneSourceNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &scenesObject);
};

class PLSSlotsNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
	void clear();

private:
	QString schemaVersion;
	QString nodeType;
	QVector<QJsonObject> outputObjects;
};

class PLSGroupNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);
	void clear();

private:
	QJsonObject outputObjects;
};

class PLSBrowserNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &slotsObject);
};

class PLSTextNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSSceneItemNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
	void clear();

private:
	struct CropInfo {
		int left = 0;
		int top = 0;
		int right = 0;
		int bottom = 0;
	};

	struct ContentInfo {
		QString schemaVersion;
		QString nodeType;
	};

	QString id;
	QString sceneNodeType;
	QString name;
	QString display;
	float x = 0;
	float y = 0;
	float scaleX = 0;
	float scaleY = 0;
	float rotation = 0;
	int align = -1;
	int boundsType = -1;
	int boundsAlign = -1;
	float boundsX = 0;
	float boundsY = 0;

	CropInfo crop;
	//filter
	QJsonArray filters;

	bool mixerHidden = false;
	bool visible = true;

	//content
	ContentInfo content;

	QJsonObject outputSettings;
	QJsonObject privateSettings;
};

class PLSGameCaptureNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString placeholderFile;
	int width = 0;
	int height = 0;
};

class PLSMediaNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	void save(QJsonObject &output);
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString localFile;
};

class PLSImageNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString fileName;
};

class PLSCameraNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
};

class PLSTransitionNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	void save(QJsonObject &output);
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QJsonObject outputObject;
};

class PLSMusicPlaylistNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSPrismStickerNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSPrismGiphyNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSImageSlideShowNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSVlcVideoNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	NodeErrorType load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};