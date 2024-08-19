#include "PLSSceneTemplateMainScene.h"
#include "ui_PLSSceneTemplateMainScene.h"
#include "PLSSceneTemplateMediaManage.h"
#include "libutils-api.h"
#include "PLSSceneTemplateResourceMgr.h"
#include <QResizeEvent>
#include "PLSAlertView.h"
#include <QLabel>
#include "obs-app.hpp"
#include "log/log.h"
#include <chrono>

using namespace std;

const int FLOW_LAYOUT_HSPACING = 15;
const int FLOW_LAYOUT_VSPACING = 15;
const int FLOW_LAYOUT_MARGIN_LEFT = 30;
const int FLOW_LAYOUT_MARGIN_RIGHT = 0;
const int FLOW_LAYOUT_MARGIN_TOP = 0;
const int FLOW_LAYOUT_MARGIN_BOTTOM = 10;

PLSSceneTemplateMainScene::PLSSceneTemplateMainScene(QWidget *parent) : QWidget(parent), ui(new Ui::PLSSceneTemplateMainScene)
{
	ui->setupUi(this);
	pls_add_css(this, {"PLSLoadingBtn"});
	initFlowLayout();
	connect(ui->mainSceneComboBox, &QComboBox::currentTextChanged, this, &PLSSceneTemplateMainScene::updateSceneList);

	connect(&PLS_SCENE_TEMPLATE_RESOURCE, &PLSSceneTemplateResourceMgr::onItemFinished, this, [this](const SceneTemplateItem &model) {
		PLS_INFO(SCENE_TEMPLATE, "PLSSceneTemplateResourceMgr::onItemFinished: %s %s", qUtf8Printable(model.groupId), qUtf8Printable(model.itemId));
		auto groupId = ui->mainSceneComboBox->currentData().toString();
		if (groupId == model.groupId) {
			refreshItems(groupId);
		}
	});

	connect(&PLS_SCENE_TEMPLATE_RESOURCE, &PLSSceneTemplateResourceMgr::onListFinished, this, [this] {
		PLS_INFO(SCENE_TEMPLATE, "PLSSceneTemplateResourceMgr::onListFinished");
		updateComboBoxList();
	});
	connect(&PLS_SCENE_TEMPLATE_RESOURCE, &PLSSceneTemplateResourceMgr::onItemsFinished, this, [this] {
		PLS_INFO(SCENE_TEMPLATE, "PLSSceneTemplateResourceMgr::onItemsFinished");
		auto groupId = ui->mainSceneComboBox->currentData().toString();
		refreshItems(groupId);
	});

	updateComboBoxList();

	ui->scrollAreaWidgetContents->installEventFilter(this);
}

PLSSceneTemplateMainScene::~PLSSceneTemplateMainScene()
{
	hideLoading();
	m_loadingEvent.stopLoadingTimer();
	delete ui;
}

void PLSSceneTemplateMainScene::initFlowLayout()
{
	m_FlowLayout = pls_new<FlowLayout>(nullptr, 0, FLOW_LAYOUT_HSPACING, FLOW_LAYOUT_VSPACING);
	m_FlowLayout->setItemRetainSizeWhenHidden(false);
	m_FlowLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	m_FlowLayout->setContentsMargins(FLOW_LAYOUT_MARGIN_LEFT, FLOW_LAYOUT_MARGIN_TOP, FLOW_LAYOUT_MARGIN_RIGHT, FLOW_LAYOUT_MARGIN_BOTTOM);
	ui->scrollAreaWidgetContents->setLayout(m_FlowLayout);
}

