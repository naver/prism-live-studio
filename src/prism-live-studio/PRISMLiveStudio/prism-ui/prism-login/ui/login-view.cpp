#include "login-view.hpp"
#include "ui_PLSLoginView.h"
#include <QDebug>
#include "log/module_names.h"
#include "liblog.h"
#include <libutils-api.h>
#include <QShortcut>

PLSLoginView::PLSLoginView(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSLoginView>();
	initUi();
}

PLSLoginView::~PLSLoginView()
{
	pls_delete(ui);
}

void PLSLoginView::initUi()
{
	ui->setupUi(this);

	m_loginBackgroundView = pls_new<LoginBackgroundView>(ui->stackedWidget);
	ui->stackedWidget->addWidget(m_loginBackgroundView);
	m_loginWithEmailView = pls_new<LoginWithEmailView>(ui->stackedWidget);
	ui->stackedWidget->addWidget(m_loginWithEmailView);
	m_signupWithEmailView = pls_new<SignupWithEmailView>(ui->stackedWidget);
	ui->stackedWidget->addWidget(m_signupWithEmailView);
	ui->stackedWidget->addWidget(m_loginBackgroundView);
	ui->stackedWidget->addWidget(m_loginWithEmailView);
	ui->stackedWidget->addWidget(m_signupWithEmailView);
	ui->stackedWidget->setCurrentWidget(m_loginBackgroundView);

#if __APPLE__
	QShortcut *shortcut = new QShortcut(QKeySequence("Ctrl+Q"), this);
	QObject::connect(shortcut, &QShortcut::activated, this, &PLSLoginView::macQuitWithCommand_Q, Qt::SingleShotConnection);
#endif
}

void PLSLoginView::macQuitWithCommand_Q()
{
	qDebug() << "macQuitWithCommand_Q invoked";

#if __APPLE__
	QDialog *dialog = nullptr;
	for (QWidget *parent = ui->stackedWidget->parentWidget(); parent && !dialog;) {
		dialog = dynamic_cast<QDialog *>(parent);
		parent = parent->parentWidget();
	}
	if (dialog) {
		dialog->done(QDialog::Rejected);
	}
#endif
}
