#include "PLSSceneTemplateMediaManage.h"
#include "libutils-api.h"
#include "PLSSceneTemplateContainer.h"
#include <pls/media-info.h>
#include <QDir>

PLSSceneTemplateMediaManage::PLSSceneTemplateMediaManage(QObject *parent) : QObject(parent) {}

PLSSceneTemplateMediaManage *PLSSceneTemplateMediaManage::instance()
{
	static PLSSceneTemplateMediaManage manage;
	return &manage;
}

PLSSceneTemplateMediaManage::~PLSSceneTemplateMediaManage() {}

PLSMediaRender *PLSSceneTemplateMediaManage::getVideoViewByPath(const QString &videoPath)
{
	if (videoViewCache.contains(videoPath)) {
		return videoViewCache.value(videoPath);
	}
	auto *videoView = new PLSMediaRender;
	videoViewCache.insert(videoPath, videoView);
	return videoView;
}

PLSSceneTemplateImageView *PLSSceneTemplateMediaManage::getImageViewByPath(const QString &imagePath)
{
	if (imageViewCache.contains(imagePath)) {
		return imageViewCache.value(imagePath);
	}
	PLSSceneTemplateImageView *imageView = new PLSSceneTemplateImageView;
	imageView->updateImagePath(imagePath);
	imageViewCache.insert(imagePath, imageView);
	return imageView;
}

const QMap<QString, PLSSceneTemplateImageView *> &PLSSceneTemplateMediaManage::getImageViewCache() const
{
	return imageViewCache;
}

void PLSSceneTemplateMediaManage::startPlayVideo(const QString &videoPath, PLSMediaRender *videoView)
{
	QMediaPlayer *player = new QMediaPlayer;
	videoView->setMediaPlayer(player);
	player->setSource(QUrl::fromLocalFile(videoPath));

	player->setLoops(-1);
	player->play();
}

void PLSSceneTemplateMediaManage::stopPlayVideo(PLSMediaRender *videoView)
{
	QMediaPlayer *player = videoView->getMediaPlayer();
	if (!player) {
		return;
	}

	player->stop();
	player->setVideoOutput(nullptr);
	delete player;
}

void PLSSceneTemplateMediaManage::closeSceneTemplateContainer()
{
	if (!pls_object_is_valid(m_sceneContainer)) {
		return;
	}
	m_sceneContainer->close();
}

PLSSceneTemplateContainer *PLSSceneTemplateMediaManage::getSceneTemplateContainer()
{
	if (!pls_object_is_valid(m_sceneContainer)) {
		return nullptr;
	}
	return m_sceneContainer;
}

void PLSSceneTemplateMediaManage::setSceneTemplateContainer(PLSSceneTemplateContainer *container)
{
	m_sceneContainer = container;
}

void PLSSceneTemplateMediaManage::enterDetailScenePage(const SceneTemplateItem &mode)
{
	if (!pls_object_is_valid(m_sceneContainer)) {
		return;
	}
	m_sceneContainer->showDetailSceneTemplatePage(mode);
}

void PLSSceneTemplateMediaManage::enterMainScenePage()
{
	if (!pls_object_is_valid(m_sceneContainer)) {
		return;
	}
	m_sceneContainer->showMainSceneTemplatePage();
}

bool PLSSceneTemplateMediaManage::isVideoType(const QString &path)
{
	QFileInfo fileInfo(path);
	bool valid = true;
	if (fileInfo.fileName().isEmpty()) {
		valid = false;
	} else {
		QStringList allFilterExtensionList;
		allFilterExtensionList << "mp4" << "webm";
		if (!allFilterExtensionList.contains(fileInfo.suffix().toLower())) {
			valid = false;
		}
	}
	return valid;
}

bool PLSSceneTemplateMediaManage::isImageType(const QString &path)
{
	QFileInfo fileInfo(path);
	bool valid = true;
	if (fileInfo.fileName().isEmpty()) {
		valid = false;
	} else {
		QStringList allFilterExtensionList;
		allFilterExtensionList << "bmp" << "jpg" << "png";
		if (!allFilterExtensionList.contains(fileInfo.suffix().toLower())) {
			valid = false;
		}
	}
	return valid;
}

bool PLSSceneTemplateMediaManage::getVideoFirstFrame(const QString &videoPath, QPixmap &pixmap)
{
	if (videoThumbnailCache.contains(videoPath)) {
		pixmap = videoThumbnailCache.value(videoPath);
		return true;
	}

	QFileInfo fileInfo(videoPath);
	QString baseName = fileInfo.baseName();
	QString targetFilePath = fileInfo.absoluteDir().path() + "/VideoThumbnail_" + baseName + ".png";
	if (QFileInfo(targetFilePath).exists()) {
		if (!pixmap.load(targetFilePath, "PNG")) {
			return false;
		}
		videoThumbnailCache.insert(videoPath, pixmap);
		return true;
	}

	//open file failed
	media_info_t mi;
	if (!mi_open(&mi, videoPath.toUtf8().constData(), MI_OPEN_DIRECTLY)) {
		return false;
	}

	auto firstFrame = (mi_frame_t *)mi_get_obj(&mi, "first_frame_obj");
	if (!firstFrame) {
		mi_free(&mi);
		return false;
	}

	qint64 width = mi_get_int(&mi, "width");
	qint64 height = mi_get_int(&mi, "height");
	QImage saveImage = QImage((const uchar *)firstFrame->data, static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBX8888);
	if (!saveImage.save(targetFilePath)) {
		mi_free(&mi);
		return false;
	}
	pixmap = QPixmap::fromImage(saveImage);
	videoThumbnailCache.insert(videoPath, pixmap);
	mi_free(&mi);
	return true;
}

void PLSSceneTemplateMediaManage::showError(QMediaPlayer::Error error, const QString &errorString)
{
	switch (error) {
	case QMediaPlayer::NoError:
		break;
	case QMediaPlayer::ResourceError:
		break;
	case QMediaPlayer::FormatError:
		break;
	case QMediaPlayer::NetworkError:
		break;
	case QMediaPlayer::AccessDeniedError:
		break;
	default:
		break;
	}
}
