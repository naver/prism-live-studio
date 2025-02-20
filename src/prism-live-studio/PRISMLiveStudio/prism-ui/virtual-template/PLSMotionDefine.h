#ifndef MOTIONDEFINE_H
#define MOTIONDEFINE_H

#include <QObject>
#include <QFile>
#include "libresource.h"

#define MOTION_RESOURCE_URL_KEY QStringLiteral("resourceUrl")
#define MOTION_THUMBNAIL_URL_KEY QStringLiteral("thumbnailUrl")

#define MOTION_GROUP_ID_KEY QStringLiteral("groupId")
#define MOTION_VERSION_KEY QStringLiteral("version")
#define MOTION_DATA_TYPE_KEY QStringLiteral("dataType")
#define MOTION_CAN_DELETE_KEY QStringLiteral("canDelete")
#define FOREBACKGROUND_PATH_KEY QStringLiteral("foregroundPath")
#define FOREBACKGROUND_STATIC_IMAGE_PATH_KEY QStringLiteral("foregroundStaticImgPath")
#define MOTION_ITEM_ID_KEY QStringLiteral("itemId")
#define MOTION_TITLE_KEY QStringLiteral("title")
#define MOTION_TYPE_KEY QStringLiteral("type")
#define RESOURCE_PATH_KEY QStringLiteral("resourcePath")
#define STATIC_IMAGE_PATH_KEY QStringLiteral("staticImgPath")
#define THUMBNAIL_PATH_KEY QStringLiteral("thumbnailPath")
#define RESOURCE_STATE_KEY QStringLiteral("resourceState")
#define THUMBNAIL_STATE_KEY QStringLiteral("thumbnailState")
#define STATIC_IMAGE_STATE_KEY QStringLiteral("staticImgState")
#define FOREGROUND_STATE_KEY QStringLiteral("foregroundState")
#define FOREGROUND_STATIC_IMAGE_STATE_KEY QStringLiteral("foregroundStaticImgState")

enum class MotionType { UNKOWN, MOTION, STATIC };
enum class MotionState { UNKOWN, LOCAL_FILE, DOWNLOAD_SUCCESS, DOWNLOAD_FAILED, NO_FILE, DOWNLOADING };

struct MotionData {
	MotionType type = MotionType::UNKOWN;
	int version = -1;

	QString itemId;
	QString title;
	QString resourceUrl;
	QString thumbnailUrl;
	QString groupId;

	MotionState resourceState = MotionState::UNKOWN;
	MotionState staticImgState = MotionState::UNKOWN;
	MotionState foregroundState = MotionState::UNKOWN;
	MotionState foregroundStaticImgState = MotionState::UNKOWN;
	MotionState thumbnailState = MotionState::UNKOWN;

	QString resourcePath;
	QString staticImgPath;
	QString foregroundPath;
	QString foregroundStaticImgPath;
	QString thumbnailPath;

	bool canDelete = false;
	MotionData(const pls::rsm::Item &item);
	void setItem(pls::rsm::Item &item);
	MotionData() {}

	static bool isUnknown(MotionState state) { return state == MotionState::UNKOWN; }
	static bool isLocalFile(MotionState state) { return state == MotionState::LOCAL_FILE; }
	static bool pathIsValid(const QString &path, MotionState state) { return QFile::exists(path) && isLocalFile(state); }

	bool isUnknown() const { return isUnknown(resourceState) || isUnknown(staticImgState) || isUnknown(foregroundState) || isUnknown(foregroundStaticImgState) || isUnknown(thumbnailState); }

	bool backgroundIsLocalFile() const { return isLocalFile(resourceState) && isLocalFile(staticImgState); }
	bool foregroundIsLocalFile() const { return isLocalFile(foregroundState) && isLocalFile(foregroundStaticImgState); }
	bool thumbnailIsLocalFile() const { return isLocalFile(thumbnailState); }

	bool backgroundIsValid() const { return pathIsValid(resourcePath, resourceState) && pathIsValid(staticImgPath, staticImgState); }
	bool foregroundIsValid() const { return pathIsValid(foregroundPath, foregroundState) && pathIsValid(foregroundStaticImgPath, foregroundStaticImgState); }
	bool thumbnailIsValid() const { return pathIsValid(thumbnailPath, thumbnailState); }

	void checkStatic(const QString &dirPath);
	void checkMotion(const QString &dirPath);
	void checkThumbnail(const QString &dirPath);
	bool checkResourceCached();

	MotionType motionTypeByString(const QString &type) const;
	pls::rsm::Item item;
};

Q_DECLARE_METATYPE(MotionData)
Q_DECLARE_METATYPE(MotionState)

#endif // MOTIONDEFINE_H
