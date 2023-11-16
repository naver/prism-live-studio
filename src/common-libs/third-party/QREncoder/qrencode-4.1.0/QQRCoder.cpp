#include "QQRCoder.h"
#include <QApplication>
#include <QImage>

namespace QRCoderSpace {
const QString QRFont = "font";
const QString QROption = "option";
const QString QRWrapTopMargin = "margin-top";

const QString QRMaxHeightStr = "QRMaxHeight";
const QString QRMaxWidthStr = "QRMaxWidthStr";
const QString QRTipMaxheightStr = "QRTipMaxheight";

}

QQRCoder::QQRCoder()
{
	mContext = "prism";
	mBackground = Qt::white;
	mForeground = Qt::black;
	mMargin = 20;
	mIsCasesensitive = true;
	mMode = MODE_8;
	mLevel = LEVEL_L;
	mPercent = 0.23f;
	mQRVersion = 4;
}

void QQRCoder::setCenterImage(const QString &path, qreal percent)
{
	mLogo.load(path);
	this->mPercent = percent < 0.5 ? percent : 0.3;
}

void QQRCoder::setCenterImage(const QImage &srcLogo, qreal percent)
{
	mLogo = srcLogo;
	this->mPercent = percent < 0.5 ? percent : 0.3;
}

QImage QQRCoder::paintImage(const QSize &outPutSize)
{
	QImage image(outPutSize, QImage::Format_RGBA8888);
	QPainter painter(&image);
	QRcode *qrcode = QRcode_encodeString(mContext.data(), mQRVersion, (QRecLevel)mLevel, (QRencodeMode)mMode, mIsCasesensitive ? 1 : 0);
	if (qrcode == nullptr) {
		return image;
	}
	// draw qrimage
	unsigned char *point = qrcode->data;
	painter.setRenderHints(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(this->mBackground);
	painter.drawRect(0, 0, outPutSize.width(), outPutSize.height());
	double scale = (outPutSize.width() - 2.0 * mMargin) / qrcode->width;
	painter.setBrush(mForeground);
	for (int y = 0; y < qrcode->width; y++) {
		for (int x = 0; x < qrcode->width; x++) {
			if (*point & 1) {
				QRectF r(mMargin + x * scale, mMargin + y * scale, scale, scale);
				painter.drawRects(&r, 1);
			}
			point++;
		}
	}
	point = NULL;
	QRcode_free(qrcode);

	if (mLogo.isNull()) {
		return image;
	}
	// draw logo
	painter.setBrush(this->mBackground);
	double icon_width = (outPutSize.width() - 2.0 * mMargin) * mPercent;
	double icon_height = icon_width;
	double wrap_x = (outPutSize.width() - icon_width) / 2.0;
	double wrap_y = (outPutSize.width() - icon_height) / 2.0;
	QRectF wrap(wrap_x - 5, wrap_y - 5, icon_width + 10, icon_height + 10);
	painter.drawRoundedRect(wrap, 50, 50);
	QRectF target(wrap_x, wrap_y, icon_width, icon_height);
	QRectF source(0, 0, mLogo.width(), mLogo.height());
	painter.drawImage(target, mLogo, source);
	return image;
}

QImage QQRCoder::paintTextImage(const QByteArray &text, const QSize &outPutSize)
{
	QImage image(outPutSize, QImage::Format_RGBA8888);
	QPainter painter(&image);
	painter.drawText(image.rect(), Qt::AlignCenter | Qt::TextWordWrap, text);
	return image;
}

QImage QQRCoder::wrapImageWithTips(const QImage &src, const QString &tips, const QVariantMap &drawInfo)
{
	const QFont font = drawInfo.value(QRCoderSpace::QRFont).value<QFont>();
	const QTextOption option = drawInfo.value(QRCoderSpace::QROption).value<QTextOption>();
	const int marginTop = drawInfo.value(QRCoderSpace::QRWrapTopMargin).toInt();
	auto srcRect = src.rect();
	QFontMetrics metrics(font);

	auto width = drawInfo.value(QRCoderSpace::QRMaxWidthStr).toInt();
	auto height = drawInfo.value(QRCoderSpace::QRMaxHeightStr).toInt();
	auto outputRect = QRect(0, 0, width, height);

	QImage output(QSize(width, height), QImage::Format_RGBA8888);
	output.fill(Qt::transparent);
	QPainter painter(&output);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

	QPoint imagePos((width - src.width()) / 2, marginTop * 2);
	painter.drawImage(imagePos, src);

	QPoint tipPos(0, marginTop * 3 + src.height());
	int tipMaxtHeight = drawInfo.value(QRCoderSpace::QRTipMaxheightStr).toInt();
	auto tipRect = QRect(0, 0, width, tipMaxtHeight);
	tipRect.moveTopLeft(tipPos);
	QPen pen(Qt::white, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	painter.setPen(pen);
	painter.setFont(font);
	painter.drawText(tipRect, tips, option);

	return output;
}
