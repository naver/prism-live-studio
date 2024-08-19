#pragma once

#include <QObject>
#include <QList>


namespace SceneTemplateDefine {
constexpr auto SCNENE_TEMPLATE_NEW = "New";
constexpr auto SCNENE_TEMPLATE_HOT = "Popular";
}

struct SceneTemplateResource {
	QString MainSceneImagePath;
	QString MainSceneVideoPath;
	QString MainSceneThumbnail_1;
	QString MainSceneThumbnail_2;
	QStringList DetailSceneThumbnailPathList;
};

struct SceneTemplateItem {
	QString groupId;
	QString itemId;
	int version;
	int scenesNumber;
	int width;
	int height;
	QString title;
	QString resourceUrl;
	QString resourcePath;
	SceneTemplateResource resource;
};

struct SceneTemplateGroup {
	QString groupId;
	QList<SceneTemplateItem> items;
};

struct SceneTemplateList {
	int version;
	QList<SceneTemplateGroup> groups;
};
