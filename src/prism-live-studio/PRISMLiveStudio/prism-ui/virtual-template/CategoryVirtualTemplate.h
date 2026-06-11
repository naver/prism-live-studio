#ifndef PLSMOTIONNETWORK_H
#define PLSMOTIONNETWORK_H

#include <QObject>
#include <functional>
#include <qnetworkaccessmanager.h>
#include <QNetworkReply>
#include <QPointer>
#include <QMap>
#include <QThread>
#include <QFile>
#include <QTimer>
#include <chrono>
#include "PLSMotionDefine.h"
#include "libresource.h"

class PLSMotionFileManager;

#define PRISM_STR QStringLiteral("PRISM")
#define FREE_STR QStringLiteral("FREE")
#define MY_STR QStringLiteral("MY")
#define RECENT_STR QStringLiteral("RECENT")

const int MAX_RECENT_COUNT = 30;

class CategoryVirtualTemplate : public QObject, public pls::rsm::ICategory {
	Q_OBJECT
public:
	PLS_RSM_CATEGORY(CategoryVirtualTemplate)
	QString categoryId(pls::rsm::IResourceManager *mgr) const override { return PLS_RSM_CID_VIRTUAL_BG; }

	bool groupNeedDownload(pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const override;

	void getCustomGroupExtras(qsizetype &pos, bool &archive, pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const override;
	void getCustomItemExtras(qsizetype &pos, bool &archive, pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;

	void getItemDownloadUrlAndHowSaves(pls::rsm::IResourceManager *mgr, std::list<pls::rsm::UrlAndHowSave> &urlAndHowSaves, pls::rsm::Item item) const override;
	void itemDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Item item, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override;
	void groupDownloaded(pls::rsm::IResourceManager *mgr, pls::rsm::Group group, bool ok, const std::list<pls::rsm::DownloadResult> &results) const override;
	bool groupNeedLoad(pls::rsm::IResourceManager *mgr, pls::rsm::Group group) const override;

	bool checkItem(pls::rsm::IResourceManager *mgr, pls::rsm::Item item) const override;
	void allDownload(pls::rsm::IResourceManager *mgr, bool ok) override;

	virtual size_t useMaxCount(pls::rsm::IResourceManager *mgr) const;

	QList<MotionData> getFreeList() const;
	QList<MotionData> getPrismList() const;
	QList<MotionData> getRecentList() const;
	QList<MotionData> getMyList();
	MotionData getMotionDataById(const QString &itemId);
	QList<MotionData> getGroupList(const QString &group) const;

	void removeAllCustomGroups();

	bool isPathEqual(const MotionData &md1, const MotionData &md2) const;
	bool groupDownloadRequestFinished(const QString &group);
signals:
	void resourceDownloadFinished(const MotionData &data, bool update = false) const;
	void allDownloadFinished(bool ok);
	void groupListFinishedSignal() const;
};

#define CategoryVirtualTemplateInstance CategoryVirtualTemplate::instance()

#endif // PLSMOTIONNETWORK_H
