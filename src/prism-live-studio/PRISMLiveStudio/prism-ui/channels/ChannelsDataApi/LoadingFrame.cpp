#include "LoadingFrame.h"
#include <QDebug>
#include <QPainter>

LoadingFrame::LoadingFrame(QWidget *parent) : QFrame(parent)
{
	ui->setupUi(this);
	connect(&mTimer, &QTimer::timeout, this, &LoadingFrame::next);
	this->setAttribute(Qt::WA_TranslucentBackground, true);
	this->setAutoFillBackground(true);
}

LoadingFrame::~LoadingFrame()
{
	mTimer.stop();
}

void LoadingFrame::initialize(const QString &file, int tick)
{
	setImage(file);
	setTick(tick);
	this->start();
	this->next();
}

void LoadingFrame::setImage(const QString &file)
{
	if (!mPic.load(file)) {
		qDebug() << " load error ";
		return;
	}
}

void LoadingFrame::setTick(int mssecond)
{
	mTick = mssecond;
}

void LoadingFrame::next()
{
	if (!mPic.isNull()) {
		mStepCount += 60;
		mStepCount %= 360;
		QPixmap tmp(mPic.size());
		tmp.fill(Qt::transparent);
		QPainter painter(&tmp);
		painter.translate(mPic.rect().center());
		painter.rotate(mStepCount);
		painter.translate(-mPic.rect().center() - QPoint(1, 1));
		painter.drawImage(mPic.rect(), mPic);

		ui->LoadingIconLabel->setPixmap(tmp);
		QApplication::processEvents();
	}
}

void LoadingFrame::start()
{
	mTimer.start(mTick);
}

void LoadingFrame::setTitleVisible(bool isVisible) const
{
	ui->TipLabel->setVisible(isVisible);
}

void LoadingFrame::changeEvent(QEvent *e)
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

void LoadingFrame::paintEvent(QPaintEvent *)
{
	QColor background(10, 10, 10, 150);
	QPainter painter(this);
	painter.setPen(Qt::transparent);
	painter.setBrush(background);

	painter.drawRoundedRect(this->rect(), 2, 2);
}
