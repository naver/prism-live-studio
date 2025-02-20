#include "PLSDialogButtonBox.h"

#include <qpushbutton.h>
#include <qvariant.h>
#include <qstyle.h>
#include <qcoreevent.h>
#include <qboxlayout.h>
#include <QMetaEnum>
#include <qpa/qplatformdialoghelper.h>

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

PLSDialogButtonBox::PLSDialogButtonBox(QWidget *parent) : QWidget(parent)
{
	//setStyleSheet(" QDialogButtonBox { background: red; margin: 0; padding: 0; min-height: 0px; max-height: 999999px; } ");
	initLayout();
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
	while (m_btns.size()) {
		QAbstractButton *button = m_btns.takeAt(0).m_btn;
		delete button;
	}
	for (int i = 0; i < ButtonRole::NRoles; ++i) {
		buttonLists[i].clear();
	}
}

QList<QAbstractButton *> PLSDialogButtonBox::buttons() const
{
	QList<QAbstractButton *> list;
	for (auto btn : m_btns) {
		list.append(btn.m_btn);
	}
	return list;
}
ButtonRole PLSDialogButtonBox::buttonRole(QAbstractButton *button) const
{
	for (const auto &btn : m_btns)
		if (btn.m_btn == button)
			return btn.m_role;
	return ButtonRole::InvalidRole;
}

