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
#include "libui.h"
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionFocusRect>

const int INVALID_ICON_SIZE = 22;
const int INVALID_ICON_LEFT_MARGIN = 4;

static inline int clampedPaintWidth(int paintWidth)
{
	return qMax(0, paintWidth);
}

QRect SourceLabel::iconRect() const
{
	if (!m_sourceInvalid)
		return QRect();
	int padding = m_iPadding + INVALID_ICON_LEFT_MARGIN + INVALID_ICON_SIZE;
	QFontMetrics fontMetrics(font());
	int paintWidth = clampedPaintWidth(qMin<int>(fontMetrics.horizontalAdvance(GetText()), width() - padding));
	QRect rect(m_iPadding + paintWidth + INVALID_ICON_LEFT_MARGIN, (height() - INVALID_ICON_SIZE) / 2,
		   INVALID_ICON_SIZE, INVALID_ICON_SIZE);
	return rect.intersected(QRect(0, 0, width(), height()));
}

void SourceLabel::resizeEvent(QResizeEvent *event)
{
	update();
	QLabel::resizeEvent(event);
}

void SourceLabel::paintEvent(QPaintEvent *event)
{
	QPainter dc(this);
	int padding = m_sourceInvalid ? m_iPadding + INVALID_ICON_LEFT_MARGIN + INVALID_ICON_SIZE : m_iPadding;
	dc.setFont(font());

	QStyleOption opt;
	opt.initFrom(this);
	auto textColor = opt.palette.color(QPalette::Text);

	QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
	option.setWrapMode(QTextOption::NoWrap);
	QFontMetrics fontMetrics(font());
	int paintWidth = clampedPaintWidth(qMin<int>(fontMetrics.horizontalAdvance(GetText()), width() - padding));
	dc.setPen(textColor);
	dc.drawText(QRect(m_iPadding, 0, paintWidth, height()), SnapSourceName(), option);
	if (m_sourceInvalid) {
		QRect iconR = iconRect();
		if (!iconR.isEmpty()) {
			auto svgPath = m_iconHover ? ":/resource/images/icon-source/icon-source-disable_hover.svg"
						   : ":/resource/images/icon-source/icon-source-disable.svg";
			dc.drawPixmap(iconR, pls_load_pixmap(svgPath, QSize(INVALID_ICON_SIZE, INVALID_ICON_SIZE)));
		}
	}
	QLabel::paintEvent(event);
}

void SourceLabel::leaveEvent(QEvent *event)
{
	if (m_iconHover) {
		m_iconHover = false;
		update();
	}
	QLabel::leaveEvent(event);
}

void SourceLabel::mouseMoveEvent(QMouseEvent *event)
{
	if (m_sourceInvalid) {
		QRect rect = iconRect();
		bool hover = rect.contains(event->pos());
		if (hover != m_iconHover) {
			m_iconHover = hover;
			update();
		}
	}
	QLabel::mouseMoveEvent(event);
}

QString SourceLabel::SnapSourceName()
{
	if (currentText.isEmpty())
		return currentText;
	auto padding = m_sourceInvalid ? m_iPadding + INVALID_ICON_LEFT_MARGIN + INVALID_ICON_SIZE : m_iPadding;
	int availableWidth = clampedPaintWidth(width() - padding);
	QFontMetrics fontWidth(font());
	if (fontWidth.horizontalAdvance(currentText) > availableWidth)
		return fontWidth.elidedText(currentText, Qt::ElideRight, availableWidth);
	else
		return currentText;
}

SourceLabel::SourceLabel(const QString &text, QWidget *parent, Qt::WindowFlags f) : QLabel(text, parent, f)
{
	this->setText(text);
	setMouseTracking(true);
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

void SourceLabel::setSourceInvalid(bool invalid)
{
	m_sourceInvalid = invalid;
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
