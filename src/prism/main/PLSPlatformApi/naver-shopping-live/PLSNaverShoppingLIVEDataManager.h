#ifndef PLSNAVERSHOPPINGLIVEDATAMANAGER_H
#define PLSNAVERSHOPPINGLIVEDATAMANAGER_H

#include <QObject>
#include <QReadWriteLock>
#include <QSvgRenderer>

#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSMotionFileManager.h"

class PLSNaverShoppingLIVEImageProcessFinished;
class PLSPlatformNaverShoppingLIVE;

class PLSPLSNaverShoppingLIVEImageLoadProcessor : public QObject {
	Q_OBJECT

public:
	PLSPLSNaverShoppingLIVEImageLoadProcessor() {}
	~PLSPLSNaverShoppingLIVEImageLoadProcessor() {}

public:
	void process(PLSNaverShoppingLIVEImageProcessFinished *finished, const QString &url, const QString &imagePath);
};

class PLSPLSNaverShoppingLIVEImageLoadThread : public PLSResourcesProcessThread<PLSPLSNaverShoppingLIVEImageLoadProcessor, std::tuple<PLSNaverShoppingLIVEImageProcessFinished *, QString, QString>> {
public:
	PLSPLSNaverShoppingLIVEImageLoadThread() {}
	~PLSPLSNaverShoppingLIVEImageLoadThread() {}

protected:
	virtual void process();
};

class PLSNaverShoppingLIVEDataManager : public QObject {
	Q_OBJECT

private:
	explicit PLSNaverShoppingLIVEDataManager(QObject *parent = nullptr);
	~PLSNaverShoppingLIVEDataManager();

public:
	struct SmartStoreInfo {
		QString storeId;
		QString storeName;
		QString accessToken;
	};

public:
	static PLSNaverShoppingLIVEDataManager *instance();

public:
	static QString getDirPath(const QString &path);
	static QString getFilePath(const QString &path);
	static QString getCacheDirPath();

	static bool priceOrRateIsInteger(double price);
	static qint64 priceOrRateToInteger(double price);
	static QString convertPrice(double price);
	static QString convertRate(double rate);

public:
	QStringList getSearchKeywords() const;
	QStringList addSearchKeyword(const QString &searchKeyword);
	void removeSearchKeyword(const QString &searchKeyword);
	void clearSearchKeywords();

	void loadSearchKeywords();
	void saveSearchKeywords();

	void downloadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &url, std::function<void(bool ok, const QString &imagePath)> callback, QObject *receiver,
			   PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);
	void downloadImage(PLSPlatformNaverShoppingLIVE *platform, const QString &url, std::function<void()> beforeDownload, std::function<void(bool ok, const QString &imagePath)> callback,
			   QObject *receiver, PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);
	void downloadImages(PLSPlatformNaverShoppingLIVE *platform, const QStringList &urls, std::function<void(const QMap<QString, QString> &imagePaths)> callback, QObject *receiver,
			    PLSNaverShoppingLIVEAPI::ReceiverIsValid receiverIsValid = nullptr, int timeout = PRISM_NET_REQUEST_TIMEOUT);

	void loadDownloadImageCache();
	void saveDownloadImageCache();

	bool findDownloadImagePath(QString &imagePath, const QString &imageUrl) const;
	bool findValidDownloadImagePath(QString &imagePath, const QString &imageUrl) const;
	void addDownloadImagePath(const QString &imagePath, const QString &imageUrl);

	bool hasRecentProductNos() const;
	QList<qint64> getRecentProductNos() const;
	QList<qint64> getRecentProductNos(bool &hasNextPage, int currentPage, int pageSize) const;
	bool addRecentProductNo(qint64 productNo);
	void addRecentProductNos(const QList<qint64> &productNos);
	void removeRecentProductNo(qint64 productNo);
	void clearRecentProductNos();

	void loadRecentProductNos();
	void saveRecentProductNos();

	int getLatestUseTab() const;
	void setLatestUseTab(int latestUseTab);
	void clearLatestUseTab(bool autoSave = true);

	QString getLatestStoreId() const;
	void setLatestStoreId(const QString &latestStoreId);
	void clearLatestStoreId(bool autoSave = true);

	void loadOtherInfos();
	void saveOtherInfos();

	QSvgRenderer &getDefaultImage();
	QSvgRenderer &getLiveProductBadgeImage();

	bool hasThumbnailPixmap(const QString &url);
	bool getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url, const QString &imagePath);
	bool getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url, const QString &imagePath, const QSize &size);
	bool getThumbnailPixmap(QPixmap &original, QPixmap &normalPixmap, QPixmap &hoveredPixmap, QPixmap &livePixmap, QPixmap &liveHoveredPixmap, const QString &url, const QString &imagePath,
				const QSize &size);
	void getThumbnailPixmapAsync(PLSNaverShoppingLIVEImageProcessFinished *finished, const QString &url, const QString &imagePath, const QSize &size, double dpi);
	void updateThumbnailPixmap(const QString &url, const QPixmap &imagePixmap);
	void removeThumbnailPixmap(const QString &url);
	void clearThumbnailPixmaps();

	void updateThumbnailPixmapSize(const QSize &size, const QMargins &margin, int radius, const QSize &liveSize);
	void getThumbnailPixmapSize(QSize &size, QMargins &margin, int &radius, QSize &liveSize);

	bool hasSmartStoreInfo() const;
	QString getSmartStoreId() const;
	QString getSmartStoreName() const;
	QString getSmartStoreAccessToken() const;
	void setSmartStoreAccessToken(PLSPlatformNaverShoppingLIVE *platform, const QString &accessToken);
	void setSmartStoreInfo(const QString &storeId, const QString &storeName);
	void clearSmartStoreInfo();
	void loadSmartStoreInfo();
	void saveSmartStoreInfo();

	void tokenExpiredClear();

	static qint64 getItemId();

