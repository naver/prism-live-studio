#include "login-common-helper.hpp"
#include <QRegularExpression>
#include <QUrl>
#include "pls-common-define.hpp"
#include <qstyle.h>
#include <QDialog>
using namespace common;
bool ::LoginCommonHelpers::isValidEmailByRegExp(const QString &email)
{
	QRegularExpression rep(EMAIL_REGEXP);
	return rep.match(email).hasMatch();
}

bool ::LoginCommonHelpers::isValidPasswordByRegExp(const QString &password)
{
	QRegularExpression rep(PASSWORD_REGEXP);
	return rep.match(password).hasMatch();
}

bool ::LoginCommonHelpers::isValidNickNameByRegExp(const QString &nickName)
{
	QRegularExpression rep(NICK_REGEXP);
	return rep.match(nickName).hasMatch();
}

QWidget* LoginCommonHelpers::setCurrentStackWidget(QStackedWidget *stackWidget, const QString &ojbName)
{
	if (stackWidget) {
		QWidget *widget = stackWidget->findChild<QWidget *>(ojbName);
		if (widget) {
			stackWidget->setCurrentWidget(widget);
		}
		return widget;
	}
	return nullptr;
}

QWidget *LoginCommonHelpers::getWidgetFromStackWidget(const QStackedWidget *stackWidget, const QString &ojbName)
{
	if (stackWidget) {
		return stackWidget->findChild<QWidget *>(ojbName);
	}
	return nullptr;
}

void LoginCommonHelpers::refreshStyle(QWidget *widget)
{
	assert(widget != nullptr);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

void LoginCommonHelpers::loginResultHandler(const QStackedWidget *stackWidget, bool isSuccess)
{
	if (!isSuccess) {
		return;
	}

	QDialog *dialog = nullptr;
	for (QWidget *parent = stackWidget->parentWidget(); parent && !dialog;) {
		dialog = dynamic_cast<QDialog *>(parent);
		parent = parent->parentWidget();
	}

	if (dialog) {
		dialog->done(QDialog::Accepted);
	}
}
