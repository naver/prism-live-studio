#include "PLSCropImage.h"
#include "ui_PLSCropImage.h"

#include "ChannelCommonFunctions.h"
#include "utils-api.h"

static const double SCALE_FACTOR = 1000.0;

template<typename WidgetPtr> static void setTranslucent(WidgetPtr w)
{
	w->setFrameShape(QFrame::NoFrame);
	w->setStyleSheet("background-color: rgba(0, 0, 0, 0.5)");
	w->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

template<typename... Rests> static void setTranslucent(Rests... rests)
{
	(setTranslucent(rests), ...);
}

static int leastEqual(int value, int least)
{
	return (value >= least) ? value : least;
}
static int mostEqual(int value, int most)
{
	return (value <= most) ? value : most;
}
static QRect calcImageRect(const QSize &previewSize, const QSize &imageSize, const QRect &middleRect)
{
	if (previewSize.isEmpty() || imageSize.isEmpty() || middleRect.isEmpty()) {
		return QRect();
	}

	if (imageSize.width() * middleRect.height() < imageSize.height() * middleRect.width()) {
		int displayHeight = leastEqual(qRound(imageSize.height() * double(middleRect.width()) / double(imageSize.width())), middleRect.height());
		return QRect(middleRect.x(), mostEqual((previewSize.height() - displayHeight) / 2, middleRect.y()), middleRect.width(), displayHeight);
	} else {
		int displayWidth = leastEqual(qRound(imageSize.width() * double(middleRect.height()) / double(imageSize.height())), middleRect.width());
		return QRect(mostEqual((previewSize.width() - displayWidth) / 2, middleRect.x()), middleRect.y(), displayWidth, middleRect.height());
	}
}
static void initImageSize(QLabel *image, QSlider *slider, const QRect &imageRect, double &minScaleFactor, double maxScaleFactor, double originalImageWidth)
{
	image->setGeometry(imageRect);

	minScaleFactor = imageRect.width() / originalImageWidth;

	slider->blockSignals(true);
	slider->setRange(int(minScaleFactor * SCALE_FACTOR), int(maxScaleFactor * SCALE_FACTOR));
	slider->blockSignals(false);

	slider->setValue(int(minScaleFactor * SCALE_FACTOR));
}

PLSCropImage::PLSCropImage(const QString &imageFilePath, const QSize &cropImageSize, QWidget *parent) : PLSCropImage(QPixmap(imageFilePath), cropImageSize, parent) {}

PLSCropImage::PLSCropImage(const QPixmap &originalImage_, const QSize &cropImageSize_, QWidget *parent) : PLSDialogView(parent), cropImageSize(cropImageSize_), originalImage(originalImage_)
{
	ui = pls_new<Ui::PLSCropImage>();

	pls_add_css(this, {"PLSCropImage"});
	setResizeEnabled(false);

	setupUi(ui);
	initSize({510, 478});

	image = pls_new<QLabel>(ui->preview);
	image->setFrameShape(QFrame::NoFrame);
	image->setScaledContents(true);
	image->setPixmap(originalImage);
	image->move(0, 0);

	image->setMouseTracking(true);
	image->installEventFilter(this);

	QFrame *left = pls_new<QFrame>(ui->preview);
	QFrame *top = pls_new<QFrame>(ui->preview);
	QFrame *right = pls_new<QFrame>(ui->preview);
	QFrame *bottom = pls_new<QFrame>(ui->preview);
	left->setObjectName("left");
	top->setObjectName("top");
	right->setObjectName("right");
	bottom->setObjectName("bottom");
	setTranslucent(left, top, right, bottom);

	middle = pls_new<QFrame>(ui->preview);
	middle->setObjectName("middle");
	middle->setFixedSize(cropImageSize);
	middle->setStyleSheet("QFrame { border: /*hdpi*/ 1px solid #effc35; background-color: rgba(216, 216, 216, 0); margin: 0; padding: 0; }");
	middle->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	middle->installEventFilter(this);

	QHBoxLayout *h = pls_new<QHBoxLayout>(ui->preview);
	h->setContentsMargins(0, 0, 0, 0);
	h->setSpacing(0);
	QVBoxLayout *v = pls_new<QVBoxLayout>();
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);
	v->addWidget(top, 1);
	v->addWidget(middle);
	v->addWidget(bottom, 1);
	h->addWidget(left, 1);
	h->addLayout(v);
	h->addWidget(right, 1);
}

PLSCropImage::~PLSCropImage()
{
	pls_delete(ui, nullptr);
}

void PLSCropImage::setButtons(int buttons)
{
	ui->backButton->setVisible(buttons & Button::Back);
	ui->okButton->setVisible(buttons & Button::Ok);
	ui->cancelButton->setVisible(buttons & Button::Cancel);
}

