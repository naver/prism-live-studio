#include "PLSAlertView.h"
#include "ui_PLSAlertView.h"

#include <QPushButton>
#include <QApplication>
#include <QHBoxLayout>
#include <QList>
#include <QPointer>
#include <QTimeZone>
#include <QMouseEvent>
#include "countdown.h"

#include <libutils-api.h>
#include <liblog.h>
#include "PLSRadioButton.h"

#ifdef Q_OS_WINDOWS
#include <Windows.h>
#endif // Q_OS_WINDOWS

const int APPEND_SPACE_WIDTH = 25;
const int MESSAGE_LABEL_FIX_WIDTH = 360;
const QString InitCss = "PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Ok\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Save\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"SaveAll\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Open\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Yes\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"YesToAll\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"No\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"NoToAll\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Abort\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Retry\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Ignore\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Close\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Cancel\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Discard\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Help\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Apply\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"Reset\"],"
			"PLSAlertView #widget > #buttonBox QPushButton[useFor=\"RestoreDefaults\"] {"
			"	min-width: %1px;"
			"	max-width: %1px;"
			"	margin: 0;"
			"	padding: 0;"
			"}";
const QString AlertKeyDisableEsc = "disableEsc";
const QString AlertKeyDisableAltF4 = "disableAltF4";

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
static void setDefaultButton(const PLSDialogButtonBox *buttonBox, PLSAlertView::Button defaultButton)
{
	for (int i = 0; i < 32; ++i) {
		if (QPushButton *button = buttonBox->button(static_cast<PLSAlertView::Button>(1 << i)); button) {
			button->setAutoDefault(false);
			button->setDefault(false);
		}
	}

	if (QPushButton *button = buttonBox->button(defaultButton)) {
		button->setDefault(true);
	}
}
static void resetButtonWidth(const PLSDialogButtonBox *buttonBox, int minWidth, int maxWidth)
{
	for (int i = 0; i < 32; ++i) {
		if (auto button = buttonBox->button(static_cast<PLSAlertView::Button>(1 << i)); button) {
			button->setMinimumWidth(minWidth);
			button->setMaximumWidth(maxWidth);
			button->resize(button->fontMetrics().size(Qt::TextShowMnemonic, button->text()));
		}
	}
}
static int calcButtonWidth(const PLSDialogButtonBox *buttonBox, int minWidth, int maxWidth)
{
	int width = minWidth;
	for (int i = 0; i < 32; ++i) {
		if (auto button = buttonBox->button(static_cast<PLSAlertView::Button>(1 << i)); button) {
			width = qMax(width, qMin(button->width(), maxWidth));
		}
	}
	return width;
}
static int calcButtonWidth(const PLSDialogButtonBox *buttonBox)
{
	resetButtonWidth(buttonBox, 0, QWIDGETSIZE_MAX);

	switch (getButtonCount(buttonBox->standardButtons())) {
	case 1:
		return calcButtonWidth(buttonBox, 140, 180);
	case 2:
		return calcButtonWidth(buttonBox, 140, 180);
	default:
		return calcButtonWidth(buttonBox, 117, 117);
	}
}

static QString checkQuoteComplete(QString before, QString after)
{
	if (!before.endsWith("'")) {
		return after;
	}
	if (!after.endsWith("'")) {
		return after.append("'");
	}
	return after;
}

