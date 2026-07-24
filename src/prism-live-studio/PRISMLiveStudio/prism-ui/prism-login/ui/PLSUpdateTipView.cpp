#include "PLSUpdateTipView.hpp"
#include "ui_PLSUpdateTipView.h"
#include "libutils-api.h"
#include "liblog.h"
#include "action.h"
#include "../PLSCommonConst.h"
#include "PLSLoginMainView.h"
#include "../PLSLoginDataHandler.h"
#include "../PLSCommonFunc.h"
#include <QDir>
#include "pls-common-define.hpp"
#include "obs-app.hpp"

PLSUpdateTipView::PLSUpdateTipView(QWidget *parent) : PLSDialogView(parent)
{
	pls_add_css(this, {"PLSUpdateTipView"});
	ui = pls_new<Ui::PLSUpdateTipView>();
	setupUi(ui);
	setResizeEnabled(false);
	auto closeEvent = [this](QCloseEvent *e) -> bool {
		if (m_browserWidget) {
			m_browserWidget->closeBrowser();
		}
		if (e) {
			reject();
		}
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSUpdateTipView::~PLSUpdateTipView()
{
	pls_delete(ui);
}

void PLSUpdateTipView::updateView(bool isForceUpdate, const QString &updateInfoUrl, const QString &version)
{
	//setup view frame and content
	m_version = version;

	//setup controls title
	m_isForceUpdate = isForceUpdate;
	m_updateInfoUrl = updateInfoUrl;

	//delay the cef widge
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
	setWindowTitle(tr("Mac.Title.Update"));

	ui->updateTopDescription->setText(tr("Update.Toptip.Advise.Text"));
	ui->nextUpdateBtn->setText(tr("Update.Bottom.Next.Button.Text"));
	ui->nowUpdateBtn->setText(tr("Update.Bottom.Force.Button.Text"));
	if (m_isForceUpdate) {
		ui->nextUpdateBtn->setText(tr("Update.Bottom.ExitApp.Button.Text"));
		ui->updateTopDescription->setText(tr("Update.Toptip.Force.Text"));
		ui->nowUpdateBtn->setText(tr("update.bottom.single.force.button.text"));
	}
}

bool PLSUpdateTipView::needPopupUpdateView(const QString &version) const
{
	QVariantMap map = PLSLoginFunc::getUpdateInfo(QStringList(common::UPDATE_NEXT_VERSION_INFO));
	QString savedVersion = map.value(common::UPDATE_NEXT_VERSION_INFO).toString();
	if (!savedVersion.isEmpty() && savedVersion == version) {
		return false;
	}
	return true;
}

void PLSUpdateTipView::on_nextUpdateBtn_clicked()
{
	if (m_isForceUpdate) {
		PLS_UI_STEP(UPDATE_MODULE, " PLSUpdateTipView ExitApp Button", ACTION_CLICK);
	} else {
		PLS_UI_STEP(UPDATE_MODULE, " PLSUpdateTipView NextUpdate Button", ACTION_CLICK);
		PLSLoginFunc::saveUpdateInfo({{common::UPDATE_NEXT_VERSION_INFO, m_version}});
		PLSLoginMainView::instance()->changeView(pls_window_type::PLS_LOGIN_VIEW);
		accept();
		return;
	}
	reject();
}

void PLSUpdateTipView::on_nowUpdateBtn_clicked()
{
	PLS_UI_STEP(UPDATE_MODULE, " PLSUpdateTipView NowUpdate Button", ACTION_CLICK);
	//update logic
	PLSLoginMainView::instance()->changeView(pls_window_type::PLS_UPDATING_VIEW);
	PLSLoginMainView::instance()->startdownloadNewInstallPackage(PLSLoginDataHandler::instance()->getInstallFileUrl(), pls_get_gcc());
	accept();
}