int PLSCropImage::cropImage(QPixmap &cropedImage, const QString &imageFilePath, const QSize &cropImageSize, int buttons, QWidget *parent)
{
	PLSCropImage cropImage(imageFilePath, cropImageSize, parent);
	cropImage.setButtons(buttons);
	int button = cropImage.exec();
	if (button == Button::Ok) {
		cropedImage = cropImage.cropedImage;
	}
	return button;
}

int PLSCropImage::cropImage(QPixmap &cropedImage, const QPixmap &originalImage, const QSize &cropImageSize, int buttons, QWidget *parent)
{
	PLSCropImage cropImage(originalImage, cropImageSize, parent);
	cropImage.setButtons(buttons);
	int button = cropImage.exec();
	if (button == Button::Ok) {
		cropedImage = cropImage.cropedImage;
	}
	return button;
}

void PLSCropImage::on_slider_valueChanged(int value)
{
	QRect oldImageRect = image->geometry();
	QRect middleRect = middle->geometry();
	if (oldImageRect.isEmpty() || middleRect.isEmpty()) {
		return;
	}
	QPoint oldCenter = image->mapFromParent(middleRect.center());
	double scaleFactor = value / SCALE_FACTOR;
	QSize imageSize = originalImage.size() * scaleFactor;
	imageSize.setWidth(leastEqual(imageSize.width(), middleRect.width()));
	imageSize.setHeight(leastEqual(imageSize.height(), middleRect.height()));
	QRect imageRect(oldImageRect.topLeft() + oldCenter * (1 - imageSize.width() / double(oldImageRect.width())), imageSize);
	if (imageRect.contains(middleRect)) {
		image->setGeometry(imageRect);
		return;
	}

	if (imageRect.left() > middleRect.left()) {
		imageRect.moveLeft(middleRect.left());
	}
	if (imageRect.top() > middleRect.top()) {
		imageRect.moveTop(middleRect.top());
	}
	if (imageRect.right() < middleRect.right()) {
		imageRect.moveRight(middleRect.right());
	}
	if (imageRect.bottom() < middleRect.bottom()) {
		imageRect.moveBottom(middleRect.bottom());
	}
	image->setGeometry(imageRect);
}

void PLSCropImage::on_smallButton_clicked()
{
	ui->slider->setValue(int(minScaleFactor * SCALE_FACTOR));
}
void PLSCropImage::on_largeButton_clicked()
{
	ui->slider->setValue(int(maxScaleFactor * SCALE_FACTOR));
}
void PLSCropImage::on_backButton_clicked()
{
	done(Button::Back);
}
void PLSCropImage::on_okButton_clicked()
{
	QRect middleRect = middle->geometry();
	QRect grabRect(image->mapFromParent(middleRect.topLeft()), middleRect.size());
	cropedImage = image->grab(grabRect);
	done(Button::Ok);
}
void PLSCropImage::on_cancelButton_clicked()
{
	done(Button::Cancel);
}

bool PLSCropImage::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == middle) {
		eventFilter_middile(event);
	} else if (watched == image) {
		eventFilter_image(event);
	}
	return PLSDialogView::eventFilter(watched, event);
}

void PLSCropImage::eventFilter_middile(QEvent *event)
{
	switch (event->type()) {
	case QEvent::Resize:
		if (QRect imageRect = calcImageRect(ui->preview->size(), originalImage.size(), QRect(middle->pos(), dynamic_cast<QResizeEvent *>(event)->size())); !imageRect.isEmpty()) {
			initImageSize(image, ui->slider, imageRect, minScaleFactor, maxScaleFactor, originalImage.width());
		}
		break;
	case QEvent::Move:
		if (QRect imageRect = calcImageRect(ui->preview->size(), originalImage.size(), QRect(dynamic_cast<QMoveEvent *>(event)->pos(), middle->size())); !imageRect.isEmpty()) {
			initImageSize(image, ui->slider, imageRect, minScaleFactor, maxScaleFactor, originalImage.width());
		}
		break;
	default:
		break;
	}
}

void PLSCropImage::eventFilter_image(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonPress:
		dragingImage = true;
		dragStartPos = dynamic_cast<QMouseEvent *>(event)->globalPos();
		setCursor(Qt::PointingHandCursor);
		break;
	case QEvent::MouseButtonRelease:
		dragingImage = false;
		setCursor(Qt::ArrowCursor);
		break;
	case QEvent::MouseMove:
		if (QPoint globalPos = dynamic_cast<QMouseEvent *>(event)->globalPos(); dragingImage && globalPos != dragStartPos) {
			if (QPoint pos = image->pos() + (globalPos - dragStartPos); QRect(pos, image->size()).contains(middle->geometry())) {
				image->move(pos);
			}
			dragStartPos = globalPos;
		}
		break;
	default:
		break;
	}
}

#if defined(Q_OS_MACOS)
QList<QWidget *> PLSCropImage::moveContentExcludeWidgetList()
{
	QList<QWidget *> excludeChildList;
	excludeChildList.append(ui->preview);
	return excludeChildList;
}

#endif
