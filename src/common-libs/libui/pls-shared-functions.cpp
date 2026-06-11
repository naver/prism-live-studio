#include "pls-shared-functions.h"

#include <qpainter.h>
#include <qfile.h>
#include <qwidget.h>
#include <QCoreApplication>
#include <QStyle>
#include <QApplication>

LIBUI_API QPixmap pls_shared_paint_svg(QSvgRenderer &renderer, const QSize &pixSize)
{
	QPixmap pixmap(pixSize);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	renderer.render(&painter);
	return pixmap;
}

LIBUI_API QPixmap pls_shared_paint_svg(const QString &pixmapPath, const QSize &pixSize)
{
	QSvgRenderer renderer(pixmapPath);
	renderer.setAspectRatioMode(Qt::IgnoreAspectRatio);
	return pls_shared_paint_svg(renderer, pixSize);
}

LIBUI_API QPixmap &pls_shared_get_cube_pix(QPixmap &mBigPix)
{
	int width = mBigPix.width();
	int height = mBigPix.height();
	int cube = qMin(width, height);
	QRect cubeRec((width - cube) / 2, (height - cube) / 2, cube, cube);
	mBigPix = mBigPix.copy(cubeRec);
	return mBigPix;
}

LIBUI_API QPixmap &pls_shared_circle_mask_image(QPixmap &pix)
{

	QSize pixSize = pix.size();

	QPixmap image(pixSize);
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

	QPainterPath path;
	path.addRoundedRect(image.rect(), pixSize.width() / 2.0, pixSize.height() / 2.0);
	painter.setClipPath(path);

	painter.drawPixmap(image.rect(), pix);

	pix = image;
	return pix;
}

LIBUI_API QPixmap pls_load_thumbnail(const QString &filePath)
{
	QFile file(filePath);
	QByteArray data;
	if (!file.open(QIODevice::ReadOnly)) {
		return QPixmap();
	}
	data = file.readAll();
	auto orginData = QByteArray::fromBase64(data);
	QPixmap pix;
	pix.loadFromData(orginData);

	return pix;
}

LIBUI_API QImage pls_make_overlay(const QRect &rec, int xR, int yR, const QColor &color)
{
	QImage pix(rec.size(), QImage::Format_RGBA8888);
	pix.fill(Qt::transparent);
	QPainter painter(&pix);
	painter.setPen(color);
	painter.setRenderHint(QPainter::Antialiasing);
	QPainterPath path;
	path.addRect(rec);
	path.addRoundedRect(rec, xR, yR);
	painter.fillPath(path, color);
	return pix;
}

LIBUI_API void pls_aligin_to_widget_center(QWidget *source, const QWidget *target)
{
	if (source == nullptr || target == nullptr) {
		return;
	}
	QRect showRect = target->geometry();
	QSize size = source->rect().size();
	QPoint pos(showRect.x() + (showRect.width() - size.width()) / 2, showRect.y() + (showRect.height() - size.height()) / 2);
#if defined(Q_OS_MACOS)
	auto app = static_cast<QApplication *>(QCoreApplication::instance());
	QStyle *style = app->style();
	int titleBarHeight = style->pixelMetric(QStyle::PM_TitleBarHeight);
	bool hasTitleBar = !(source->windowFlags() & Qt::FramelessWindowHint);
	bool hasParentTitleBar = !(target->windowFlags() & Qt::FramelessWindowHint);

	if (hasParentTitleBar && !hasTitleBar) {
		QPoint diffPosition = pos;
		diffPosition.setY(pos.y() - titleBarHeight * 0.5);
		pos = diffPosition;
	}

	if (!hasParentTitleBar && hasTitleBar) {
		QPoint diffPosition = pos;
		diffPosition.setY(pos.y() + titleBarHeight * 0.5f);
		pos = diffPosition;
	}

#endif
	source->setGeometry(pos.x(), pos.y(), size.width(), size.height());
}
