#include "PLSScrollingLabel.h"

#include <QTimerEvent>
#include <QPainter>

PLSScrollingLabel::PLSScrollingLabel(QWidget *parent) : QLabel(parent) {}

PLSScrollingLabel::~PLSScrollingLabel()
{
	StopTimer();
}

void PLSScrollingLabel::SetText(const QString &_text)
{
	text = _text;
	UpateRollingStatus();
	repaint();
}

void PLSScrollingLabel::paintEvent(QPaintEvent *)
{
	QString strText = text;
	QPainter painter(this);

	if (timerId < 0) {
		QFontMetrics fm(font());
		int nW = fm.horizontalAdvance(text);
		QRect rc = rect();
		rc.setLeft((rc.right() - nW) / 2);
		painter.drawText(rc, Qt::AlignVCenter, strText);
	} else {
		strText += "   " + text;
		QRect rc = rect();
		rc.setLeft(rc.left() - left);
		painter.drawText(rc, Qt::AlignVCenter, strText);
	}
}

void PLSScrollingLabel::UpateRollingStatus()
{
	QFontMetrics fm(font());
	int textWidth = fm.horizontalAdvance(text);

	left = 0;
	if (textWidth > width()) {
		StartTimer();
		return;
	}

	if (timerId >= 0) {
		StopTimer();
	}
}

void PLSScrollingLabel::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == timerId && isVisible()) {
		handleScorllingText();
	}

	QLabel::timerEvent(event);
}

void PLSScrollingLabel::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);

	UpateRollingStatus();
}

void PLSScrollingLabel::StartTimer(const int interval)
{
	if (-1 != timerId) {
		return;
	}
	timerId = startTimer(interval);
}

void PLSScrollingLabel::StopTimer()
{
	if (-1 != timerId) {
		killTimer(timerId);
		timerId = -1;
	}
}

void PLSScrollingLabel::handleScorllingText()
{
	left += 1;
	QFontMetrics fm(font());
	int txtWidth = fm.horizontalAdvance(text);
	int spaceWidth = fm.horizontalAdvance("   ");

	if ((txtWidth + spaceWidth) < left)
		left = 0;

	repaint();
}
