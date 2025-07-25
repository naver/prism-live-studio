/******************************************************************************
    Copyright (C) 2015 by Ruwen Hahn <palana@stunned.de>

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

#include "moc_source-label.cpp"
#include "pls/pls-dual-output.h"
#include "libutils-api.h"
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionFocusRect>

void SourceLabel::resizeEvent(QResizeEvent *event)
{
	update();
	QLabel::resizeEvent(event);
}

void SourceLabel::paintEvent(QPaintEvent *event)
{
	QPainter dc(this);
	int padding = m_iPadding;
	dc.setFont(font());

	QStyleOption opt;
	opt.initFrom(this);
	auto textColor = opt.palette.color(QPalette::Text);

	QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
	option.setWrapMode(QTextOption::NoWrap);

	dc.setPen(textColor);
	dc.drawText(QRect(padding, 0, width() - padding, height()), SnapSourceName(), option);
	QLabel::paintEvent(event);
}

QString SourceLabel::SnapSourceName()
{
	if (currentText.isEmpty())
		return currentText;

	QFontMetrics fontWidth(font());
	if (fontWidth.horizontalAdvance(currentText) > width() - 5)
		return fontWidth.elidedText(currentText, Qt::ElideRight, width() - 5);
	else
		return currentText;
}

SourceLabel::SourceLabel(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f)
{
	this->setText(text);
}

void SourceLabel::setText(const QString &text)
{
	currentText = text;
	update();
}

void SourceLabel::setText(const char *text)
{
	currentText = text ? text : "";
	update();
}

QString SourceLabel::GetText() const
{
	return currentText;
}

void SourceLabel::appendDeviceName(const char *name, const char *appendDeviceName)
{
	if (pls_is_empty(name) || pls_is_empty(appendDeviceName)) {
		return;
	}
	this->setText(QString::fromStdString(name) + appendDeviceName);
	this->setToolTip(QString::fromStdString(name) + appendDeviceName);
}

void OBSSourceLabel::clearSignals()
{
	destroyedSignal.Disconnect();
	removedSignal.Disconnect();
	renamedSignal.Disconnect();
}

void OBSSourceLabel::SourceRenamed(void *data, calldata_t *params)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);

	const char *name = calldata_string(params, "new_name");
	label.setText(name);

	emit label.Renamed(name);
}

void OBSSourceLabel::SourceRemoved(void *data, calldata_t *)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);
	emit label.Removed();
}

void OBSSourceLabel::SourceDestroyed(void *data, calldata_t *)
{
	auto &label = *static_cast<OBSSourceLabel *>(data);
	emit label.Destroyed();

	label.destroyedSignal.Disconnect();
	label.removedSignal.Disconnect();
	label.renamedSignal.Disconnect();
}
