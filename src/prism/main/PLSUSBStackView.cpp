#include "PLSUSBStackView.h"
#include "ui_PLSUSBStackView.h"

PLSUSBStackView::PLSUSBStackView(QWidget *parent) : QFrame(parent), ui(new Ui::PLSUSBStackView)
{
	ui->setupUi(this);
}

PLSUSBStackView::~PLSUSBStackView()
{
	delete ui;
}
