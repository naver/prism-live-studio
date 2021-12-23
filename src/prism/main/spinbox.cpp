#include "spinbox.hpp"

#include <QWheelEvent>
#include <QLineEdit>

PLSSpinBox::PLSSpinBox(QWidget *parent) : QSpinBox(parent)
{
	// setButtonSymbols(QAbstractSpinBox::ButtonSymbols::NoButtons);
}

PLSSpinBox::~PLSSpinBox() {}

void PLSSpinBox::makeTextVCenter()
{
	QLineEdit *edit = this->findChild<QLineEdit *>();
	if (edit) {
		edit->setContentsMargins({0, 0, 0, 1});
	}
}

void PLSSpinBox::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}

PLSDoubleSpinBox::PLSDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
	// setButtonSymbols(QAbstractSpinBox::ButtonSymbols::NoButtons);
}
PLSDoubleSpinBox ::~PLSDoubleSpinBox() {}

void PLSDoubleSpinBox::wheelEvent(QWheelEvent *event)
{
	event->ignore();
}
