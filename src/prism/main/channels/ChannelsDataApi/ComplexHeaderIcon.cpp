#include "ComplexHeaderIcon.h"
#include <QImageReader>
#include <QPaintEvent>
#include <qDebug>
#include "ChannelCommonFunctions.h"
#include "PLSDpiHelper.h"

static void sharpPix(QPixmap &pix, bool isBigPix, bool sharp)
{
	if (!sharp || pix.isNull()) {
		return;
	}

	if (isBigPix && pix.width() != pix.height()) {
		pix = getCubePix(pix);
	}

	QSize pixSize = pix.size();

	QPixmap image(pixSize);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QPainterPath path;
	path.addRoundedRect(image.rect(), pixSize.width() / 2.0, pixSize.height() / 2.0);
	painter.setClipPath(path);

	painter.drawPixmap(image.rect(), pix);

	pix = image;
}

ComplexHeaderIcon::ComplexHeaderIcon(QWidget *parent) : QLabel(parent), mDelayTimer(nullptr)
{
	mPaintObj.mDefaultIconPath = ChannelData::g_defaultHeaderIcon;
	mPaintObj.mPixSize = QSize(34, 34);
	mPaintObj.mPlatPixSize = QSize(18, 18);

	mDelayTimer = new QTimer(this);
	mDelayTimer->setSingleShot(true);
	mDelayTimer->setInterval(100);
	connect(mDelayTimer, &QTimer::timeout, this, &ComplexHeaderIcon::delayDraw, Qt::QueuedConnection);
	connect(this, &ComplexHeaderIcon::paintingFinished, this, QOverload<>::of(&ComplexHeaderIcon::update), Qt::QueuedConnection);

	PLSDpiHelper dpiHelper;
	dpiHelper.notifyDpiChanged(this, [this](double dpi) {
		if (mDelayTimer) {
			mDelayTimer->start();
		}
	});
}

ComplexHeaderIcon::~ComplexHeaderIcon()
{
	delete mDelayTimer;
	mDelayTimer = nullptr;
}

