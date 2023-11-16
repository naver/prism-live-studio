#pragma once
#include <QPixmap>
#include <QSvgRenderer>
#include <qbitmap.h>
#include <qpainter.h>
#include <qpen.h>
#include <qcolor.h>
#include <qpainterpath.h>
#include "libui-globals.h"

LIBUI_API QPixmap pls_shared_paint_svg(QSvgRenderer &renderer, const QSize &pixSize);
LIBUI_API QPixmap pls_shared_paint_svg(const QString &pixmapPath, const QSize &pixSize);

// to get image of cueb,eg image with 120x150,image will scale to 120x120
LIBUI_API QPixmap &pls_shared_get_cube_pix(QPixmap &src);

// if image not good in different screen with label widget, use scale content attribute
LIBUI_API QPixmap &pls_shared_circle_mask_image(QPixmap &pix);

LIBUI_API QPixmap pls_load_thumbnail(const QString &filePath);

LIBUI_API QImage pls_make_overlay(const QRect &rec, int xR, int yR, const QColor &color);

LIBUI_API void pls_aligin_to_widget_center(QWidget *source, const QWidget *target);
