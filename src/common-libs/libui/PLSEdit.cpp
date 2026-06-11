#include "PLSEdit.h"
#include "libui.h"

template<typename T, typename TextCb> static void updatePlaceholderColor(T *widget, TextCb textCb)
{
	auto length = textCb(widget).length();
	if (length == 0) {
		widget->setStyleSheet("* {color: #666666;}");
	} else {
		widget->setStyleSheet(QString());
	}
	pls_flush_style(widget);
}

PLSLineEdit::PLSLineEdit(QWidget *parent) : QLineEdit(parent)
{
	updatePlaceholderColor(this, [](auto w) { return w->text(); });
	connect(this, &QLineEdit::textChanged, this, [this]() { updatePlaceholderColor(this, [](auto w) { return w->text(); }); });
}

void PLSLineEdit::setText(const QString &text)
{
	QLineEdit::setText(text);
	if (!text.isEmpty()) {
		this->setStyleSheet(QString());
		pls_flush_style(this);
	}
}

PLSTextEdit::PLSTextEdit(QWidget *parent)
{
	updatePlaceholderColor(this, [](auto w) { return w->toPlainText(); });
	connect(this, &QTextEdit::textChanged, this, [this]() { updatePlaceholderColor(this, [](auto w) { return w->toPlainText(); }); });
}

void PLSTextEdit::setText(const QString &text)
{
	QTextEdit::setText(text);
	if (!text.isEmpty()) {
		this->setStyleSheet(QString());
		pls_flush_style(this);
	}
}

PLSPlainTextEdit::PLSPlainTextEdit(QWidget *parent)
{
	updatePlaceholderColor(this, [](auto w) { return w->toPlainText(); });
	connect(this, &QPlainTextEdit::textChanged, this, [this]() { updatePlaceholderColor(this, [](auto w) { return w->toPlainText(); }); });
}

void PLSPlainTextEdit::setPlainText(const QString &text)
{
	QPlainTextEdit::setPlainText(text);
	if (!text.isEmpty()) {
		this->setStyleSheet(QString());
		pls_flush_style(this);
	}
}
