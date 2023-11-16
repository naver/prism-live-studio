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
#include <array>
#include "utils-api.h"

constexpr const char *PROPERTY_RECENT_LIST = "propertyRecentList";
constexpr const char *PROPERTY_CATEGORY_INDEX = "propertyCategoryIndex";
constexpr const char *PROPERTY_CATEGORY_POSITION = "propertyCategoryPosition";
constexpr const char *VIRTUAL_BACKGROUND_RECENT_LIST = "virtualRecentList";
constexpr const char *VIRTUAL_CATEGORY_INDEX = "virtualCategoryIndex";
constexpr const char *VIRTUAL_CATEGORY_POSITION = "virtualCategoryPosition";
constexpr const char *MY_FILE_LIST = "myFileList";

template<typename Processor, typename Data> class PLSResourcesProcessThread : public QThread {
	Q_DISABLE_COPY(PLSResourcesProcessThread)

public:
	using DataType = Data;

	PLSResourcesProcessThread()
	{
		m_processor = pls_new<Processor>();
		m_processor->moveToThread(this);
	}
	~PLSResourcesProcessThread() override { stopThread(); }

	void startThread()
	{
		if (!m_running) {
			m_running = true;
			QThread::start();
		}
	}
	void stopThread()
	{
		if (m_running) {
			m_running = false;
			m_datasSem.release();
			QThread::wait();
		}
	}

	bool &running() { return m_running; }
	QSemaphore &datasSem() { return m_datasSem; }
	QMutex &datasLock() { return m_datasLock; }
	QList<DataType> &datas() { return m_datas; }
	Processor *&processor() { return m_processor; }

	void push(const DataType &data)
	{
		pushData(data);
		m_datasSem.release(1);
	}
	void push(const QList<DataType> &datas)
	{
		if (int count = datas.count(); count > 0) {
			pushData(datas);
			m_datasSem.release(count);
		}
	}
	bool pop(DataType &data, int timeout = 1000)
	{
		if (!m_datasSem.tryAcquire(1, timeout)) {
			return false;
		}

		if (!m_running) {
			return false;
		}

		QMutexLocker locker(&m_datasLock);
		if (m_datas.isEmpty()) {
			return false;
		}

		data = m_datas.takeFirst();
		return true;
	}

private:
	using QThread::start;

protected:
	template<typename T> void pushData(const T &data)
	{
		QMutexLocker locker(&m_datasLock);
		this->m_datas.append(data);
	}
	template<typename Is> bool hasData(const Is &is) const
	{
		QMutexLocker locker(&m_datasLock);
		return std::any_of(m_datas.begin(), m_datas.end(), [is](const DataType &data) { return is(data); });
	}

	void run() final
	{
		process();
		pls_delete(m_processor, nullptr);
	}

	virtual void process() = 0;

private:
	bool m_running = false;
	QSemaphore m_datasSem;
	mutable QMutex m_datasLock;
	QList<DataType> m_datas;
	Processor *m_processor = nullptr;
};

template<typename Processor, typename Data> class PLSResourcesBatchProcessThread : public PLSResourcesProcessThread<Processor, std::pair<QObject *, Data>> {
public:
	using BaseType = PLSResourcesProcessThread<Processor, std::pair<QObject *, Data>>;
	using DataType = typename BaseType::DataType;

	PLSResourcesBatchProcessThread() = default;
	~PLSResourcesBatchProcessThread() override = default;

	void push(QObject *sourceUi, const Data &data) { BaseType::push(DataType{sourceUi, data}); }
	void push(QObject *sourceUi, const QList<Data> &datas)
	{
		for (const Data &data : datas) {
			push(sourceUi, data);
		}
	}
	bool isLast(QObject *sourceUi) const
	{
		return !BaseType::hasData([sourceUi](const DataType &data) { return data.first == sourceUi; });
	}
};

class PLSAddMyResourcesProcessor : public QObject {
	Q_OBJECT

public:
	PLSAddMyResourcesProcessor() = default;
	~PLSAddMyResourcesProcessor() override = default;

	static const qint64 MAX_RESOLUTION_SIZE = 3840 * 2160;
	static const int THUMBNAIL_WIDTH = 600;
	static const int THUMBNAIL_HEIGHT = 337;
	static const int NoError = 0;
	static const int MaxResolutionError = (1 << 0);
	static const int OpenFailedError = (1 << 1);
	static const int GetMotionFirstFrameFailedError = (1 << 2);
	static const int FileFormatError = (1 << 3);
	static const int FileNotFountError = (1 << 4);
	static const int SaveImageFailedError = (1 << 5);
	static const int OtherError = (1 << 6);

	template<typename IsLast> int process(MotionData &md, QObject *sourceUi, const QString &file, IsLast isLast);

private:
	bool saveImages(QString &thumbnailPath, const QImage &image, const QSize &imageSize, const QString &itemId, const QString &filePath) const;

signals:
	void addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(QObject *sourceUi, int error);
};

class PLSDeleteMyResourcesProcessor : public QObject {
	Q_OBJECT

public:
	PLSDeleteMyResourcesProcessor() = default;
	~PLSDeleteMyResourcesProcessor() override = default;

	void process(const QObject *sourceUi, const MotionData &data) const;

signals:
	void addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast);
	void addResourcesFinished(QObject *sourceUi, int error);
};

