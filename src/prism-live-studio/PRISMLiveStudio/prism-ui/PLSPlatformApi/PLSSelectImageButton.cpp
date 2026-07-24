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
#include "PLSPlatformBase.hpp"
#include "PLSErrorHandler.h"

static QString setImageDir(const QString &imageDir)
{
	config_set_string(App()->GetUserConfig(), "SelectImageButton", "ImageDir", imageDir.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	return imageDir;
}

static QString getImageDir()
{
	const char *dir = config_get_string(App()->GetUserConfig(), "SelectImageButton", "ImageDir");
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
	PLS_PERFORMANCE_GLOBAL_START("PLSSelectImageButton::constructor", "PLSLiveInfoNaverShoppingLIVE::setupUi");
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
	initButton(ui->deleteButton);
	pls_flush_style(ui->selectButton, "isBottom", true);

	QVBoxLayout *layout = pls_new<QVBoxLayout>(ui->imageLabel);
	layout->setSpacing(0);
	icon = pls_new<QLabel>();
	icon->setObjectName("icon");
	icon->lower();
	icon->installEventFilter(this);
	layout->addWidget(icon, 0, Qt::AlignHCenter | Qt::AlignVCenter);

	m_tipLabel = pls_new<QLabel>();
	m_tipLabel->setObjectName("tipLabel");
	m_tipLabel->lower();
	m_tipLabel->setText("");
	layout->addWidget(m_tipLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
	m_tipLabel->hide();

	setScaledContents(true);
	setMouseTracking(true);
	ui->takeButton->setMouseTracking(true);
	ui->selectButton->setMouseTracking(true);
	ui->deleteButton->setMouseTracking(true);

	setMaskBgWidgetVisible(false);
	setRemoveRetainSizeWhenHidden(ui->maskBgWidget);
	setRemoveRetainSizeWhenHidden(ui->imageLabel);
	PLS_PERFORMANCE_GLOBAL_END("PLSSelectImageButton::constructor");
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
		m_tipLabel->setVisible(icon->isVisible() && m_isShowTipLabel);
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
		m_tipLabel->setVisible(icon->isVisible() && m_isShowTipLabel);
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
	painter.setOpacity(property("ignoreHover").toBool() ? 0.5 : 1);

	QPainterPath path;
	path.addRoundedRect(image.rect(), 4, 4);
	painter.setClipPath(path);

	painter.drawPixmap(image.rect(), croped);

	ui->imageLabel->setPixmap(image);
	icon->hide();
	m_tipLabel->setVisible(icon->isVisible() && m_isShowTipLabel);
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

void PLSSelectImageButton::mouseEnter()
{
	if (property("ignoreHover").toBool()) {
		return;
	}
	if (!mouseHover) {
		mouseHover = true;
		setMaskBgWidgetVisible(icon->isEnabled());
		ui->imageLabel->setVisible(!ui->maskBgWidget->isVisible());
		m_tipLabel->setVisible(icon->isVisible() && m_isShowTipLabel);
	}
}

void PLSSelectImageButton::mouseLeave()
{
	if (mouseHover) {
		mouseHover = false;
		setMaskBgWidgetVisible(false);
		ui->imageLabel->setVisible(!ui->maskBgWidget->isVisible());
		m_tipLabel->setVisible(icon->isVisible() && m_isShowTipLabel);
	}
}

void PLSSelectImageButton::on_takeButton_clicked()
{
	mouseLeave();
	setMaskBgWidgetVisible(false);
	emit takeButtonClicked();

	setFocus();

	QString camera;
	PLSTakeCameraSnapshot takeCameraSnapshot(camera, this);
	takeCameraSnapshot.setAttribute(Qt::WA_DeleteOnClose, false);

	for (;;) {
		QString imageFilePath = takeCameraSnapshot.getSnapshot(PLSTakeCameraSnapshot::Hide);

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

		QString cropedImageFile;
		dealCropedImage(cropedImage, cropedImageFile);

		setPixmap(cropedImage);
		this->imagePath = cropedImageFile;
		emit imageSelected(cropedImageFile);
		break;
	}
}
void PLSSelectImageButton::on_selectButton_clicked()
{
	mouseLeave();
	setMaskBgWidgetVisible(false);
	emit selectButtonClicked();

	setFocus();

	QString imageDir = getImageDir();
	QWidget *toplevelView = pls_get_toplevel_view(this);
	pls::HotKeyLocker locker;
	QString imageFilePath = QFileDialog::getOpenFileName(toplevelView, tr("Browse"), imageDir, "Image Files (*.jpg *.jpeg *.bmp *.png)");
	if (imageFilePath.isEmpty()) {
		return;
	}

	QFileInfo fileInfo(imageFilePath);
	setImageDir(fileInfo.dir().absolutePath());

	QPixmap originalImagge(imageFilePath);
	if (originalImagge.isNull()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_TAKE_PHOTO_NULL, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSSelectImageButton::on_selectButton_clicked"),
						      toplevelView);
		return;
	}

	if (!m_isIgoreMinSize && (originalImagge.width() < imageSize.width() || originalImagge.height() < imageSize.height())) {
		PLSErrorHandler::ExtraData extraData("PLSSelectImageButton::on_selectButton_clicked");
		extraData.defaultArg = {QString::number(imageSize.width()), QString::number(imageSize.height())};
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SELECT_IMAGE_PHOTO_TOO_SMALL, PLSErrKeyAllAlert, {}, extraData, toplevelView);
		return;
	}

	if ((originalImagge.width() * originalImagge.height()) > (MAX_PHOTO_WIDTH * MAX_PHOTO_HEIGHT)) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SELECT_IMAGE_PHOTO_TOO_LARGE, PLSErrKeyAllAlert, {},
						      PLSErrorHandler::ExtraData("PLSSelectImageButton::on_selectButton_clicked"), toplevelView);
		return;
	}
	QPixmap cropedImage;
	int button = PLSCropImage::cropImage(cropedImage, originalImagge, imageSize, PLSCropImage::Ok | PLSCropImage::Cancel, this);
	if (button != PLSCropImage::Ok || cropedImage.isNull()) {
		return;
	}

	QString cropedImageFile;
	dealCropedImage(cropedImage, cropedImageFile);

	setPixmap(cropedImage);

	this->imagePath = cropedImageFile;
	emit imageSelected(cropedImageFile);
}

