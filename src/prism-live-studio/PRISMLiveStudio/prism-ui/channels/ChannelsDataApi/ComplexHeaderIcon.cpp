#include "ComplexHeaderIcon.h"
#include <QImageReader>
#include <QPaintEvent>
#include <QPainterPath>
#include "ChannelCommonFunctions.h"
#include "libui.h"

static void sharpPix(QPixmap &pix, bool isBigPix, bool sharp)
{
	if (!sharp || pix.isNull()) {
		return;
	}

	if (isBigPix && pix.width() != pix.height()) {
		pix = pls_shared_get_cube_pix(pix);
	}

	pls_shared_circle_mask_image(pix);
}

ComplexHeaderIcon::ComplexHeaderIcon(QWidget *parent) : QLabel(parent)
{
	mPaintObj.mDefaultIconPath = ChannelData::g_defaultHeaderIcon;
	mPaintObj.mPixSize = QSize(30, 30);
	mPaintObj.mPlatPixSize = QSize(16, 16);

	mDelayTimer = new QTimer(this);
	mDelayTimer->setSingleShot(true);
	mDelayTimer->setInterval(100);
	connect(mDelayTimer, &QTimer::timeout, this, &ComplexHeaderIcon::delayDraw, Qt::QueuedConnection);
	connect(this, &ComplexHeaderIcon::paintingFinished, this, QOverload<>::of(&ComplexHeaderIcon::update), Qt::QueuedConnection);
	m_ActiveImage = pls_load_pixmap(":/channels/resource/images/ChannelsSource/badge-liveon.svg", QSize(38, 38) * 4);
	mDelayTimer->start();
	this->setFocusPolicy(Qt::NoFocus);
}

ComplexHeaderIcon::~ComplexHeaderIcon()
{
	m_destroying = true;
	while (m_using > 0) {
		if (pls_get_app_exiting()) {
			break;
		}
	}
	delete mDelayTimer;
	mDelayTimer = nullptr;
}

void ComplexHeaderIcon::updateSmallPix(PaintObject &painterObj)
{
	auto newSize = painterObj.mPlatPixSize * 4;
	painterObj.mSmallPix = PLSCHANNELS_API->updateImage(painterObj.mPlatPixPath, newSize);
	if (!painterObj.mSmallPix.isNull() && painterObj.mSmallPix.size() != painterObj.mPlatPixSize * painterObj.dpi) {
		painterObj.mSmallPix = painterObj.mSmallPix.scaled(painterObj.mPlatPixSize * painterObj.dpi, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		painterObj.mSmallPix.setDevicePixelRatio(painterObj.dpi);
	}
	sharpPix(painterObj.mSmallPix, false, painterObj.mPlatPixSharp);
}

void ComplexHeaderIcon::delayDraw()
{
	if (m_destroying)
		return;
	++m_using;
	const auto scaleFactor = this->window()->devicePixelRatioF();
	QPointer<ComplexHeaderIcon> widPtr = this;
	mPaintObj.isEnabled = this->isEnabled();
	mPaintObj.mContentRect = QRect(QPoint(0, 0), this->contentsRect().size() * scaleFactor);
	mPaintObj.mRect = QRect(QPoint(0, 0), this->rect().size() * scaleFactor);
	mPaintObj.width = this->width() * scaleFactor;
	mPaintObj.height = this->height() * scaleFactor;
	mPaintObj.dpi = scaleFactor;

	auto fun = [this]() {
		if (m_destroying || m_using > 1) {
			--m_using;
			return;
		}
		auto tmpObj = mPaintObj;
		updateBixPix(tmpObj);
		updateSmallPix(tmpObj);
		tmpObj.mViewPixmap = QPixmap(tmpObj.width, tmpObj.height);
		tmpObj.mViewPixmap.fill(Qt::transparent);
		drawOnDevice(tmpObj);
		mPaintObj = tmpObj;
		emit paintingFinished();
		--m_using;
	};

	PLSCHANNELS_API->runTaskInOtherThread(fun);
}

void ComplexHeaderIcon::updateBixPix(PaintObject &painterObj)
{
	auto newSize = painterObj.mPixSize * 4;
	painterObj.mBigPix = PLSCHANNELS_API->updateImage(painterObj.mPixPath, newSize);
	if (painterObj.mBigPix.isNull()) {
		loadPixmap(painterObj.mBigPix, painterObj.mDefaultIconPath, newSize);
	}
	if (!painterObj.mBigPix.isNull() && painterObj.mBigPix.size() != newSize) {
		painterObj.mBigPix = painterObj.mBigPix.scaled(newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		painterObj.mBigPix.setDevicePixelRatio(painterObj.dpi);
	}
	sharpPix(painterObj.mBigPix, true, painterObj.mPixSharp);
}

void ComplexHeaderIcon::setMainPixmap(const QString &pix, const QSize &pixSize, bool sharp, double)
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
	auto rec = rect.adjusted(painterObj.mPadding * painterObj.dpi, painterObj.mPadding * painterObj.dpi, -painterObj.mPadding * painterObj.dpi, -painterObj.mPadding * painterObj.dpi);
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
		QRect imagerect(rec.bottomRight() - QPoint(picSize.width() - 6 * static_cast<int>(painterObj.dpi), picSize.height()), picSize);
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
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setBrush(Qt::transparent);

	if (isActive()) {
		QSize scaledSize = QSize(38, 38) * devicePixelRatio();
		QPixmap pix = m_ActiveImage.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		pix.setDevicePixelRatio(devicePixelRatio());
		painter.drawPixmap(QRect(2, 2, 38, 38), pix);
	}
	painter.drawPixmap(this->rect(), mPaintObj.mViewPixmap);
}
