#include "PLSMessageBox.h"

PLSAlertView::Button PLSMessageBox::question(QWidget *parent, const QString &title, const QString &text, PLSAlertView::Buttons buttons, PLSAlertView::Button defaultButton)
{
	QMap<PLSAlertView::Button, QString> buttonMap;

#define TRANSLATE_BUTTON(x)                                                         \
	do {                                                                        \
		if (buttons & PLSAlertView::Button::x) {                            \
			buttonMap.insert(PLSAlertView::Button::x, QObject::tr(#x)); \
		}                                                                   \
	} while (false)

	TRANSLATE_BUTTON(Open);
	TRANSLATE_BUTTON(Save);
	TRANSLATE_BUTTON(Cancel);
	TRANSLATE_BUTTON(Close);
	TRANSLATE_BUTTON(Discard);
	TRANSLATE_BUTTON(Apply);
	TRANSLATE_BUTTON(Reset);
	TRANSLATE_BUTTON(Yes);
	TRANSLATE_BUTTON(No);
	TRANSLATE_BUTTON(No);
	TRANSLATE_BUTTON(Abort);
	TRANSLATE_BUTTON(Retry);
	TRANSLATE_BUTTON(Ignore);

#undef TRANSLATE_BUTTON

	if (buttons & PLSAlertView::Button::Ok) {
		buttonMap.insert(PLSAlertView::Button::Ok, QObject::tr("OK"));
	}
	return PLSAlertView::question(parent, title, text, buttonMap, defaultButton);
}

PLSAlertView::Button PLSMessageBox::question(QWidget *parent, const QString &title, const QString &textTitle, const QString &textContent, PLSAlertView::Buttons buttons,
					     PLSAlertView::Button defaultButton)
{
	return PLSAlertView::question(title, textTitle, textContent, parent, buttons, defaultButton);
}

void PLSMessageBox::information(QWidget *parent, const QString &title, const QString &text)
{
	PLSAlertView::information(parent, title, text, {{PLSAlertView::Button::Ok, QObject::tr("OK")}}, PLSAlertView::Button::Ok);
}

void PLSMessageBox::warning(QWidget *parent, const QString &title, const QString &text, bool enableRichText)
{
	PLSAlertView alertView(parent, PLSAlertView::Icon::Warning, title, text, QString(), {{PLSAlertView::Button::Ok, QObject::tr("OK")}}, PLSAlertView::Button::Ok);
	if (enableRichText) {
		alertView.setTextFormat(Qt::RichText);
	}
	alertView.exec();
}

void PLSMessageBox::critical(QWidget *parent, const QString &title, const QString &text)
{
	PLSAlertView::critical(parent, title, text, {{PLSAlertView::Button::Ok, QObject::tr("OK")}}, PLSAlertView::Button::Ok);
}
