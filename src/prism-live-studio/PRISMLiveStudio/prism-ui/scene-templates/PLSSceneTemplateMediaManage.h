#include <QObject>
#include <QMediaPlayer>
#include <QReadWriteLock>
#include "PLSSceneTemplateMainSceneItem.h"
#include "PLSSceneTemplateModel.h"
#include <qmediaplayer.h>
#include "PLSSceneTemplateImageView.h"
#include "PLSMediaRender.h"

class PLSSceneTemplateContainer;

class PLSSceneTemplateMediaManage : public QObject {
	Q_OBJECT

public:
	explicit PLSSceneTemplateMediaManage(QObject *parent = nullptr);
	static PLSSceneTemplateMediaManage *instance();
	~PLSSceneTemplateMediaManage() override;

public:
	PLSMediaRender *getVideoViewByPath(const QString &videoPath);
	PLSSceneTemplateImageView *getImageViewByPath(const QString &imagePath);
	const QMap<QString, PLSSceneTemplateImageView *> &getImageViewCache() const;
	void startPlayVideo(const QString &videoPath, PLSMediaRender *videoView);
	void stopPlayVideo(PLSMediaRender *videoView);
	void closeSceneTemplateContainer();
	PLSSceneTemplateContainer *getSceneTemplateContainer();
	void setSceneTemplateContainer(PLSSceneTemplateContainer *container);
	void enterDetailScenePage(const SceneTemplateItem &mode);
	void enterMainScenePage();
	bool isVideoType(const QString &path);
	bool isImageType(const QString &path);
	bool getVideoFirstFrame(const QString &videoPath, std::function<void(bool, QPixmap)> callback);

private:
private slots:
	void showError(QMediaPlayer::Error error, const QString &errorString);

private:
	QMap<QString, PLSMediaRender *> videoViewCache;
	QMap<QString, PLSSceneTemplateImageView *> imageViewCache;
	PLSSceneTemplateContainer *m_sceneContainer{nullptr};
	QMap<QString, QPixmap> videoThumbnailCache;
	QReadWriteLock videoThumbnailCacheLock{QReadWriteLock::Recursive};
};

#define PLS_SCENE_TEMPLATE_MEDIA_MANAGE PLSSceneTemplateMediaManage::instance()