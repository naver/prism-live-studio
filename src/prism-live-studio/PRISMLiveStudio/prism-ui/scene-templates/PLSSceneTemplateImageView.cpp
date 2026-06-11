#include "PLSSceneTemplateImageView.h"
#include "ui_PLSSceneTemplateImageView.h"
#include "libui.h"

extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);

PLSSceneTemplateImageView::PLSSceneTemplateImageView(QWidget *parent) : QWidget(parent), ui(new Ui::PLSSceneTemplateImageView)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_StyledBackground);
	ui->imageLabel->installEventFilter(this);
}

PLSSceneTemplateImageView::~PLSSceneTemplateImageView()
{
	delete ui;
}

void PLSSceneTemplateImageView::updateImagePath(const QString &path)
{
	if (path == m_path) {
		return;
	}
	m_path = path;

	loadPixmap(imagePix, m_path, QSize());
	loadImagePixel();
}

void PLSSceneTemplateImageView::setHasBorder(bool hasBorder)
{
	ui->imageLabel->setHasBorder(hasBorder);
}

const QString &PLSSceneTemplateImageView::imagePath() const
{
	return m_path;
}

void PLSSceneTemplateImageView::setSceneName(const QString &sceneName)
{
	ui->imageLabel->setSceneNameLabel(sceneName);
}

bool PLSSceneTemplateImageView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->imageLabel && event->type() == QEvent::Resize) {
		loadImagePixel();
	} else if (watched == ui->imageLabel && event->type() == QEvent::MouseButtonRelease) {
		emit clicked(this);
	}
	return QWidget::eventFilter(watched, event);
}

void PLSSceneTemplateImageView::loadImagePixel()
{
	if (!imagePix.isNull()) {
		ui->imageLabel->setPixmap(imagePix.scaled(ui->imageLabel->width(), ui->imageLabel->height(), Qt::KeepAspectRatio));
	}
}

void PLSSceneTemplateImageView::showAIBadge(const QPixmap &pixmap, bool bLongAIBadge)
{
	ui->imageLabel->showAIBadge(pixmap, bLongAIBadge);
}