void PLSSceneTemplateMainScene::showLoading(QWidget *parent)
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	hideLoading();

	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = new QWidget(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setStyleSheet("background-color: #272727;");

#if defined(Q_OS_MACOS)
	m_pWidgetLoadingBG->setAttribute(Qt::WA_DontCreateNativeAncestors);
	m_pWidgetLoadingBG->setAttribute(Qt::WA_NativeWindow);
#endif

	m_pWidgetLoadingBG->setGeometry(ui->scrollAreaWidgetContents->geometry());
	m_pWidgetLoadingBG->show();

	auto layout = pls_new<QVBoxLayout>(m_pWidgetLoadingBG);
	layout->setSpacing(20);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	layout->addStretch();
	layout->addWidget(loadingBtn, 0, Qt::AlignCenter);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->setStyleSheet("background-color: transparent;");
	loadingBtn->show();
	auto labelLoading = pls_new<QLabel>(tr("SceneTemplate.Label.Loading"), m_pWidgetLoadingBG);
	labelLoading->setObjectName("labelLoading");
	layout->addWidget(labelLoading, 0, Qt::AlignCenter);
	layout->addStretch();

	m_loadingEvent.startLoadingTimer(loadingBtn);
}

void PLSSceneTemplateMainScene::hideLoading()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	if (m_pWidgetLoadingBGParent && pls_object_is_valid(m_pWidgetLoadingBGParent)) {
		m_pWidgetLoadingBGParent = nullptr;
	}

	if (m_pWidgetLoadingBG && pls_object_is_valid(m_pWidgetLoadingBG)) {
		m_loadingEvent.stopLoadingTimer();

		pls_delete(m_pWidgetLoadingBG);
		m_pWidgetLoadingBG = nullptr;
	}
}

void PLSSceneTemplateMainScene::updateComboBoxList()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	if (!PLS_SCENE_TEMPLATE_RESOURCE.isListFinished()) {
		showLoading(ui->scrollAreaWidgetContents);
		return;
	}
	if (!PLS_SCENE_TEMPLATE_RESOURCE.isListValid()) {
		hideLoading();
		showRetry();
		return;
	}

	ui->mainSceneComboBox->blockSignals(true);
	ui->mainSceneComboBox->clear();
	for (auto &item : PLS_SCENE_TEMPLATE_RESOURCE.getGroupIdList()) {
		ui->mainSceneComboBox->addItem(tr(item.toUtf8().constData()), item);
	}
	ui->mainSceneComboBox->blockSignals(false);
	updateSceneList();
}

void PLSSceneTemplateMainScene::showMainSceneView()
{
	PLS_SCENE_TEMPLATE_RESOURCE.downloadList();
}

bool PLSSceneTemplateMainScene::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->scrollAreaWidgetContents && event->type() == QEvent::Resize) {
		const QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);

		if (m_pWidgetLoadingBG) {
			m_pWidgetLoadingBG->setGeometry(QRect(QPoint(0, 0), resizeEvent->size()));
		}

		if (m_pWidgetRetryContainer) {
			m_pWidgetRetryContainer->setGeometry(QRect(QPoint(0, 0), resizeEvent->size()));
		}
	}

	return QWidget::eventFilter(watcher, event);
}

void PLSSceneTemplateMainScene::updateSceneList()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	for (auto i = 0; i < m_FlowLayout->count(); ++i) {
		auto item = qobject_cast<PLSSceneTemplateMainSceneItem *>(m_FlowLayout->itemAt(i)->widget());
		item->hide();
	}

	auto groupId = ui->mainSceneComboBox->currentData().toString();
	refreshItems(groupId);
}

