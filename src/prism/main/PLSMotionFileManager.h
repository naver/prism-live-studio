#ifndef PLSMOTIONFILEMANAGER_H
#define PLSMOTIONFILEMANAGER_H

#include <QObject>
#include "PLSMotionDefine.h"
#include <QFileInfo>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QMap>
#include <QSvgRenderer>
#include <QReadWriteLock>

#define PROPERTY_RECENT_LIST "propertyRecentList"
#define PROPERTY_CATEGORY_INDEX "propertyCategoryIndex"
#define PROPERTY_CATEGORY_POSITION "propertyCategoryPosition"
#define VIRTUAL_BACKGROUND_RECENT_LIST "virtualRecentList"
#define VIRTUAL_CATEGORY_INDEX "virtualCategoryIndex"
#define VIRTUAL_CATEGORY_POSITION "virtualCategoryPosition"
#define MY_FILE_LIST "myFileList"

template<typename Processor, typename Data> class PLSResourcesProcessThread : public QThread {
public:
	PLSResourcesProcessThread() : running(false), datasSem(), datasLock(), datas()
	{
		processor = new Processor();
		processor->moveToThread(this);
	}
	virtual ~PLSResourcesProcessThread() { stop(); }

public:
	void start()
	{
		if (!running) {
			running = true;
			QThread::start();
		}
	}
	void stop()
	{
		if (running) {
			running = false;
			datasSem.release();
			QThread::wait();
		}
	}

	Processor *getProcessor() { return processor; }

	void push(const Data &data)
	{
		QMutexLocker locker(&datasLock);
		this->datas.append(data);

		datasSem.release(datas.count());
	}
	void push(const QList<Data> &datas)
	{
		if (datas.isEmpty()) {
			return;
		}

		QMutexLocker locker(&datasLock);
		this->datas.append(datas);

		datasSem.release(datas.count());
	}
	bool pop(Data &data)
	{
		if (!datasSem.tryAcquire(1, 1000)) {
			return false;
		}

		if (!running) {
			return false;
		}

		QMutexLocker locker(&datasLock);
		if (datas.isEmpty()) {
			return false;
		}

		data = datas.takeFirst();
		return true;
	}

protected:
	virtual void run() final
	{
		process();

		delete processor;
		processor = nullptr;
	}
	virtual void process() = 0;

protected:
	bool running = false;
	QSemaphore datasSem;
	QMutex datasLock;
	QList<Data> datas;
	Processor *processor = nullptr;
};

template<typename Processor, typename Data> class PLSResourcesBatchProcessThread : public PLSResourcesProcessThread<Processor, Data> {
public:
	PLSResourcesBatchProcessThread() {}
	virtual ~PLSResourcesBatchProcessThread() {}

public:
	void push(QObject *sourceUi, const Data &data)
	{
		QMutexLocker locker(&datasLock);
		this->sourceUi = sourceUi;
		this->datas.append(data);

		datasSem.release(datas.count());
	}
	void push(QObject *sourceUi, const QList<Data> &datas)
	{
		if (datas.isEmpty()) {
			return;
		}

		QMutexLocker locker(&datasLock);
		this->sourceUi = sourceUi;
		this->datas.append(datas);

		datasSem.release(datas.count());
	}

protected:
	QObject *sourceUi = nullptr;
};

class PLSAddMyResourcesProcessor : public QObject {
	Q_OBJECT

public:
	PLSAddMyResourcesProcessor() {}
	~PLSAddMyResourcesProcessor() {}

public:
	static const qint64 MAX_RESOLUTION_SIZE = 3840 * 2160;
	static const int THUMBNAIL_WIDTH = 600;
	static const int THUMBNAIL_HEIGHT = 337;
	enum ProcessError {
		NoError = 0,
		MaxResolutionError = (1 << 0),
		OpenFailedError = (1 << 1),
		GetMotionFirstFrameFailedError = (1 << 2),
		FileFormatError = (1 << 3),
		FileNotFountError = (1 << 4),
		SaveImageFailedError = (1 << 5),
		OtherError = (1 << 6)
	};

public:
	int process(MotionData &md, QObject *sourceUi, const QString &file, std::function<bool()> isLast);

private:
	bool saveImages(QString &thumbnailPath, const QImage &image, const QSize &imageSize, const QString &itemId, const QString &filePath);

signals:
	void addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(QObject *sourceUi, int error);
};

