#include "PLSMotionErrorLayer.h"
#include "ui_PLSMotionErrorLayer.h"

PLSMotionErrorLayer::PLSMotionErrorLayer(QWidget *parent, int timerInterval) : WidgetDpiAdapter(parent), ui(new Ui::PLSMotionErrorLayer), m_timerInterval(timerInterval)
{
	ui->setupUi(this);
	this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
	this->setAttribute(Qt::WA_TranslucentBackground);
	connect(&m_timerToastHide, &QTimer::timeout, this, &PLSMotionErrorLayer::hiddenView);
	connect(ui->closeButton, &QPushButton::clicked, this, &PLSMotionErrorLayer::hiddenView);
}

PLSMotionErrorLayer::~PLSMotionErrorLayer()
{
	delete ui;
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

void PLSMotionErrorLayer::on_pushButton_clicked() {}

bool PLSMotionErrorLayer::needQtProcessDpiChangedEvent() const
{
	return false;
}

bool PLSMotionErrorLayer::needProcessScreenChangedEvent() const
{
	return false;
}