void ComplexHeaderIcon::updateSmallPix(double dpi, PaintObject &painterObj)
{
	auto newSize = PLSDpiHelper::calculate(dpi, painterObj.mPlatPixSize);
	painterObj.mSmallPix = PLSCHANNELS_API->updateImage(painterObj.mPlatPixPath, newSize);
	if (!painterObj.mSmallPix.isNull() && painterObj.mSmallPix.size() != newSize) {
		painterObj.mSmallPix = painterObj.mSmallPix.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	sharpPix(painterObj.mSmallPix, false, painterObj.mPlatPixSharp);
}

void ComplexHeaderIcon::delayDraw()
{
	QPointer<ComplexHeaderIcon> widPtr = this;
	mPaintObj.isEnabled = this->isEnabled();
	mPaintObj.mContentRect = this->contentsRect();
	mPaintObj.mRect = this->rect();
	mPaintObj.width = this->width();
	mPaintObj.height = this->height();
	mPaintObj.dpi = PLSDpiHelper::getDpi(this);

	auto fun = [widPtr]() {
		double dpi = PLSDpiHelper::getDpi(widPtr);
		if (widPtr == nullptr) {
			return;
		}
		auto tmpObj = widPtr->mPaintObj;
		updateBixPix(dpi, tmpObj);
		updateSmallPix(dpi, tmpObj);
		tmpObj.mViewPixmap = QPixmap(tmpObj.width, tmpObj.height);
		tmpObj.mViewPixmap.fill(Qt::transparent);
		drawOnDevice(tmpObj);
		if (widPtr == nullptr) {
			return;
		}
		widPtr->mPaintObj = tmpObj;
		if (widPtr == nullptr) {
			return;
		}
		emit widPtr->paintingFinished();
	};
	QMetaObject::invokeMethod(PLSCHANNELS_API, fun, Qt::QueuedConnection);
}

void ComplexHeaderIcon::updateBixPix(double dpi, PaintObject &painterObj)
{
	auto newSize = PLSDpiHelper::calculate(dpi, painterObj.mPixSize);
	painterObj.mBigPix = PLSCHANNELS_API->updateImage(painterObj.mPixPath, newSize);
	if (painterObj.mBigPix.isNull()) {
		loadPixmap(painterObj.mBigPix, painterObj.mDefaultIconPath, newSize);
	}
	if (!painterObj.mBigPix.isNull() && painterObj.mBigPix.size() != newSize) {
		painterObj.mBigPix = painterObj.mBigPix.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	sharpPix(painterObj.mBigPix, true, painterObj.mPixSharp);
}

void ComplexHeaderIcon::setPixmap(const QString &pix, const QSize &pixSize, bool sharp, double showDpiValue)
{
	mPaintObj.mPixPath = pix;
	mPaintObj.mPixSize = pixSize;
	mPaintObj.mPixSharp = sharp;

	mDelayTimer->start();
}

void ComplexHeaderIcon::setDefaultIconPath(const QString &pix)
{
	mPaintObj.mDefaultIconPath = pix;
}

void ComplexHeaderIcon::setPlatformPixmap(const QString &pix, const QSize &pixSize, bool sharp)
{
	mPaintObj.mPlatPixPath = pix;
	mPaintObj.mPlatPixSize = pixSize;
	mPaintObj.mPlatPixSharp = sharp;

	mDelayTimer->start();
}

void ComplexHeaderIcon::setActive(bool isActive)
{
	mPaintObj.mActive = isActive;
	refreshStyle(this);
}

void ComplexHeaderIcon::setUseContentsRect(bool isUseContentsRect)
{
	mPaintObj.mUseContentsRect = isUseContentsRect;
	update();
}

void ComplexHeaderIcon::setPadding(int padding)
{
	mPaintObj.mPadding = padding;
	update();
}

void ComplexHeaderIcon::drawOnDevice(PaintObject &painterObj)
{
	QPainter painter(&painterObj.mViewPixmap);
	painter.setBrush(Qt::transparent);
	painter.setPen(Qt::transparent);
	painter.setRenderHint(QPainter::Antialiasing);
	double dpi = painterObj.dpi;
	auto rect = painterObj.mUseContentsRect ? painterObj.mContentRect : painterObj.mRect;
	auto rec = rect.adjusted(PLSDpiHelper::calculate(dpi, painterObj.mPadding), PLSDpiHelper::calculate(dpi, painterObj.mPadding), PLSDpiHelper::calculate(dpi, -painterObj.mPadding),
				 PLSDpiHelper::calculate(dpi, -painterObj.mPadding));
	if (!painterObj.mBigPix.isNull()) {
		painter.save();
		auto tmp = painterObj.mBigPix.scaled(rec.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter.drawPixmap(rec, tmp);
		if (!painterObj.isEnabled) {
			QBrush brush(QColor(10, 10, 10, 50));
			painter.setBrush(brush);
			painter.drawEllipse(rec);
		}
		painter.restore();
	}
	if (!painterObj.mSmallPix.isNull()) {
		auto picSize = painterObj.mSmallPix.size();
		QRect imagerect(rec.bottomRight() - QPoint(picSize.width() - PLSDpiHelper::calculate(dpi, 6), picSize.height()), picSize);
		auto tmp = painterObj.mSmallPix.scaled(imagerect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter.drawPixmap(imagerect, tmp);
		if (!painterObj.isEnabled) {
			QBrush brush(QColor(10, 10, 10, 50));
			painter.setBrush(brush);
			painter.drawEllipse(imagerect);
		}
	}
}
void ComplexHeaderIcon::paintEvent(QPaintEvent *)
{
	if (mPaintObj.mViewPixmap.isNull()) {
		return;
	}
	QPainter painter(this);
	painter.setBrush(Qt::transparent);
	painter.drawPixmap(this->rect(), mPaintObj.mViewPixmap);
}
