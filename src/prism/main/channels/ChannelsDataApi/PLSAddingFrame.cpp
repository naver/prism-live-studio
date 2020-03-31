#include "PLSAddingFrame.h"
#include "ui_PLSAddingFrame.h"

PLSAddingFrame::PLSAddingFrame(QWidget *parent) : QFrame(parent), ui(new Ui::PLSAddingFrame), mStep(0)
{
	ui->setupUi(this);
	connect(&mUpdateTimer, &QTimer::timeout, this, &PLSAddingFrame::nextFrame);
}

PLSAddingFrame::~PLSAddingFrame()
{
	delete ui;
}

void PLSAddingFrame::setSourceFirstFile(const QString &sfile)
{
	mSourceFile = sfile;
	nextFrame();
}

void PLSAddingFrame::setContent(const QString &txt)
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

void PLSAddingFrame::nextFrame()
{
	QCoreApplication::processEvents();
	++mStep;
	if (mStep >= 9) {
		mStep = 1;
	}
	ui->PixLabel->setPixmap(mSourceFile.arg(mStep));
}
