#include "PLSSceneTemplateContainer.h"
#include "ui_PLSSceneTemplateContainer.h"
#include "obs-app.hpp"
#include "PLSSceneTemplateMediaManage.h"

PLSSceneTemplateContainer::PLSSceneTemplateContainer(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent), ui(new Ui::PLSSceneTemplateContainer)
{
	setupUi(ui);
	pls_add_css(this, {"PLSSceneTemplateContainer"});
	setWindowTitle(tr("SceneTemplate.Title"));

#ifdef _WIN32
	setMinimumSize(900, 600);
	setMaximumSize(1145, 762);
	initSize(1145, 762);
#else
	setMinimumSize(900, 572);
	setMaximumSize(1145, 734);
	initSize(1145, 734);
#endif

	PLS_SCENE_TEMPLATE_MEDIA_MANAGE->setSceneTemplateContainer(this);
}

PLSSceneTemplateContainer::~PLSSceneTemplateContainer()
{
	delete ui;
}

void PLSSceneTemplateContainer::showMainSceneTemplatePage()
{
	ui->rightPage->hide();
	ui->leftPage->show();
}

void PLSSceneTemplateContainer::showDetailSceneTemplatePage(const SceneTemplateItem &model)
{
	ui->leftPage->hide();
	ui->rightPage->show();
	ui->rightPage->updateUI(model);
}

void PLSSceneTemplateContainer::showEvent(QShowEvent *event)
{
	PLSSideBarDialogView::showEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::SceneTemplateConfig, true);
	showMainSceneTemplatePage();
	ui->leftPage->showMainSceneView();
}

void PLSSceneTemplateContainer::hideEvent(QHideEvent *event)
{
	PLSSideBarDialogView::hideEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::SceneTemplateConfig, false);
}
