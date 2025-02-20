#pragma once

#include <QObject>

#include "PLSSceneTemplateModel.h"
#include "libresource.h"

class CategorySceneTemplate : public QObject, public pls::rsm::ICategory {
	Q_OBJECT
public:
	PLS_RSM_CATEGORY(CategorySceneTemplate)

	QString categoryId(pls::rsm::IResourceManager *mgr) const override { return QStringLiteral("scene_templates"); }

	void jsonLoaded(pls::rsm::IResourceManager *mgr, pls::rsm::Category category) override;

	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override;
	void itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override;
	void groupDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Group group, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override;

	bool checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;

	void allDownload(pls::rsm::IResourceManager *mgr, bool ok) override;

	bool checkItem(const SceneTemplateItem& item) const;
public slots:
	void findResource(SceneTemplateItem &item) const;

signals:
	void onJsonDownloaded() const;
	void onItemDownloaded(const SceneTemplateItem &item) const;
	void onGroupDownloadFailed(const QString &groupId) const;
};

#define PLS_SCENE_TEMPLATE_RESOURCE CategorySceneTemplate::instance()
