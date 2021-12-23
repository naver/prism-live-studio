#ifndef PLSSTICKERDATAHANDLER_H
#define PLSSTICKERDATAHANDLER_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#define PORTRAIT "PORTRAIT"
#define LANDSCAPE "LANDSCAPE"

enum class Orientation { portrait = 0, landscape };
struct StickerParam {
	QString resourceDirectory;
	int frameCount;
	int fps;
	Orientation orientation;
};

struct StickerData {
	QString id;
	qint64 version;
	QString title;
	QString thumbnailUrl;
	QString resourceUrl;
	QString category;
};

Q_DECLARE_METATYPE(StickerData)

struct StickerDataIndex {
	size_t startIndex = 0;
	size_t endIndex = 0;
	QString categoryName;
};

struct StickerHandleResult {
	bool success = false;
	bool breakFlow = false;
	QString landscapeVideoFile;
	QString landscapeImage;
	QString portraitVideo;
	QString portraitImage;
	StickerData data;
	void AdjustParam()
	{
		if (landscapeVideoFile.isEmpty()) {
			landscapeVideoFile = portraitVideo;
			landscapeImage = portraitImage;
		}

		if (portraitVideo.isEmpty()) {
			portraitVideo = landscapeVideoFile;
			portraitImage = landscapeImage;
		}
	}
};

Q_DECLARE_METATYPE(StickerHandleResult)

class StickerParamWrapper;

class PLSStickerDataHandler : public QObject {

	Q_OBJECT
public:
	explicit PLSStickerDataHandler(QObject *parent = nullptr);
	~PLSStickerDataHandler() override;

	bool HandleDownloadedFile(const QString &stickerFile);
	static bool CheckStickerSource();

	static bool MediaRemux(const QString &filePath, const QString &outputFileName, uint fps);

	static bool UnCompress(const QString &srcFile, const QString &dstPath, QString &error);
	static bool ParseStickerParamJson(const QString &fileName, QJsonObject &obj);

	static StickerParamWrapper *CreateStickerParamWrapper(const QString &categoryId);
	static QString GetStickerConfigJsonFileName(const StickerData &data);
	static QString GetStickerResourcePath(const StickerData &data);
	static QString GetStickerResourceFile(const StickerData &data);
	static QString GetStickerResourceParentDir(const StickerData &data);
	static bool WriteDownloadCache(const QString &key, qint64 version, QJsonObject &cacheObj);
	static bool ReadDownloadCacheLocal(QJsonObject &cache);
	static bool WriteDownloadCacheToLocal(const QJsonObject &cacheObj);
	static bool ClearPrismStickerData();
	static void SetClearDataFlag(bool clearData);
	static bool GetClearDataFlag();

private:
	static bool clearData;
};

class StickerParamWrapper {

public:
	virtual ~StickerParamWrapper() {}

	virtual bool Serialize(const QString &jsonFileName) = 0;

public:
	std::vector<StickerParam> m_config;
	QString m_stickerId;
	QString m_name;
};

class Touch2DStickerParamWrapper : public StickerParamWrapper {
public:
	Touch2DStickerParamWrapper() : StickerParamWrapper() {}
	bool Serialize(const QString &jsonFileName) override
	{
		QJsonObject obj;
		if (PLSStickerDataHandler::ParseStickerParamJson(jsonFileName, obj)) {
			m_stickerId = obj.value("stickerId").toString();
			m_name = obj.value("name").toString();
			auto configArray = obj.value("items").toArray();
			for (const auto &item : configArray) {
				auto itemObj = item.toObject();
				StickerParam param;
				param.frameCount = itemObj.value("frameCount").toInt();
				param.resourceDirectory = itemObj.value("resourceDirectory").toString();
				param.fps = itemObj.value("fps").toInt();
				param.orientation = (0 == itemObj.value("orientation").toString().compare(LANDSCAPE)) ? Orientation::landscape : Orientation::portrait;
				m_config.emplace_back(param);
			}
			return true;
		}
		return false;
	}
};

class RandowTouch3DParamWrapper : public StickerParamWrapper {
public:
	RandowTouch3DParamWrapper() : StickerParamWrapper() {}
	bool Serialize(const QString &jsonFileName) override
	{
		QJsonObject obj;
		if (PLSStickerDataHandler::ParseStickerParamJson(jsonFileName, obj)) {
			m_name = obj.value("name").toString();
			auto configArray = obj.value("randomItems").toArray();
			for (const auto &item : configArray) {
				auto itemObj = item.toObject();
				StickerParam param;
				param.frameCount = itemObj.value("resourceCount").toInt();
				param.resourceDirectory = itemObj.value("resourceDirectory").toString();
				param.fps = itemObj.value("frameRate").toInt();
				param.orientation = Orientation::landscape;
				m_config.emplace_back(param);
			}
			return true;
		}
		return false;
	}
};

#endif // PLSSTICKERDATAHANDLER_H