// class PLSAlertView Implements
PLSAlertView::PLSAlertView(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const Buttons &buttons, Button defaultButton,
			   const QMap<QString, QVariant> &properties)
	: PLSDialogView(parent), m_savedProperties(properties)
{
	ui = pls_new<Ui::PLSAlertView>();

	setFixedWidth(410);
	setEscapeCloseEnabled(!m_savedProperties.value(AlertKeyDisableEsc, QVariant(false)).toBool());
	setResizeEnabled(false);

	setupUi(ui);
	setWindowTitle(title);
	initSize(410, 190);

	if (!title.isEmpty()) {
		setHasCaption(true);
	} else {
		setHasCaption(false);
		setHasHLine(false);
	}

	setHasCloseButton(false);
	setIcon(icon);

	ui->nameLabel->hide();
	ui->message->setText(message);
	ui->buttonBox->setStandardButtons(buttons);
	connect(ui->buttonBox, &PLSDialogButtonBox::clicked, this, &PLSAlertView::onButtonClicked);

	ui->buttonBox->setExtraLog(QString("\t(content: %1)").arg(message));

	if (!checkbox.isEmpty()) {
		QWidget *widget = pls_new<QWidget>(this);
		QHBoxLayout *hLayout = pls_new<QHBoxLayout>(widget);
		hLayout->setContentsMargins(0, 0, 0, 0);
		hLayout->setAlignment(Qt::AlignCenter);
		m_checkBox = pls_new<PLSCheckBox>(checkbox, this);
		hLayout->addWidget(m_checkBox);
		ui->contentLayout->addSpacing(24);
		ui->contentLayout->addWidget(widget);
		ui->contentLayout->addSpacing(9);
		ui->horizontalLayout_2->setContentsMargins(25, 30, 25, 20);
	} else {
		m_checkBox = nullptr;
		ui->horizontalLayout_2->setContentsMargins(25, 30, 25, 30);
	}
	setDefaultButton(ui->buttonBox, defaultButton);

#define TranslateButtonText(name, text)                                                        \
	do {                                                                                   \
		if (QPushButton *button = ui->buttonBox->button(PLSAlertView::Button::name)) { \
			++m_btnCount;                                                          \
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

	int width = calcButtonWidth(ui->buttonBox);
	if (m_savedProperties.contains("minBtnWidth")) {
		int configMinWidth = m_savedProperties["minBtnWidth"].toUInt();
		width = qMax(width, configMinWidth);
	}
	ui->buttonBox->setStyleSheet(InitCss.arg(width));

	//#PRISM_PC-1375 windows need add a item after linked label, so can show mouse
	QWidget *placeholderMouse = new QWidget(this);
	placeholderMouse->hide();
	ui->contentLayout->addWidget(placeholderMouse);
}

PLSAlertView::PLSAlertView(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
			   const QMap<QString, QVariant> &properties)
	: PLSAlertView(parent, icon, title, message, checkbox, getButtons(buttons), defaultButton, properties)
{
	for (auto iter = buttons.begin(); iter != buttons.end(); ++iter) {
		if (auto button = ui->buttonBox->button(iter.key())) {
			button->setText(iter.value());
		}
	}
}

PLSAlertView::PLSAlertView(Icon icon, const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, PLSAlertView::Buttons buttons, Button defaultButton,
			   const QMap<QString, QVariant> &properties)
	: PLSAlertView(parent, icon, title, !messageTitle.isEmpty() ? messageTitle.left(1) : QString(), QString(), buttons, defaultButton, properties)
{
	ui->message->setFixedWidth(MESSAGE_LABEL_FIX_WIDTH);
	ui->message->setText(checkQuoteComplete(messageTitle, GetNameElideString(messageTitle, ui->message)));
	ui->nameLabel->setText(checkQuoteComplete(messageContent, GetNameElideString(messageContent, ui->nameLabel)) + "?");
	ui->nameLabel->setAlignment(Qt::AlignCenter);
	ui->nameLabel->show();
}

QString PLSAlertView::GetNameElideString(const QString &name, const QWidget *widget) const
{
	if (widget) {
		QFontMetrics fontWidth(widget->font());
		if (fontWidth.horizontalAdvance(name) > widget->width() - APPEND_SPACE_WIDTH * 2)
			return fontWidth.elidedText(name, Qt::ElideRight, widget->width() - APPEND_SPACE_WIDTH * 2);
	}

	return name;
}

PLSAlertView::~PLSAlertView()
{
	pls_delete(ui, nullptr);
	stopDelayAutoClick();
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &message, const Buttons &buttons, Button defaultButton, const std::optional<int> &timeout,
					const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, QString(), message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const std::optional<int> &timeout,
					const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, QString(), message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &title, const QString &message, const Buttons &buttons, Button defaultButton, const std::optional<int> &timeout,
					const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const std::optional<int> &timeout,
					const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const Buttons &buttons, Button defaultButton, const std::optional<int> &timeout,
					const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, icon, title, message, QString(), buttons, defaultButton, properties);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Button PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton,
					const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, icon, title, message, QString(), buttons, defaultButton, properties);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Result PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const Buttons &buttons, Button defaultButton,
					const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, icon, title, message, checkbox, buttons, defaultButton, properties);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return {static_cast<Button>(alertView.exec()), alertView.isChecked()};
}

