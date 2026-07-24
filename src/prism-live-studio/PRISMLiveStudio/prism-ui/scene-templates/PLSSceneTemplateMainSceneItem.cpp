#include "PLSSceneTemplateMainSceneItem.h"
#include "PLSSceneTemplateContainer.h"
#include "ui_PLSSceneTemplateMainSceneItem.h"
#include "PLSSceneTemplateMediaManage.h"
#include "log/log.h"
#include "libui.h"
#include <chrono>

using namespace std;

PLSSceneTemplateMainSceneItem::PLSSceneTemplateMainSceneItem(QWidget *parent) : QWidget(parent), ui(new Ui::PLSSceneTemplateMainSceneItem)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
	ui->mainSceneInstallView->setVisible(false);
	ui->mainSceneTopVideoView->setVisible(false);

	// Install event filter on mainSceneTopVideoView
	ui->mainSceneTopVideoView->installEventFilter(this);

	QObject::connect(&m_checkMouseLeaveTimer, &QTimer::timeout, this, [this] { checkMouseLeaveEvent(); });
	m_performMouseEnterTimer.setSingleShot(true);
	QObject::connect(&m_performMouseEnterTimer, &QTimer::timeout, this, [this] { startHoverVideo(); });
	ui->mainSceneTopImageView->setProperty("keepAspectRatioByExpanding", true);
	ui->mainSceneLeftImageView->setProperty("keepAspectRatioByExpanding", true);
	ui->mainSceneRightImageView->setProperty("keepAspectRatioByExpanding", true);
	connect(ui->mainSceneTopVideoView, &PLSMediaRender::clicked, this, [this] { PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterDetailScenePage(m_item); });
	connect(ui->mainSceneTopImageView, &PLSSceneTemplateImageView::clicked, this, [this] { PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterDetailScenePage(m_item); });
	connect(ui->mainSceneLeftImageView, &PLSSceneTemplateImageView::clicked, this, [this] { PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterDetailScenePage(m_item); });
	connect(ui->mainSceneRightImageView, &PLSSceneTemplateImageView::clicked, this, [this] { PLS_SCENE_TEMPLATE_MEDIA_MANAGE->enterDetailScenePage(m_item); });
}

PLSSceneTemplateMainSceneItem::~PLSSceneTemplateMainSceneItem()
{
	if (m_checkMouseLeaveTimer.isActive()) {
		m_checkMouseLeaveTimer.stop();
	}
	if (m_performMouseEnterTimer.isActive()) {
		m_performMouseEnterTimer.stop();
	}
	delete ui;
}

void PLSSceneTemplateMainSceneItem::updateUI(const SceneTemplateItem &model)
{
	auto dtStart = chrono::steady_clock::now();

	if (m_hoverEnter) {
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->stopPlayVideo(ui->mainSceneTopVideoView);
	}

	m_item = model;
	ui->mainSceneIntroView->updateUI(model);
	ui->mainSceneTopImageView->updateImagePath(m_item.resource.mainSceneImagePath());
	ui->mainSceneTopVideoView->setDefaultBgImagePath(m_item.resource.mainSceneImagePath());
	ui->mainSceneLeftImageView->updateImagePath(m_item.resource.mainSceneThumbnail_1());
	ui->mainSceneRightImageView->updateImagePath(m_item.resource.mainSceneThumbnail_2());
	ui->mainSceneInstallView->updateUI(model);

	auto pDialog = qobject_cast<PLSSceneTemplateContainer *>(pls_get_toplevel_view(this));
	if (nullptr != pDialog) {
		if (model.isAI()) {
			ui->mainSceneTopImageView->showAIBadge(pDialog->getAIBadge(), false);
			ui->mainSceneTopVideoView->showAIBadge(pDialog->getAIBadge(), false);
		} else {
			ui->mainSceneTopImageView->showAIBadge(QPixmap(), false);
			ui->mainSceneTopVideoView->showAIBadge(QPixmap(), false);
		}
	}

	if (m_hoverEnter) {
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->startPlayVideo(m_item.resource.mainSceneVideoPath(), ui->mainSceneTopVideoView);
	}

	auto dtEnd = chrono::steady_clock::now();
	PLS_INFO(SCENE_TEMPLATE, "%s: duration=%lld", __FUNCTION__, chrono::duration_cast<chrono::milliseconds>(dtEnd - dtStart).count());
}

void PLSSceneTemplateMainSceneItem::enterEvent(QEnterEvent *event)
{
	checkMouseEnterEvent();
}

void PLSSceneTemplateMainSceneItem::leaveEvent(QEvent *event)
{
	checkMouseLeaveEvent();
}

void PLSSceneTemplateMainSceneItem::checkMouseEnterEvent()
{
	if (rect().contains(this->mapFromGlobal(QCursor::pos())) && !m_hoverEnter) {
		m_hoverEnter = true;
		PLS_UI_ACTION("In Scene Template Main Window, the scene template item enter event triggered.");
		m_checkMouseLeaveTimer.stop();
		m_performMouseEnterTimer.stop();
		m_checkMouseLeaveTimer.start(100);
		showHoverUI();                    // UI responds immediately
		m_performMouseEnterTimer.start(300); // video playback debounced
	}
}

void PLSSceneTemplateMainSceneItem::checkMouseLeaveEvent()
{
	if (!rect().contains(this->mapFromGlobal(QCursor::pos())) && m_hoverEnter) {
		m_hoverEnter = false;
		m_checkMouseLeaveTimer.stop();
		m_performMouseEnterTimer.stop();
		performMouseLeaveEvent();
	}
}

void PLSSceneTemplateMainSceneItem::showHoverUI()
{
	ui->mainSceneInstallView->setVisible(true);
	ui->mainSceneIntroView->setVisible(false);
	ui->mainSceneTopImageView->setVisible(false);
	ui->mainSceneTopVideoView->setVisible(true);
	PLS_UI_ACTION("In Scene Template Main Window, the install button and view more button displayed.");
}

void PLSSceneTemplateMainSceneItem::startHoverVideo()
{
	if (!m_item.resource.mainSceneVideoPath().isEmpty()) {
		PLS_SCENE_TEMPLATE_MEDIA_MANAGE->startPlayVideo(m_item.resource.mainSceneVideoPath(), ui->mainSceneTopVideoView);
	}
}

void PLSSceneTemplateMainSceneItem::performMouseEnterEvent()
{
	showHoverUI();
	startHoverVideo();
}

void PLSSceneTemplateMainSceneItem::performMouseLeaveEvent()
{
	ui->mainSceneIntroView->setVisible(true);
	ui->mainSceneInstallView->setVisible(false);
	ui->mainSceneTopImageView->setVisible(true);
	ui->mainSceneTopVideoView->setVisible(false);
	PLS_SCENE_TEMPLATE_MEDIA_MANAGE->stopPlayVideo(ui->mainSceneTopVideoView);
}


bool PLSSceneTemplateMainSceneItem::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->mainSceneTopVideoView && event->type() == QEvent::Show) {
		QPoint cursorPos = QCursor::pos();
		if (!rect().contains(mapFromGlobal(cursorPos))) {
			PLS_INFO(SCENE_TEMPLATE, "Hiding mainSceneTopVideoView because cursor is outside the widget");
			ui->mainSceneTopImageView->show();
			ui->mainSceneTopVideoView->hide();
		} else if (ui->mainSceneTopImageView->isVisible()) {
			PLS_INFO(SCENE_TEMPLATE, "Hiding mainSceneTopImageView because cursor is inside the widget");
			ui->mainSceneTopImageView->hide();
		}
	}
	return QWidget::eventFilter(watched, event);
}