void PLSSelectImageButton::dealCropedImage(QPixmap &cropedImage, QString &cropedImageFile)
{

	if (m_jpgMaxKB == -1) {
		cropedImageFile = getTempImageFilePath(".jpg");
		cropedImage.save(cropedImageFile, "JPG");
		PLS_INFO(MODULE_PlatformService, "select image use default scale size.");
		return;
	}
	for (int quality = 100; quality >= 0; quality -= 10) {
		cropedImageFile = getTempImageFilePath(".jpg");
		cropedImage.save(cropedImageFile, "JPG", quality);
		if (QFile(cropedImageFile).size() <= (m_jpgMaxKB * 1024)) {
			cropedImage = QPixmap(cropedImageFile);
			PLS_INFO(MODULE_PlatformService, "select image use scale size: %i.", quality);
			break;
		}
	}
}

void PLSSelectImageButton::on_deleteButton_clicked()
{
	mouseLeave();
	emit deleteButtonClicked();
	setFocus();
	setImagePath("");
	PLS_UI_ACTION("Widget PLSSelectImageButton Clear Done");
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

void PLSSelectImageButton::setRemoveRetainSizeWhenHidden(QWidget *widget) const
{
	QSizePolicy policy = widget->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	widget->setSizePolicy(policy);
}

void PLSSelectImageButton::setTipLabelString(const QString &tipLabel)
{
	m_isShowTipLabel = !tipLabel.isEmpty();
	if (m_tipLabel) {
		m_tipLabel->setText(tipLabel);
		auto labelLayout = ui->imageLabel->layout();
		auto tempMargins = labelLayout->contentsMargins();
		tempMargins.setTop(25);
		labelLayout->setContentsMargins(tempMargins);

		labelLayout->setAlignment(icon, Qt::AlignHCenter | (m_isShowTipLabel ? Qt::AlignBottom : Qt::AlignVCenter));
		m_tipLabel->setVisible(m_isShowTipLabel);
	}
}

void PLSSelectImageButton::setMaskBgWidgetVisible(bool isVisible)
{
	if (isVisible) {
		bool deleteButtonShow = m_isShowDeleteBtn && isVisible && !imagePath.isEmpty();
		ui->deleteButton->setVisible(m_isShowDeleteBtn && isVisible && !imagePath.isEmpty());
		pls_flush_style(ui->selectButton, "isBottom", !deleteButtonShow);
	}
	ui->maskBgWidget->setVisible(isVisible);
}
