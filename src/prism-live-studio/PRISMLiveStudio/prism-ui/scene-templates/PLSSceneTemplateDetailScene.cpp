#include "PLSSceneTemplateDetailScene.h"
#include "PLSSceneTemplateContainer.h"
#include "ui_PLSSceneTemplateDetailScene.h"
#include "obs-app.hpp"
#include "libui.h"
#include "PLSSceneTemplateMediaManage.h"
#include "PLSSceneTemplateImageView.h"
#include "PLSNodeManager.h"

PLSSceneTemplateDetailScene::PLSSceneTemplateDetailScene(QWidget *parent) : QWidget(parent), ui(new Ui::PLSSceneTemplateDetailScene)
{
	ui->setupUi(this);
	connect(ui->returnButton, &PLSSceneTemplateReturnButton::clicked, this, [=] {
		removeImageAndVideoView();
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterMainScenePage();
	});
	ui->detailSceneNameLabel->installEventFilter(this);
	ui->scrollArea->setContentsMargins(0, 0, 0, 0);
	ui->mainSceneVideoView->setVisible(false);
	ui->mainSceneImageView->setVisible(false);
	ui->detailSceneTipButton->setHidden(true);
}

PLSSceneTemplateDetailScene::~PLSSceneTemplateDetailScene()
{
	delete ui;
}

bool PLSSceneTemplateDetailScene::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->detailSceneNameLabel && event->type() == QEvent::Resize) {
		QString text = ui->detailSceneNameLabel->fontMetrics().elidedText(m_item.title(), Qt::ElideRight, ui->detailSceneNameLabel->maximumWidth());
		ui->detailSceneNameLabel->setText(text);
	}
	return QWidget::eventFilter(watched, event);
}

void PLSSceneTemplateDetailScene::removeImageAndVideoView()
{
	QLayoutItem *child;
	while ((child = ui->verticalResourceLayout->takeAt(0))) {
		ui->verticalResourceLayout->removeItem(child);
	}
	for (PLSSceneTemplateImageView *imageView : initImageViewCache.values()) {
		imageView->disconnect();
		imageView->setVisible(false);
	}
	for (auto *videoView : initVideoViewCache.values()) {
		videoView->disconnect();
		videoView->setVisible(false);
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->stopPlayVideo(videoView);
	}
	initVideoViewCache.clear();
	initImageViewCache.clear();
	PLS_SCENE_TEMPLATE_MEDIA_MANAGE->stopPlayVideo(ui->mainSceneVideoView);
}

void PLSSceneTemplateDetailScene::updateMainSceneResourcePath(const QString &path, int iIndex)
{
	PLS_SCENE_TEMPLATE_MEDIA_MANAGE->stopPlayVideo(ui->mainSceneVideoView);
	if (PLS_SCENE_TEMPLATE_MEDIA_MANAGE->isVideoType(path)) {
		ui->mainSceneVideoView->setVisible(true);
		ui->mainSceneVideoView->setSceneName(iIndex > 0 ? m_sceneNames.value(iIndex - 1) : QString());
		ui->mainSceneImageView->setVisible(false);
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->startPlayVideo(path, ui->mainSceneVideoView);
	} else if (PLS_SCENE_TEMPLATE_MEDIA_MANAGE->isImageType(path)) {
		ui->mainSceneVideoView->setVisible(false);
		ui->mainSceneImageView->updateImagePath(path);
		ui->mainSceneImageView->setVisible(true);
		ui->mainSceneImageView->setSceneName(iIndex > 0 ? m_sceneNames.value(iIndex - 1) : QString());
	}
}

