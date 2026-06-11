
#include "PLSUpdateView.hpp"
#include "ui_PLSUpdateView.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#endif
#include "PLSBasic.h"

PLSUpdateView::PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent)
	: PLSDialogView(parent), m_manualUpdate(manualUpdate), m_isForceUpdate(isForceUpdate), m_version(version), m_fileUrl(fileUrl), m_updateInfoUrl(updateInfoUrl)
{
	ui = pls_new<Ui::PLSUpdateView>();
	initUI();
}

PLSUpdateView::~PLSUpdateView()
{
	pls_delete(ui);
}

void PLSUpdateView::updateBrowserUrl(const QString &url) const
{
	m_browserWidget->url(url);
}

void PLSUpdateView::initUI()
{
	//setup view frame and content
	setupUi(ui);
	pls_add_css(this, {"PLSUpdateView"});
	setMoveInContent(true);
	m_pressed = false;

	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url(m_updateInfoUrl)
								 .initBkgColor(QColor(17, 17, 17))
								 .css("html, body { background-color: #111111; }")
								 .showAtLoadEnded(true));

	if (!m_updateInfoUrl.isEmpty()) {
		ui->verticalLayout_2->addWidget(m_browserWidget);
	}

	//setup controls title
	ui->updateTopDescription->setText(tr("Update.Toptip.Advise.Text"));
	ui->nextUpdateBtn->setText(tr("Update.Bottom.Next.Button.Text"));
	ui->nowUpdateBtn->setText(tr("Update.Bottom.Force.Button.Text"));
	if (m_isForceUpdate) {
		ui->horizontalBottomLayout->removeWidget(ui->nextUpdateBtn);
		ui->nextUpdateBtn->setVisible(false);
		ui->updateTopDescription->setText(tr("Update.Toptip.Force.Text"));
		ui->nowUpdateBtn->setText(tr("Confirm"));
	}

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);

	//setup signal and slots
	initConnect();

#if defined(Q_OS_MACOS)
	customMacWindow()->setCornerRadius(true);
	setWindowTitle(tr("Mac.Title.Update"));
	setProperty("type", "Mac");
#elif defined(Q_OS_WIN)
	setProperty("type", "Win");
#endif
	connect(PLSBasic::instance(), &PLSBasic::sigUpdateUrlChanged, this, &PLSUpdateView::updateBrowserUrl);
}

void PLSUpdateView::initConnect() const
{
	const PLSMainView *mainView = dynamic_cast<PLSMainView *>(parentWidget());
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

void PLSUpdateView::showEvent(QShowEvent *event)
{
	this->setHidden(parentWidget()->isHidden());
	PLSDialogView::showEvent(event);
}

void PLSUpdateView::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_WIN
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		event->ignore();
	} else {
		m_browserWidget->closeBrowser();
		PLSDialogView::closeEvent(event);
	}
#else
	m_browserWidget->closeBrowser();
	PLSDialogView::closeEvent(event);
#endif
}
