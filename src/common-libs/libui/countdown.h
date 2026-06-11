#pragma once

#include <qpainter.h>
#include <qtimer.h>
#include <qobject.h>
#include <qpixmap.h>
#include <qlabel.h>
#include <qcolor.h>
#include <qpen.h>

class CountDownLabel : public QObject {

public:
	explicit CountDownLabel(QObject *parent = nullptr) : QObject(parent)
	{

		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, &CountDownLabel::paint, Qt::QueuedConnection);
	}

	static CountDownLabel *start(QLabel *lb, const QFont &txtFontIn, const quint64 &totalTimeIn = 10 * 1000, const QColor &ringBackgroundIn = Qt::white, const QColor &ringColorIn = Qt::gray,
				     QColor fontColorIn = Qt::yellow)
	{
		auto counter = new CountDownLabel(lb);
		counter->totalTime = totalTimeIn;
		counter->ringBackground = ringBackgroundIn;
		counter->ringColor = ringColorIn;
		counter->fontColor = fontColorIn;
		counter->txtFont = txtFontIn;
		lb->setScaledContents(true);
		lb->setAlignment(Qt::AlignCenter);
		counter->start(lb);

		return counter;
	};

	void start(QLabel *lb)
	{
		label = lb;
		timer->setInterval(interval);
		timeLeft = totalTime;
		//in order show the begin number and 0,add 2 intervals .
		step = 360.0 / double(totalTime - 2 * interval);
		txtFont.setPointSize(txtFont.pointSize() * scaleFactor);
		paint();
		timer->start();
	}

private:
	void paint()
	{

		if (label == nullptr) {
			timer->stop();
			return;
		}
		timeLeft -= interval;
		if (timeLeft <= 0) {
			timer->stop();
			this->deleteLater();
		}
		auto cubeSize = scaleFactor * qMin(label->size().width(), label->size().height());
		auto tmpPen = QPen(ringBackground, penWidth * 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		QSize size(cubeSize, cubeSize);
		QPixmap pix(size);
		pix.fill(Qt::transparent);
		QPainter painter(&pix);

		painter.setRenderHint(QPainter::Antialiasing);

		//ring background
		painter.setPen(tmpPen);
		auto centerRec = pix.rect().adjusted(2, 2, -2, -2);
		painter.drawEllipse(centerRec);

		//text
		//the begin numer show
		auto txtV = (timeLeft + (1000 - interval)) / 1000;
		tmpPen.setColor(fontColor);
		painter.setPen(tmpPen);
		painter.setFont(txtFont);
		painter.drawText(centerRec, Qt::AlignCenter, QString::number(txtV));

		//ring current
		tmpPen.setColor(ringColor);
		painter.setPen(tmpPen);
		painter.drawArc(centerRec, 90 * 16, int(interval * currentCount * step * 16));

		++currentCount;
		label->setPixmap(pix);
	}

	//private:
	int scaleFactor = 2;
	int penWidth = 2;
	int interval = 100; //ms
	int currentCount = 0;
	quint64 totalTime = 10 * 1000; //ms
	quint64 timeLeft = 0;
	double step = 360.0 / double(totalTime - 2 * interval); // degree / ms
	QColor fontColor = Qt::yellow;
	QColor ringBackground = Qt::white;
	QColor ringColor = Qt::gray;
	QFont txtFont = QFont("Helvetica", 14);

	QTimer *timer = nullptr;
	QPointer<QLabel> label = nullptr;
};
