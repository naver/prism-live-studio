#include "PLSSceneTemplateMainScene.h"
#include "ui_PLSSceneTemplateMainScene.h"
#include "ui_PLSSceneTemplateToast.h"
#include "PLSSceneTemplateMediaManage.h"
#include "libutils-api.h"
#include "PLSSceneTemplateResourceMgr.h"
#include <QResizeEvent>
#include "PLSErrorHandler.h"
#include "frontend-api.h"
#include "pls-common-define.hpp"
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

	pls_uistep_v2_set_name(ui->mainSceneComboBox, "Sort by Latest");

	if (auto groups = PLS_SCENE_TEMPLATE_RESOURCE->getGroups(); !groups.empty()) {
		if (PLS_SCENE_TEMPLATE_RESOURCE->getGroupState(groups.front().groupId()) == pls::rsm::State::Ok) {
			showLoading(this, "SceneTemplate.Label.Showing");
		}
	}

	connect(ui->mainSceneComboBox, &QComboBox::currentTextChanged, this, [this]() {
		updateSceneList();

		if ((PLS_SCENE_TEMPLATE_RESOURCE->getGroupState(ui->mainSceneComboBox->currentData().toString()) == pls::rsm::State::Failed)) {
			pls_async_call(this, [this]() { showToast(); });
		}
	});
	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onItemDownloaded, this, [this](const SceneTemplateItem &item, bool ok) {
		PLS_INFO(SCENE_TEMPLATE, "CategorySceneTemplate::onItemDownloaded: %s, status: %d", qUtf8Printable(item.itemId()), ok);

		auto groupId = ui->mainSceneComboBox->currentData().toString();
		auto groups = item.groups();
		if (find_if(groups.begin(), groups.end(), [groupId](const pls::rsm::Group &group) { return groupId == group.groupId(); }) != groups.end()) {
			refreshItems(groupId);
		}
	});

	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onJsonDownloaded, this, [this] {
		PLS_INFO(SCENE_TEMPLATE, "CategorySceneTemplate::onJsonDownloaded");

		updateComboBoxList();
	});
	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onAllDownloaded, this, [this](bool ok) {
		if (ok) {
			return;
		}

		bool partFailed = false;
		auto groups = PLS_SCENE_TEMPLATE_RESOURCE->getGroups();
		for (const auto &group : groups) {
			if (!group) {
				continue;
			}
			auto items = group.items();
			for (const auto &item : items) {
				if (!item) {
					continue;
				}
				if (item.state() == pls::rsm::State::Ok) {
					partFailed = true;
					break;
				}
			}
		}

		if (partFailed) {
			showToast();
		} else {
			showRetry();
		}
	});

	updateComboBoxList();
	this->installEventFilter(this);
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

