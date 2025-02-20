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
	setMinimumSize(902, 602);
	setMaximumSize(1147, 764);
	initSize(1145, 762);
#else
	setMinimumSize(902, 574);
	setMaximumSize(1147, 736);
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

QPixmap &PLSSceneTemplateContainer::getAIBadge()
{
	if (m_pixmapAIBadge.isNull()) {
		m_pixmapAIBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai.svg", QSize(27 * 4, 27 * 4));
	}

	return m_pixmapAIBadge;
}

QPixmap &PLSSceneTemplateContainer::getAILongBadge()
{
	if (m_pixmapAILongBadge.isNull()) {
		auto lang = pls_get_current_language_short_str();
		if ("ko" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_ko.svg", QSize(104 * 4, 32 * 4));
		} else if ("id" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_id.svg", QSize(175 * 4, 32 * 4));
		} else if ("vi" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_vi.svg", QSize(130 * 4, 32 * 4));
		} else if ("pt" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_pt.svg", QSize(152 * 4, 32 * 4));
		} else if ("es" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_es.svg", QSize(165 * 4, 32 * 4));
		} else if ("ja" == lang) {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_ja.svg", QSize(99 * 4, 32 * 4));
		} else {
			m_pixmapAILongBadge = pls_load_pixmap("://resource/images/scene-template/ic_scene_badge_ai_en.svg", QSize(143 * 4, 32 * 4));
		}
	}

	return m_pixmapAILongBadge;
}
