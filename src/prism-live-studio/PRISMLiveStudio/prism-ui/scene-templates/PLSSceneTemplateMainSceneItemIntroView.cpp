#include "PLSSceneTemplateMainSceneItemIntroView.h"
#include "ui_PLSSceneTemplateMainSceneItemIntroView.h"
#include "libui.h"
#include "obs-app.hpp"
#include <QTimer>

#define NAME_MAX_WIDTH 196

PLSSceneTemplateMainSceneItemIntroView::PLSSceneTemplateMainSceneItemIntroView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PLSSceneTemplateMainSceneItemIntroView)
{
    ui->setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
	ui->nameLabel->installEventFilter(this);
}

PLSSceneTemplateMainSceneItemIntroView::~PLSSceneTemplateMainSceneItemIntroView()
{
    delete ui;
}

void PLSSceneTemplateMainSceneItemIntroView::updateUI(const SceneTemplateItem &model)
{
    m_item = model;
	QString text = ui->nameLabel->fontMetrics().elidedText(m_item.title(), Qt::ElideRight, NAME_MAX_WIDTH);
    ui->nameLabel->setText(text);
	QString resolution = QString("%1 x %2").arg(model.width()).arg(model.height());
    ui->resolutionLabel->setText(resolution);
    QString sceneCount = QTStr("SceneTemplate.Scene.Count").arg(model.scenesNumber());
    ui->sceneCountLabel->setText(sceneCount);
}

bool PLSSceneTemplateMainSceneItemIntroView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->nameLabel && event->type() == QEvent::Resize) {
		if (!m_item.title().isEmpty()) {
		    QString text = ui->nameLabel->fontMetrics().elidedText(m_item.title(), Qt::ElideRight, NAME_MAX_WIDTH);
		    ui->nameLabel->setText(text);   
        }
    }
    return QWidget::eventFilter(watched, event);
}
