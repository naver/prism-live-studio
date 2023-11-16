#ifndef _PRISM_COMMON_LIBHDPI_EDIT_H
#define _PRISM_COMMON_LIBHDPI_EDIT_H

#include "libui-globals.h"
#include <qlineedit.h>
#include <qtextedit.h>
#include <qplaintextedit.h>
#include <QFocusEvent>

class LIBUI_API PLSLineEdit : public QLineEdit {
	Q_OBJECT

public:
	explicit PLSLineEdit(QWidget *parent = nullptr);
	~PLSLineEdit() override = default;
	void setText(const QString &text);
};

class LIBUI_API PLSTextEdit : public QTextEdit {
	Q_OBJECT
public:
	explicit PLSTextEdit(QWidget *parent = nullptr);
	~PLSTextEdit() override = default;
	void setText(const QString &text);
};
class LIBUI_API PLSPlainTextEdit : public QPlainTextEdit {
	Q_OBJECT
public:
	explicit PLSPlainTextEdit(QWidget *parent = nullptr);
	~PLSPlainTextEdit() override = default;
	void setPlainText(const QString &text);
};

#endif // _PRISM_COMMON_LIBHDPI_LABEL_H
