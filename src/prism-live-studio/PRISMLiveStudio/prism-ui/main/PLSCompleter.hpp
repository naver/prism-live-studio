#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QLabel>
#include <QStringList>
#include <QScrollArea>

//#include "PLSWidgetDpiAdapter.hpp"
//#include "PLSDpiHelper.h"
#include "utils-api.h"

class PLSCompleterPopupList : public QFrame {
	Q_OBJECT

public:
	explicit PLSCompleterPopupList(QWidget *toplevel, QLineEdit *lineEdit, const QStringList &completions);
	~PLSCompleterPopupList() override = default;

	bool isInCompleter(const QPoint &globalPos) const;

signals:
	void activated(const QString &text);

public slots:
	void showPopup(const QString &text);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QLineEdit *lineEdit = nullptr;
	QScrollArea *scrollArea = nullptr;
	QList<QLabel *> labels;
};

class PLSCompleter : public QObject {
	Q_OBJECT
	PLS_NEW_DELETE_FRIENDS

private:
	explicit PLSCompleter(QWidget *toplevel, QLineEdit *lineEdit, const QStringList &completions);
	~PLSCompleter() override;

public:
	static PLSCompleter *attachLineEdit(QWidget *toplevel, QLineEdit *lineEdit, const QStringList &completions);
	static void detachLineEdit(const QLineEdit *lineEdit);

signals:
	void activated(const QString &text);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QLineEdit *lineEdit = nullptr;
	PLSCompleterPopupList *popupList = nullptr;
	bool eatFocusOut = true;
};
