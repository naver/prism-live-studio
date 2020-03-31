#include "PLSScheduleMenuItem.h"
#include <QVBoxLayout>
#include <QApplication>

PLSScheduleMenuItem::PLSScheduleMenuItem(const ComplexItemData data, int superWidth, QWidget *parent) : QWidget(parent), _data(data), detailLabel(new QLabel())
{
	if (data.type == PLSScheduleItemType::Ty_Loading) {
		QHBoxLayout *hbl = new QHBoxLayout(this);
		titleLabel = new QLabel(_data.title);
		titleLabel->setObjectName("titleLabel");
		setupTimer();
		titleLabel->setText("");
		m_pTimer->start();
		hbl->addWidget(titleLabel);

		loadingDetailLabel = new QLabel(_data.time);
		loadingDetailLabel->setObjectName("loadingDetailLabel");
		hbl->addWidget(loadingDetailLabel);
		hbl->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
		this->setLayout(hbl);
	} else {
		QVBoxLayout *vbl = new QVBoxLayout(this);
		vbl->setSpacing(4);
		vbl->setContentsMargins(12, 0, 0, 0);
		titleLabel = new QLabel();
		titleLabel->setObjectName("titleLabel");
		QFontMetrics fontWidth(titleLabel->font());
		QString elidedText = fontWidth.elidedText(_data.title, Qt::ElideRight, superWidth - 80);
		titleLabel->setAlignment(Qt::AlignBottom | Qt::AlignLeading | Qt::AlignLeft);
		titleLabel->setText(elidedText);
		vbl->addWidget(titleLabel);

		detailLabel = new QLabel(_data.time);
		detailLabel->setObjectName("detailLabel");
		this->detailLabel->setStyleSheet("color:#777777");
		detailLabel->setAlignment(Qt::AlignTop | Qt::AlignLeading | Qt::AlignLeft);
		vbl->addWidget(detailLabel);
		detailLabel->setHidden(data.type == PLSScheduleItemType::Ty_NormalLive);
		this->setLayout(vbl);

		if (detailLabel->isHidden()) {
			titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeading | Qt::AlignLeft);
		}
	}
}

PLSScheduleMenuItem::~PLSScheduleMenuItem() {}

void PLSScheduleMenuItem::enterEvent(QEvent *event)
{
	this->titleLabel->setStyleSheet("color:#effc35");
	this->detailLabel->setStyleSheet("color:#7a7f34");
	QWidget::enterEvent(event);
}

void PLSScheduleMenuItem::leaveEvent(QEvent *event)
{
	this->titleLabel->setStyleSheet("color:#ffffff");
	this->detailLabel->setStyleSheet("color:#777777");
	QWidget::leaveEvent(event);
}

void PLSScheduleMenuItem::setupTimer()
{
	m_pTimer = new QTimer();
	m_pTimer->setInterval(300);
	connect(m_pTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
}

void PLSScheduleMenuItem::updateProgress()
{
	static int showIndex = 0;
	showIndex = showIndex > 7 ? 1 : ++showIndex;
	QString imageName = QString(":/Images/skin/loading-%1.png").arg(showIndex);
	QImage *errImage = new QImage(imageName);
	titleLabel->setPixmap(QPixmap::fromImage(*errImage));
	QApplication::processEvents();
}