void PLSDialogButtonBox::setStandardButtons(StandardButtons buttons)
{
	clear();
	createStandardButtons(buttons);
}
StandardButtons PLSDialogButtonBox::standardButtons() const
{
	StandardButtons buttons;
	for (const auto &btn : m_btns)
		buttons.setFlag(btn.m_button, true);
	return buttons;
}
StandardButton PLSDialogButtonBox::standardButton(QAbstractButton *button) const
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
QString PLSDialogButtonBox::getText(StandardButton button)
{
	QMetaEnum metaEnum = QMetaEnum::fromType<StandardButtons>();
	return metaEnum.valueToKey(button);
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

void PLSDialogButtonBox::setOrientation(Qt::Orientation orientation) {}

void PLSDialogButtonBox::setCenterButtons(bool center)
{
	if (m_center != center) {
		m_center = center;
		initLayout();
		layoutButtons();
	}
}

bool PLSDialogButtonBox::centerButtons() const
{
	return m_center;
}
void PLSDialogButtonBox::ensureFirstAcceptIsDefault()
{
	const QList<QAbstractButton *> &acceptRoleList = buttonLists[QDialogButtonBox::AcceptRole];
	QPushButton *firstAcceptButton = acceptRoleList.isEmpty() ? nullptr : qobject_cast<QPushButton *>(acceptRoleList.at(0));

	if (!firstAcceptButton)
		return;

	bool hasDefault = false;
	QWidget *dialog = nullptr;
	QWidget *p = this;
	while (p && !p->isWindow()) {
		p = p->parentWidget();
		if ((dialog = qobject_cast<QDialog *>(p)))
			break;
	}

	QWidget *parent = dialog ? dialog : this;
	Q_ASSERT(parent);

	const auto pushButtons = parent->findChildren<QPushButton *>();
	for (QPushButton *pushButton : pushButtons) {
		if (pushButton->isDefault() && pushButton != firstAcceptButton) {
			hasDefault = true;
			break;
		}
	}
	if (!hasDefault && firstAcceptButton) {
		firstAcceptButton->setDefault(true);
		// When the QDialogButtonBox is focused, and it doesn't have an
		// explicit focus widget, it will transfer focus to its focus
		// proxy, which is the first button in the layout. This behavior,
		// combined with the behavior that QPushButtons in a QDialog will
		// by default have their autoDefault set to true, results in the
		// focus proxy/first button stealing the default button status
		// immediately when the button box is focused, which is not what
		// we want. Account for this by explicitly making the firstAcceptButton
		// focused as well, unless an explicit focus widget has been set, or
		// a dialog child has Qt::StrongFocus.
		if (dialog && !(dialog->focusWidget()))
			firstAcceptButton->setFocus();
	}
}
bool PLSDialogButtonBox::event(QEvent *event)
{
	if (event->type() == QEvent::DynamicPropertyChange) {
		auto dpce = dynamic_cast<QDynamicPropertyChangeEvent *>(event);
		if ((dpce->propertyName() == "autoClose") && property(dpce->propertyName().constData()).toBool()) {
			if (const PLSDialogView *dialogView = getDialogView(this); dialogView) {
				connect(this, &PLSDialogButtonBox::accepted, dialogView, &PLSDialogView::accept);
				connect(this, &PLSDialogButtonBox::rejected, dialogView, &PLSDialogView::reject);
			}
		}
	} else if (event->type() == QEvent::Show) {
		ensureFirstAcceptIsDefault();
	}
	return QWidget::event(event);
}

QPushButton *PLSDialogButtonBox::createButton(StandardButton sbutton, bool doLayout)
{
	auto name = getText(sbutton);
	auto button = pls_new<QPushButton>(tr(name.toUtf8().constData()));
	button->setObjectName(name.toLower());
	button->setProperty("useFor", name);
	button->setProperty("ui-step.controls", name);
	pls_flush_style(button);
	Btn btn(button, sbutton, buttonRole(sbutton));
	m_btns.append(btn);
	buttonLists[buttonRole(sbutton)].append(button);
	QObject::connect(button, &QPushButton::clicked, this, &PLSDialogButtonBox::handleButtonClicked);

	return button;
}

void PLSDialogButtonBox::createStandardButtons(StandardButtons buttons)
{
	uint i = QDialogButtonBox::FirstButton;
	while (i <= QDialogButtonBox::LastButton) {
		if (i & buttons) {
			createButton(QDialogButtonBox::StandardButton(i), false);
		}
		i = i << 1;
	}
	layoutButtons();
}

ButtonRole PLSDialogButtonBox::buttonRole(QDialogButtonBox::StandardButton sbutton)
{
	switch (sbutton) {
	case QDialogButtonBox::NoButton:
		return QDialogButtonBox::InvalidRole;

	case QDialogButtonBox::Ok:
	case QDialogButtonBox::Save:
	case QDialogButtonBox::SaveAll:
	case QDialogButtonBox::Open:
	case QDialogButtonBox::Retry:
	case QDialogButtonBox::Ignore:
		return QDialogButtonBox::AcceptRole;

	case QDialogButtonBox::Yes:
	case QDialogButtonBox::YesToAll:
		return QDialogButtonBox::YesRole;

	case QDialogButtonBox::No:
	case QDialogButtonBox::NoToAll:
		return QDialogButtonBox::NoRole;

	case QDialogButtonBox::Abort:
	case QDialogButtonBox::Close:
	case QDialogButtonBox::Cancel:
		return QDialogButtonBox::RejectRole;

	case QDialogButtonBox::Discard:
		return QDialogButtonBox::DestructiveRole;

	case QDialogButtonBox::Help:
		return QDialogButtonBox::HelpRole;

	case QDialogButtonBox::Apply:
		return QDialogButtonBox::ApplyRole;

	case QDialogButtonBox::Reset:
	case QDialogButtonBox::RestoreDefaults:
		return QDialogButtonBox::ResetRole;

	default:
		assert(false);
		return QDialogButtonBox::InvalidRole;
	}
}

void PLSDialogButtonBox::layoutButtons()
{
	const int MacGap = 36 - 8; // 8 is the default gap between a widget and a spacer item

	for (int i = m_buttonLayout->count() - 1; i >= 0; --i) {
		QLayoutItem *item = m_buttonLayout->takeAt(i);
		if (QWidget *widget = item->widget())
			widget->hide();
		delete item;
	}

	int tmpPolicy = m_layoutPolicy;

	static const int M = 5;
	static const int ModalRoles[M] = {QPlatformDialogHelper::AcceptRole, QPlatformDialogHelper::RejectRole, QPlatformDialogHelper::DestructiveRole, QPlatformDialogHelper::YesRole,
					  QPlatformDialogHelper::NoRole};

	const int *currentLayout = QPlatformDialogHelper::buttonLayout(Qt::Horizontal, static_cast<QPlatformDialogHelper::ButtonLayout>(tmpPolicy));

	const QList<QAbstractButton *> &acceptRoleList = buttonLists[QPlatformDialogHelper::AcceptRole];

	while (*currentLayout != QPlatformDialogHelper::EOL) {
		int role = (*currentLayout & ~QPlatformDialogHelper::Reverse);
		bool reverse = (*currentLayout & QPlatformDialogHelper::Reverse);

		switch (role) {
		case QPlatformDialogHelper::Stretch:
			m_buttonLayout->addStretch();
			break;
		case QPlatformDialogHelper::AcceptRole: {
			if (acceptRoleList.isEmpty())
				break;
			// Only the first one
			QAbstractButton *button = acceptRoleList.first();
			m_buttonLayout->addWidget(button);
			button->show();
		} break;
		case QPlatformDialogHelper::AlternateRole:
			if (acceptRoleList.size() > 1)
				addButtonsToLayout(acceptRoleList.mid(1), reverse);
			break;
		case QPlatformDialogHelper::DestructiveRole:
		case QPlatformDialogHelper::RejectRole:
		case QPlatformDialogHelper::ActionRole:
		case QPlatformDialogHelper::HelpRole:
		case QPlatformDialogHelper::YesRole:
		case QPlatformDialogHelper::NoRole:
		case QPlatformDialogHelper::ApplyRole:
		case QPlatformDialogHelper::ResetRole:
			addButtonsToLayout(buttonLists[role], reverse);
		}
		++currentLayout;
	}

	QWidget *lastWidget = nullptr;
	setFocusProxy(nullptr);
	for (int i = 0; i < m_buttonLayout->count(); ++i) {
		QLayoutItem *item = m_buttonLayout->itemAt(i);
		if (QWidget *widget = item->widget()) {
			if (lastWidget)
				QWidget::setTabOrder(lastWidget, widget);
			else
				setFocusProxy(widget);
			lastWidget = widget;
		}
	}
	if (m_center) {
		m_buttonLayout->setAlignment(Qt::AlignCenter);
		m_buttonLayout->addStretch();
	}
}

void PLSDialogButtonBox::initLayout()
{
	if (m_buttonLayout)
		delete m_buttonLayout;

	m_layoutPolicy = QDialogButtonBox::ButtonLayout(this->style()->styleHint(QStyle::SH_DialogButtonLayout, nullptr, this));
	m_buttonLayout = pls_new<QHBoxLayout>();
	m_buttonLayout->setContentsMargins(0, 0, 0, 0);
	m_buttonLayout->setSpacing(10);
	setLayout(m_buttonLayout);
}
void PLSDialogButtonBox::addButtonsToLayout(const QList<QAbstractButton *> &buttonList, bool reverse)
{
	int start = reverse ? buttonList.size() - 1 : 0;
	int end = reverse ? -1 : buttonList.size();
	int step = reverse ? -1 : 1;

	for (int i = start; i != end; i += step) {
		QAbstractButton *button = buttonList.at(i);
		m_buttonLayout->addWidget(button);
		button->show();
	}
}

void PLSDialogButtonBox::handleButtonClicked()
{
	if (QAbstractButton *button = qobject_cast<QAbstractButton *>(sender()); button) {
		// Can't fetch this *after* emitting clicked, as clicked may destroy the button
		// or change its role. Now changing the role is not possible yet, but arguably
		// both clicked and accepted/rejected/etc. should be emitted "atomically"
		// depending on whatever role the button had at the time of the click.
		auto role = buttonRole(button);

		emit clicked(button);

		switch (role) {
		case ButtonRole::AcceptRole:
		case ButtonRole::YesRole:
			emit accepted();
			break;
		case ButtonRole::RejectRole:
		case ButtonRole::NoRole:
			emit rejected();
			break;
		case ButtonRole::HelpRole:
			emit helpRequested();
			break;
		default:
			break;
		}
	}
}