void PLSSceneTemplateMainScene::showLoading(QWidget *parent, const char *loadingText)
{
	PLS_UI_ACTION("PLSSceneTemplateMainScene::showLoading");

	if (m_pWidgetLoadingBG && m_pWidgetLoadingBG->property("loadingText").toString() == loadingText) {
		return;
	}

	hideLoading();
	m_pWidgetLoadingBGParent = parent;

	m_pWidgetLoadingBG = new QWidget(parent);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setStyleSheet("background-color: #272727;");

#if defined(Q_OS_MACOS)
	m_pWidgetLoadingBG->setAttribute(Qt::WA_DontCreateNativeAncestors);
	m_pWidgetLoadingBG->setAttribute(Qt::WA_NativeWindow);
#endif

	m_pWidgetLoadingBG->setGeometry(0, getMarginTopAndBottom(), width(), height() - getMarginTopAndBottom());
	m_pWidgetLoadingBG->show();

	auto layout = pls_new<QVBoxLayout>(m_pWidgetLoadingBG);
	layout->setSpacing(20);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	layout->addStretch();
	layout->addWidget(loadingBtn, 0, Qt::AlignCenter);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->setStyleSheet("background-color: transparent;");
	loadingBtn->show();
	auto labelLoading = pls_new<QLabel>(tr(loadingText), m_pWidgetLoadingBG);
	labelLoading->setObjectName("labelLoading");
	layout->addWidget(labelLoading, 0, Qt::AlignCenter);
	layout->addSpacing(90);
	layout->addStretch();
	m_pWidgetLoadingBG->setProperty("loadingText", loadingText);

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
		showLoading(this);
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
	if (watcher == this && event->type() == QEvent::Resize) {
		const QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);

		if (m_pWidgetLoadingBG) {
			m_pWidgetLoadingBG->setGeometry(0, getMarginTopAndBottom(), width(), height() - getMarginTopAndBottom());
		}

		if (m_pWidgetRetryContainer) {
			m_pWidgetRetryContainer->setGeometry(0, getMarginTopAndBottom(), width(), height() - getMarginTopAndBottom());
		}

		if (m_toast) {
			m_toast->setGeometry(width() - m_toast->width() - 20, 20, m_toast->width(), m_toast->height());
			m_toast->customResize();
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
	PLS_INFO(SCENE_TEMPLATE, "%p-%s: group=%s", this, __FUNCTION__, qUtf8Printable(groupId));

	PLS_SCENE_TEMPLATE_RESOURCE->getGroup(groupId, this, [this, groupId](pls::rsm::Group group) {
		if (!group) {
			return;
		}
		if (m_bRefreshing) {
			m_refreshPending = true;
			m_pendingGroupId = groupId;
			return;
		}
		m_bRefreshing = true;
		pls_async_call(this, [this, groupId, group]() {
				auto bExists = false;
				bool needShowLoading = true;
				auto iIndex = 0;
				auto items = group.items();
				items.sort([groupId](const pls::rsm::Item &itemLeft, const pls::rsm::Item &itemRight) {
					return PLS_SCENE_TEMPLATE_RESOURCE->getOrder(groupId, itemLeft.itemId()) < PLS_SCENE_TEMPLATE_RESOURCE->getOrder(groupId, itemRight.itemId());
				});

				for (auto item : items) {
					if (item.state() == pls::rsm::State::Ok) {
						bExists = true;
					}

					PLS_INFO(SCENE_TEMPLATE, "%p-%s: index:%d, group:%s, item:%s is valid", this, __FUNCTION__, iIndex, qUtf8Printable(groupId), qUtf8Printable(item.itemId()));

					PLSSceneTemplateMainSceneItem *itemWidget = nullptr;
					if (auto iter = m_mapItems.find(item.itemId()); m_mapItems.end() != iter) {
						itemWidget = *iter;
						needShowLoading = false;
					} else {
						itemWidget = pls_new<PLSSceneTemplateMainSceneItem>();
						m_mapItems.insert(item.itemId(), itemWidget);
						itemWidget->setObjectName("PLSSceneTemplateMainSceneItem");
						if (PLS_SCENE_TEMPLATE_RESOURCE->getGroupState(groupId) == pls::rsm::State::Ok) {
							QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, common::FEED_UI_MAX_TIME);
						}
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
					} else if (needShowLoading) {
						hideRetry();
						showLoading(this);
					}
				} else {
					hideLoading();
					hideRetry();
				}

				m_bRefreshing = false;
				const bool needAgain = m_refreshPending;
				QString pendingGid = m_pendingGroupId;
				m_refreshPending = false;
				m_pendingGroupId.clear();
				if (needAgain && !pendingGid.isEmpty()) {
					pls_async_call(this, [this, pendingGid]() { refreshItems(pendingGid); });
				}
			});
	});
}

