#include "PLSLaboratoryInstallView.h"
#include "ui_PLSLaboratoryInstallView.h"
#include "PLSLaboratoryManage.h"

const int g_installSuccess = 1000;
const int g_installNetworkError = 1001;
const int g_installUnkownError = 1002;

PLSLaboratoryInstallView::PLSLaboratoryInstallView(const QString &labId, QWidget *parent) : PLSDialogView(parent), m_labId(labId)
{
	ui = pls_new<Ui::PLSLaboratoryInstallView>();
	pls_add_css(this, {"PLSLaboratoryInstallView", "PLSLoadingBtn"});
	setupUi(ui);
	setResizeEnabled(false);
	QMetaObject::connectSlotsByName(this);
	ui->confirmBackView->installEventFilter(this);
	ui->installLabel->setText(tr("laboratory.item.install"));
	QSizePolicy sizePolicy = ui->loadingBtn->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->loadingBtn->setSizePolicy(sizePolicy);
	ui->loadingBtn->setHidden(true);
	ui->titleLabel->setText(tr("laboratory.item.install.content"));
	connect(this, &PLSDialogView::rejected, this, [this]() {
		m_plsCancel = true;
		if (returnCode != 0) {
			setResult(returnCode);
		}
	});
	setWindowTitle(tr("Alert.Title"));
}

PLSLaboratoryInstallView::~PLSLaboratoryInstallView()
{
	ui->loadingBtn->setHidden(true);
	pls_delete(ui);
}

void PLSLaboratoryInstallView::showLoading()
{
	m_installing = true;
	m_loadingEvent.startLoadingTimer(ui->loadingBtn);
	ui->loadingBtn->setHidden(false);
}

void PLSLaboratoryInstallView::hideLoading()
{
	m_installing = false;
	ui->loadingBtn->setHidden(true);
	m_loadingEvent.stopLoadingTimer();
}

void PLSLaboratoryInstallView::startInstall()
{
	if (m_installing) {
		LAB_LOG("Do not repeat downloads during installation");
		return;
	}
	LAB_LOG("user click install button,  start installing");
	showLoading();
	ui->installLabel->setText(tr("laboratory.item.Installing"));
	LabManage->downloadLabZipFile(m_plsCancel, this, m_labId, [this](bool succeeded, DownloadZipErrorType errorType) {
		hideLoading();
		LAB_LOG(QString("download and unzip finished result is %1 , error type is %2").arg(succeeded ? "success" : "failed").arg(static_cast<int>(errorType)));
		if (succeeded) {
			returnCode = g_installSuccess;
			done(g_installSuccess);
			return;
		}
		if (errorType == DownloadZipErrorType::DownloadZipNetworkError) {
			returnCode = g_installNetworkError;
			done(g_installNetworkError);
			return;
		}
		returnCode = g_installUnkownError;
		done(g_installUnkownError);
	});
}

bool PLSLaboratoryInstallView::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (i_Object == ui->confirmBackView) {
		if (i_Event->type() == QEvent::MouseButtonRelease) {
			ui->confirmBackView->setStyleSheet("background-color:#444444");
			startInstall();
		} else if (i_Event->type() == QEvent::Enter) {
			ui->confirmBackView->setStyleSheet("background-color:#666666");
		} else if (i_Event->type() == QEvent::Leave) {
			ui->confirmBackView->setStyleSheet("background-color:#444444");
		} else if (i_Event->type() == QEvent::MouseButtonPress) {
			ui->confirmBackView->setStyleSheet("background-color:#222222");
		}
	}
	return PLSDialogView::eventFilter(i_Object, i_Event);
}
