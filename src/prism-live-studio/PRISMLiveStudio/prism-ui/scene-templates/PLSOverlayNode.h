#pragma once
#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>

#include "obs-app.hpp"

class PLSOverlayNode : public QObject {
	Q_OBJECT
public:
	// parse nodeType from config.json
	virtual bool load(const QJsonObject &content) = 0;

	// save to prism scene collection format
	virtual void save(QJsonObject &output) = 0;

	// export overlay file
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr) = 0;
};

class PLSBaseNode : public PLSOverlayNode {
	Q_OBJECT
public:
	explicit PLSBaseNode(const QString &sourceId = "");
	bool load(const QJsonObject &content);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

	QString getSourceId();

protected:
	QJsonObject outputObject;
	QString sourceId;
};

class PLSRootNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &rootObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	bool loadScenes(const QJsonObject &scenesObject);
	bool loadTransitions(const QJsonObject &transitionsObject);

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
	bool load(const QJsonObject &scenesObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

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
	bool load(const QJsonObject &scenesObject);
};

class PLSSlotsNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString schemaVersion;
	QString nodeType;
	QVector<QJsonObject> outputObjects;
};

class PLSGroupNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);

private:
	QJsonObject outputObjects;
};

class PLSBrowserNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &slotsObject);
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
	bool load(const QJsonObject &slotsObject);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

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
};

class PLSGameCaptureNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
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
	bool load(const QJsonObject &content);
	void save(QJsonObject &output);
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString localFile;
	QJsonObject outputObject;
};

class PLSImageNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	void save(QJsonObject &output);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QString fileName;
	QJsonObject outputObject;
};

class PLSCameraNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
};

class PLSTransitionNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	void save(QJsonObject &output);
	virtual bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);

private:
	QJsonObject outputObject;
};

class PLSMusicPlaylistNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSPrismStickerNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSPrismGiphyNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSImageSlideShowNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};

class PLSVlcVideoNode : public PLSBaseNode {
	Q_OBJECT
	using PLSBaseNode::PLSBaseNode;

public:
	bool load(const QJsonObject &content);
	bool doExport(obs_data_t *settings, obs_data_t *priSettings, QJsonObject &output, void *param = nullptr);
};