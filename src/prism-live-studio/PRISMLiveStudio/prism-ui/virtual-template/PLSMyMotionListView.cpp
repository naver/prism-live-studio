#include "PLSMyMotionListView.h"
#include "ui_PLSMyMotionListView.h"
#include <pls/media-info.h>
#include "PLSMotionFileManager.h"
#include "PLSAlertView.h"
#include "obs-app.hpp"
#include "PLSMotionImageListView.h"
#include "PLSBasic.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QTime>

PLSMyMotionListView::PLSMyMotionListView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSMyMotionListView>();
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
	pls_delete(ui, nullptr);
}

void PLSMyMotionListView::updateMotionList(const QList<MotionData> &list)
{
	QList<MotionData> copied;
	QList<MotionData> removed;

	PLSMotionFileManager *manager = PLSMotionFileManager::instance();
	if (!manager->copyList(copied, m_list, list, &removed)) {
		return;
	}

	if (QList<MotionData> needNotified; manager->isRemovedChanged(needNotified, removed, m_rmlist)) {
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

void PLSMyMotionListView::setRemoveRetainSizeWhenHidden(QWidget *widget) const
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
	pls::HotKeyLocker locker;
	QStringList files = QFileDialog::getOpenFileNames(this, QString(), dir, filter);
	if (!files.isEmpty()) {
		PLSMotionFileManager::instance()->addMyResources(this, files);
	}

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	pls_check_app_exiting();
	auto view = pls_get_toplevel_view(this);
	if (view) {
		view->raise();
	}
#endif
}

PLSMotionImageListView *PLSMyMotionListView::getListView()
{
	for (QWidget *parent = this->parentWidget(); parent; parent = parent->parentWidget()) {
		if (auto view = dynamic_cast<PLSMotionImageListView *>(parent); view) {
			return view;
		}
	}
	return nullptr;
}

void PLSMyMotionListView::addResourceFinished(const QObject *sourceUi, const MotionData &md, bool isLast)
{
	auto data = PLSMotionFileManager::instance()->insertMotionData(md, MY_FILE_LIST);

	ui->emptyWidget->hide();
	ui->listFrame->show();

	m_list.prepend(data);
	ui->listFrame->updateItems(m_list);

	if (!isLast) {
		return;
	}

	if (sourceUi != this) {
		return;
	}

	//choose the first item list
	if (itemSelectedEnabled) {
		if (PLSMotionImageListView *listView = getListView(); listView) {
			listView->clickItemWithSendSignal(data);
		}
	}
}

void PLSMyMotionListView::addResourcesFinished(const QObject *sourceUi, int error)
{
	if (sourceUi != this) {
		return;
	}

	if (error == PLSAddMyResourcesProcessor::NoError) {
		return;
	}

	if (error != PLSAddMyResourcesProcessor::MaxResolutionError) {
		pls_alert_error_message(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.add.file.other.error.tip"));
	}
	if (error & PLSAddMyResourcesProcessor::MaxResolutionError) {
		pls_alert_error_message(PLSBasic::instance()->GetPropertiesWindow(), QTStr("Alert.Title"), QTStr("virtual.resource.add.file.exceed.tip"));
	}
}

void PLSMyMotionListView::deleteResourceFinished(const QObject *sourceUi, const QString &itemId, bool isVbUsed, bool isSourceUsed)
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
