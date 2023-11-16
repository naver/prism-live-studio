#include "PLSMotionErrorLayer.h"
#include "ui_PLSMotionErrorLayer.h"

#include "libutils-api.h"

PLSMotionErrorLayer::PLSMotionErrorLayer(QWidget *parent, int timerInterval) : QFrame(parent), m_timerInterval(timerInterval)
{
	ui = pls_new<Ui::PLSMotionErrorLayer>();
	ui->setupUi(this);
	this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	this->setAttribute(Qt::WA_TranslucentBackground);
	connect(&m_timerToastHide, &QTimer::timeout, this, &PLSMotionErrorLayer::hiddenView);
	connect(ui->closeButton, &QPushButton::clicked, this, &PLSMotionErrorLayer::hiddenView);
}

PLSMotionErrorLayer::~PLSMotionErrorLayer()
{
	pls_delete(ui);
}

void PLSMotionErrorLayer::showView()
{
	m_timerToastHide.setInterval(m_timerInterval);
	m_timerToastHide.setSingleShot(true);
	m_timerToastHide.start();
	show();
}

void PLSMotionErrorLayer::hiddenView()
{
	m_timerToastHide.stop();
	hide();
}

void PLSMotionErrorLayer::setToastText(const QString &text)
{
	ui->titleLabel->setText(text);
}

void PLSMotionErrorLayer::setToastTextAlignment(Qt::Alignment alignment)
{
	ui->titleLabel->setAlignment(alignment);
}

void PLSMotionErrorLayer::setToastWordWrap(bool wrap)
{
	ui->titleLabel->setWordWrap(wrap);
}

void PLSMotionErrorLayer::on_pushButton_clicked() const {}
