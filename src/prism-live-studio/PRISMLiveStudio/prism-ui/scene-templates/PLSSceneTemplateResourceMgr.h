#pragma once

#include <atomic>

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <mutex>

#include "PLSSceneTemplateModel.h"

class PLSSceneTemplateResourceMgr : public QObject {
	Q_OBJECT

public:
	static PLSSceneTemplateResourceMgr &instance();

	const SceneTemplateList &getSceneTemplateList() const;
	const QList<SceneTemplateItem> &getSceneTemplateGroup(const QString &groupId) const;
	QList<SceneTemplateItem> getSceneTemplateValidGroup(const QString &groupId) const;
	const SceneTemplateItem &getSceneTemplateItem(const QString &itemId) const;
	QStringList getGroupIdList();
	bool isListFinished() const;
	bool isItemsFinished() const;
	bool isListValid() const;
	bool isItemValid(const SceneTemplateItem &item) const;
	bool isItemsValid(const QString &groupId) const;
	bool isDownloadingFinished() const;

public slots:
	void downloadList(const QString &categoryPath);
	void downloadList();
	void downloadItems();
	void downloadItem(SceneTemplateItem &item);
	void onItemDownloaded(SceneTemplateItem &item, bool bSuccess);

signals:
	void onListFinished(const SceneTemplateList &);
	void onItemFinished(const SceneTemplateItem &);
	void onItemsFinished();

protected:
	bool parseJson();
	void findResource(SceneTemplateItem &item);
	void loadFileVersion(const QString &path);

private:
	PLSSceneTemplateResourceMgr(QObject * = nullptr);
	~PLSSceneTemplateResourceMgr();

	QString m_strResourceUrl;
	QString m_strJsonPath;

	bool m_bListFinished = false;
	bool m_bItemsFinished = false;
	SceneTemplateList m_listSceneTemplate;
	int m_iListVersion = -1;
	QMap<QString, int> m_mapItemVersion;

	QMap<QString, QList<SceneTemplateItem*>> m_setDownloadingItem;

	std::atomic_int m_iDownloadingCount = 0;
};

#define PLS_SCENE_TEMPLATE_RESOURCE PLSSceneTemplateResourceMgr::instance()
