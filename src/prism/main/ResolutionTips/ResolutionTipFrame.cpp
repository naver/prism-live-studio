#include "ResolutionTipFrame.h"
#include "ui_ResolutionTipFrame.h"
#include <QPainter>
#include <QResizeEvent>

ResolutionTipFrame::ResolutionTipFrame(QWidget *parent) : QFrame(parent), ui(new Ui::ResolutionTipFrame)
{
	ui->setupUi(this);
}

ResolutionTipFrame::~ResolutionTipFrame()
{
	delete ui;
}

void ResolutionTipFrame::setText(const QString &txt)
{
	ui->ContentLabel->setText(txt);
}

void ResolutionTipFrame::on_CloseBtn_clicked()
{
	this->close();
}

void ResolutionTipFrame::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void ResolutionTipFrame::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
	updateBackground();
}

void ResolutionTipFrame::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawImage(QPoint(0, 0), mBackground);
}

void ResolutionTipFrame::updateBackground()
{
	auto size = this->size();
	auto rect = QRectF(QPointF(0, 0), size);
	auto RoundRec = rect.adjusted(1, 1, -17, -1);

	/*
		*******************
		*		  *
		*		  *pos1
		*		  *	    *pos0T
		*		  *           * pos 0
		*		  *	    *pos0B
		*                 *pos2
		*		  *
		*******************
		*

	*/

	QPainterPath path;
	path.addRoundedRect(RoundRec, 3, 3);
	path.moveTo(rect.width() - 9, rect.height() / 2.0);

	auto pos0 = path.currentPosition();
	auto pos0T = pos0 + QPointF(-2, -2);
	auto pos0B = pos0 + QPointF(-2, 2);
	auto pos1 = QPointF(pos0.x() - 8, pos0.y() - 7.5);
	auto pos2 = QPointF(pos0.x() - 8, pos0.y() + 7.5);

	path.lineTo(pos0T);
	path.lineTo(pos1);
	path.lineTo(pos2);
	path.lineTo(pos0B);
	path.lineTo(pos0);

	mPath = path;
	QImage image(this->size(), QImage::Format_RGBA8888);
	image.fill(Qt::transparent);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setClipPath(mPath);
	painter.fillPath(path, QColor("#666666"));

	/*painter.setPen(Qt::red);
	painter.drawRect(RoundRec);*/
	mBackground = image;
}
