#include "PLSSceneTemplateBorderLabel.h"
#include "ui_PLSSceneTemplateBorderLabel.h"
#include "libui.h"
#include <QStyleOption>
#include <QPainter>

PLSSceneTemplateBorderLabel::PLSSceneTemplateBorderLabel(QWidget *parent) :
    QLabel(parent),
    ui(new Ui::PLSSceneTemplateBorderLabel)
{
    ui->setupUi(this);
    ui->sceneNameRoundLabel->setVisible(false);
}

PLSSceneTemplateBorderLabel::~PLSSceneTemplateBorderLabel()
{
    delete ui;
}

void PLSSceneTemplateBorderLabel::setHasBorder(bool hasBorder) 
{
    if (hasBorder) {
	    ui->borderFrame->setStyleSheet("border: 2px solid #EFFC35;");
    } else {
	    ui->borderFrame->setStyleSheet("border: none;");
    }
}

void PLSSceneTemplateBorderLabel::setSceneNameLabel(const QString &sceneName) {
	ui->sceneNameRoundLabel->setVisible(!sceneName.isEmpty());
    ui->sceneNameRoundLabel->setText(sceneName);
}