#include "PLSSceneTemplateImageView.h"
#include "ui_PLSSceneTemplateImageView.h"
#include "libui.h"

#include <QPainter>
#include <QPainterPath>
#include <QImage>

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
	if (path == m_path && !imagePix.isNull()) {
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
		PLS_UI_ACTION("In Scene Template Window, the scene template image thumbnail has been clicked.");
		emit clicked(this);
	}
	return QWidget::eventFilter(watched, event);
}

void PLSSceneTemplateImageView::loadImagePixel()
{
	auto scaleMode = property("keepAspectRatioByExpanding").toBool() ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
	auto pixWidth = ui->imageLabel->width();
	auto pixHeight = ui->imageLabel->height();

	if (scaleMode == Qt::KeepAspectRatioByExpanding) {
		ui->imageLabel->setScaledContents(true);
		if (!imagePix.isNull()) {
			ui->imageLabel->setPixmap(imagePix);
		} else {
			if (m_defaultImage.isNull()) {
				m_defaultImage = pls_load_pixmap(QString(":resource/images/scene-template/BG.png"), QSize(960, 540));
			}
			ui->imageLabel->setPixmap(m_defaultImage);
		}
	} else {
		ui->imageLabel->setScaledContents(false);

		// Use QImage for better scaling quality on high DPI displays (Mac Retina, Windows high DPI, etc.)
		auto scalePixmap = [this, pixWidth, pixHeight, scaleMode](const QPixmap &sourcePix) -> QPixmap {
			if (sourcePix.isNull() || sourcePix.width() == 0 || sourcePix.height() == 0) {
				return QPixmap();
			}

			// Calculate aspect ratio and adjust size
			auto ratio = double(sourcePix.height()) / sourcePix.width();
			int finalWidth = pixWidth;
			int finalHeight = pixHeight;
			auto necessaryHeight = pixWidth * ratio;
			if (necessaryHeight <= pixHeight) {
				finalHeight = necessaryHeight;
			} else {
				finalWidth = pixHeight / ratio;
			}

			// Get device pixel ratio for high DPI displays (Mac Retina, Windows high DPI scaling, etc.)
			// On normal displays, dpr is 1.0; on high DPI displays, it can be 1.25, 1.5, 2.0, etc.
			qreal dpr = ui->imageLabel->devicePixelRatio();
			QSize scaledSize(finalWidth * dpr, finalHeight * dpr);

			// Convert to QImage for better scaling quality (QImage scaling is more accurate than QPixmap)
			QImage sourceImage = sourcePix.toImage();
			QImage scaledImage = sourceImage.scaled(scaledSize, scaleMode, Qt::SmoothTransformation);

			// Convert back to QPixmap and set device pixel ratio for proper rendering
			QPixmap result = QPixmap::fromImage(std::move(scaledImage));
			result.setDevicePixelRatio(dpr);
			return result;
		};

		if (!imagePix.isNull()) {
			ui->imageLabel->setPixmap(scalePixmap(imagePix));
		} else {
			if (m_defaultImage.isNull()) {
				m_defaultImage = pls_load_pixmap(QString(":resource/images/scene-template/BG.png"), QSize(960, 540));
			}
			ui->imageLabel->setPixmap(scalePixmap(m_defaultImage));
		}
	}
}

void PLSSceneTemplateImageView::showAIBadge(const QPixmap &pixmap, bool bLongAIBadge)
{
	ui->imageLabel->showAIBadge(pixmap, bLongAIBadge);
}

void PLSSceneTemplateImageView::showPlusBadge(const QPixmap &pixmap)
{
	ui->imageLabel->showPlusBadge(pixmap);
}
