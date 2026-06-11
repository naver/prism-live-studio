#include "PLSSceneTemplateBorderLabel.h"
#include "ui_PLSSceneTemplateBorderLabel.h"
#include "libui.h"
#include <QStyleOption>
#include <QPainter>

PLSSceneTemplateBorderLabel::PLSSceneTemplateBorderLabel(QWidget *parent) : QLabel(parent), ui(new Ui::PLSSceneTemplateBorderLabel)
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
		setStyleSheet("#imageLabel { border: 2px solid #EFFC35; }");
	} else {
		setStyleSheet("#imageLabel { border: none; }");
	}
}

void PLSSceneTemplateBorderLabel::setSceneNameLabel(const QString &sceneName)
{
	ui->sceneNameRoundLabel->setVisible(!sceneName.isEmpty());
	ui->sceneNameRoundLabel->setText(sceneName);
}

void PLSSceneTemplateBorderLabel::showAIBadge(const QPixmap &pixmap, bool bLongAIBadge)
{
	if (pixmap.isNull()) {
		ui->labelAIBadge->hide();
	} else {
		if (ui->labelAIBadge->pixmap().isNull()) {
			ui->labelAIBadge->setPixmap(pixmap);
			ui->labelAIBadge->setStyleSheet(
				QString("background:transparent; min-width:%1px;max-width:%1px; min-height:%2px;max-height:%2px").arg(pixmap.width() / 4).arg(pixmap.height() / 4));
		}
		ui->labelAIBadge->show();
	}
	if (bLongAIBadge) {
		ui->horizontalLayout->setContentsMargins(20, 23, 20, 20);
	} else {
		ui->horizontalLayout->setContentsMargins(6, 6, 20, 20);
	}
}