signals:
	void smartStoreInfoUpdated(bool ok, const QString &storeId, const QString &storeName);

public:
	static const int MAX_RECENT_COUNT = 30;
	static const int MAX_SEARCH_KEYWORDS_COUNT = 15;
	static const int MAX_LIVEINFO_PRODUCT_COUNT = 30;
	static const int MAX_FIXED_PRODUCT_COUNT = 3;
	static const int MIN_LIVEINFO_PRODUCT_COUNT = 1;
	static const int PRODUCT_PAGE_SIZE = 20;

	// product status
	static const QString PRODUCT_STATUS_WAIT;
	static const QString PRODUCT_STATUS_SALE;
	static const QString PRODUCT_STATUS_OUTOFSTOCK;
	static const QString PRODUCT_STATUS_UNADMISSION;
	static const QString PRODUCT_STATUS_REJECTION;
	static const QString PRODUCT_STATUS_SUSPENSION;
	static const QString PRODUCT_STATUS_CLOSE;
	static const QString PRODUCT_STATUS_PROHIBITION;
	static const QString PRODUCT_STATUS_DELETE;

private:
	QStringList searchKeywords;

	mutable QReadWriteLock downloadImageCacheRWLock{QReadWriteLock::Recursive};
	QMap<QString, QString> downloadImageCache; // url -> path

	QList<qint64> recentProductNos;

	int latestUseTab = -1;
	QString latestStoreId;

	PLSPLSNaverShoppingLIVEImageLoadThread *imageLoadThread = nullptr;

	QSvgRenderer defaultImage;
	QSvgRenderer liveProductBadgeImage;

	QReadWriteLock downloadImagePixmapCacheRWLock{QReadWriteLock::Recursive};
	// url => (imagePixmap, scaledImagePixmapNormal, scaledImagePixmapHover, scaledImagePixmapLive, scaledImagePixmapLiveHover)
	QMap<QString, std::tuple<QPixmap, QPixmap, QPixmap, QPixmap, QPixmap>> downloadImagePixmapCache;

	QReadWriteLock downloadImagePixmapInfoRWLock{QReadWriteLock::Recursive};
	QSize downloadImagePixmapSize;
	QSize downloadImagePixmapLiveSize;
	QMargins downloadImagePixmapMargin;
	int downloadImagePixmapRadius = 0;

	SmartStoreInfo smartStoreInfo;
};

#endif // PLSNAVERSHOPPINGLIVEDATAMANAGER_H
