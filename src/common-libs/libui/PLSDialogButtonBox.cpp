#include "PLSDialogButtonBox.h"

#include <qpushbutton.h>
#include <qvariant.h>
#include <qstyle.h>
#include <qcoreevent.h>
#include <qboxlayout.h>

#include <liblog.h>

#include <libutils-api.h>
#include "PLSDialogView.h"

static PLSDialogView *getDialogView(QWidget *widget)
{
	PLSDialogView *dialogView = nullptr;
	for (QWidget *parent = widget; !dialogView && parent; parent = parent->parentWidget()) {
		dialogView = dynamic_cast<PLSDialogView *>(parent);
	}
	return dialogView;
}

PLSDialogButtonBox::PLSDialogButtonBox(QWidget *parent) : QDialogButtonBox(parent)
{
	//setStyleSheet(" QDialogButtonBox { background: red; margin: 0; padding: 0; min-height: 0px; max-height: 999999px; } ");
}

void PLSDialogButtonBox::updateStandardButtonsStyle(QDialogButtonBox *buttonBox, const ButtonCb &buttonCb)
{
#define BUTTON_USE_FOR(Name)                                                                           \
	do {                                                                                           \
		if (QPushButton *button = buttonBox->button(StandardButton::Name)) {                   \
			if (buttonCb)                                                                  \
				buttonCb(button, StandardButton::Name, buttonBox->buttonRole(button)); \
			button->setObjectName(QStringLiteral(#Name).toLower());                        \
			button->setProperty("useFor", QStringLiteral(#Name));                          \
			button->setProperty("ui-step.controls", QStringLiteral(#Name));                \
			pls_flush_style(button);                                                       \
		}                                                                                      \
	} while (false)

	BUTTON_USE_FOR(Ok);
	BUTTON_USE_FOR(Save);
	BUTTON_USE_FOR(SaveAll);
	BUTTON_USE_FOR(Open);
	BUTTON_USE_FOR(Yes);
	BUTTON_USE_FOR(YesToAll);
	BUTTON_USE_FOR(No);
	BUTTON_USE_FOR(NoToAll);
	BUTTON_USE_FOR(Abort);
	BUTTON_USE_FOR(Retry);
	BUTTON_USE_FOR(Ignore);
	BUTTON_USE_FOR(Close);
	BUTTON_USE_FOR(Cancel);
	BUTTON_USE_FOR(Discard);
	BUTTON_USE_FOR(Help);
	BUTTON_USE_FOR(Apply);
	BUTTON_USE_FOR(Reset);
	BUTTON_USE_FOR(RestoreDefaults);

#undef BUTTON_USE_FOR

	if (auto l = pls_dynamic_cast<QBoxLayout>(buttonBox->layout()); l && l->spacing() < 10) {
		l->setSpacing(10);
	}
}

void PLSDialogButtonBox::clear()
{
	QDialogButtonBox::clear();
	m_btns.clear();
}

QList<QAbstractButton *> PLSDialogButtonBox::buttons() const
{
	return QDialogButtonBox::buttons();
}
PLSDialogButtonBox::ButtonRole PLSDialogButtonBox::buttonRole(QAbstractButton *button) const
{
	for (const auto &btn : m_btns)
		if (btn.m_btn == button)
			return btn.m_role;
	return ButtonRole::InvalidRole;
}

void PLSDialogButtonBox::setStandardButtons(StandardButtons buttons)
{
	QDialogButtonBox::setStandardButtons(buttons);
	m_btns.clear();
	updateStandardButtonsStyle(this, [this](QPushButton *btn, StandardButton button, ButtonRole role) {
		m_btns.append(Btn(btn, button, role));
		if (button == StandardButton::Discard)
			QDialogButtonBox::addButton(btn, ResetRole);
	});
}
PLSDialogButtonBox::StandardButtons PLSDialogButtonBox::standardButtons() const
{
	StandardButtons buttons;
	for (const auto &btn : m_btns)
		buttons.setFlag(btn.m_button, true);
	return buttons;
}
PLSDialogButtonBox::StandardButton PLSDialogButtonBox::standardButton(QAbstractButton *button) const
{
	for (const auto &btn : m_btns)
		if (btn.m_btn == button)
			return btn.m_button;
	return StandardButton::NoButton;
}
QPushButton *PLSDialogButtonBox::button(StandardButton which) const
{
	for (const auto &btn : m_btns)
		if (btn.m_button == which)
			return btn.m_btn;
	return nullptr;
}

QString PLSDialogButtonBox::extraLog() const
{
	return m_extraLog;
}
void PLSDialogButtonBox::setExtraLog(const QString &extraLog)
{
	m_extraLog = extraLog;

#define BUTTON_EXTRA_LOG(Name)                                                  \
	do {                                                                    \
		if (QPushButton *button = this->button(StandardButton::Name)) { \
			button->setProperty("ui-step.additional", extraLog);    \
		}                                                               \
	} while (false)

	BUTTON_EXTRA_LOG(Ok);
	BUTTON_EXTRA_LOG(Save);
	BUTTON_EXTRA_LOG(SaveAll);
	BUTTON_EXTRA_LOG(Open);
	BUTTON_EXTRA_LOG(Yes);
	BUTTON_EXTRA_LOG(YesToAll);
	BUTTON_EXTRA_LOG(No);
	BUTTON_EXTRA_LOG(NoToAll);
	BUTTON_EXTRA_LOG(Abort);
	BUTTON_EXTRA_LOG(Retry);
	BUTTON_EXTRA_LOG(Ignore);
	BUTTON_EXTRA_LOG(Close);
	BUTTON_EXTRA_LOG(Cancel);
	BUTTON_EXTRA_LOG(Discard);
	BUTTON_EXTRA_LOG(Help);
	BUTTON_EXTRA_LOG(Apply);
	BUTTON_EXTRA_LOG(Reset);
	BUTTON_EXTRA_LOG(RestoreDefaults);

#undef BUTTON_EXTRA_LOG
}

bool PLSDialogButtonBox::event(QEvent *event)
{
	if (event->type() == QEvent::DynamicPropertyChange) {
		auto dpce = dynamic_cast<QDynamicPropertyChangeEvent *>(event);
		if ((dpce->propertyName() == "autoClose") && property(dpce->propertyName().constData()).toBool()) {
			if (const PLSDialogView *dialogView = getDialogView(this); dialogView) {
				connect(this, &QDialogButtonBox::accepted, dialogView, &PLSDialogView::accept);
				connect(this, &QDialogButtonBox::rejected, dialogView, &PLSDialogView::reject);
			}
		}
	}
	return QDialogButtonBox::event(event);
}
