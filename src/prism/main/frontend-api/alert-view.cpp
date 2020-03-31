#include "alert-view.hpp"
#include "ui_PLSAlertView.h"

#include <QPushButton>
#include <QApplication>

#include <log.h>

#ifdef Q_OS_WINDOWS
#include <Windows.h>
#endif // Q_OS_WINDOWS

#define APPEND_SPACE_WIDTH 10
#define MESSAGE_LABEL_FIX_WIDTH 360

static PLSAlertView::Buttons getButtons(const QMap<PLSAlertView::Button, QString> &buttonMap)
{
	PLSAlertView::Buttons buttons = PLSAlertView::Button::NoButton;
	for (auto iter = buttonMap.begin(); iter != buttonMap.end(); ++iter) {
		buttons |= iter.key();
	}
	return buttons;
}
static int getButtonCount(PLSAlertView::Buttons buttons)
{
	int count = 0;
	for (int i = 0; i < 32; ++i) {
		if (buttons.testFlag(static_cast<PLSAlertView::Button>(1 << i))) {
			++count;
		}
	}
	return count;
}
static void setButtonWidth(QDialogButtonBox *buttonBox, int minWidth, int maxWidth)
{
	int buttonCount = 0;
	QPushButton *buttons[32] = {};

	int width = minWidth;
	for (int i = 0; i < 32; ++i) {
		if (QPushButton *button = buttonBox->button(static_cast<PLSAlertView::Button>(1 << i)); button) {
			buttons[buttonCount++] = button;
			width = qMax(width, qMin(button->width(), maxWidth));
		}
	}

	for (int i = 0; i < buttonCount; ++i) {
		buttons[i]->setFixedWidth(width);
		buttons[i]->setAutoDefault(false);
		buttons[i]->setDefault(false);
	}
}

// class PLSAlertView Implements
PLSAlertView::PLSAlertView(QWidget *parent, Icon icon_, const QString &title, const QString &message, const QString &checkbox, const Buttons &buttons, Button defaultButton, const QSize &sugsize_)
	: PLSDialogView(parent), ui(new Ui::PLSAlertView), icon(Icon::NoIcon), checkBox(nullptr), sugsize(sugsize_)
{
	PLS_INFO("alert-view", "UI: [ALERT] %s", message.toUtf8().constData());

	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setEscapeCloseEnabled(true);
	setResizeEnabled(false);

	resize(410, 188);
	setFixedWidth(410);
	QWidget *content = this->content();
	ui->setupUi(content);

	setWindowTitle(title);

	if (!title.isEmpty()) {
		setHasCaption(true);
	} else {
		setHasCaption(false);
		setHasHLine(false);
	}

	setIcon(icon_);

	ui->nameLabel->hide();
	ui->message->setText(message);
	ui->buttonBox->setStandardButtons(buttons);
	connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &PLSAlertView::onButtonClicked);

	if (!checkbox.isEmpty()) {
		checkBox = new QCheckBox(checkbox, this);
		ui->contentLayout->addWidget(checkBox);
	} else {
		checkBox = nullptr;
	}

	switch (getButtonCount(buttons)) {
	case 1:
		setButtonWidth(ui->buttonBox, 140, 180);
		break;
	case 2:
		setButtonWidth(ui->buttonBox, 140, 180);
		break;
	case 3:
	default:
		setButtonWidth(ui->buttonBox, 117, 117);
		break;
	}

	adjustSize();
	if (QPushButton *button = ui->buttonBox->button(defaultButton)) {
		button->setDefault(true);
	}