class PLSDeleteMyResourcesProcessor : public QObject {
	Q_OBJECT

public:
	PLSDeleteMyResourcesProcessor() {}
	~PLSDeleteMyResourcesProcessor() {}

public:
	void process(QObject *sourceUi, const MotionData &data);

signals:
	void addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(QObject *sourceUi, int error);
};

class PLSAddMyResourcesThread : public PLSResourcesBatchProcessThread<PLSAddMyResourcesProcessor, QString> {
public:
	PLSAddMyResourcesThread() {}
	~PLSAddMyResourcesThread() {}

protected:
	virtual void process();
};

class PLSResourcesThumbnailProcessFinished {
public:
	virtual ~PLSResourcesThumbnailProcessFinished() {}
	virtual void processThumbnailFinished(QThread *thread, const QString &itemId, const QPixmap &normalPixmap, const QPixmap &selectedPixmap) = 0;
};

class PLSResourcesThumbnailProcessor : public QObject {
	Q_OBJECT

public:
	PLSResourcesThumbnailProcessor() {}
	~PLSResourcesThumbnailProcessor() {}

public:
	void process(PLSResourcesThumbnailProcessFinished *finished, const MotionData &md, bool properties);
};

class PLSResourcesThumbnailThread : public PLSResourcesProcessThread<PLSResourcesThumbnailProcessor, std::tuple<PLSResourcesThumbnailProcessFinished *, MotionData, bool>> {
public:
	PLSResourcesThumbnailThread() {}
	~PLSResourcesThumbnailThread() {}

protected:
	virtual void process();
};

class PLSDeleteResourcesThread : public PLSResourcesBatchProcessThread<PLSDeleteMyResourcesProcessor, MotionData> {
	Q_OBJECT

public:
	PLSDeleteResourcesThread() {}
	~PLSDeleteResourcesThread() {}

protected:
	virtual void process();
};

class PLSMotionFileManager : public QObject {
	Q_OBJECT

public:
	static PLSMotionFileManager *instance();
	~PLSMotionFileManager();
	explicit PLSMotionFileManager(QObject *parent = nullptr);
	QVariantList getPrismList();
	QVariantList getFreeList();
	QString getFilePathByURL(const MotionData &data, const QString &url);
	MotionType motionTypeByString(const QString &type);
	bool isDownloadFileExist(const QString &filePath);
	bool isValidMotionData(const MotionData &data, bool onlyCheckValues = false);
	bool insertMotionData(const MotionData &data, const QString &key);
	void removeRepeatedMotionData(const MotionData &data, QList<MotionData> &list);
	bool deleteMotionData(QObject *sourceUi, const QString &itemId, const QString &key, bool isVbUsed, bool isSourceUsed);
	bool copyFileToPrismPath(const QFileInfo &fileInfo, QString &fileUuid, QString &destPath);
	QString getPrismResourcePathByUUid(const QString &uuid, const QString &suffix);
	QString getPrismThumbnailPathByUUid(const QString &uuid, const QString &suffix);
	QString getPrismStaticImgPathByUUid(const QString &uuid, const QString &suffix);
	bool isVideoTypeFile(const QString &suffix, bool &isGif);
	bool isStaticImageTypeFile(const QString &suffix);
	QList<MotionData> &getRecentMotionList(const QString &recentKey);
	QList<MotionData> &getMyMotionList();
	void saveMotionList();
	void saveCategoryIndex(int index, const QString &key);
	QString categoryIndex(const QString &key);

	// async add my resources
	void addMyResources(QObject *sourceUi, const QStringList &files);
	void deleteAllMyResources(QObject *sourceUi);
	bool copyList(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src, QList<MotionData> *removed = nullptr);
	bool copyListForPrism(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src);
	bool isRemovedChanged(QList<MotionData> &needNotified, QList<MotionData> &removed, QList<MotionData> &dst);
	void notifyCheckedRemoved(const QList<MotionData> &removed);

