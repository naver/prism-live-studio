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
#include <libresource.h>

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
	connect(ui->mainSceneComboBox, &QComboBox::currentTextChanged, this, [this]() {
		updateSceneList();

		if ((PLS_SCENE_TEMPLATE_RESOURCE->getGroupState(ui->mainSceneComboBox->currentData().toString()) == pls::rsm::State::Failed)) {
			pls_alert_error_message(pls_get_toplevel_view(this), tr("Alert.Title"), tr("SceneTemplate.Alert.Download.Failed"));
		}
	});
	connect(ui->comboBoxPlus, &QComboBox::currentIndexChanged, this, &PLSSceneTemplateMainScene::updateSceneList);

	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onItemDownloaded, this, [this](const SceneTemplateItem &item) {
		PLS_INFO(SCENE_TEMPLATE, "CategorySceneTemplate::onItemDownloaded: %s", qUtf8Printable(item.itemId()));

		auto groupId = ui->mainSceneComboBox->currentData().toString();
		auto groups = item.groups();
		if (find_if(groups.begin(), groups.end(), [groupId](const pls::rsm::Group &group) { return groupId == group.groupId(); }) != groups.end()) {
			refreshItems(groupId);
		}
	});

	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onGroupDownloadFailed, this, [this](const QString &groupId) {
		if (isVisible() && groupId == ui->mainSceneComboBox->currentData().toString()) {
			pls_alert_error_message(pls_get_toplevel_view(this), tr("Alert.Title"), tr("SceneTemplate.Alert.Download.Failed"));
		}
	});

	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onJsonDownloaded, this, [this] {
		PLS_INFO(SCENE_TEMPLATE, "CategorySceneTemplate::onJsonDownloaded");

		updateComboBoxList();
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
	if (auto state = PLS_SCENE_TEMPLATE_RESOURCE->getJsonState(); pls::rsm::State::Initialized == state || pls::rsm::State::Downloading == state) {
		showLoading(ui->scrollAreaWidgetContents);
		return;
	} else if (state == pls::rsm::State::Failed) {
		hideLoading();
		showRetry();
		return;
	}

	PLS_SCENE_TEMPLATE_RESOURCE->getGroups([this](const std::list<pls::rsm::Group> &groups) {
		pls_async_call(this, [this, groups]() {
			ui->mainSceneComboBox->blockSignals(true);

			ui->mainSceneComboBox->clear();
			for (auto &group : groups) {
				ui->mainSceneComboBox->addItem(tr(group.groupId().toUtf8().constData()), group.groupId());
			}
			updateSceneList();

			ui->mainSceneComboBox->blockSignals(false);
		});
	});
}

void PLSSceneTemplateMainScene::showMainSceneView()
{
	PLS_SCENE_TEMPLATE_RESOURCE->download();
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
	QLayoutItem *item = nullptr;
	while (nullptr != (item = m_FlowLayout->takeAt(0))) {
		item->widget()->hide();
	}

	auto groupId = ui->mainSceneComboBox->currentData().toString();
	if (!groupId.isEmpty()) {
		refreshItems(groupId);
	}
}

void PLSSceneTemplateMainScene::refreshItems(const QString &groupId)
{
	const int INDEX_PLUS = 1;
	const int INDEX_FREE = 2;

	PLS_INFO(SCENE_TEMPLATE, "%p-%s: group=%s", this, __FUNCTION__, qUtf8Printable(groupId));

	PLS_SCENE_TEMPLATE_RESOURCE->getGroup(groupId, [this, groupId](pls::rsm::Group group) {
		if (!group) {
			return;
		}
		if (!m_bRefreshing) {
			m_bRefreshing = true;
			pls_async_call(this, [this, groupId, group]() {
				auto iPlusFilter = ui->comboBoxPlus->currentIndex();
				auto bExists = false;
				auto iIndex = 0;

				auto items = group.items();
				items.sort([groupId](const pls::rsm::Item &itemLeft, const pls::rsm::Item &itemRight) {
					return PLS_SCENE_TEMPLATE_RESOURCE->getOrder(groupId, itemLeft.itemId()) < PLS_SCENE_TEMPLATE_RESOURCE->getOrder(groupId, itemRight.itemId());
				});

				for (auto item : items) {
					if (item.state() != pls::rsm::State::Ok || !PLS_SCENE_TEMPLATE_RESOURCE->checkItem(item))
						continue;

					if (INDEX_PLUS == iPlusFilter) {
						if (!SceneTemplateItem(item).isPaid()) {
							continue;
						}
					} else if (INDEX_FREE == iPlusFilter) {
						if (SceneTemplateItem(item).isPaid()) {
							continue;
						}
					}

					if (!bExists) {
						bExists = true;
					}

					PLS_INFO(SCENE_TEMPLATE, "%p-%s: index:%d, group:%s, item:%s is valid", this, __FUNCTION__, iIndex, qUtf8Printable(groupId), qUtf8Printable(item.itemId()));

					PLSSceneTemplateMainSceneItem *itemWidget = nullptr;
					if (auto iter = m_mapItems.find(item.itemId()); m_mapItems.end() != iter) {
						itemWidget = *iter;
					} else {
						itemWidget = pls_new<PLSSceneTemplateMainSceneItem>();
						m_mapItems.insert(item.itemId(), itemWidget);
						itemWidget->setObjectName("PLSSceneTemplateMainSceneItem");
					}

					m_FlowLayout->addWidget(itemWidget);
					itemWidget->show();
					itemWidget->updateUI(item);

					++iIndex;
				}

				if (!bExists) {
					if (PLS_SCENE_TEMPLATE_RESOURCE->getGroupState(groupId) == pls::rsm::State::Failed) {
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

				m_bRefreshing = false;
			});
		}
	});
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

				PLS_SCENE_TEMPLATE_RESOURCE->download();
			} else {
				PLSAlertView::warning(pls_get_toplevel_view(this), QTStr("Alert.Title"), QTStr("login.check.note.network"));
			}
		});

		layout->addStretch();
	}

	m_pWidgetRetryContainer->show();
}

void PLSSceneTemplateMainScene::hideRetry()
{
	if (nullptr != m_pWidgetRetryContainer) {
		m_pWidgetRetryContainer->hide();
	}
}