void PLSSceneTemplateMainScene::refreshItems(const QString &groupId)
{
	PLS_INFO(SCENE_TEMPLATE, "%s: groupId=%s", __FUNCTION__, qUtf8Printable(groupId));
	auto dtStart = chrono::steady_clock::now();

	const QList<SceneTemplateItem> models = PLS_SCENE_TEMPLATE_RESOURCE.getSceneTemplateGroup(groupId);
	auto iIndex = 0;
	for (const SceneTemplateItem &model : models) {
		if (PLSSceneTemplateResourceMgr::instance().isItemValid(model)) {
			PLS_INFO(SCENE_TEMPLATE, "%s index:%d, group:%s, item:%s is valid", __FUNCTION__, iIndex, qUtf8Printable(model.groupId), qUtf8Printable(model.itemId));

			PLSSceneTemplateMainSceneItem *item = nullptr;
			if (iIndex < m_FlowLayout->count()) {
				item = qobject_cast<PLSSceneTemplateMainSceneItem *>(m_FlowLayout->itemAt(iIndex)->widget());
			} else {
				item = pls_new<PLSSceneTemplateMainSceneItem>();
				item->setObjectName("PLSSceneTemplateMainSceneItem");
				m_FlowLayout->addWidget(item);
			}
			item->updateUI(model);
			item->show();

			++iIndex;
		} else {
			PLS_INFO(SCENE_TEMPLATE, "%s group:%s, item:%s is invalid", __FUNCTION__, qUtf8Printable(model.groupId), qUtf8Printable(model.itemId));
		}
	}

	if (0 == iIndex) {
		if (PLS_SCENE_TEMPLATE_RESOURCE.isItemsFinished()) {
			hideLoading();
			showRetry();
		} else {
			hideRetry();
			showLoading(ui->scrollAreaWidgetContents);
		}
	} else {
		hideLoading();
		hideRetry();
	}

	auto dtEnd = chrono::steady_clock::now();
	PLS_INFO(SCENE_TEMPLATE, "%s: duration=%lld", __FUNCTION__, chrono::duration_cast<chrono::milliseconds>(dtEnd - dtStart).count());

	if (isVisible() && iIndex != models.count() && PLS_SCENE_TEMPLATE_RESOURCE.isItemsFinished()) {
		pls_alert_error_message(this, tr("Alert.Title"), tr("SceneTemplate.Alert.Download.Failed"));
	}
}

void PLSSceneTemplateMainScene::showRetry()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	if (nullptr == m_pWidgetRetryContainer) {
		m_pWidgetRetryContainer = pls_new<QWidget>(ui->scrollAreaWidgetContents);
#if defined(Q_OS_MACOS)
		m_pWidgetRetryContainer->setAttribute(Qt::WA_DontCreateNativeAncestors);
		m_pWidgetRetryContainer->setAttribute(Qt::WA_NativeWindow);
#endif
		m_pWidgetRetryContainer->setGeometry(ui->scrollAreaWidgetContents->geometry());
		m_pWidgetRetryContainer->setStyleSheet("background-color: #272727;");

		auto layout = pls_new<QVBoxLayout>(m_pWidgetRetryContainer);
		layout->setSpacing(22);
		layout->addStretch();

		auto imageRetrying = pls_new<QLabel>(m_pWidgetRetryContainer);
		imageRetrying->show();
		imageRetrying->setFixedSize(90, 90);
		imageRetrying->setPixmap(QPixmap(":/resource/images/scene-template/ic_prism.png"));
		imageRetrying->setScaledContents(true);
		layout->addWidget(imageRetrying, 0, Qt::AlignCenter);
		imageRetrying->setStyleSheet("min-width:90px; max-width:90px; min-height:90px; max-height:90px");

		auto labelRetrying = pls_new<QLabel>(tr("SceneTemplate.Label.Retry"), m_pWidgetRetryContainer);
		labelRetrying->setObjectName("labelRetry");
		labelRetrying->show();
		labelRetrying->setAlignment(Qt::AlignCenter);
		layout->addWidget(labelRetrying, 0, Qt::AlignCenter);

		auto buttonRetrying = pls_new<QPushButton>(tr("SceneTemplate.Button.Retry"), m_pWidgetRetryContainer);
		buttonRetrying->setObjectName("buttonRetry");
		buttonRetrying->show();
		layout->addWidget(buttonRetrying, 0, Qt::AlignCenter);
		connect(buttonRetrying, &QPushButton::clicked, this, [=] {
			if (pls_get_network_state()) {
				showLoading(ui->scrollAreaWidgetContents);
				hideRetry();
				PLS_SCENE_TEMPLATE_RESOURCE.downloadList();
			} else {
				PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("login.check.note.network"));
			}
		});

		layout->addStretch();
	}

	m_pWidgetRetryContainer->show();
}

void PLSSceneTemplateMainScene::hideRetry()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	if (nullptr != m_pWidgetRetryContainer) {
		m_pWidgetRetryContainer->hide();
	}
}
