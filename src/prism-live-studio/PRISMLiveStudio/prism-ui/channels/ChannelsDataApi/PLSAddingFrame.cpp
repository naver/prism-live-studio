#include "PLSAddingFrame.h"

PLSAddingFrame::PLSAddingFrame(QWidget *parent) : QFrame(parent)
{
	ui->setupUi(this);
	connect(&mUpdateTimer, &QTimer::timeout, this, &PLSAddingFrame::nextFrame);
	connect(qApp, &QCoreApplication::aboutToQuit, &mUpdateTimer, &QTimer::stop);
}

PLSAddingFrame::~PLSAddingFrame()
{
	mUpdateTimer.stop();
}

void PLSAddingFrame::setSourceFirstFile(const QString &sfile)
{
	mSourceFile = sfile;
	nextFrame();
}

void PLSAddingFrame::setContent(const QString &txt) const
{
	if (txt.isEmpty()) {
		ui->TextLabel->hide();
	} else {
		ui->TextLabel->show();
	}
	ui->TextLabel->setText(txt);
}

void PLSAddingFrame::start(int time)
{
	mUpdateTimer.start(time);
}

void PLSAddingFrame::stop()
{
	mUpdateTimer.stop();
}

void PLSAddingFrame::changeEvent(QEvent *e)
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

extern QPixmap pls_shared_paint_svg(const QString &pixmapPath, const QSize &pixSize);
void PLSAddingFrame::nextFrame()
{
	++mStep;
	if (mStep >= 9) {
		mStep = 1;
	}
	QPixmap pixmap = pls_shared_paint_svg(mSourceFile.arg(mStep), QSize(24, 24));
	ui->PixLabel->setPixmap(pixmap);
}