	QSvgRenderer *getMotionFlagSvg();
	void loadMotionFlagSvg();

	bool hasThumbnailPixmap(const QString &itemId);
	bool getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &selectedPixmap, const MotionData &md, bool properties);
	bool getThumbnailPixmap(QPixmap &normalPixmap, QPixmap &selectedPixmap, const QString &itemId, const QSize &size, bool properties);
	bool getThumbnailPixmap(QPixmap &original, QPixmap &normalPixmap, QPixmap &selectedPixmap, const QString &itemId, const QSize &size, bool properties);
	void getThumbnailPixmapAsync(PLSResourcesThumbnailProcessFinished *itemView, const MotionData &md, const QSize &size, double dpi, bool properties);
	void updateThumbnailPixmap(const QString &itemId, const QPixmap &thumbnailPixmap);
	void removeThumbnailPixmap(const QString &itemId);

	void updateThumbnailPixmapSize(const QSize &size, const QMargins &margin, int radius, bool properties);
	void getThumbnailPixmapSize(QSize &size, QMargins &margin, int &radius, bool properties);

	bool isMy(const QString &itemId) const;
	bool findMy(MotionData &md, const QString &itemId) const;
	bool removeAt(QList<MotionData> &mds, const QString &itemId) const;
	bool findAndRemoveAt(MotionData &md, QList<MotionData> &mds, const QString &itemId) const;

	QString getChooseFileDir() const;

	void logoutClear();

signals:
	void addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(QObject *sourceUi, int error);
	void deleteResourceFinished(QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed);
	void checkedRemoved(const MotionData &md, bool isVbUsed, bool isSourceUsed);
	void deleteMyResources();

private:
	bool createMotionJsonFile();
	bool getLocalMotionObject(QJsonObject &object);
	QString getUserPath(const QString &subPath);
	QString getFileNameByURL(const QString &url);
	QVariantList getListForKey(const QString &key);
	void getMotionListByJsonArray(const QJsonArray &array, QList<MotionData> &list, DataType dataType);
	void deleteMotionListItem(const QString &itemId, QList<MotionData> &list, bool deleteFile = true);
	void deleteLocalFile(const QString &path);
	void saveMotionListToLocalPath(const QString &listTypeKey, QJsonObject &jsonObject, QList<MotionData> &list);

private:
	QList<MotionData> m_virtualRecentList;
	QList<MotionData> m_propertyRecentList;
	QList<MotionData> m_myList;
	QString m_virtualCategoryIndex = nullptr;
	QString m_propertyCategoryIndex = nullptr;
	PLSAddMyResourcesThread *addMyResourcesThread = nullptr;
	PLSResourcesThumbnailThread *resourcesThumbnailThread = nullptr;
	PLSDeleteResourcesThread *deleteAllMyResourcesThread = nullptr;

	QSvgRenderer motionFlagSvg;

	QReadWriteLock thumbnailPixmapCacheRWLock{QReadWriteLock::Recursive};
	// itemId => (thumbnailPixmap, scaledThumbnailPixmapPropNormal, scaledThumbnailPixmapPropSelected, scaledThumbnailPixmapVbNormal, scaledThumbnailPixmapVbSelected)
	QMap<QString, std::tuple<QPixmap, QPixmap, QPixmap, QPixmap, QPixmap>> thumbnailPixmapCache;

	QReadWriteLock thumbnailPixmapRWLock{QReadWriteLock::Recursive};
	QSize thumbnailPixmapSize[2];       // 0:Properties, 1:VirtualBackground
	QMargins thumbnailPixmapMargin[2];  // 0:Properties, 1:VirtualBackground
	int thumbnailPixmapRadius[2]{0, 0}; // calc by dpi 0:Properties, 1:VirtualBackground

	QString chooseFileDir;
};

#define MotionFileManage PLSMotionFileManager::instance()

#endif // PLSMOTIONFILEMANAGER_H
