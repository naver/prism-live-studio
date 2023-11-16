#include "PLSFileItemView.hpp"
#include "ui_PLSFileItemView.h"
#include "pls-common-define.hpp"
#include "login-common-helper.hpp"
#include <qstyle.h>
#include "utils-api.h"
using namespace common;
PLSFileItemView::PLSFileItemView(int index, QWidget *parent) : QFrame(parent), m_index(index)
{
	ui = pls_new<Ui::PLSFileItemView>();
	ui->setupUi(this);
	ui->fileNameLabel->style()->unpolish(ui->fileNameLabel);
	ui->fileNameLabel->style()->polish(ui->fileNameLabel);
}

PLSFileItemView::~PLSFileItemView()
{
	pls_delete(ui);
}

QFont PLSFileItemView::fileNameLabelFont() const
{
	return ui->fileNameLabel->font();
}

void PLSFileItemView::setFileName(const QString &filename)
{
	ui->fileNameLabel->setText(filename);
}

void PLSFileItemView::enterEvent(QEnterEvent *)
{
	setProperty(STATUS, STATUS_HOVER);
	ui->deleteIcon->setProperty(STATUS, STATUS_HOVER);
	LoginCommonHelpers::refreshStyle(ui->deleteIcon);
	LoginCommonHelpers::refreshStyle(this);
}

void PLSFileItemView::leaveEvent(QEvent *)
{
	setProperty(STATUS, STATUS_NORMAL);
	ui->deleteIcon->setProperty(STATUS, STATUS_NORMAL);
	LoginCommonHelpers::refreshStyle(ui->deleteIcon);
	LoginCommonHelpers::refreshStyle(this);
}

void PLSFileItemView::on_deleteIcon_clicked()
{
	emit deleteItem(m_index);
}
