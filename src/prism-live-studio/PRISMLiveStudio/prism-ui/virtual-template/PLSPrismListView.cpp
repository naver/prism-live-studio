#include "PLSPrismListView.h"
#include "ui_PLSPrismListView.h"
#include "PLSMotionNetwork.h"
#include "PLSMotionItemView.h"
#include "PLSMotionFileManager.h"
#include <QTimer>
#include "utils-api.h"
#include "PLSAlertView.h"
#include "liblog.h"
#include "log/module_names.h"

PLSPrismListView::PLSPrismListView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSPrismListView>();
	ui->setupUi(this);
	ui->retryWidget->installEventFilter(this);
	setRemoveRetainSizeWhenHidden(ui->listFrame);
	setRemoveRetainSizeWhenHidden(ui->emptyWidget);
	connect(ui->previewBtn, &QPushButton::clicked, ui->listFrame, &PLSImageListView::filterButtonClicked);
	ui->emptyWidget->setHidden(true);
}

PLSPrismListView::~PLSPrismListView()
{
	pls_delete(ui);
}

bool PLSPrismListView::setSelectedItem(const QString &itemId)
{
	return ui->listFrame->setSelectedItem(itemId);
}

void PLSPrismListView::setItemSelectedEnabled(bool enabled)
{
	ui->listFrame->setItemSelectedEnabled(enabled);
}

void PLSPrismListView::clearSelectedItem()
{
	ui->listFrame->clearSelectedItem();
}

void PLSPrismListView::initListView(bool freeView)
{
	m_freeView = freeView;

	//retry button logic
	connect(ui->retryButton, &QPushButton::clicked, this, &PLSPrismListView::retryDownloadList);

	//when the single resource download finished, refresh the flow layout, because ux required
	if (m_freeView) {
		connect(PLSMotionNetwork::instance(), &PLSMotionNetwork::freeResourceDownloadFinished, this, [this](const MotionData &md, bool update) {
			if (!update) {
				initItemListView();
			} else {
				updateItem(md);
			}
		});
	} else {
		connect(PLSMotionNetwork::instance(), &PLSMotionNetwork::prismResourceDownloadFinished, this, [this](const MotionData &md, bool update) {
			if (!update) {
				initItemListView();
			} else {
				updateItem(md);
			}
		});
	}

	//init the list view
	initItemListView();

	//show loading view
	initLoadingView();
}

void PLSPrismListView::setFilterButtonVisible(bool visible)
{
	ui->listFrame->setFilterButtonVisible(visible);
	ui->previewBtnWidget->setVisible(visible);

	if (!visible) { // virtual background
		ui->verticalLayout->removeItem(ui->verticalSpacer_4);
	} else {
		ui->verticalLayout->removeItem(ui->verticalSpacer_5);
	}
}

void PLSPrismListView::initLoadingView()
{
	const PLSMotionNetwork *mn = PLSMotionNetwork::instance();
	//when the prism list download finished, if request count is zero, show NodataUI, if not show list view
	if (m_freeView) {
		connect(mn, &PLSMotionNetwork::freeListStartedSignal, this, &PLSPrismListView::itemListStarted);
		connect(mn, &PLSMotionNetwork::freeListFinishedSignal, this, &PLSPrismListView::itemListFinished);
		if (mn->freeRequestFinished()) {
			itemListFinished();
		} else if (ui->listFrame->getShowViewListCount() == 0) {
			showLoading();
		}
	} else {
		connect(mn, &PLSMotionNetwork::prismListStartedSignal, this, &PLSPrismListView::itemListStarted);
		connect(mn, &PLSMotionNetwork::prismListFinishedSignal, this, &PLSPrismListView::itemListFinished);
		if (mn->prismRequestFinished()) {
			itemListFinished();
		} else if (ui->listFrame->getShowViewListCount() == 0) {
			showLoading();
		}
	}
}

PLSImageListView *PLSPrismListView::getImageListView()
{
	return ui->listFrame;
}

void PLSPrismListView::setForProperties(bool forProperties)
{
	m_forProperties = forProperties;
}

bool PLSPrismListView::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->retryWidget && event->type() == QEvent::Resize && m_pWidgetLoadingBG) {
		m_pWidgetLoadingBG->setGeometry(getLoadingBGRect());
	}
	return QFrame::eventFilter(watcher, event);
}

void PLSPrismListView::setRemoveRetainSizeWhenHidden(QWidget *widget) const
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSPrismListView::initItemListView()
{
	//enumeration the list
	QList<MotionData> list;
	if (m_freeView) {
		list = PLSMotionNetwork::instance()->getFreeCacheList();
	} else {
		list = PLSMotionNetwork::instance()->getPrismCacheList();
	}

	QList<MotionData> copied;
	if (!PLSMotionFileManager::instance()->copyListForPrism(copied, m_list, list)) {
		return;
	}

	if (!copied.isEmpty()) {
		hideLoading();
	}

	if (copied.isEmpty()) {
		ui->listFrame->hide();
		ui->emptyWidget->show();
	} else {
		ui->emptyWidget->hide();
		ui->listFrame->show();
	}

	m_list = copied;
	ui->listFrame->updateItems(m_list);
	ui->listFrame->triggerSelectedSignal();
}

void PLSPrismListView::updateItem(const MotionData &md)
{
	for (auto &it : m_list) {
		if (it.itemId == md.itemId) {
			it = md;
			break;
		}
	}

	ui->listFrame->updateItem(md);
}

void PLSPrismListView::retryDownloadList()
{
	ui->emptyWidget->setHidden(true);
	ui->listFrame->setHidden(false);
	retryClicked = true;
	showLoading();

	PLSMotionNetwork::instance()->downloadFreeListRequest();
	PLSMotionNetwork::instance()->downloadPrismListRequest();
}

void PLSPrismListView::itemListStarted()
{
	showLoading();
}

void PLSPrismListView::itemListFinished()
{
	bool zero = ui->listFrame->getShowViewListCount() == 0;
	ui->emptyWidget->setHidden(!zero);
	ui->listFrame->setHidden(zero);
	hideLoading();

	if (zero && retryClicked) {
		PLSAlertView::warning(this, tr("Alert.Title"), tr("login.check.note.network"));
	}
	retryClicked = false;
}

void PLSPrismListView::showLoading()
{
	if (m_pWidgetLoadingBG) {
		return;
	}

	m_pWidgetLoadingBG = pls_new<QWidget>(ui->retryWidget);
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(getLoadingBGRect());
	m_pWidgetLoadingBG->show();

	ui->listFrame->hide();
	ui->emptyWidget->show();

	auto layout = pls_new<QHBoxLayout>(m_pWidgetLoadingBG);
	auto loadingBtn = pls_new<QPushButton>(m_pWidgetLoadingBG);
	layout->addWidget(loadingBtn);
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	m_loadingEvent.startLoadingTimer(loadingBtn);
}

void PLSPrismListView::hideLoading()
{
	if (nullptr != m_pWidgetLoadingBG) {
		if (m_list.isEmpty()) {
			ui->listFrame->hide();
			ui->emptyWidget->show();
		} else {
			ui->emptyWidget->hide();
			ui->listFrame->show();
		}

		m_loadingEvent.stopLoadingTimer();

		pls_delete(m_pWidgetLoadingBG, nullptr);
	}
}

QRect PLSPrismListView::getLoadingBGRect()
{
	QRect rect = ui->retryWidget->rect();
	if (m_forProperties) {
		rect -= QMargins(0, 0, 10, 0);
	} else {
		rect -= QMargins(0, 0, 15, 15);
	}
	return rect;
}
