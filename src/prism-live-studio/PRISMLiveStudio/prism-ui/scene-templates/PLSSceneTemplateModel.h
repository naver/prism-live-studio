#pragma once

#include <QObject>
#include <QList>
#include "libresource.h"

class SceneTemplateResource {
public:
	SceneTemplateResource(pls::rsm::Item _item) : item(_item) {}

	void updateItem(pls::rsm::Item _item) { item = _item; }

	void configJsonPath(const QString &value) { item.customAttr("Resource.ConfigJsonPath", value); }
	QString configJsonPath() const { return item ? item.customAttr("Resource.ConfigJsonPath").toString() : QString(); }

	void mainSceneImagePath(const QString &value) { item.customAttr("Resource.MainSceneImagePath", value); }
	QString mainSceneImagePath() const { return item ? item.customAttr("Resource.MainSceneImagePath").toString() : QString(); }

	void mainSceneVideoPath(const QString &value) { item.customAttr("Resource.MainSceneVideoPath", value); };
	QString mainSceneVideoPath() const { return item ? item.customAttr("Resource.MainSceneVideoPath").toString() : QString(); }

	void mainSceneThumbnail_1(const QString &value) { item.customAttr("Resource.MainSceneThumbnail_1", value); }
	QString mainSceneThumbnail_1() const { return item ? item.customAttr("Resource.MainSceneThumbnail_1").toString() : QString(); }

	void mainSceneThumbnail_2(const QString &value) { item.customAttr("Resource.MainSceneThumbnail_2", value); }
	QString mainSceneThumbnail_2() const { return item ? item.customAttr("Resource.MainSceneThumbnail_2").toString() : QString(); }

	void detailSceneThumbnailPathList(const QStringList &value) { item.customAttr("Resource.DetailSceneThumbnail", value); }
	QStringList detailSceneThumbnailPathList() const { return item ? item.customAttr("Resource.DetailSceneThumbnail").toStringList() : QStringList(); }

private:
	pls::rsm::Item item;
};

class SceneTemplateItem {
public:
	SceneTemplateItem() : SceneTemplateItem(pls::rsm::Item{}) {}
	SceneTemplateItem(pls::rsm::Item _item) : item(_item), resource(_item) {}

	void updateItem(pls::rsm::Item _item) { item = _item; }

	QString itemId() const { return item ? item.itemId() : QString(); }
	std::list<pls::rsm::Group> groups() const { return item ? item.groups() : std::list<pls::rsm::Group>{}; }
	int version() const { return item ? item.version() : 0; }
	int scenesNumber() const { return item ? item.attr({"properties", "scenesNumber"}).toInt() : 0; }
	int width() const { return item ? item.attr({"properties", "width"}).toInt() : 0; }
	int height() const { return item ? item.attr({"properties", "height"}).toInt() : 0; }
	bool isAI() const { return item ? item.attr({"properties", "ai"}).toBool() : false; }
	bool isPaid() const { return item ? item.attr("paidFlag").toBool() : false; }
	QString title() const { return item ? item.attr({"properties", "title"}).toString() : QString(); }
	QString resourceUrl() const { return item ? item.attr("resourceUrl").toString() : QString(); }
	QString versionLimit() const { return item ? item.attr({"properties", "versionLimit"}).toString() : QString(); }
	QString resourcePath() const { return item ? item.dir() : QString(); }

	SceneTemplateResource resource;

private:
	pls::rsm::Item item;
};
