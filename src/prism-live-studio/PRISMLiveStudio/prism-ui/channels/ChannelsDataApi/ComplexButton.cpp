#include "ComplexButton.h"
#include <QStyle>
#include "ui_ComplexButton.h"

ComplexButton::ComplexButton(QWidget *parent) : QFrame(parent), ui(new Ui::ComplexButton)
{
	ui->setupUi(this);
	this->installEventFilter(this);
	ui->IconButton->installEventFilter(this);
	ui->TextButton->installEventFilter(this);
	this->setAttribute(Qt::WA_Hover);
}

ComplexButton::~ComplexButton()
{
	delete ui;
}

void ComplexButton::setAliginment(Qt::Alignment ali, int space)
{
	switch (ali) {
	case Qt::AlignLeft:
		ui->horizontalLayout->addStretch(space);
		break;
	case Qt::AlignCenter:
		ui->horizontalLayout->insertStretch(0, space);
		ui->horizontalLayout->addStretch(space);
		break;
	case Qt::AlignRight:
		ui->horizontalLayout->insertStretch(0, space);
		break;
	default:
		break;
	}
}

void ComplexButton::setText(const QString &txt)
{
	ui->TextButton->setText(txt);
}

bool ComplexButton::eventFilter(QObject *watched, QEvent *event)
{
	auto widget = dynamic_cast<QWidget *>(watched);
	switch (event->type()) {
	case QEvent::Enter:
		setState("hover");
		break;
	case QEvent::Leave:
		if (watched == this) {
			setState("normal");
		}
		break;
	case QEvent::MouseButtonPress:
		setState("pressed");
		break;
	case QEvent::MouseButtonRelease:
		setState("normal");
		emit clicked();
		break;
	case QEvent::EnabledChange:
		if (!widget->isEnabled()) {
			setState("disbaled");
		}
		break;
	default:

		break;
	}
	return QFrame::eventFilter(watched, event);
}

void ComplexButton::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void ComplexButton::setState(const QString &state)
{
	ui->IconButton->setProperty("state", state);
	setWidgetState(state, ui->IconButton);
	setWidgetState(state, ui->TextButton);
	setWidgetState(state, this);
}

void ComplexButton::setWidgetState(const QString &state, QWidget *widget) const
{
	widget->setProperty("state", state);
	auto style = widget->style();
	style->unpolish(widget);
	style->polish(widget);
}
