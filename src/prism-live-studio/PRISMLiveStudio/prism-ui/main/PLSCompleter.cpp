#include "PLSCompleter.hpp"
#include <QVBoxLayout>
#include <QEvent>
#include <QApplication>

#include "frontend-api.h"
#include "libui.h"

const int ItemHeight = 40;

PLSCompleterPopupList::PLSCompleterPopupList(QWidget *toplevel, QLineEdit *lineEdit_, const QStringList &completions)
	: QFrame(toplevel, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint), lineEdit(lineEdit_)
{
	pls_add_css(this, {"QScrollBar", "PLSCompleterPopupList"});
	setMouseTracking(true);

	auto layout1 = pls_new<QVBoxLayout>(this);
	layout1->setSpacing(0);
	layout1->setContentsMargins(0, 0, 0, 0);
	this->scrollArea = pls_new<QScrollArea>(this);
	this->scrollArea->setWidgetResizable(true);
	this->scrollArea->setFrameShape(QFrame::NoFrame);
	layout1->addWidget(scrollArea);

	auto widget = pls_new<QWidget>();
	widget->setObjectName("scrollAreaWidget");
	this->scrollArea->setWidget(widget);

	auto layout2 = pls_new<QVBoxLayout>(widget);
	layout2->setSpacing(0);
	layout2->setContentsMargins(0, 0, 0, 0);

	for (auto &completion : completions) {
		auto label = pls_new<QLabel>(completion, widget);
		label->setMouseTracking(true);
		label->installEventFilter(this);
		layout2->addWidget(label);
		labels.append(label);
	}

	connect(qApp, &QApplication::applicationStateChanged, this, &PLSCompleterPopupList::hide);
	connect(lineEdit, &QLineEdit::textEdited, this, &PLSCompleterPopupList::showPopup);
}

bool PLSCompleterPopupList::isInCompleter(const QPoint &globalPos) const
{
	if (this->geometry().contains(globalPos)) {
		return true;
	}
	if (QRect(lineEdit->mapToGlobal(QPoint(0, 0)), lineEdit->size()).contains(globalPos)) {
		return true;
	}
	return false;
}

void PLSCompleterPopupList::showPopup(const QString &text)
{
	int showCount = 0;
	for (auto &label : labels) {
		if (label->text().contains(text)) {
			label->show();
			pls_flush_style(label);
			++showCount;
		} else {
			label->hide();
		}
	}

	if (showCount <= 0) {
		hide();
		return;
	}

	setFixedSize(this->lineEdit->width(), (showCount > 5 ? 5 : showCount) * ItemHeight + 5);
	move(this->lineEdit->mapToGlobal(QPoint(0, int(this->lineEdit->height()))));
	show();
}

bool PLSCompleterPopupList::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::Enter:
		if (auto label = dynamic_cast<QLabel *>(watched); label) {
			pls_flush_style(label, "state", "hover");
		}
		break;
	case QEvent::Leave:
		if (auto label = dynamic_cast<QLabel *>(watched); label) {
			pls_flush_style(label, "state", "");
		}
		break;
	case QEvent::MouseButtonRelease:
		if (auto label = dynamic_cast<QLabel *>(watched); label) {
			QSignalBlocker blocker(lineEdit);
			emit activated(label->text());
			hide();
		}
		break;
	default:
		break;
	}
	return QFrame::eventFilter(watched, event);
}

PLSCompleter::PLSCompleter(QWidget *toplevel, QLineEdit *lineEdit_, const QStringList &completions) : QObject(lineEdit_), lineEdit(lineEdit_)
{
	popupList = pls_new<PLSCompleterPopupList>(toplevel, lineEdit, completions);
	connect(popupList, &PLSCompleterPopupList::activated, this, [this](const QString &text) {
		if (lineEdit->text() != text) {
			lineEdit->setText(text);
			emit activated(text);
		}
	});

	auto focusPolicy = lineEdit->focusPolicy();
	popupList->setFocusPolicy(Qt::NoFocus);
	popupList->setFocusProxy(lineEdit);
	lineEdit->setFocusPolicy(focusPolicy);

	lineEdit->installEventFilter(this);
	popupList->installEventFilter(this);
}

PLSCompleter::~PLSCompleter()
{
	pls_delete(popupList, nullptr);
}

constexpr const char *QLINEEDIT_ATTACHED_KEY = "__QLineEdit_attached_PLSCompleter";

PLSCompleter *PLSCompleter::attachLineEdit(QWidget *toplevel, QLineEdit *lineEdit, const QStringList &completions)
{
	auto completer = (PLSCompleter *)lineEdit->property(QLINEEDIT_ATTACHED_KEY).value<void *>();
	if (completer) {
		return nullptr;
	}

	completer = pls_new<PLSCompleter>(toplevel, lineEdit, completions);
	lineEdit->setProperty(QLINEEDIT_ATTACHED_KEY, QVariant::fromValue<void *>(completer));
	return completer;
}

void PLSCompleter::detachLineEdit(const QLineEdit *lineEdit)
{
	if (auto completer = (PLSCompleter *)lineEdit->property(QLINEEDIT_ATTACHED_KEY).value<void *>(); completer) {
		pls_delete(completer);
	}
}

bool PLSCompleter::eventFilter(QObject *watched, QEvent *event)
{
	if (eatFocusOut && watched == lineEdit && event->type() == QEvent::FocusOut && popupList->isVisible()) {
		return true;
	}
	if (watched == lineEdit && ((event->type() == QEvent::FocusIn) || (event->type() == QEvent::MouseButtonPress)) && !popupList->isVisible()) {
		popupList->showPopup(lineEdit->text());
	}

	if (watched == lineEdit && ((event->type() == QEvent::FocusOut) || (event->type() == QEvent::FocusIn))) {
		lineEdit->setProperty("focusState", event->type() == QEvent::FocusIn ? "FocusIn" : "FocusOut");
		pls_flush_style(lineEdit);
	}

	if (watched != popupList) {
		return QObject::eventFilter(watched, event);
	}

	switch (event->type()) {
	case QEvent::KeyPress:
		eatFocusOut = false;
		(static_cast<QObject *>(lineEdit))->event(event);
		eatFocusOut = true;
		return true;
	case QEvent::KeyRelease:
		eatFocusOut = false;
		static_cast<QObject *>(lineEdit)->event(event);
		eatFocusOut = true;
		return true;
	case QEvent::InputMethod:
	case QEvent::ShortcutOverride:
		QApplication::sendEvent(lineEdit, event);
		break;
	default:
		return false;
	}
	return false;
}
