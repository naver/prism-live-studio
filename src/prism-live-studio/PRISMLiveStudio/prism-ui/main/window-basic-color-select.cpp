/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "window-basic-color-select.hpp"
#include "ui_ColorSelectNew.h"

#include "libutils-api.h"
#include "libui.h"

#include <QPainter>

void ColorButton::SetColor(QString fillColor)
{
	color = fillColor;
}

void ColorButton::SetSelect(bool select)
{
	selected = select;
	update();
}

void ColorButton::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
	painter.setPen(QColor(Qt::transparent));

	if (!isVisible()) {
		return;
	}

	int fixedWidthHeight = height();

	if (selected) {
		painter.setBrush(QColor("#bababa"));
		painter.drawEllipse(QPoint(fixedWidthHeight / 2, fixedWidthHeight / 2), 24 / 2, 24 / 2);
		painter.setBrush(QColor("#2d2d2d"));
		painter.drawEllipse(QPoint(fixedWidthHeight / 2, fixedWidthHeight / 2), 23 / 2, 23 / 2);
	}

	painter.setBrush(QColor(Qt::white));
	painter.drawEllipse(QPoint(fixedWidthHeight / 2, fixedWidthHeight / 2), 18 / 2, 18 / 2);

	painter.setBrush(QColor(color));
	painter.drawEllipse(QPoint(fixedWidthHeight / 2, fixedWidthHeight / 2), 16 / 2, 16 / 2);
}

ColorSelectNew::ColorSelectNew(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::ColorSelectNew>();
	ui->setupUi(this);

	pls_add_css(this, {"ColorSelect"});

	for (int i = 1; i < 9; i++) {
		ColorButton *presetBtn = pls_new<ColorButton>(this);
		presetBtn->setObjectName(QString("preset%1").arg(i));
		presetBtn->setProperty("bgColor", i);
		ui->horizontalLayout->addWidget(presetBtn);
	}

	ui->recentWidgetLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	ui->recentColorWidget->hide();
}

ColorSelectNew::~ColorSelectNew()
{
	pls_delete(ui);
}

void ColorSelectNew::UpdateRecentColorOrder(obs_data_t *private_settings)
{
	obs_data_array_t *colorArray = obs_data_get_array(private_settings, "color-order");
	size_t count = obs_data_array_count(colorArray);
	if (0 == count) {
		ui->recentColorWidget->hide();
	} else {
		for (int i = 0; i < SOURCE_ITEM_MAX_COLOR_COUNT; i++) {
			obs_data_t *color = obs_data_array_item(colorArray, i);
			const char *colorStr = obs_data_get_string(color, "color");
			obs_data_release(color);
			AppendRecentColorButton(colorStr, i + 1);
		}
		ui->recentColorWidget->show();
	}
}

void ColorSelectNew::AppendRecentColorButton(const char *color, int index)
{
	QColor colorStr(color);

	ColorButton *recentBtn = pls_new<ColorButton>(this);
	if (color && strlen(color) > 0) {
		recentBtn->SetColor(colorStr.toRgb().name());
		recentBtn->setProperty("bgColor", color);
		recentBtn->setObjectName(QString("custom%1").arg(index));
	} else {
		recentBtn->SetColor(QColor(Qt::transparent).toRgb().name());
		recentBtn->setProperty("bgColor", QColor(Qt::transparent).toRgb().name());
		recentBtn->setVisible(false);
	}

	ui->recentWidgetLayout->addWidget(recentBtn);
}
