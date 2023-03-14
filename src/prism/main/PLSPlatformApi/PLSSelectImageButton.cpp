#include <Windows.h>

#include "PLSSelectImageButton.h"
#include "ui_PLSSelectImageButton.h"

#include <ctime>

#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>

#include "log.h"
#include "action.h"

#include "PLSCropImage.h"
#include "PLSTakeCameraSnapshot.h"
#include "ChannelCommonFunctions.h"
#include "pls-app.hpp"

#define MIN_PHOTO_WIDTH 440
#define MIN_PHOTO_HEIGHT 245

static QString setImageDir(const QString &imageDir)
{
	config_set_string(App()->GlobalConfig(), "SelectImageButton", "ImageDir", imageDir.toUtf8().constData());
	config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
	return imageDir;
}

static QString getImageDir()
{
	const char *dir = config_get_string(App()->GlobalConfig(), "SelectImageButton", "ImageDir");
	if (!dir || !dir[0]) {
		return setImageDir(QStandardPaths::standardLocations(QStandardPaths::PicturesLocation).first());
	}
	return QString::fromUtf8(dir);
}

static void initButton(QPushButton *button)
{
	QHBoxLayout *layout = new QHBoxLayout(button);
	layout->setMargin(0);
	layout->setSpacing(0);
	QLabel *icon = new QLabel(button);
	icon->setObjectName("icon");
	QLabel *text = new QLabel(button->text(), button);
	text->setObjectName("text");
	layout->addWidget(icon);
	layout->addWidget(text);
	button->setText(QString());
}

QString getTempImageFilePath(const QString &suffix)
{
	QDir temp = QDir::temp();
	temp.mkdir("cropedImages");
	temp.cd("cropedImages");
	QString tempImageFilePath = temp.absoluteFilePath(QString("cropedImage-%1").arg(std::time(nullptr)) + suffix);
	return tempImageFilePath;
}

PLSSelectImageButton::PLSSelectImageButton(QWidget *parent, PLSDpiHelper dpiHelper)
	: QLabel(parent), ui(new Ui::PLSSelectImageButton), imageSize(MIN_PHOTO_WIDTH, MIN_PHOTO_HEIGHT), imageChecker([](const QPixmap &, const QString &) -> QPair<bool, QString> {
		  return {true, QString()};
	  })
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSSelectImageButton});
	setAttribute(Qt::WA_Hover);
	setAttribute(Qt::WA_NativeWindow);

	ui->setupUi(this);
	initButton(ui->takeButton);
	initButton(ui->selectButton);

	icon = new QLabel(this);
	icon->setObjectName("icon");
	icon->lower();
	icon->installEventFilter(this);

	ui->takeButton->hide();
	ui->selectButton->hide();

	setScaledContents(true);
	setMouseTracking(true);
	ui->takeButton->setMouseTracking(true);
	ui->selectButton->setMouseTracking(true);

	ui->takeButton->installEventFilter(this);
	ui->selectButton->installEventFilter(this);
}

PLSSelectImageButton::~PLSSelectImageButton()
{
	delete ui;
}

QString PLSSelectImageButton::getLanguage() const
{
	return pls_get_current_language();
}