class PLSAddMyResourcesThread : public PLSResourcesBatchProcessThread<PLSAddMyResourcesProcessor, QString> {
public:
	using BaseType = PLSResourcesBatchProcessThread<PLSAddMyResourcesProcessor, QString>;
	using DataType = BaseType::DataType;

	PLSAddMyResourcesThread() = default;
	~PLSAddMyResourcesThread() override = default;

protected:
	void process() override;
};

class PLSResourcesThumbnailProcessFinished {
public:
	virtual ~PLSResourcesThumbnailProcessFinished() = default;
	virtual void processThumbnailFinished(QThread *thread, const QString &itemId, const QPixmap &normalPixmap, const QPixmap &selectedPixmap) = 0;
};

class PLSResourcesThumbnailProcessor : public QObject {
	Q_OBJECT

public:
	PLSResourcesThumbnailProcessor() = default;
	~PLSResourcesThumbnailProcessor() override = default;

	void process(PLSResourcesThumbnailProcessFinished *finished, const MotionData &md, bool properties) const;
};

class PLSResourcesThumbnailThread : public PLSResourcesProcessThread<PLSResourcesThumbnailProcessor, std::tuple<PLSResourcesThumbnailProcessFinished *, MotionData, bool>> {
public:
	PLSResourcesThumbnailThread() = default;
	~PLSResourcesThumbnailThread() override = default;

protected:
	void process() override;
};

class PLSDeleteResourcesThread : public PLSResourcesBatchProcessThread<PLSDeleteMyResourcesProcessor, MotionData> {
	Q_OBJECT

public:
	PLSDeleteResourcesThread() = default;
	~PLSDeleteResourcesThread() override = default;

protected:
	void process() override;
};

class PLSMotionFileManager : public QObject {
	Q_OBJECT

public:
	static PLSMotionFileManager *instance();
	~PLSMotionFileManager() override = default;
	explicit PLSMotionFileManager(QObject *parent = nullptr);
	QVariantList getPrismList() const;
	QVariantList getFreeList() const;
	QString getFilePathByURL(const MotionData &data, const QString &url) const;
	MotionType motionTypeByString(const QString &type) const;
	bool isDownloadFileExist(const QString &filePath) const;
	bool isValidMotionData(const MotionData &data, bool onlyCheckValues = false) const;
	bool insertMotionData(const MotionData &data, const QString &key);
	void removeRepeatedMotionData(const MotionData &data, QList<MotionData> &list) const;
	bool deleteMotionData(QObject *sourceUi, const QString &itemId, const QString &key, bool isVbUsed, bool isSourceUsed);
	bool copyFileToPrismPath(const QFileInfo &fileInfo, QString &fileUuid, QString &destPath) const;
	QString getPrismResourcePathByUUid(const QString &uuid, const QString &suffix) const;
	QString getPrismThumbnailPathByUUid(const QString &uuid, const QString &suffix) const;
	QString getPrismStaticImgPathByUUid(const QString &uuid, const QString &suffix) const;
	bool isVideoTypeFile(const QString &suffix, bool &isGif) const;
	bool isStaticImageTypeFile(const QString &suffix) const;
	QList<MotionData> &getRecentMotionList(const QString &recentKey);
	QList<MotionData> &getMyMotionList();
	void saveMotionList() const;
	void saveCategoryIndex(int index, const QString &key);
	QString categoryIndex(const QString &key) const;

	// async add my resources
	void addMyResources(QObject *sourceUi, const QStringList &files);
	void deleteAllMyResources(QObject *sourceUi);
	bool copyList(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src, QList<MotionData> *removed = nullptr) const;
	bool copyListForPrism(QList<MotionData> &copied, const QList<MotionData> &dst, const QList<MotionData> &src);
	bool isRemovedChanged(QList<MotionData> &needNotified, QList<MotionData> &removed, QList<MotionData> &dst) const;
	void notifyCheckedRemoved(const QList<MotionData> &removed);
	void redownloadResource(MotionData &md, bool update = false, const std::function<void(const MotionData &md)> &ok = nullptr,
				const std::function<void(const MotionData &md)> &fail = nullptr) const;

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
	void setThumbnailPixmap(const QString &itemId, const QPixmap &thumbnailPixmap);

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
	bool createMotionJsonFile() const;
	bool getLocalMotionObject(QJsonObject &object) const;
	QString getUserPath(const QString &subPath) const;
	QString getFileNameByURL(const QString &url) const;
	QVariantList getListForKey(const QString &key) const;
	void getMotionListByJsonArray(const QJsonArray &array, QList<MotionData> &list, DataType dataType) const;
	void deleteMotionListItem(const QString &itemId, QList<MotionData> &list, bool deleteFile = true) const;
	void deleteLocalFile(const QString &path) const;
	void saveMotionListToLocalPath(const QString &listTypeKey, QJsonObject &jsonObject, const QList<MotionData> &list) const;

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
	std::array<QSize, 2> thumbnailPixmapSize;       // 0:Properties, 1:VirtualBackground
	std::array<QMargins, 2> thumbnailPixmapMargin;  // 0:Properties, 1:VirtualBackground
	std::array<int, 2> thumbnailPixmapRadius{0, 0}; // calc by dpi 0:Properties, 1:VirtualBackground

	QString chooseFileDir;
};

#define MotionFileManage PLSMotionFileManager::instance()

#endif // PLSMOTIONFILEMANAGER_H