void PLSSceneTemplateMainScene::showRetry()
{
	PLS_INFO(SCENE_TEMPLATE, "%s", __FUNCTION__);

	if (m_toast) {
		m_toast->hide();
	}

	if (nullptr == m_pWidgetRetryContainer) {
		m_pWidgetRetryContainer = pls_new<QWidget>(this);
#if defined(Q_OS_MACOS)
		m_pWidgetRetryContainer->setAttribute(Qt::WA_DontCreateNativeAncestors);
		m_pWidgetRetryContainer->setAttribute(Qt::WA_NativeWindow);
#endif
		m_pWidgetRetryContainer->setGeometry(0, getMarginTopAndBottom(), width(), height() - getMarginTopAndBottom());
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
		imageRetrying->setStyleSheet("min-width:90px; max-width:90px; min-height:90px; max-height:90px;");

		auto labelRetrying = pls_new<QLabel>(tr("SceneTemplate.Label.Retry"), m_pWidgetRetryContainer);
		labelRetrying->setObjectName("labelRetry");
		labelRetrying->show();
		labelRetrying->setAlignment(Qt::AlignCenter);
		layout->addWidget(labelRetrying, 0, Qt::AlignCenter);

		auto buttonRetrying = pls_new<QPushButton>(tr("SceneTemplate.Button.Retry"), m_pWidgetRetryContainer);
		buttonRetrying->setObjectName("buttonRetry");
		buttonRetrying->show();
		layout->addWidget(buttonRetrying, 0, Qt::AlignCenter);
		layout->addSpacing(90);
		connect(buttonRetrying, &QPushButton::clicked, this, [=] {
			if (pls_get_network_state()) {
				showLoading(this);
				hideRetry();

				PLS_SCENE_TEMPLATE_RESOURCE->download();
			} else {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LOGIN_CHECK_NOTE_NETWORK, PLSErrKeyAllAlert, QString(),
								      PLSErrorHandler::ExtraData(QStringLiteral("PLSSceneTemplateMainScene::retryButton.network")), pls_get_toplevel_view(this));
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

void PLSSceneTemplateMainScene::showToast()
{
	if (m_pWidgetRetryContainer && m_pWidgetRetryContainer->isVisible()) {
		if (m_toast) {
			m_toast->hide();
		}
		return;
	}

	if (!m_toast) {
		m_toast = pls_new<PLSSceneTemplateToast>(this);
		m_toast->setObjectName("toastView");
	}
	pls_async_call(this, [this]() {
		m_toast->setGeometry(width() - m_toast->width() - 20, 20, m_toast->width(), m_toast->height());
		m_toast->show();
		m_toast->customResize();
	});
}

int PLSSceneTemplateMainScene::getMarginTopAndBottom()
{
	if (m_toast && m_toast->isVisible()) {
		return std::max(95 + ui->mainSceneTitle->height(), 20 + m_toast->height());
	}
	return 95 + ui->mainSceneTitle->height();
}

PLSSceneTemplateToast::PLSSceneTemplateToast(QWidget *parent) : QFrame(parent), ui(new Ui::PLSSceneTemplateToast)
{
	ui->setupUi(this);
	connect(PLS_SCENE_TEMPLATE_RESOURCE, &CategorySceneTemplate::onAllDownloaded, this, [this](bool ok) {
		PLSLoadingView::deleteLoadingView(m_loadingView);
		ui->refreshBtn->setEnabled(true);
		if (ok) {
			close();
		}
	});
	connect(ui->refreshBtn, &QPushButton::clicked, this, [this]() {
		if (pls_get_network_state()) {
			m_loadingView = PLSLoadingView::newLoadingView(ui->refreshBtn, 2, QStringLiteral(":resource/images/loading/loading-%1.svg"), QSize(18, 18));
			ui->refreshBtn->setEnabled(false);
			PLS_SCENE_TEMPLATE_RESOURCE->download();
		}
	});
	connect(ui->closeBtn, &QPushButton::clicked, this, &PLSSceneTemplateToast::close);
};

PLSSceneTemplateToast::~PLSSceneTemplateToast()
{
	delete ui;
}

void PLSSceneTemplateToast::customResize()
{
	QLabel *label = ui->descLabel;
	const int textWidth = 250; // effective width for wrapping (match layout)
	QFontMetrics fm(label->font());
	QString text = label->text();
	QRect rect = fm.boundingRect(QRect(0, 0, textWidth, 0), Qt::TextWordWrap | Qt::AlignLeft, text);
	int labelHeight = rect.height();
	// Ensure at least one line height for short text
	if (labelHeight <= 0) {
		labelHeight = fm.lineSpacing();
	}
	label->setFixedHeight(labelHeight);
	// Frame height = top margin(10) + label + bottom margin(15) + extra for alignment; buttons are in same row
	setFixedHeight(labelHeight + 35);
}
