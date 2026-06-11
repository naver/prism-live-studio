#include "PLSFacebookGameLineEdit.h"
#include "ui_PLSFacebookGameLineEdit.h"
#include "PLSLoadingComboxMenu.h"
#include <QFocusEvent>
#include "utils-api.h"

PLSFacebookGameLineEdit::PLSFacebookGameLineEdit(QWidget *parent) : PLSLineEdit(parent)
{
	ui = pls_new<Ui::PLSFacebookGameLineEdit>();
	ui->setupUi(this);

	//set the clear button focus policy
	bool empty = this->text().isEmpty();
	ui->pushButtonClear->setCursor(Qt::ArrowCursor);
	ui->pushButtonClear->setFocusPolicy(Qt::NoFocus);
	ui->searchButton->setFocusPolicy(Qt::NoFocus);
	ui->pushButtonClear->setHidden(empty);

	//set connect method
	connect(this, &QLineEdit::textChanged, this, [this](const QString &text) {
		if (text.isEmpty()) {
			ui->pushButtonClear->hide();
			this->clear();
			emit clearText();
		} else {
			ui->pushButtonClear->show();
			emit searchKeyword(text);
		}
	});
	connect(ui->pushButtonClear, &QPushButton::clicked, this, [this]() { this->setText(QString()); });
}

PLSFacebookGameLineEdit::~PLSFacebookGameLineEdit()
{
	pls_delete(ui);
}

void PLSFacebookGameLineEdit::focusInEvent(QFocusEvent *event)
{
	ui->searchButton->setStyleSheet("image: url(:/channels/resource/images/navershopping/btn-search-on-normal.svg);");
	QLineEdit::focusInEvent(event);
}

void PLSFacebookGameLineEdit::focusOutEvent(QFocusEvent *event)
{
	ui->searchButton->setStyleSheet("image: url(:/channels/resource/images/navershopping/btn-search-normal.svg);");
	QLineEdit::focusOutEvent(event);
}
