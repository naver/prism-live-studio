#include "PLSSceneInstallButton.h"
#include "ui_PLSSceneInstallButton.h"
#include "libui.h"
#include "obs-app.hpp"
#ifdef __APPLE__
#include "platform.hpp"
#endif

PLSSceneInstallButton::PLSSceneInstallButton(QWidget *parent) : QPushButton(parent), ui(new Ui::PLSSceneInstallButton)
{
	ui->setupUi(this);
	QSizePolicy sizePolicy = ui->loadingBtn_SceneTemplate->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->loadingBtn_SceneTemplate->setSizePolicy(sizePolicy);
	ui->loadingBtn_SceneTemplate->setHidden(true);
}

PLSSceneInstallButton::~PLSSceneInstallButton()
{
	delete ui;
}

void PLSSceneInstallButton::showLoading()
{
	m_installing = true;
	if (nullptr == m_pLoadingView) {
		m_pLoadingView =
			PLSLoadingView::newLoadingView(ui->loadingBtn_SceneTemplate, -1, nullptr, QString(":resource/images/loading/black-loading-%1.svg"), std::make_optional<QColor>(239, 252, 53));
		m_pLoadingView->setAttribute(Qt::WA_NoSystemBackground);
		m_pLoadingView->setAttribute(Qt::WA_OpaquePaintEvent);
	}
	ui->loadingBtn_SceneTemplate->setHidden(false);
}

void PLSSceneInstallButton::hideLoading()
{
	m_installing = false;
	ui->loadingBtn_SceneTemplate->setHidden(true);
	if (nullptr != m_pLoadingView) {
		PLSLoadingView::deleteLoadingView(m_pLoadingView);
	}
}

void PLSSceneInstallButton::startInstall()
{
	if (m_installing) {
		return;
	}
	showLoading();
	ui->installLabel->setText(tr("SceneTemplate.Installing.Button"));
}

void PLSSceneInstallButton::endInstall()
{
	hideLoading();
	ui->installLabel->setText(tr("SceneTemplate.Install.Button"));
	App()->getMainView()->activateWindow();
#ifdef __APPLE__
	bringWindowToTop(App()->getMainView());
#endif
}
