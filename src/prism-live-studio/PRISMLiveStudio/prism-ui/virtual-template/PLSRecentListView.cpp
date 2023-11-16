#include "PLSRecentListView.h"
#include "ui_PLSRecentListView.h"
#include "PLSMotionFileManager.h"
#include <QTimer>
#include "utils-api.h"
#include "libui.h"

PLSRecentListView::PLSRecentListView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSRecentListView>();
	ui->setupUi(this);

	ui->listFrame->setAutoUpdateCloseButtons(true);
	setRemoveRetainSizeWhenHidden(ui->emptyWidget);
	setRemoveRetainSizeWhenHidden(ui->listFrame);
	ui->listFrame->hide();
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::deleteResourceFinished, this, &PLSRecentListView::deleteResourceFinished, Qt::QueuedConnection);
	connect(ui->previewBtn, &QPushButton::clicked, ui->listFrame, &PLSImageListView::filterButtonClicked);
}

PLSRecentListView::~PLSRecentListView()
{
	pls_delete(ui);
}

void PLSRecentListView::updateMotionList(const QList<MotionData> &list)
{
	QList<MotionData> copied;
	QList<MotionData> removed;

	PLSMotionFileManager *manager = PLSMotionFileManager::instance();
	manager->copyList(copied, m_list, list, &removed);

	if (QList<MotionData> needNotified; manager->isRemovedChanged(needNotified, removed, m_rmlist)) {
		manager->notifyCheckedRemoved(needNotified);
	}

	if (copied.isEmpty()) {
		ui->listFrame->hide();
		ui->emptyWidget->show();
	} else {
		ui->emptyWidget->hide();
		ui->listFrame->show();
	}

	m_list = copied;
	m_rmlist = removed;
	ui->listFrame->updateItems(m_list);
}

bool PLSRecentListView::setSelectedItem(const QString &itemId)
{
	return ui->listFrame->setSelectedItem(itemId);
}

void PLSRecentListView::setItemSelectedEnabled(bool enabled)
{
	ui->listFrame->setItemSelectedEnabled(enabled);
}

void PLSRecentListView::clearSelectedItem()
{
	ui->listFrame->clearSelectedItem();
}

void PLSRecentListView::deleteItem(const QString &itemId)
{
	PLSMotionFileManager::instance()->removeAt(m_list, itemId);
	ui->listFrame->deleteItem(itemId, true);
	bool zero = ui->listFrame->getShowViewListCount() == 0 ? true : false;
	if (!ui->emptyWidget->isVisible() && zero) {
		ui->listFrame->hide();
		ui->emptyWidget->show();
	}
}

void PLSRecentListView::setFilterButtonVisible(bool visible)
{
	ui->listFrame->setFilterButtonVisible(visible);
	ui->previewBtnWidget->setVisible(visible);

	if (!visible) { // virtual background
		ui->verticalLayout_2->removeItem(ui->verticalSpacer_3);
		ui->verticalLayout_2->removeItem(ui->verticalSpacer_5);
	} else {
		ui->verticalLayout_2->removeItem(ui->verticalSpacer_4);
	}
}

PLSImageListView *PLSRecentListView::getImageListView()
{
	return ui->listFrame;
}

void PLSRecentListView::setRemoveRetainSizeWhenHidden(QWidget *widget) const
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSRecentListView::deleteResourceFinished(const QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed)
{
	Q_UNUSED(sourceUi)
	Q_UNUSED(isVbUsed)
	Q_UNUSED(isSourceUsed)

	PLSMotionFileManager::instance()->removeAt(m_list, itemId);
	ui->listFrame->deleteItem(itemId, true);

	if (m_list.isEmpty()) {
		ui->listFrame->hide();
		ui->emptyWidget->show();
	} else {
		ui->emptyWidget->hide();
		ui->listFrame->show();
	}
}
