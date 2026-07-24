#include "login-password-lineEdit.hpp"

#include <QCursor>
#include <QDebug>
#include <QEvent>
#include <QMouseEvent>
#include "pls-common-define.hpp"
#include "libutils-api.h"
#include "libui.h"

using namespace common;
LoginPasswordLineEdit::LoginPasswordLineEdit(QWidget *parent) : PLSLineEdit(parent)
{
	ui = pls_new<Ui::PLSLoginPasswordLineEdit>();
	ui->setupUi(this);
	this->layout()->setContentsMargins(0, 0, 6, 0);
	this->setEchoMode(QLineEdit::Password);
	ui->stateButton->setProperty(PASSWORD, STATUS_TRUE);
	ui->stateButton->setCursor(Qt::ArrowCursor);
	ui->stateButton->setFocusPolicy(Qt::ClickFocus);
	pls_uistep_v2_set_value(ui->stateButton, "PwdIcon");
	pls_uistep_v2_enable(ui->stateButton, true);
}

LoginPasswordLineEdit::~LoginPasswordLineEdit()
{
	pls_delete(ui, nullptr);
}

void LoginPasswordLineEdit::on_stateButton_clicked()
{
	if (m_isVisablePwd) {
		this->setEchoMode(QLineEdit::Normal);
		ui->stateButton->setProperty(PASSWORD, STATUS_FALSE);
	} else {
		this->setEchoMode(QLineEdit::Password);
		ui->stateButton->setProperty(PASSWORD, STATUS_TRUE);
	}
	pls_flush_style(ui->stateButton);
	m_isVisablePwd = !m_isVisablePwd;
	auto stateStr = [this](bool isVisible) -> const char * { return isVisible ? "Visible" : "InVisible"; };
	PLS_UI_ACTION("State Form %s To %s", stateStr(m_isVisablePwd), stateStr(!m_isVisablePwd));
}