void PLSSceneTemplateDetailScene::updateUI(const SceneTemplateItem &model)
{
	removeImageAndVideoView();

	m_item = model;
	m_sceneNames = PLSNodeManager::instance()->getScenesNames(m_item);

	ui->detailSceneNameLabel->setText(model.title());
	QString resolution = QString("%1 x %2").arg(model.width()).arg(model.height());
	ui->detailResolutionLabel->setText(QTStr("SceneTemplate.Detail.Scene.Resolution").arg(resolution));
	ui->detailSceneTipButton->setVisible(model.width() < model.height());
	if (ui->detailSceneTipButton->isHidden()) {
		ui->horizontalSpacer_4->changeSize(0, 0);
	} else {
		ui->horizontalSpacer_4->changeSize(4, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
	}
	QString sceneCount = QTStr("SceneTemplate.Scene.Count").arg(model.scenesNumber());
	ui->detailSceneCountLabel->setText(sceneCount);

	auto thumbnailList = model.resource.detailSceneThumbnailPathList();
	for (int i = 0; i < thumbnailList.count(); i++) {
		const QString &path = thumbnailList[i];
		if (PLS_SCENE_TEMPLATE_MEDIA_MANAGE->isVideoType(path)) {
			auto *videoView = PLS_SCENE_TEMPLATE_MEDIA_MANAGE->getVideoViewByPath(path);
			PLS_SCENE_TEMPLATE_MEDIA_MANAGE->startPlayVideo(path, videoView);
			connect(videoView, &PLSMediaRender::clicked, this, [=]() { setSelectedViewBorder(path, i); });
			ui->verticalResourceLayout->addWidget(videoView);
			videoView->setVisible(true);
			initVideoViewCache.insert(path, videoView);
		} else if (PLS_SCENE_TEMPLATE_MEDIA_MANAGE->isImageType(path)) {
			PLSSceneTemplateImageView *imageView = PLS_SCENE_TEMPLATE_MEDIA_MANAGE->getImageViewByPath(path);
			connect(imageView, &PLSSceneTemplateImageView::clicked, this, [=](PLSSceneTemplateImageView *imageView) { setSelectedViewBorder(imageView->imagePath(), i); });
			ui->verticalResourceLayout->addWidget(imageView);
			imageView->setVisible(true);
			initImageViewCache.insert(path, imageView);
		}
	}
	ui->verticalResourceLayout->addStretch();
	if (thumbnailList.count() > 0) {
		const QString &path = thumbnailList[0];
		setSelectedViewBorder(path, 0);
		ui->scrollArea->ensureVisible(0, 0);
	}
	pls_flush_style_recursive(this);

	auto pDialog = qobject_cast<PLSSceneTemplateContainer *>(pls_get_toplevel_view(this));
	if (nullptr != pDialog && model.isAI()) {
		ui->mainSceneImageView->showAIBadge(pDialog->getAILongBadge(), true);
		ui->mainSceneVideoView->showAIBadge(pDialog->getAILongBadge(), true);
	} else {
		ui->mainSceneImageView->showAIBadge(QPixmap(), true);
		ui->mainSceneVideoView->showAIBadge(QPixmap(), true);
	}
}

void PLSSceneTemplateDetailScene::setSelectedViewBorder(const QString &path, int iIndex)
{
	PLSSceneTemplateImageView *imageView = initImageViewCache.value(path);
	if (nullptr != imageView && m_preClickImageView != imageView) {
		if (nullptr != m_preClickImageView) {
			m_preClickImageView->setHasBorder(false);
		}
		m_preClickImageView = imageView;
		m_preClickImageView->setHasBorder(true);
		if (nullptr != m_preClickVideoView) {
			m_preClickVideoView->setHasBorder(false);
			m_preClickVideoView = nullptr;
		}
	}

	auto videoView = initVideoViewCache.value(path);
	if (nullptr != videoView && m_preClickVideoView != videoView) {
		if (nullptr != m_preClickVideoView) {
			m_preClickVideoView->setHasBorder(false);
		}
		m_preClickVideoView = videoView;
		m_preClickVideoView->setHasBorder(true);
		if (nullptr != m_preClickImageView) {
			m_preClickImageView->setHasBorder(false);
			m_preClickImageView = nullptr;
		}
	}

	updateMainSceneResourcePath(path, iIndex);
}

void PLSSceneTemplateDetailScene::on_installButton_clicked()
{
	if (QDateTime::currentMSecsSinceEpoch() - m_dtLastInstall < 1000) {
		return;
	}

	ui->installButton->startInstall();
	bool res = pls_install_scene_template(m_item);
	ui->installButton->endInstall();

	m_dtLastInstall = QDateTime::currentMSecsSinceEpoch();
}
