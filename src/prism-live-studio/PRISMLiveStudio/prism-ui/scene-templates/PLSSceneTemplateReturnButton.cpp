#include "PLSSceneTemplateReturnButton.h"
#include "ui_PLSSceneTemplateReturnButton.h"
#include "pls-common-define.hpp"
#include "libui.h"

using namespace common;

PLSSceneTemplateReturnButton::PLSSceneTemplateReturnButton(QWidget *parent) : QFrame(parent),
    ui(new Ui::PLSSceneTemplateReturnButton)
{
    ui->setupUi(this);
	ui->returnLabel->setText(tr("SceneTemplate.Scene.Return.Button"));
}

PLSSceneTemplateReturnButton::~PLSSceneTemplateReturnButton()
{
    delete ui;
}

void PLSSceneTemplateReturnButton::enterEvent(QEnterEvent *event)
{
    this->setProperty(STATUS, STATUS_HOVER);
    pls_flush_style_recursive(this);
    QFrame::enterEvent(event);
}

void PLSSceneTemplateReturnButton::leaveEvent(QEvent *event)
{
    this->setProperty(STATUS, STATUS_NORMAL);
    pls_flush_style_recursive(this);
    QFrame::leaveEvent(event);
}

void PLSSceneTemplateReturnButton::mousePressEvent(QMouseEvent *event)
{
    this->setProperty(STATUS, STATUS_CLICKED);
    pls_flush_style_recursive(this);
    QFrame::mousePressEvent(event);
}

void PLSSceneTemplateReturnButton::mouseReleaseEvent(QMouseEvent *event)
{
    this->setProperty(STATUS, STATUS_NORMAL);
    pls_flush_style_recursive(this);
    emit clicked();
    QFrame::mouseReleaseEvent(event);
}