#ifndef PLSRESOURCEMGR_H
#define PLSRESOURCEMGR_H

#include <QObject>
#include "PLSHttpApi/PLSHttpHelper.h"
#include "PLSHttpApi/PLSHmacNetworkReplyBuilder.h"
#include "PLSHttpApi/PLSNetworkReplyBuilder.h"
#include "frontend-api.h"
#include "PLSResourceHandler.h"

#include <QMetaEnum>
#include "log/module_names.h"
#include "log.h"

#define VIRTUALFILENAME "virtual_bg.json"
typedef struct ResourceInfo {
	int version;
	QString resourceUrl;
} ResourceInfo;
using downloadCategoryResCallback = std::function<void(bool isSuccess, bool isAleadyShow)>;
class PLSResourceMgr {
	Q_GADGET
public:
	enum class ResourceFlag { Library, SenseTime, Beauty, Color, Music, Virtual_bg, Template, Reaction };
	Q_ENUM(ResourceFlag)
public:
	static PLSResourceMgr *instance();

	void getNewResourceJson(downloadCategoryResCallback callback);
	QString getResourceJson(ResourceFlag resourceFlag);

	/*
	* cannot find the resource when lanuch it ,need redownload it .
	* resourceFlag : resource flag
	* QMap<QString, QString> :key--resUrl, value--Absolute path
	* this interface not suitable for large file downloads
	*/
	void downloadPartResources(ResourceFlag resourceFlag, const QMap<QString, QString> resourceUrlToPaths);
	/*
	* Download all the resources needed for the module 
	*/
	void downloadAllResources(ResourceFlag resourceFlag);
	/*
	* get resourceHandler instance
	* resourceFlag : resource flag
	*/
	PLSResourceHandler *getResourceHandler(ResourceFlag resourceFlag);
	/*
	* Synchronized download sensetime resource 
	*/
	void downloadSenseTimeResources();

	bool isSuccessDownloadCategory();
	void setAleadyShowTips();
	bool isAleadyShowTips();

	void abort();

private:
	explicit PLSResourceMgr();
	~PLSResourceMgr();
	void getLocalResourceInfo();
	void parseResourceJson(QMap<QString, ResourceInfo> &resourceInfos, const QByteArray &resourceJsonData);
	void checkResourceUpdate();
	PLSResourceHandler *creatResourceHandler(ResourceFlag resourceFlag, const QStringList &urls, const QString &resourceId, const QString &relativePath);
	QString getResourcePath(ResourceFlag resourceFlag);
	bool isNotExistPath(const QString &path, ResourceFlag resourceFlag);

private:
	QMap<QString, ResourceInfo> m_localeResourceInfos;
	QMap<QString, ResourceInfo> m_newResourceInfos;
	QByteArray m_LocalCategoriesJsonData;
	QMap<ResourceFlag, QPointer<PLSResourceHandler>> m_resourceHandlers;
	bool m_bSuccessDownloadCategory = false;
	bool m_isAlreadyTips = false;
};

#endif // PLSRESOURCEMGR_H
