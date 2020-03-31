#include <Windows.h>
#include "PLSChatDialog.h"
#include "ui_PLSChatDialog.h"

PLSChatDialog::PLSChatDialog(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSChatDialog)
{
	ui->setupUi(this->content());
}

PLSChatDialog::~PLSChatDialog()
{

	delete ui;
}
