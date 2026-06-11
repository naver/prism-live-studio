#pragma once
#include <QObject.h>
#include <libresource.h>

class CategoryPrismSticker : public QObject, public pls::rsm::ICategory {
	Q_OBJECT
public:
	PLS_RSM_CATEGORY(CategoryPrismSticker)

	// Inherited via ICategory
	QString categoryId(pls::rsm::IResourceManager *mgr) const override;
	void jsonDownloaded(pls::rsm::IResourceManager *mgr, const pls::rsm::DownloadResult &result) override;
	void jsonLoaded(pls::rsm::IResourceManager *mgr, pls::rsm::Category category) override;

	bool groupManualDownload(pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const override;
	bool itemManualDownload(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;

	bool itemNeedLoad(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;
	bool checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;

	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override;
	void itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override;

	size_t useMaxCount(pls::rsm::IResourceManager *mgr) const override;

signals:
	void finishDownloadJson(bool ok, bool timeout) const;
	void finishLoadJson();
	void finishDownloadItem(pls::rsm::Item item, bool ok, bool timeout) const;
};