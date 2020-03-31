#include "ComplexHeaderIcon.h"
#include "ChannelCommonFunctions.h"
#include <QPaintEvent>
#include <qDebug>
ComplexHeaderIcon::ComplexHeaderIcon(QWidget *parent) : QLabel(parent) {}

ComplexHeaderIcon::~ComplexHeaderIcon() {}

void ComplexHeaderIcon::setPixmap(const QString &pix, bool sharp)
{

	mBigPix.load(pix);
	if (sharp) {
		circleMaskImage(mBigPix);
	}
}

void ComplexHeaderIcon::setPlatformPixmap(const QString &pix, bool sharp)
{
	mSmallPix.load(pix);
	if (sharp) {
		circleMaskImage(mSmallPix);
	}
}

void ComplexHeaderIcon::setActive(bool isActive)
{
	mActive = isActive;
	refreshStyle(this);
}

void ComplexHeaderIcon::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setBrush(Qt::transparent);
	painter.setRenderHint(QPainter::Antialiasing);
	auto rec = this->contentsRect();

	if (!mBigPix.isNull()) {
		painter.save();
		auto tmp = mBigPix.scaled(rec.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter.drawPixmap(rec, tmp);
		if (!this->isEnabled()) {
			QBrush brush(QColor(10, 10, 10, 50));
			painter.setBrush(brush);
			painter.drawEllipse(rec);
		}
		painter.restore();
	}
	if (!mSmallPix.isNull()) {
		auto picSize = mSmallPix.size();
		QRect imagerect(rec.bottomRight() - QPoint(picSize.width() - 6, picSize.height()), picSize);
		auto tmp = mSmallPix.scaled(imagerect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter.drawPixmap(imagerect, tmp);
		if (!this->isEnabled()) {
			QBrush brush(QColor(10, 10, 10, 50));
			painter.setBrush(brush);
			painter.drawEllipse(imagerect);
		}
	}
	QLabel::paintEvent(event);
}
