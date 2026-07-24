
#include "PLSUpdateView.hpp"
#include "ui_PLSUpdateView.h"
#include "window-basic-main.hpp"
#include "log/log.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#endif
#include "PLSBasic.h"
#include "pls-performance.h"
#include <QDialog>

bool PLSUpdateView::s_userChoseExitApp = false;

void PLSUpdateView::resetUserChoseExitApp()
{
	s_userChoseExitApp = false;
}

bool PLSUpdateView::takeUserChoseExitApp()
{
	const bool v = s_userChoseExitApp;
	s_userChoseExitApp = false;
	return v;
}

void PLSUpdateView::noteForceUpdateDialogResult(bool is_force_update, int exec_result)
{
	if (is_force_update && exec_result != QDialog::Accepted) {
		s_userChoseExitApp = true;
	}
}

PLSUpdateView::PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent)
	: PLSDialogView(parent), m_manualUpdate(manualUpdate), m_isForceUpdate(isForceUpdate), m_version(version), m_fileUrl(fileUrl), m_updateInfoUrl(updateInfoUrl)
{
	PLS_PERFORMANCE_FUNCTION();
	ui = pls_new<Ui::PLSUpdateView>();
	PLS_PERFORMANCE_START(PLSUpdateView_initUI);
	initUI();
	PLS_PERFORMANCE_END(PLSUpdateView_initUI);
}

PLSUpdateView::~PLSUpdateView()
{
	pls_delete(ui);
}

void PLSUpdateView::updateBrowserUrl(const QString &url) const
{
	if (m_browserWidget) {
		m_browserWidget->url(url);
	}
}

void PLSUpdateView::initUI()
{
	//setup view frame and content
	setResizeEnabled(false);
	setupUi(ui);
	pls_add_css(this, {"PLSUpdateView"});
	m_pressed = false;
	setResizeEnabled(false);
	if (!m_updateInfoUrl.isEmpty()) {
		m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
									 .url(m_updateInfoUrl)
									 .allowPopups(false)
									 .initBkgColor(QColor(17, 17, 17))
									 .css("html, body { background-color: #111111; }")
									 .showAtLoadEnded(true));
		ui->verticalLayout_2->addWidget(m_browserWidget);
	}

#if defined(Q_OS_WIN)
	ui->verticalLayout->removeWidget(ui->updateTop);
	setTitleWidget(ui->updateTop);
#endif

	//setup controls title
	ui->updateTopDescription->setText(tr("Update.Toptip.Advise.Text"));
	ui->nextUpdateBtn->setText(tr("Update.Bottom.Next.Button.Text"));
	ui->nowUpdateBtn->setText(tr("Update.Bottom.Force.Button.Text"));
	if (m_isForceUpdate) {
		ui->nextUpdateBtn->setText(tr("Update.Bottom.ExitApp.Button.Text"));
		ui->updateTopDescription->setText(tr("Update.Toptip.Force.Text"));
		ui->nowUpdateBtn->setText(tr("Confirm"));
	}

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		if (m_browserWidget) {
			m_browserWidget->closeBrowser();
		}
		return true;
	};
	setCloseEventCallback(closeEvent);

	//setup signal and slots
	initConnect();
#if defined(Q_OS_MACOS)
	customMacWindow()->setCloseButtonHidden(true);
	customMacWindow()->setCornerRadius(true);
	setWindowTitle(tr("Mac.Title.Update"));
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
	if (m_isForceUpdate) {
		s_userChoseExitApp = true;
	}
	reject();
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
	PLS_PERFORMANCE_GLOBAL_END("ShowUpdateView");
	PLS_PERFORMANCE_GLOBAL_END("updateLogoUpdateView");
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
