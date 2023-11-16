#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#include "PLSSelectImageButton.h"
#include "ui_PLSSelectImageButton.h"

#include <ctime>

#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <qevent.h>
#include <util/config-file.h>
#include "log/log.h"
#include "action.h"

#include "PLSCropImage.h"
#include "PLSTakeCameraSnapshot.h"
#include "ChannelCommonFunctions.h"
#include "PLSMessageBox.h"
#include "utils-api.h"
#include "obs-app.hpp"

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
	QHBoxLayout *layout = pls_new<QHBoxLayout>(button);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	QLabel *icon = pls_new<QLabel>(button);
	icon->setObjectName("icon");
	icon->setAttribute(Qt::WA_TransparentForMouseEvents);
	QLabel *text = pls_new<QLabel>(button->text(), button);
	text->setObjectName("text");
	text->setAttribute(Qt::WA_TransparentForMouseEvents);
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

PLSSelectImageButton::PLSSelectImageButton(QWidget *parent) : QLabel(parent)
{
	ui = pls_new<Ui::PLSSelectImageButton>();

	pls_add_css(this, {"PLSSelectImageButton"});
	setAttribute(Qt::WA_Hover);
#if defined(Q_OS_WIN)
	setAttribute(Qt::WA_NativeWindow);
#endif

	setAttribute(Qt::WA_TranslucentBackground);

	ui->setupUi(this);
	initButton(ui->takeButton);
	initButton(ui->selectButton);

	icon = pls_new<QLabel>(ui->imageLabel);
	icon->setObjectName("icon");
	icon->lower();
	icon->installEventFilter(this);

	setScaledContents(true);
	setMouseTracking(true);
	ui->takeButton->setMouseTracking(true);
	ui->selectButton->setMouseTracking(true);

	ui->maskBgWidget->setHidden(true);
	setRemoveRetainSizeWhenHidden(ui->maskBgWidget);
	setRemoveRetainSizeWhenHidden(ui->imageLabel);
}

PLSSelectImageButton::~PLSSelectImageButton()
{
	pls_delete(ui, nullptr);
}

QString PLSSelectImageButton::getLanguage() const
{
	return pls_get_current_language();
}

QString PLSSelectImageButton::getImagePath() const
{
	return imagePath;
}
void PLSSelectImageButton::setImagePath(const QString &imagePath_)
{
	imagePath = imagePath_;
	if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
		setPixmap(QPixmap(imagePath));
	} else {
		setPixmap(QPixmap());
		icon->show();
	}
}
void PLSSelectImageButton::setPixmap(const QPixmap &pixmap, const QSize &size)
{
	originPixmap = pixmap;
	QSize showSize = size.isValid() ? size : this->size();
	QSize imgSize = pixmap.size();
	if (pixmap.isNull() || imgSize.isEmpty() || showSize.isEmpty()) {
		ui->imageLabel->setPixmap(pixmap);
		icon->show();
		return;
	}

	QPixmap croped;
	if ((double(imgSize.width()) / double(imgSize.height())) > (double(showSize.width()) / double(showSize.height()))) {
		QPixmap scaled = pixmap.scaledToHeight(showSize.height());
		croped = scaled.copy((scaled.width() - showSize.width()) / 2, 0, showSize.width(), showSize.height());
	} else {
		QPixmap scaled = pixmap.scaledToWidth(showSize.width());
		croped = scaled.copy(0, (scaled.height() - showSize.height()) / 2, showSize.width(), showSize.height());
	}

	QPixmap image(croped.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QPainterPath path;
	path.addRoundedRect(image.rect(), 4, 4);
	painter.setClipPath(path);

	painter.drawPixmap(image.rect(), croped);

	ui->imageLabel->setPixmap(image);
	icon->hide();
}

void PLSSelectImageButton::setButtonEnabled(bool enabled)
{
	icon->setEnabled(enabled);
	pls_flush_style(icon);
}

void PLSSelectImageButton::setImageSize(const QSize &imageSize_)
{
	imageSize = imageSize_;
}

void PLSSelectImageButton::setImageChecker(const ImageChecker &imageChecker_)
{
	imageChecker = imageChecker_;
}

void PLSSelectImageButton::mouseEnter()
{
	if (!mouseHover) {
		mouseHover = true;
		ui->maskBgWidget->setVisible(icon->isEnabled());
		ui->imageLabel->setVisible(!ui->maskBgWidget->isVisible());
	}
}

void PLSSelectImageButton::mouseLeave()
{
	if (mouseHover) {
		mouseHover = false;
		ui->maskBgWidget->hide();
		ui->imageLabel->setVisible(!ui->maskBgWidget->isVisible());
	}
}

void PLSSelectImageButton::on_takeButton_clicked()
{
	ui->maskBgWidget->hide();

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
			PLSMessageBox::warning(pls_get_toplevel_view(this), tr("Alert.Title"), result.second);
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
	ui->maskBgWidget->hide();

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
		pls_alert_error_message(toplevelView, tr("Alert.Title"), tr("SelectImage.Alert.Message.ErrorPhoto"));
		return;
	}

	if (!m_isIgoreMinSize && (originalImagge.width() < imageSize.width() || originalImagge.height() < imageSize.height())) {
		pls_alert_error_message(toplevelView, tr("Alert.Title"), tr("SelectImage.Alert.Message.PhotoTooSmall").arg(imageSize.width()).arg(imageSize.height()));
		return;
	}

	if ((originalImagge.width() * originalImagge.height()) > (MAX_PHOTO_WIDTH * MAX_PHOTO_HEIGHT)) {
		pls_alert_error_message(toplevelView, tr("Alert.Title"), tr("SelectImage.Alert.Message.PhotoTooLarge"));
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
	case QEvent::Resize:
		moveIconToCenter(static_cast<QResizeEvent *>(event)->size());
		if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
			setPixmap(QPixmap(imagePath), static_cast<QResizeEvent *>(event)->size());
		} else if (!originPixmap.isNull()) {
			setPixmap(originPixmap, static_cast<QResizeEvent *>(event)->size());
		}
		break;
	default:
		break;
	}

	return QLabel::event(event);
}
bool PLSSelectImageButton::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == icon) {
		switch (event->type()) {
		case QEvent::Resize:
			moveIconToCenter(this->size());
			break;
		default:
			break;
		}
	}
	return QLabel::eventFilter(watched, event);
}

bool PLSSelectImageButton::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#if defined(Q_OS_WIN)
	auto msg = (const MSG *)message;
	if (msg->message == WM_MOUSEMOVE) {
		mouseEnter();
	}
#endif
	return QLabel::nativeEvent(eventType, message, result);
}

QPair<bool, QString> PLSSelectImageButton::defaultImageChecker(const QPixmap &, const QString &)
{
	return {true, QString()};
}

void PLSSelectImageButton::moveIconToCenter(const QSize &containerSize)
{
	QSize size = containerSize - icon->size();
	icon->move(size.width() / 2, size.height() / 2);
}

void PLSSelectImageButton::setRemoveRetainSizeWhenHidden(QWidget *widget) const
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}
