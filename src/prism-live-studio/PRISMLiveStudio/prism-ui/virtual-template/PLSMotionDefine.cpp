#include "PLSMotionDefine.h"
#include "PLSMotionFileManager.h"
#include "CategoryVirtualTemplate.h"
#include "libresource.h"

#include <QDir>
#include <QPixmap>

static QString findImageByPrefix(const QStringList &filePaths, const QString &prefix)
{
	for (const QString &filePath : filePaths) {
		QFileInfo fi(filePath);
		if (fi.fileName().startsWith(prefix)) {
			return filePath;
		}
	}
	return QString();
}

MotionData::MotionData(const pls::rsm::Item &item_) : item(item_)
{
	canDelete = item.customAttr(MOTION_CAN_DELETE_KEY).toBool();

	if (canDelete) {
		itemId = item.customAttr(MOTION_ITEM_ID_KEY).toString();
		groupId = item.customAttr(MOTION_GROUP_ID_KEY).toString();
		title = item.customAttr(MOTION_TITLE_KEY).toString();
		resourceState = static_cast<MotionState>(item.customAttr(RESOURCE_STATE_KEY).toInt());
		thumbnailState = static_cast<MotionState>(item.customAttr(THUMBNAIL_STATE_KEY).toInt());
		staticImgState = static_cast<MotionState>(item.customAttr(STATIC_IMAGE_STATE_KEY).toInt());
		foregroundState = static_cast<MotionState>(item.customAttr(FOREGROUND_STATE_KEY).toInt());
		foregroundStaticImgState = static_cast<MotionState>(item.customAttr(FOREGROUND_STATIC_IMAGE_STATE_KEY).toInt());
		type = static_cast<MotionType>(item.customAttr(MOTION_TYPE_KEY).toInt());
		foregroundPath = item.customAttr(FOREBACKGROUND_PATH_KEY).toString();
		foregroundStaticImgPath = item.customAttr(FOREBACKGROUND_STATIC_IMAGE_PATH_KEY).toString();
		resourcePath = item.customAttr(RESOURCE_PATH_KEY).toString();
		staticImgPath = item.customAttr(STATIC_IMAGE_PATH_KEY).toString();
		thumbnailPath = item.customAttr(THUMBNAIL_PATH_KEY).toString();
		return;
	}

	itemId = item.attr(MOTION_ITEM_ID_KEY).toString();
	title = item.attr(MOTION_TITLE_KEY).toString();
	version = item.attr(MOTION_VERSION_KEY).toInt();

	auto groups = item.groups();
	if (groups.size() > 0) {
		auto group = groups.front();
		groupId = group.groupId();
	}

	type = motionTypeByString(item.attr(MOTION_TYPE_KEY).toString());
	resourceUrl = item.attr(MOTION_RESOURCE_URL_KEY).toString();
	thumbnailUrl = item.attr(MOTION_THUMBNAIL_URL_KEY).toString();

	canDelete = false;
	auto state = item.state();
	if (state == pls::rsm::State::Downloading) {
		resourceState = staticImgState = foregroundState = foregroundStaticImgState = thumbnailState = MotionState::DOWNLOADING;
	} else if (state == pls::rsm::State::Failed) {
		resourceState = staticImgState = foregroundState = foregroundStaticImgState = thumbnailState = MotionState::DOWNLOAD_FAILED;
	} else if (state == pls::rsm::State::Ok) {
		resourceState = staticImgState = foregroundState = foregroundStaticImgState = thumbnailState = MotionState::DOWNLOAD_SUCCESS;
	}

	QString dirPath = item.dir();
	if (type == MotionType::STATIC) {
		checkStatic(dirPath);
	} else if (type == MotionType::MOTION) {
		checkMotion(dirPath);
	}
	checkThumbnail(dirPath);
}

void MotionData::setItem(pls::rsm::Item &item_)
{
	item = item_;
}

void MotionData::checkStatic(const QString &dirPath)
{
	auto imagePath = pls_find_subdir_contains_spec_filename(dirPath, "background_static");
	if (imagePath) {
		resourcePath = staticImgPath = imagePath.value();
		resourceState = staticImgState = MotionState::LOCAL_FILE;
	} else {
		resourceState = staticImgState = MotionState::NO_FILE;
		resourcePath.clear();
		staticImgPath.clear();
	}

	auto foregroundImagePath = pls_find_subdir_contains_spec_filename(dirPath, "foreground_static");
	if (foregroundImagePath) {
		foregroundPath = foregroundStaticImgPath = foregroundImagePath.value();
		foregroundState = foregroundStaticImgState = MotionState::LOCAL_FILE;
	} else {
		foregroundState = foregroundStaticImgState = MotionState::NO_FILE;
		foregroundPath.clear();
		foregroundStaticImgPath.clear();
	}
}

void MotionData::checkMotion(const QString &dirPath)
{
	auto videoPath = pls_find_subdir_contains_spec_filename(dirPath, "background_motion");

	if (videoPath) {
		resourcePath = videoPath.value();
		resourceState = MotionState::LOCAL_FILE;
	} else {
		resourceState = MotionState::NO_FILE;
		resourcePath.clear();
	}
	auto imagePath = pls_find_subdir_contains_spec_filename(dirPath, "background_static");
	if (imagePath) {
		staticImgPath = imagePath.value();
		staticImgState = MotionState::LOCAL_FILE;
	} else {
		staticImgState = MotionState::NO_FILE;
		staticImgPath.clear();
	}
	auto foregroundVideoPath = pls_find_subdir_contains_spec_filename(dirPath, "foreground_motion");
	if (foregroundVideoPath) {
		foregroundPath = foregroundVideoPath.value();
		foregroundState = MotionState::LOCAL_FILE;
	} else {
		foregroundState = MotionState::NO_FILE;
		foregroundPath.clear();
	}

	auto foregroundImagePath = pls_find_subdir_contains_spec_filename(dirPath, "foreground_static");
	if (foregroundImagePath) {
		foregroundStaticImgPath = foregroundImagePath.value();
		foregroundStaticImgState = MotionState::LOCAL_FILE;
	} else {
		foregroundStaticImgState = MotionState::NO_FILE;
		foregroundStaticImgPath.clear();
	}
}

void MotionData::checkThumbnail(const QString &dirPath)
{
	auto thumbnailImagePath = pls_find_subdir_contains_spec_filename(dirPath, "thumbnail");

	if (thumbnailImagePath) {
		thumbnailPath = thumbnailImagePath.value();
		thumbnailState = MotionState::LOCAL_FILE;
	} else {
		thumbnailState = MotionState::NO_FILE;
		thumbnailPath.clear();
	}
}

bool MotionData::checkResourceCached()
{
	if (((MotionData::pathIsValid(resourcePath, resourceState) && MotionData::pathIsValid(staticImgPath, staticImgState)) ||
	     (MotionData::pathIsValid(foregroundPath, foregroundState) && MotionData::pathIsValid(foregroundStaticImgPath, foregroundStaticImgState))) &&
	    MotionData::pathIsValid(thumbnailPath, thumbnailState)) {
		return true;
	}
	return false;
}

MotionType MotionData::motionTypeByString(const QString &type) const
{
	if (type == "motion") {
		return MotionType::MOTION;
	} else if (type == "static") {
		return MotionType::STATIC;
	}
	return MotionType::UNKOWN;
}
