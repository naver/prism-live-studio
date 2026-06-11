#include "ResolutionTipFrame.h"
#include "ui_ResolutionTipFrame.h"
#include <QPainter>
#include <QResizeEvent>
#include <qregularexpression.h>
#include <QTimer>
#include <QPainterPath>
#include "liblog.h"

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

void ResolutionTipFrame::setBackgroundColor(const QColor &color)
{
	mBackgroundColor = color;
}

void ResolutionTipFrame::on_CloseBtn_clicked()
{
	this->close();
}

void ResolutionTipFrame::delayShow(bool visible, int time)
{
	QTimer::singleShot(time, this, [this, visible]() {
		PLS_INFO("ResolutionTipFrame", "delayShow");
		this->setVisible(visible);
		if (visible) {
			this->updateUI();
		}
	});
}

void ResolutionTipFrame::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	case QEvent::Show:
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
	painter.fillPath(path, mBackgroundColor);

	mBackground = image;
}

void ResolutionTipFrame::updateUI()
{
	calculatePos();
}

void ResolutionTipFrame::calculatePos()
{

	auto fontM = ui->ContentLabel->fontMetrics();
	auto txt = ui->ContentLabel->text();
	auto maxRect = QRect(0, 0, 205 - fontM.leftBearing('c') - fontM.rightBearing('c'), 800);
	auto rect = fontM.boundingRect(maxRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextIncludeTrailingSpaces | Qt::TextWordWrap, txt);
	int margin = 8 + 16 /*padding*/ + fontM.leading();
	this->resize(QSize(this->width(), rect.height() + margin));

	aligin();
}

void ResolutionTipFrame::delayCalculate()
{
	if (mDelayTimer == nullptr) {
		mDelayTimer = new QTimer(this);
		mDelayTimer->setInterval(200);
		mDelayTimer->setSingleShot(true);
		connect(mDelayTimer, &QTimer::timeout, this, &ResolutionTipFrame::calculatePos, Qt::QueuedConnection);
	}
	mDelayTimer->start();
}

void ResolutionTipFrame::aligin()
{
	if (parentWidget() == nullptr) {
		return;
	}
	if (mAliginWidget == nullptr) {
		return;
	}
	auto differ = QPoint(this->frameSize().width() - 2, (this->height() - mAliginWidget->height()) / 2);
	auto pos = mAliginWidget->mapToGlobal(QPoint(0, 0) - differ);
	this->move(pos);
}

void ResolutionTipFrame::setAliginWidget(QWidget *aliginWidget)
{
	mAliginWidget = aliginWidget;
}