QString PLSSelectImageButton::getImagePath() const
{
	return imagePath;
}
void PLSSelectImageButton::setImagePath(const QString &imagePath)
{
	this->imagePath = imagePath;
	if (!imagePath.isEmpty()) {
		setPixmap(QPixmap(imagePath));
	} else {
		setPixmap(QPixmap());
		icon->show();
	}
}
void PLSSelectImageButton::setPixmap(const QPixmap &pixmap, const QSize &size)
{
	originPixmap = pixmap;
	if (pixmap.isNull()) {
		QLabel::setPixmap(pixmap);
		icon->show();
		return;
	}

	QPixmap scaled = pixmap.scaled(size.isValid() ? size : this->size());
	QPixmap image(scaled.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	double dpi = PLSDpiHelper::getDpi(this);

	QPainterPath path;
	path.addRoundedRect(image.rect(), PLSDpiHelper::calculate(dpi, 4), PLSDpiHelper::calculate(dpi, 4));
	painter.setClipPath(path);

	painter.drawPixmap(image.rect(), scaled);

	QLabel::setPixmap(image);
	icon->hide();
}

void PLSSelectImageButton::setEnabled(bool enabled)
{
	// QLabel::setEnabled(enabled);
	icon->setEnabled(enabled);
	pls_flush_style(icon);
}

void PLSSelectImageButton::setImageSize(const QSize &imageSize)
{
	this->imageSize = imageSize;
}

void PLSSelectImageButton::setImageChecker(const ImageChecker &imageChecker)
{
	this->imageChecker = imageChecker;
}

void PLSSelectImageButton::mouseEnter()
{
	if (!mouseHover) {
		mouseHover = true;
		ui->takeButton->setVisible(icon->isEnabled());
		ui->selectButton->setVisible(icon->isEnabled());
	}
}

void PLSSelectImageButton::mouseLeave()
{
	if (mouseHover) {
		mouseHover = false;
		ui->takeButton->hide();
		ui->selectButton->hide();
	}
}

void PLSSelectImageButton::on_takeButton_clicked()
{
	ui->takeButton->hide();
	ui->selectButton->hide();

	emit takeButtonClicked();

	setFocus();

	for (QString camera;;) {
		PLSTakeCameraSnapshot takeCameraSnapshot(camera, this);
		QString imageFilePath = takeCameraSnapshot.getSnapshot();
		if (imageFilePath.isEmpty()) {
			break;
		}

		QPixmap cropedImage;
		int button = PLSCropImage::cropImage(cropedImage, imageFilePath, imageSize, PLSCropImage::Back | PLSCropImage::Ok, this);
		if (button == PLSCropImage::Back) {
			continue;
		} else if (button != PLSCropImage::Ok || cropedImage.isNull()) {
			break;
		}

		QString cropedImageFile = getTempImageFilePath(".png");
		cropedImage.save(cropedImageFile, "PNG");

		if (auto result = imageChecker(cropedImage, cropedImageFile); !result.first) {
			PLSAlertView::warning(pls_get_toplevel_view(this), tr("Alert.Title"), result.second);
			return;
		}

		setPixmap(cropedImage);

		this->imagePath = cropedImageFile;
		emit imageSelected(cropedImageFile);
		break;
	}
}
void PLSSelectImageButton::on_selectButton_clicked()
{
	ui->takeButton->hide();
	ui->selectButton->hide();

	emit selectButtonClicked();

	setFocus();

	QString imageDir = getImageDir();
	QWidget *toplevelView = pls_get_toplevel_view(this);
	QString imageFilePath = QFileDialog::getOpenFileName(toplevelView, tr("Browse"), imageDir, "Image Files (*.jpg *.jpeg *.bmp *.png)");
	if (imageFilePath.isEmpty()) {
		return;
	}

	QFileInfo fileInfo(imageFilePath);
	setImageDir(fileInfo.dir().absolutePath());

	QPixmap originalImagge(imageFilePath);
	if (originalImagge.isNull()) {
		PLSAlertView::warning(toplevelView, tr("Alert.Title"), tr("SelectImage.Alert.Message.ErrorPhoto"));
		return;
	}

	if (originalImagge.width() < imageSize.width() || originalImagge.height() < imageSize.height()) {
		PLSAlertView::warning(toplevelView, tr("Alert.Title"), tr("SelectImage.Alert.Message.PhotoTooSmall").arg(imageSize.width()).arg(imageSize.height()));
		return;
	}

	QPixmap cropedImage;
	int button = PLSCropImage::cropImage(cropedImage, originalImagge, imageSize, PLSCropImage::Ok | PLSCropImage::Cancel, this);
	if (button != PLSCropImage::Ok || cropedImage.isNull()) {
		return;
	}

	QString cropedImageFile = getTempImageFilePath(".png");
	cropedImage.save(cropedImageFile, "PNG");

	if (auto result = imageChecker(cropedImage, cropedImageFile); !result.first) {
		PLSAlertView::warning(toplevelView, tr("Alert.Title"), result.second);
		return;
	}

	setPixmap(cropedImage);

	this->imagePath = cropedImageFile;
	emit imageSelected(cropedImageFile);
}

bool PLSSelectImageButton::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:
		mouseEnter();
		break;
	case QEvent::HoverLeave:
		mouseLeave();
		break;
	case QEvent::Resize: {
		QResizeEvent *resizeEvent = dynamic_cast<QResizeEvent *>(event);
		QSize size = resizeEvent->size() - icon->size();
		icon->move(size.width() / 2, size.height() / 2);
		if (!imagePath.isEmpty()) {
			setPixmap(QPixmap(imagePath), resizeEvent->size());
		}
		break;
	}
	}

	return QLabel::event(event);
}
bool PLSSelectImageButton::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == icon) {
		if (event->type() == QEvent::Resize) {
			QSize size = this->size() - icon->size();
			icon->move(size.width() / 2, size.height() / 2);
		}
	}
	return QLabel::eventFilter(watched, event);
}

bool PLSSelectImageButton::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
	MSG *msg = (MSG *)message;
	if (msg->message == WM_MOUSEMOVE) {
		mouseEnter();
	}

	return QLabel::nativeEvent(eventType, message, result);
}
