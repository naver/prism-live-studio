#include "update-view.hpp"
#include "ui_PLSUpdateView.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#include <QTimer>
#include "main-view.hpp"

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

PLSUpdateView::PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper),
	  ui(new Ui::PLSUpdateView),
	  m_manualUpdate(manualUpdate),
	  m_isForceUpdate(isForceUpdate),
	  m_version(version),
	  m_fileUrl(fileUrl),
	  m_updateInfoUrl(updateInfoUrl)
{
	App()->DisableHotkeys();
	initUI();
	dpiHelper.setCss(this, {PLSCssIndex::PLSUpdateView});
	dpiHelper.setFixedSize(this, {720, 486});
}

PLSUpdateView::~PLSUpdateView()
{
	App()->UpdateHotkeyFocusSetting();
	timer->stop();
	delete ui;
}

void PLSUpdateView::initUI()
{
	//setup view frame and content
	ui->setupUi(this->content());

	m_pressed = false;

	if (!cef) {
		return;
	}

	//delay the cef widge
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(onTimeOut()));
	timer->start(100);

	//setup controls title
	ui->updateTopDescription->setText(tr("update.toptip.advise.text"));
	ui->nextUpdateBtn->setText(tr("update.bottom.next.button.text"));
	ui->nowUpdateBtn->setText(tr("update.bottom.force.button.text"));
	if (m_isForceUpdate) {
		ui->horizontalBottomLayout->removeWidget(ui->nextUpdateBtn);
		ui->nextUpdateBtn->setVisible(false);
		ui->updateTopDescription->setText(tr("update.toptip.force.text"));
		ui->nowUpdateBtn->setText(tr("Confirm"));
	}

	//setup signal and slots
	initConnect();
}

void PLSUpdateView::initConnect()
{
	connect(ui->nextUpdateBtn, SIGNAL(clicked()), this, SLOT(on_nextUpdateBtn_clicked()));
	connect(ui->nowUpdateBtn, SIGNAL(clicked()), this, SLOT(on_nowUpdateBtn_clicked()));
	PLSMainView *mainView = dynamic_cast<PLSMainView *>(parentWidget());
	if (mainView) {
		connect(mainView, &PLSMainView::isshowSignal, this, &PLSUpdateView::isShowMainView);
	}
}

void PLSUpdateView::on_nextUpdateBtn_clicked()
{
	PLS_UI_STEP(UPDATE_MODULE, " PLSUpdateView NextUpdate Button", ACTION_CLICK);
	this->reject();
}

void PLSUpdateView::on_nowUpdateBtn_clicked()
{
	PLS_UI_STEP(UPDATE_MODULE, " PLSUpdateView NowUpdate Button", ACTION_CLICK);
	this->accept();
}

void PLSUpdateView::isShowMainView(bool isShow)
{
	setHidden(!isShow);
}

void PLSUpdateView::onTimeOut()
{
	PLSBasic::InitBrowserPanelSafeBlock();
	if (cef) {
		m_cefWidget = cef->create_widget(nullptr, m_updateInfoUrl.toUtf8().constData(), "", panel_cookies);
		ui->middleWidget->addWidget(m_cefWidget);
		ui->middleWidget->setCurrentWidget(m_cefWidget);
	}
	timer->stop();
}

void PLSUpdateView::showEvent(QShowEvent *event)
{
	this->setHidden(parentWidget()->isHidden());
	PLSDialogView::showEvent(event);
}

bool PLSUpdateView::isInCustomControl(QWidget *child) const
{
	if (!child || child == this) {
		return false;
	} else if (child == ui->middleWidget) {
		return true;
	} else if (child == m_cefWidget) {
		return true;
	} else {
		return isInCustomControl(child->parentWidget());
	}
}
