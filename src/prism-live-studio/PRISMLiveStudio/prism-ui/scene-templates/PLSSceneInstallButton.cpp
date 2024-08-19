#include "PLSSceneInstallButton.h"
#include "ui_PLSSceneInstallButton.h"
#include "libui.h"
#include "obs-app.hpp"

PLSSceneInstallButton::PLSSceneInstallButton(QWidget *parent) :
    QPushButton(parent),
    ui(new Ui::PLSSceneInstallButton)
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
	m_loadingEvent.startLoadingTimer(ui->loadingBtn_SceneTemplate);
    ui->loadingBtn_SceneTemplate->setHidden(false);
}

void PLSSceneInstallButton::hideLoading()
{
    m_installing = false;
	ui->loadingBtn_SceneTemplate->setHidden(true);
    m_loadingEvent.stopLoadingTimer();
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
}