#define TranslateButtonText(name, text)                                                        \
	do {                                                                                   \
		if (QPushButton *button = ui->buttonBox->button(PLSAlertView::Button::name)) { \
			button->setText(tr(#text));                                            \
		}                                                                              \
	} while (false)
	TranslateButtonText(Ok, OK);
	TranslateButtonText(Open, Open);
	TranslateButtonText(Save, Save);
	TranslateButtonText(Cancel, Cancel);
	TranslateButtonText(Close, Close);
	TranslateButtonText(Discard, Discard);
	TranslateButtonText(Apply, Apply);
	TranslateButtonText(Reset, Reset);
	TranslateButtonText(Yes, Yes);
	TranslateButtonText(No, No);
	TranslateButtonText(Abort, Abort);
	TranslateButtonText(Retry, Retry);
	TranslateButtonText(Ignore, Ignore);
	TranslateButtonText(RestoreDefaults, Defaults);
#undef TranslateButtonText
}

PLSAlertView::PLSAlertView(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
			   const QSize &sugsize)
	: PLSAlertView(parent, icon, title, message, checkbox, getButtons(buttons), defaultButton, sugsize)
{
	for (auto iter = buttons.begin(); iter != buttons.end(); ++iter) {
		if (auto button = ui->buttonBox->button(iter.key())) {
			button->setText(iter.value());
		}
	}
}

PLSAlertView::PLSAlertView(Icon icon, const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, PLSAlertView::Buttons buttons,
			   Button defaultButton /*= Button::NoButton*/, const QSize &sugsize /*= QSize()*/)
	: PLSAlertView(parent, icon, title, messageTitle, QString(), buttons, defaultButton, sugsize)
{
	ui->message->setFixedWidth(MESSAGE_LABEL_FIX_WIDTH);
	ui->message->setText(GetNameElideString(messageTitle, ui->message));

	QString name = GetNameElideString(messageContent, ui->nameLabel) + "?";
	ui->nameLabel->show();
	ui->nameLabel->setText(name);
	ui->nameLabel->setAlignment(Qt::AlignCenter);
}

QString PLSAlertView::GetNameElideString(const QString &name, QWidget *widget)
{
	if (widget) {
		QFontMetrics fontWidth(widget->font());
		if (fontWidth.width(name) > widget->width() - APPEND_SPACE_WIDTH)
			return fontWidth.elidedText(name, Qt::ElideRight, widget->width() - APPEND_SPACE_WIDTH);
	}

	return name;
}

PLSAlertView::~PLSAlertView()
{
	delete ui;
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &message, const Buttons &buttons, Button defaultButton, const QSize &size)
{
	return open(parent, Icon::Information, QString(), message, buttons, defaultButton, size);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const QSize &size)
{
	return open(parent, Icon::Information, QString(), message, buttons, defaultButton, size);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &title, const QString &message, const Buttons &buttons, Button defaultButton, const QSize &size)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, size);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const QSize &size)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, size);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const Buttons &buttons, Button defaultButton, const QSize &size)
{
	PLSAlertView alertView(parent, icon, title, message, QString(), buttons, defaultButton, size);
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const QSize &size)
{
	PLSAlertView alertView(parent, icon, title, message, QString(), buttons, defaultButton, size);
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Result PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const Buttons &buttons, Button defaultButton,
					const QSize &size)
{
	PLSAlertView alertView(parent, icon, title, message, checkbox, buttons, defaultButton, size);
	return {static_cast<Button>(alertView.exec()), alertView.isChecked()};
}

PLSAlertView::Result PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					const QSize &size)
{
	PLSAlertView alertView(parent, icon, title, message, checkbox, buttons, defaultButton, size);
	return {static_cast<Button>(alertView.exec()), alertView.isChecked()};
}

PLSAlertView::Button PLSAlertView::open(Icon icon, const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, Buttons buttons,
					Button defaultButton /*= Button::NoButton*/, const QSize &sugsize /*= QSize()*/)
{
	PLSAlertView alertView(icon, title, messageTitle, messageContent, parent, buttons, defaultButton, sugsize);
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Button PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Information, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Information, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Question, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Question, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Question, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Question, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::question(const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, PLSAlertView::Buttons buttons,
					    Button defaultButton /*= Button::NoButton*/)
{
	return open(Icon::Question, title, messageTitle, messageContent, parent, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Warning, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Warning, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Warning, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Warning, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Critical, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Button PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Critical, title, message, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton)
{
	return open(parent, Icon::Critical, title, message, checkbox, buttons, defaultButton, QSize());
}

PLSAlertView::Result PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton)
{
	return open(parent, Icon::Critical, title, message, checkbox, buttons, defaultButton, QSize());
}

bool PLSAlertView::isChecked() const
{
	return checkBox ? checkBox->isChecked() : false;
}

Qt::TextFormat PLSAlertView::getTextFormat() const
{
	return ui->message->textFormat();
}

void PLSAlertView::setTextFormat(Qt::TextFormat format)
{
	ui->message->setTextFormat(format);
}

PLSAlertView::Icon PLSAlertView::getIcon() const
{
	return icon;
}

void PLSAlertView::setIcon(Icon icon)
{
	this->icon = icon;

	QString iconPath;
	switch (icon) {
	case Icon::Information:
		break;
	case Icon::Question:
		break;
	case Icon::Warning:
		break;
	case Icon::Critical:
		break;
	}

	if (!iconPath.isEmpty()) {
		QPixmap pixmap(iconPath);
		ui->icon->setPixmap(pixmap);
		ui->icon->setFixedSize(pixmap.width(), pixmap.height());
		ui->icon->show();
	} else {
		ui->icon->hide();
	}
}

void PLSAlertView::onButtonClicked(QAbstractButton *button)
{
	done(ui->buttonBox->standardButton(button));
}

void PLSAlertView::showEvent(QShowEvent *event)
{
	adjustSize();
	setHeightForFixedWidth();
	moveToCenter();
	PLSDialogView::showEvent(event);
}