PLSAlertView::Result PLSAlertView::open(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, icon, title, message, checkbox, buttons, defaultButton, properties);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return {static_cast<Button>(alertView.exec()), alertView.isChecked()};
}

PLSAlertView::Button PLSAlertView::open(Icon icon, const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, Buttons buttons, Button defaultButton,
					const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(icon, title, messageTitle, messageContent, parent, buttons, defaultButton);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return static_cast<Button>(alertView.exec());
}

PLSAlertView::Button PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton, const std::optional<int> &timeout,
					       const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton,
					       const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton,
					       const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::information(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					       const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Information, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton, const std::optional<int> &timeout,
					    const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Question, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Question, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Question, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::question(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Question, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::question(const QString &title, const QString &messageTitle, const QString &messageContent, QWidget *parent, PLSAlertView::Buttons buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(Icon::Question, title, messageTitle, messageContent, parent, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton, const std::optional<int> &timeout,
					   const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Warning, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton, const std::optional<int> &timeout,
					   const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Warning, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton,
					   const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Warning, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::warning(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					   const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Warning, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, Buttons buttons, Button defaultButton, const std::optional<int> &timeout,
					    const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Critical, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Button PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Critical, title, message, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, Buttons buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Critical, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

PLSAlertView::Result PLSAlertView::critical(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons, Button defaultButton,
					    const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	return open(parent, Icon::Critical, title, message, checkbox, buttons, defaultButton, timeout, properties);
}

class ContactUsButton : public QFrame {
	Q_OBJECT
	bool hovered = false;
	bool pressed = false;

public:
	ContactUsButton(const QString &buttonText, QWidget *parent = nullptr) : QFrame(parent)
	{
		setObjectName("errMsgContactUsButton");
		setProperty("showHandCursor", true);
		setProperty("ui-step.customButton", true); // must
		//setProperty("ui-step.signalName", "clicked"); // optional, default: clicked
		setMouseTracking(true);

		QLabel *icon = pls_new<QLabel>(this);
		icon->setObjectName("errMsgContactUsButtonIcon");
		icon->setAttribute(Qt::WA_TransparentForMouseEvents);

		QLabel *text = pls_new<QLabel>(buttonText, this);
		text->setObjectName("errMsgContactUsButtonText");
		text->setAttribute(Qt::WA_TransparentForMouseEvents);

		QHBoxLayout *layout = pls_new<QHBoxLayout>(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(5);
		layout->addWidget(text);

		QWidget *iconContainer = pls_new<QWidget>();
		iconContainer->setObjectName("errMsgContactUsButtonIconContainer");
		iconContainer->setAttribute(Qt::WA_TransparentForMouseEvents);
		QHBoxLayout *layout2 = pls_new<QHBoxLayout>(iconContainer);
		layout2->setContentsMargins(0, 1, 0, 0);
		layout2->addWidget(icon);
		layout->addWidget(iconContainer, 0, Qt::AlignVCenter);
	}
	~ContactUsButton() override = default;

signals:
	void clicked();

private:
	void setState(const char *name, bool &state, bool value)
	{
		if (state != value) {
			state = value;
			pls_flush_style_recursive(this, name, value);
		}
	}

protected:
	bool event(QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::Enter:
			setState("hovered", hovered, true);
			break;
		case QEvent::Leave:
			setState("hovered", hovered, false);
			break;
		case QEvent::MouseButtonPress:
			setState("pressed", pressed, true);
			break;
		case QEvent::MouseButtonRelease:
			setState("pressed", pressed, false);
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				emit clicked();
			}
			break;
		case QEvent::MouseMove:
			setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
			break;
		default:
			break;
		}
		return QFrame::event(event);
	}
};

static void initErrorMessage(PLSAlertView *alertView, QHBoxLayout *horizontalLayout, QVBoxLayout *contentLayout, const QString &title, const QString &message, const QString &errorCode,
			     const QString &userId,
			     const std::function<void(const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QString &time)> &contactUsCb, int btnCount)
{
	auto margins = horizontalLayout->contentsMargins();
	margins.setBottom(20);
	horizontalLayout->setContentsMargins(margins);

	contentLayout->addSpacing(19);

	QFrame *errorContainer = pls_new<QFrame>();
	errorContainer->setObjectName("errMsgErrorContainer");
	contentLayout->addWidget(errorContainer);

	QVBoxLayout *errorLayout = pls_new<QVBoxLayout>(errorContainer);
	errorLayout->setContentsMargins(20, 23, 20, 21);
	errorLayout->setSpacing(0);
	errorLayout->setSizeConstraint(QLayout::SetMinimumSize);

	if (!errorCode.isEmpty()) {
		QLabel *errorCodeLabel = pls_new<QLabel>(QObject::tr("Alert.ErrorMessage.ErrorCode").arg(errorCode));
		errorCodeLabel->setObjectName("errMsgErrorCodeLabel");
		errorCodeLabel->setAlignment(Qt::AlignCenter);
		errorLayout->addWidget(errorCodeLabel);

		errorLayout->addSpacing(3);
	}

	if (!userId.isEmpty()) {
		QLabel *userIdLabel = pls_new<QLabel>(QObject::tr("Alert.ErrorMessage.UserId").arg(userId));
		userIdLabel->setObjectName("errMsgUserIdLabel");
		userIdLabel->setAlignment(Qt::AlignCenter);
		errorLayout->addWidget(userIdLabel);

		errorLayout->addSpacing(2);
	}

	QDateTime currentTime = QDateTime::currentDateTime();
	currentTime.setTimeZone(QTimeZone::systemTimeZone());
	QString time = currentTime.toString(Qt::ISODate);
	QLabel *currentTimeLabel = pls_new<QLabel>(time);
	currentTimeLabel->setObjectName("errMsgCurrentTimeLabel");
	currentTimeLabel->setAlignment(Qt::AlignCenter);
	errorLayout->addWidget(currentTimeLabel);

	errorLayout->addSpacing(10);

	QLabel *guideLabel = pls_new<QLabel>(QObject::tr("Alert.ErrorMessage.Guide"));
	guideLabel->setObjectName("errMsgGuideLabel");
	guideLabel->setAlignment(Qt::AlignCenter);
	guideLabel->setWordWrap(true);
	errorLayout->addWidget(guideLabel);

	errorLayout->addSpacing(16);

	ContactUsButton *contactUsButton = pls_new<ContactUsButton>(QObject::tr("Alert.ErrorMessage.ContactUs"));
	errorLayout->addWidget(contactUsButton, 0, Qt::AlignCenter);
	QObject::connect(contactUsButton, &ContactUsButton::clicked, [alertView, title, message, errorCode, userId, time, contactUsCb, btnCount]() {
		PLS_UI_STEP("alert-view", "Error Message's Contact Us Button", ACTION_CLICK);
		if (pls_object_is_valid(alertView) && (btnCount != 2)) {
			alertView->done(PLSAlertView::Button::NoButton);
		}
		pls_async_call_mt([title, message, errorCode, userId, time, contactUsCb]() {
			pls_invoke_safe(contactUsCb, title, message, errorCode, userId, time); //
		});
	});
}

PLSAlertView::Button
PLSAlertView::errorMessage(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId,
			   const std::function<void(const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QString &time)> &contactUsCb, Buttons buttons,
			   Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, Icon::Warning, title, message, QString(), buttons, defaultButton, properties);
	initErrorMessage(&alertView, alertView.ui->horizontalLayout_2, alertView.ui->contentLayout, title, message, errorCode, userId, contactUsCb, alertView.m_btnCount);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return static_cast<Button>(alertView.exec());
}
PLSAlertView::Button
PLSAlertView::errorMessage(QWidget *parent, const QString &title, const QString &message, const QString &errorCode, const QString &userId,
			   const std::function<void(const QString &title, const QString &message, const QString &errorCode, const QString &userId, const QString &time)> &contactUsCb,
			   const QMap<Button, QString> &buttons, Button defaultButton, const std::optional<int> &timeout, const QMap<QString, QVariant> &properties)
{
	PLSAlertView alertView(parent, Icon::Warning, title, message, QString(), buttons, defaultButton, properties);
	initErrorMessage(&alertView, alertView.ui->horizontalLayout_2, alertView.ui->contentLayout, title, message, errorCode, userId, contactUsCb, alertView.m_btnCount);
	alertView.delayAutoClick(timeout, defaultButton);
	pls_for_each(properties, [&alertView](const QString &key, const QVariant &value) { alertView.setProperty(key.toUtf8().constData(), value); });
	return static_cast<Button>(alertView.exec());
}

bool PLSAlertView::isChecked() const
{
	return m_checkBox ? m_checkBox->isChecked() : false;
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
	return m_icon;
}

void PLSAlertView::setIcon(Icon icon)
{
	m_icon = icon;

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
	default:
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

void PLSAlertView::delayAutoClick(const std::optional<int> &timeout /* milliseconds */, Button button)
{
	stopDelayAutoClick();

	if (timeout.has_value()) {
		m_delayAutoClickTimer = pls_new<QTimer>();
		connect(m_delayAutoClickTimer, &QTimer::timeout, this, [this, timeout, button]() {
			pls_async_call_mt(this, [this, timeout, button]() {
				PLS_INFO("alert-view", "auto close alert view after %f seconds", timeout.value() / 1000.0);
				done(button);
			});
		});
		m_delayAutoClickTimer->setSingleShot(true);
		m_delayAutoClickTimer->start(timeout.value());
	}
}

void PLSAlertView::stopDelayAutoClick()
{
	if (m_delayAutoClickTimer) {
		m_delayAutoClickTimer->stop();
		pls_delete(m_delayAutoClickTimer, nullptr);
	}
}

void PLSAlertView::onButtonClicked(QAbstractButton *button)
{
	done(ui->buttonBox->standardButton(button));
}

void PLSAlertView::showEvent(QShowEvent *event)
{
#if defined(Q_OS_WIN)
	m_needCorrectedHeight = true;
	m_needUpdatePosWhenCorrectedHeight = false;
#endif

	adjustSize();

#if defined(Q_OS_WIN)
	m_needCorrectedHeight = true;
	m_needUpdatePosWhenCorrectedHeight = false;
	resize(size() - QSize(0, titleBarHeight()));
#endif

	PLS_LOGEX(PLS_LOG_INFO, "alert-view", {{"alert-msg", ui->message->text().toUtf8().constData()}}, "UI: [ALERT] %s%s", ui->message->text().toUtf8().constData(),
		  ui->nameLabel->text().toUtf8().constData());
	PLSDialogView::showEvent(event);
}
void PLSAlertView::closeEvent(QCloseEvent *event)
{
	// ALT+F4
#if defined(Q_OS_WINDOWS)
	if ((GetAsyncKeyState(VK_MENU) < 0) && (GetAsyncKeyState(VK_F4) < 0)) {
		if (m_savedProperties.value(AlertKeyDisableAltF4, QVariant(false)).toBool()) {
			event->ignore();
			return;
		}
	}
#endif
	PLSDialogView::closeEvent(event);
}

void PLSAlertView::nativeResizeEvent(const QSize &size, const QSize &nativeSize)
{
	PLSDialogView::nativeResizeEvent(size, nativeSize);

#if defined(Q_OS_WIN)
	if ((size.height() % 2) != 0) {
		pls_async_call(this, [this, size]() {
			if (m_needCorrectedHeight) {
				m_needCorrectedHeight = false;
				m_needUpdatePosWhenCorrectedHeight = true;
				resize(QSize(size.width(), size.height() + 1));
			}
		});
	} else if (auto parent = pls_get_toplevel_view(parentWidget()); parent && m_needUpdatePosWhenCorrectedHeight) {
		m_needUpdatePosWhenCorrectedHeight = false;
		auto g = parent->geometry();
		QPoint pos(g.x() + (g.width() - size.width()) / 2, g.y() + (g.height() - size.height()) / 2);
		pls_async_call(this, [this, pos]() { move(pos); });
	} else {
		m_needCorrectedHeight = false;
		m_needUpdatePosWhenCorrectedHeight = false;
	}
#endif
}

//for countdown
PLSAlertView::Result PLSAlertView::openWithCountDownView(QWidget *parent, Icon icon, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons,
							 Button defaultButton, const quint64 &timeout, int buttonBoxWidth)
{
	PLSAlertView alertView(parent, icon, title, message, checkbox, buttons, defaultButton);
	auto timeLabel = pls_new<QLabel>(&alertView);
	timeLabel->setObjectName("countdownLabel");

	timeLabel->setBaseSize(QSize(44, 44));
	alertView.ui->horizontalLayout_2->setContentsMargins(25, 24, 25, 30);
	alertView.ui->contentLayout->insertWidget(0, timeLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
	alertView.delayAutoClick(static_cast<qint32>(timeout + 1000), defaultButton);

	CountDownLabel::start(timeLabel, QFont("Segoe UI", 14), timeout);
	alertView.ui->buttonBox->setStyleSheet(InitCss.arg(buttonBoxWidth));

	return {static_cast<Button>(alertView.exec()), alertView.isChecked()};
}

PLSAlertView::Result PLSAlertView::questionWithCountdownView(QWidget *parent, const QString &title, const QString &message, const QString &checkbox, const QMap<Button, QString> &buttons,
							     Button defaultButton, const quint64 &timeout, int buttonBoxWidth)
{
	return openWithCountDownView(parent, Icon::Question, title, message, checkbox, buttons, defaultButton, timeout);
}

PLSAlertView::Button PLSAlertView::dualOutputApplyResolutionWarn(QWidget *parent, const QString &title, const QString &message, const QMap<Button, QString> &buttons, const QString &hRadioMsg,
								 const QString &vRadioMsg, bool &selectVRadio, Button defaultButton)
{
	PLSAlertView alertView(parent, Icon::Warning, title, message, QString(), buttons, defaultButton);
	alertView.ui->horizontalLayout_2->setContentsMargins(25, 28, 25, 30);
	auto layout = pls_new<QVBoxLayout>();
	layout->setSpacing(10);
	layout->setContentsMargins(0, 20, 0, 0);
	auto hRadio = pls_new<PLSRadioButton>(hRadioMsg, &alertView);
	auto vRadio = pls_new<PLSRadioButton>(vRadioMsg, &alertView);
	layout->addWidget(hRadio, 0, Qt::AlignLeft);
	layout->addWidget(vRadio, 0, Qt::AlignLeft);
	hRadio->setChecked(true);
	alertView.ui->contentLayout->addLayout(layout);
	alertView.ui->contentLayout->setAlignment(layout, Qt::AlignHCenter | Qt::AlignVCenter);
	auto ret = static_cast<Button>(alertView.exec());
	selectVRadio = vRadio->isChecked();
	return ret;
}

#include "PLSAlertView.moc"
