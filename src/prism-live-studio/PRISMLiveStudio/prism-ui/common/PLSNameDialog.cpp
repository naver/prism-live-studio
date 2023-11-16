/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "PLSNameDialog.hpp"
#include "qt-wrappers.hpp"
#include "ui_PLSNameDialog.h"
#include "obs-app.hpp"
#include "libui.h"

#include <QPushButton>
#include <QStyle>
#include <QLineEdit>

using namespace std;

void PLSNameDialog::OnTextEditd(const QString &) const
{
	ui->userText->setProperty("status", "editing");
	style()->unpolish(ui->userText);
	style()->polish(ui->userText);
}

void PLSNameDialog::OnTextEditingFinished() const
{
	ui->userText->setProperty("status", "editingFinish");
	style()->unpolish(ui->userText);
	style()->polish(ui->userText);
}

void PLSNameDialog::showEvent(QShowEvent *event)
{
	PLSDialogView::showEvent(event);

	ui->userText->selectAll();
	ui->userText->setFocus();
	activateWindow();
}

bool PLSNameDialog::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == ui->userText && event->type() == QEvent::FocusIn) {
		OnTextEditd("");
		return false;
	}

	if (watcher == ui->userText && event->type() == QEvent::FocusOut) {
		OnTextEditingFinished();
		return false;
	}
	return PLSDialogView::eventFilter(watcher, event);
}

PLSNameDialog::PLSNameDialog(QWidget *parent, bool option) : PLSDialogView(parent)
{
	ui = std::make_unique<Ui::PLSNameDialog>();
	setupUi(ui);
	pls_add_css(this, {"PLSNameDialog"});

	if (option) {
		initSize(QSize(410, 278));
	} else {
		initSize(QSize(410, 243));
	}

	installEventFilter(CreateShortcutFilter(parent));
	ui->userText->installEventFilter(this);
	connect(ui->userText, &QLineEdit::textEdited, this, &PLSNameDialog::OnTextEditd);
	connect(ui->userText, &QLineEdit::editingFinished, this, &PLSNameDialog::OnTextEditingFinished);

	setResizeEnabled(false);

	ui->buttonBox->button(QDialogButtonBox::Cancel)->setObjectName("Cancel");
	ui->buttonBox->button(QDialogButtonBox::Ok)->setObjectName("Ok");

	connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &PLSNameDialog::accept);
	connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &PLSNameDialog::reject);
}

static bool IsWhitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

bool PLSNameDialog::AskForName(QWidget *parent, const QString &title, const QString &text, string &str, const QString &placeHolder, int maxSize)
{
	maxSize = qBound(1, maxSize, 100);

	PLSNameDialog dialog(parent);
	dialog.setWindowTitle(title);
	dialog.ui->optionWidget->setVisible(false);
	dialog.ui->titleLabel->setText(text);
	dialog.ui->userText->setMaxLength(maxSize);
	dialog.ui->userText->setText(placeHolder);

	bool accepted = (dialog.exec() == DialogCode::Accepted);
	if (accepted) {
		str = QT_TO_UTF8(dialog.ui->userText->text());

		while (str.size() && IsWhitespace(str.back()))
			str.erase(str.end() - 1);
		while (str.size() && IsWhitespace(str.front()))
			str.erase(str.begin());
	}

	return accepted;
}

bool PLSNameDialog::AskForNameWithOption(QWidget *parent, const QString &title, const QString &text, std::string &userTextInput, const QString &optionLabel, bool &optionChecked,
					 const QString &placeHolder)
{
	PLSNameDialog dialog(parent, true);
	dialog.setProperty("option", true);
	dialog.setWindowTitle(title);

	dialog.ui->titleLabel->setText(text);
	dialog.ui->userText->setMaxLength(170);
	dialog.ui->userText->setText(placeHolder);
	dialog.ui->optionCheckBox->setText(optionLabel);
	dialog.ui->optionCheckBox->setChecked(optionChecked);
	pls_flush_style_recursive(&dialog);

	bool accepted = (dialog.exec() == DialogCode::Accepted);
	if (accepted) {
		userTextInput = QT_TO_UTF8(dialog.ui->userText->text());

		while (userTextInput.size() && IsWhitespace(userTextInput.back()))
			userTextInput.erase(userTextInput.end() - 1);
		while (userTextInput.size() && IsWhitespace(userTextInput.front()))
			userTextInput.erase(userTextInput.begin());
	}

	return accepted;
}
