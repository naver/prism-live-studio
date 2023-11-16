#ifndef MOTIONDEFINE_H
#define MOTIONDEFINE_H

#include <QObject>
#include <QFile>

enum class MotionType { UNKOWN, MOTION, STATIC };
enum class MotionState { UNKOWN, LOCAL_FILE, DOWNLOAD_SUCCESS, DOWNLOAD_FAILED, NO_FILE, DOWNLOADING };
enum class DataType { UNKOWN, PROP_RECENT, VIRTUAL_RECENT, PRISM, FREE, MYLIST };

struct MotionData {
	DataType dataType = DataType::UNKOWN;
	MotionType type = MotionType::UNKOWN;
	int version = -1;

	QString itemId;
	QString title;
	QString resourceUrl;
	QString thumbnailUrl;

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
};

Q_DECLARE_METATYPE(MotionData)
Q_DECLARE_METATYPE(MotionState)

#endif // MOTIONDEFINE_H
