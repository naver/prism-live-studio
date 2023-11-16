#pragma once

#include <PLSAlertView.h>

class PLSMessageBox {
public:
	static PLSAlertView::Button question(QWidget *parent, const QString &title, const QString &text,
					     PLSAlertView::Buttons buttons = PLSAlertView::Buttons(PLSAlertView::Button::Yes | PLSAlertView::Button::No),
					     PLSAlertView::Button defaultButton = PLSAlertView::Button::NoButton);
	static PLSAlertView::Button question(QWidget *parent, const QString &title, const QString &textTitle, const QString &textContent,
					     PLSAlertView::Buttons buttons = PLSAlertView::Buttons(PLSAlertView::Button::Yes | PLSAlertView::Button::No),
					     PLSAlertView::Button defaultButton = PLSAlertView::Button::NoButton);
	static void information(QWidget *parent, const QString &title, const QString &text);
	static void warning(QWidget *parent, const QString &title, const QString &text, bool enableRichText = false);
	static void critical(QWidget *parent, const QString &title, const QString &text);
};
