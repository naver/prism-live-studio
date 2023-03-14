#include "PLSMyMotionListView.h"
#include "ui_PLSMyMotionListView.h"
#include "pls/media-info.h"
#include "PLSMotionFileManager.h"
#include "frontend-api/alert-view.hpp"
#include "pls-app.hpp"
#include "PLSMotionImageListView.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QTime>

#define MAX_RESOLUTION_SIZE 3840 * 2160

PLSMyMotionListView::PLSMyMotionListView(QWidget *parent) : QFrame(parent), ui(new Ui::PLSMyMotionListView)
{
	ui->setupUi(this);
	setRemoveRetainSizeWhenHidden(ui->listFrame);
	ui->verticalLayout->setAlignment(ui->addButton, Qt::AlignHCenter);
	ui->listFrame->insertAddFileItem();
	ui->listFrame->hide();
	ui->listFrame->setDeleteAllButtonVisible(true);

	connect(ui->addButton, &QPushButton::clicked, this, &PLSMyMotionListView::chooseLocalFileDialog);
	connect(ui->listFrame, &PLSImageListView::addFileButtonClicked, this, &PLSMyMotionListView::chooseLocalFileDialog);
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::addResourceFinished, this, &PLSMyMotionListView::addResourceFinished, Qt::QueuedConnection);
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::addResourcesFinished, this, &PLSMyMotionListView::addResourcesFinished, Qt::QueuedConnection);
	connect(PLSMotionFileManager::instance(), &PLSMotionFileManager::deleteResourceFinished, this, &PLSMyMotionListView::deleteResourceFinished);
	connect(ui->previewBtn, &QPushButton::clicked, ui->listFrame, &PLSImageListView::filterButtonClicked);
}

PLSMyMotionListView::~PLSMyMotionListView()
{
	delete ui;
}

void PLSMyMotionListView::updateMotionList(QList<MotionData> &list)
{
	QList<MotionData> copied, removed;

	PLSMotionFileManager *manager = PLSMotionFileManager::instance();
	if (!manager->copyList(copied, m_list, list, &removed)) {
		return;
	}

	QList<MotionData> needNotified;
	if (manager->isRemovedChanged(needNotified, removed, m_rmlist)) {
		manager->notifyCheckedRemoved(needNotified);
		m_rmlist = removed;
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
}

void PLSMyMotionListView::setFilterButtonVisible(bool visible)
{
	ui->listFrame->setFilterButtonVisible(visible);
	ui->previewBtnWidget->setVisible(visible);

	if (!visible) { // virtual background
		ui->verticalLayout->removeItem(ui->verticalSpacer_3);
		ui->verticalLayout->removeItem(ui->verticalSpacer_4);
		ui->verticalLayout->removeItem(ui->verticalSpacer_7);
	} else {
		ui->verticalLayout->removeItem(ui->verticalSpacer_5);
		ui->verticalLayout->removeItem(ui->verticalSpacer_6);
	}
}

bool PLSMyMotionListView::setSelectedItem(const QString &itemId)
{
	return ui->listFrame->setSelectedItem(itemId);
}

void PLSMyMotionListView::setItemSelectedEnabled(bool enabled)
{
	itemSelectedEnabled = enabled;
	ui->listFrame->setItemSelectedEnabled(enabled);
}

void PLSMyMotionListView::clearSelectedItem()
{
	ui->listFrame->clearSelectedItem();
}

void PLSMyMotionListView::deleteItem(const QString &itemId)
{
	PLSMotionFileManager::instance()->removeAt(m_list, itemId);
	ui->listFrame->deleteItem(itemId, false);
	bool zero = ui->listFrame->getShowViewListCount() == 0 ? true : false;
	if (!ui->emptyWidget->isVisible() && zero) {
		ui->listFrame->hide();
		ui->emptyWidget->show();
	}
}

void PLSMyMotionListView::deleteItemEx(const QString &itemId)
{
	PLSMotionFileManager::instance()->removeAt(m_list, itemId);
	deleteItem(itemId);
}

void PLSMyMotionListView::deleteAll()
{
	m_list.clear();
	ui->listFrame->deleteAll();

	ui->listFrame->hide();
	ui->emptyWidget->show();
}

PLSImageListView *PLSMyMotionListView::getImageListView()
{
	return ui->listFrame;
}

void PLSMyMotionListView::setRemoveRetainSizeWhenHidden(QWidget *widget)
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSMyMotionListView::chooseLocalFileDialog()
{
	QString dir = PLSMotionFileManager::instance()->getChooseFileDir();
	//PRISM/Liuying/20210323/#None/add JFIF Files
	QString filter("Background Files(*.bmp *.tga *.png *.jpg *.gif *.jfif *.psd *.mp4 *.ts *.mov *.flv *.mkv *.avi *.webm)");
	QStringList files = QFileDialog::getOpenFileNames(this, QString(), dir, filter);
	if (!files.isEmpty()) {
		PLSMotionFileManager::instance()->addMyResources(this, files);
	}
}

PLSMotionImageListView *PLSMyMotionListView::getListView()
{
	for (QWidget *parent = this->parentWidget(); parent; parent = parent->parentWidget()) {
		if (PLSMotionImageListView *view = dynamic_cast<PLSMotionImageListView *>(parent); view) {
			return view;
		}
	}
	return nullptr;
}

void PLSMyMotionListView::addResourceFinished(QObject *sourceUi, const MotionData &md, bool isLast)
{
	Q_UNUSED(sourceUi);

	PLSMotionFileManager::instance()->insertMotionData(md, MY_FILE_LIST);

	ui->emptyWidget->hide();
	ui->listFrame->show();

	m_list.prepend(md);
	ui->listFrame->updateItems(m_list);

	if (!isLast) {
		return;
	}

	PLSMotionFileManager::instance()->saveMotionList();

	if (sourceUi != this) {
		return;
	}

	//choose the first item list
	if (itemSelectedEnabled) {
		if (PLSMotionImageListView *listView = getListView(); listView) {
			listView->clickItemWithSendSignal(md);
		}
	}
}

void PLSMyMotionListView::addResourcesFinished(QObject *sourceUi, int error)
{
	if (sourceUi != this) {
		return;
	}

	if (error == PLSAddMyResourcesProcessor::NoError) {
		return;
	}

	if (error != PLSAddMyResourcesProcessor::ProcessError::MaxResolutionError) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("virtual.resource.add.file.other.error.tip"));
	}
	if (error & PLSAddMyResourcesProcessor::ProcessError::MaxResolutionError) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("virtual.resource.add.file.exceed.tip"));
	}
}

void PLSMyMotionListView::deleteResourceFinished(QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed)
{
	MotionData md;
	if (!PLSMotionFileManager::instance()->findAndRemoveAt(md, m_list, itemId)) {
		return;
	}

	ui->listFrame->deleteItem(itemId, false);

	if (PLSMotionImageListView *listView = getListView(); listView && listView != sourceUi) {
		listView->deleteItemWithSendSignal(md, isVbUsed, isSourceUsed, false);
	}
}
