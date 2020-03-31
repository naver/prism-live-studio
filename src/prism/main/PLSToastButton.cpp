#include "PLSToastButton.hpp"
#include "ui_PLSToastButton.h"
#include "login-common-helper.hpp"

#include <QPainter>
#define RADIU 10
#define OFFSET_X 14
#define OFFSET_Y 6
#define MAXNUM 9
PLSToastButton::PLSToastButton(QWidget *parent) : QPushButton(parent), ui(new Ui::PLSToastButton), m_num(0)
{
	ui->setupUi(this);
	//updateIconStyle(m_num);
}

PLSToastButton::~PLSToastButton()
{
	delete ui;
}

void PLSToastButton::setNum(const int num)
{
	m_num = num;
	//updateIconStyle(m_num);
	update();
}

int PLSToastButton::num() const
{
	return m_num;
}

QString PLSToastButton::getNumText() const
{
	if (m_num > 9) {
		return QString("9+");
	} else {
		return QString::number(m_num);
	}
}

void PLSToastButton::paintEvent(QPaintEvent *event)
{
	QPushButton::paintEvent(event);
	QPainter p(this);
	QPen pen;
	QRectF textRec;

	pen.setWidthF(0.5);
	pen.setColor("#c34151");
	p.setBrush(QBrush("#c34151"));
	p.setPen(pen);
	p.setRenderHint(QPainter::Antialiasing);
	QPoint f = this->rect().center();
	if (m_num > MAXNUM) {
		//4 is rect length
		p.drawEllipse(QPointF(f.rx() + OFFSET_X - 1, f.ry() - OFFSET_Y), RADIU, RADIU);
		p.drawRect(QRectF(QPointF(f.rx() + OFFSET_X - 1, f.ry() - OFFSET_Y - RADIU / 2), QSize(4, RADIU)));
		p.drawEllipse(QPointF(f.rx() + OFFSET_X + 3 - 1, f.ry() - OFFSET_Y), RADIU, RADIU);

	} else if (m_num > 0 && m_num <= MAXNUM) {
		p.drawEllipse(QPointF(f.rx() + OFFSET_X, f.ry() - OFFSET_Y), RADIU, RADIU);
	}

	pen.setColor(Qt::white);
	p.setPen(pen);

	if (m_num > MAXNUM) {
		textRec = QRectF(f.rx() + OFFSET_X + 2 - RADIU, f.ry() - OFFSET_Y - RADIU - 2, RADIU * 2, RADIU * 2);
		p.drawText(textRec, Qt::AlignCenter, getNumText());

	} else if (m_num > 0 && m_num <= MAXNUM) {
		textRec = QRectF(f.rx() + OFFSET_X - RADIU, f.ry() - OFFSET_Y - RADIU - 2, RADIU * 2, RADIU * 2);
		p.drawText(textRec, Qt::AlignCenter, getNumText());
	}
}

void PLSToastButton::updateIconStyle(bool num)
{
	false == num ? setProperty("hasToast", false) : setProperty("hasToast", true);
	LoginCommonHelpers::refreshStyle(this);
}
